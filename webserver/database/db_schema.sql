CREATE TABLE "hub"(
    "id" VARCHAR(255) PRIMARY KEY,
    "threshold_low" DOUBlE PRECISION,
    "threshold_med" DOUBlE PRECISION,
    "electricity_price" DOUBLE PRECISION
);

CREATE TABLE "device"(
    "id" NUMERIC(20, 0) PRIMARY KEY,
    "hub_id" VARCHAR(255) NOT NULL,
    "name" VARCHAR(255),
    "priority" VARCHAR(255) CHECK ("priority" IN ('LOW', 'MED', 'HIGH')) NOT NULL DEFAULT 'HIGH',
    "is_on" BOOLEAN NOT NULL,
    "online" BOOLEAN NOT NULL,
    CONSTRAINT "device_hub_id_foreign" FOREIGN KEY ("hub_id") REFERENCES "hub"("id")
);
CREATE OR REPLACE FUNCTION set_default_device_name()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.name IS NULL THEN
        NEW.name := 'device_' || NEW.id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_set_device_name
BEFORE INSERT ON "device"
FOR EACH ROW
EXECUTE FUNCTION set_default_device_name();

CREATE TABLE "device_readings"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "device_id" NUMERIC(20, 0) NOT NULL,
    "type" VARCHAR(255) CHECK ("type" IN ('DATA_TYPE_POWER', 'DATA_TYPE_ENERGY', 'DATA_TYPE_VOLTAGE', 'DATA_TYPE_CURRENT')) NOT NULL DEFAULT 'DATA_TYPE_POWER',
    "value" DOUBLE PRECISION,
    "timeperiod" TIMESTAMP(0) WITHOUT TIME ZONE,
    "price_during_reading" DOUBLE PRECISION,
    CONSTRAINT "device_readings_device_id_foreign" FOREIGN KEY ("device_id") REFERENCES "device"("id")
);
CREATE OR REPLACE FUNCTION populate_price_during_reading()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.price_during_reading IS NULL THEN
        SELECT h.electricity_price
        INTO NEW.price_during_reading
        FROM "hub" h
        JOIN "device" d ON d.hub_id = h.id
        WHERE d.id = NEW.device_id;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_populate_price_during_reading
BEFORE INSERT ON "device_readings"
FOR EACH ROW
EXECUTE FUNCTION populate_price_during_reading();

CREATE TABLE "hub_user"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "name" VARCHAR(255) NOT NULL,
    "email" VARCHAR(255) NOT NULL,
    "profile_picture" VARCHAR(255),
    "hub_id" VARCHAR(255),
    "hub_signature" BYTEA,
    CONSTRAINT "hub_user_hub_id_foreign" FOREIGN KEY ("hub_id") REFERENCES "hub"("id")
);

CREATE TABLE "hub_logs"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "hub_id" VARCHAR(255) NOT NULL,
    "log_text" TEXT,
    "timestamp" TIMESTAMP(0) WITHOUT TIME ZONE,
    CONSTRAINT "hub_logs_hub_id_foreign" FOREIGN KEY ("hub_id") REFERENCES "hub"("id")
);

-- Per hour of day
CREATE OR REPLACE FUNCTION get_hourly_energy_by_date(target_day DATE, userId BIGINT)
RETURNS TABLE (
    device_id NUMERIC(20, 0),
    device_name VARCHAR(255),
    timeperiod TEXT,
    kwh_consumed NUMERIC,
    avg_price NUMERIC
) 
LANGUAGE sql AS $$
WITH user_devices AS (
    SELECT
        d.id AS device_id,
        d.name as device_name
    FROM device d
    INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
    WHERE hu.id = userId
),
power_readings AS (
    SELECT 
        dr.device_id,
        dr.timeperiod + INTERVAL '3 hours' AS shifted_timeperiod,
        dr.value AS power_w,
        dr.price_during_reading AS price,
        LAG(dr.timeperiod + INTERVAL '3 hours') OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_shifted_timeperiod,
        LAG(dr.value) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_power_w,
        LAG(dr.price_during_reading) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_price
    FROM device_readings dr
    INNER JOIN user_devices ud ON dr.device_id = ud.device_id
    WHERE dr.type = 'DATA_TYPE_POWER'
),
time_intervals AS (
    SELECT 
        p.device_id,
        date_trunc('hour', p.shifted_timeperiod) AS hour_bucket,
        EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) AS delta_seconds,
        (p.power_w + p.prev_power_w) / 2.0 AS avg_power_w,
        (COALESCE(p.price, p.prev_price) + COALESCE(p.prev_price, p.price)) / 2.0 AS avg_price_val
    FROM power_readings p
    WHERE p.prev_shifted_timeperiod IS NOT NULL
      AND p.shifted_timeperiod >= target_day
      AND p.shifted_timeperiod < target_day + INTERVAL '1 day'
      AND EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) <= 3600
),
hourly_calculated AS (
    SELECT 
        device_id,
        hour_bucket,
        SUM(avg_power_w * (delta_seconds / 3600.0)) / 1000.0 AS kwh,
        AVG(avg_price_val) AS avg_price
    FROM time_intervals
    GROUP BY device_id, hour_bucket
),
hours_series AS (
    SELECT generate_series(
        target_day::timestamp,
        target_day::timestamp + INTERVAL '23 hours',
        INTERVAL '1 hour'
    ) AS hour_bucket
),
device_hours AS (
    SELECT ud.device_id, ud.device_name, h.hour_bucket
    FROM user_devices ud
    CROSS JOIN hours_series h
),
hourly_complete AS (
    SELECT 
        dh.device_id,
        dh.device_name,
        dh.hour_bucket,
        COALESCE(hc.kwh, 0.0) AS kwh,
        hc.avg_price
    FROM device_hours dh
    LEFT JOIN hourly_calculated hc 
           ON dh.device_id = hc.device_id 
          AND dh.hour_bucket = hc.hour_bucket
)
SELECT 
    device_id,
    device_name,
    CASE 
        WHEN GROUPING(hour_bucket) = 1 THEN 'TOTAL'
        ELSE TO_CHAR(hour_bucket, 'YYYY-MM-DD HH24:MI')
    END AS timeperiod,
    ROUND(SUM(kwh)::numeric, 6) AS kwh_consumed,
    ROUND(AVG(avg_price)::numeric, 4) AS avg_price
FROM hourly_complete
GROUP BY device_id, device_name, ROLLUP(hour_bucket)
ORDER BY device_id, hour_bucket NULLS LAST;
$$;

-- Per day of week
CREATE OR REPLACE FUNCTION get_daily_energy_by_week(chosen_date DATE, userId BIGINT)
RETURNS TABLE (
    device_id NUMERIC(20, 0),
    device_name VARCHAR(255),
    timeperiod TEXT,
    kwh_consumed NUMERIC,
    avg_price NUMERIC
) 
LANGUAGE sql AS $$
WITH user_devices AS (
    SELECT
        d.id AS device_id,
        d.name as device_name
    FROM device d
    INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
    WHERE hu.id = userId
),
power_readings AS (
    SELECT 
        dr.device_id,
        dr.timeperiod + INTERVAL '3 hours' AS shifted_timeperiod,
        dr.value AS power_w,
        dr.price_during_reading AS price,
        LAG(dr.timeperiod + INTERVAL '3 hours') OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_shifted_timeperiod,
        LAG(dr.value) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_power_w,
        LAG(dr.price_during_reading) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_price
    FROM device_readings dr
    INNER JOIN user_devices ud ON dr.device_id = ud.device_id
    WHERE dr.type = 'DATA_TYPE_POWER'
),
time_intervals AS (
    SELECT 
        p.device_id,
        date_trunc('day', p.shifted_timeperiod) AS day_bucket,
        EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) AS delta_seconds,
        (p.power_w + p.prev_power_w) / 2.0 AS avg_power_w,
        (COALESCE(p.price, p.prev_price) + COALESCE(p.prev_price, p.price)) / 2.0 AS avg_price_val
    FROM power_readings p
    WHERE p.prev_shifted_timeperiod IS NOT NULL
      AND p.shifted_timeperiod >= date_trunc('week', chosen_date)
      AND p.shifted_timeperiod < date_trunc('week', chosen_date) + INTERVAL '7 days'
      AND EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) <= 3600
),
daily_calculated AS (
    SELECT 
        device_id,
        day_bucket,
        SUM(avg_power_w * (delta_seconds / 3600.0)) / 1000.0 AS kwh,
        AVG(avg_price_val) AS avg_price
    FROM time_intervals
    GROUP BY device_id, day_bucket
),
days_series AS (
    SELECT generate_series(
        date_trunc('week', chosen_date),
        date_trunc('week', chosen_date) + INTERVAL '6 days',
        INTERVAL '1 day'
    ) AS day_bucket
),
device_days AS (
    SELECT ud.device_id, ud.device_name, ds.day_bucket
    FROM user_devices ud
    CROSS JOIN days_series ds
),
daily_complete AS (
    SELECT 
        dd.device_id,
        dd.device_name,
        dd.day_bucket,
        COALESCE(dc.kwh, 0.0) AS kwh,
        dc.avg_price
    FROM device_days dd
    LEFT JOIN daily_calculated dc 
           ON dd.device_id = dc.device_id 
          AND dd.day_bucket = dc.day_bucket
)
SELECT 
    device_id,
    device_name,
    CASE 
        WHEN GROUPING(day_bucket) = 1 THEN 'TOTAL'
        ELSE TO_CHAR(day_bucket, 'YYYY-MM-DD (Dy)')
    END AS timeperiod,
    ROUND(SUM(kwh)::numeric, 6) AS kwh_consumed,
    ROUND(AVG(avg_price)::numeric, 4) AS avg_price
FROM daily_complete
GROUP BY device_id, device_name, ROLLUP(day_bucket)
ORDER BY device_id, day_bucket NULLS LAST;
$$;

-- Per day of month
CREATE OR REPLACE FUNCTION get_daily_energy_by_month(chosen_date DATE, userId BIGINT)
RETURNS TABLE (
    device_id NUMERIC(20, 0),
    device_name VARCHAR(255),
    timeperiod TEXT,
    kwh_consumed NUMERIC,
    avg_price NUMERIC
) 
LANGUAGE sql AS $$
WITH user_devices AS (
    SELECT
        d.id AS device_id,
        d.name as device_name
    FROM device d
    INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
    WHERE hu.id = userId
),
power_readings AS (
    SELECT 
        dr.device_id,
        dr.timeperiod + INTERVAL '3 hours' AS shifted_timeperiod,
        dr.value AS power_w,
        dr.price_during_reading AS price,
        LAG(dr.timeperiod + INTERVAL '3 hours') OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_shifted_timeperiod,
        LAG(dr.value) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_power_w,
        LAG(dr.price_during_reading) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_price
    FROM device_readings dr
    INNER JOIN user_devices ud ON dr.device_id = ud.device_id
    WHERE dr.type = 'DATA_TYPE_POWER'
),
time_intervals AS (
    SELECT 
        p.device_id,
        date_trunc('day', p.shifted_timeperiod) AS day_bucket,
        EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) AS delta_seconds,
        (p.power_w + p.prev_power_w) / 2.0 AS avg_power_w,
        (COALESCE(p.price, p.prev_price) + COALESCE(p.prev_price, p.price)) / 2.0 AS avg_price_val
    FROM power_readings p
    WHERE p.prev_shifted_timeperiod IS NOT NULL
      AND p.shifted_timeperiod >= date_trunc('month', chosen_date)
      AND p.shifted_timeperiod < date_trunc('month', chosen_date) + INTERVAL '1 month'
      AND EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) <= 3600
),
daily_calculated AS (
    SELECT 
        device_id,
        day_bucket,
        SUM(avg_power_w * (delta_seconds / 3600.0)) / 1000.0 AS kwh,
        AVG(avg_price_val) AS avg_price
    FROM time_intervals
    GROUP BY device_id, day_bucket
),
days_series AS (
    SELECT generate_series(
        date_trunc('month', chosen_date),
        date_trunc('month', chosen_date) + INTERVAL '1 month' - INTERVAL '1 day',
        INTERVAL '1 day'
    ) AS day_bucket
),
device_days AS (
    SELECT ud.device_id, ud.device_name, ds.day_bucket
    FROM user_devices ud
    CROSS JOIN days_series ds
),
daily_complete AS (
    SELECT 
        dd.device_id,
        dd.device_name,
        dd.day_bucket,
        COALESCE(dc.kwh, 0.0) AS kwh,
        dc.avg_price
    FROM device_days dd
    LEFT JOIN daily_calculated dc 
           ON dd.device_id = dc.device_id 
          AND dd.day_bucket = dc.day_bucket
)
SELECT 
    device_id,
    device_name,
    CASE 
        WHEN GROUPING(day_bucket) = 1 THEN 'TOTAL'
        ELSE TO_CHAR(day_bucket, 'YYYY-MM-DD (Dy)')
    END AS timeperiod,
    ROUND(SUM(kwh)::numeric, 6) AS kwh_consumed,
    ROUND(AVG(avg_price)::numeric, 4) AS avg_price
FROM daily_complete
GROUP BY device_id, device_name, ROLLUP(day_bucket)
ORDER BY device_id, day_bucket NULLS LAST;
$$;

-- Per month of year
CREATE OR REPLACE FUNCTION get_monthly_energy_by_year(chosen_date DATE, userId BIGINT)
RETURNS TABLE (
    device_id NUMERIC(20, 0),
    device_name VARCHAR(255),
    timeperiod TEXT,
    kwh_consumed NUMERIC,
    avg_price NUMERIC
) 
LANGUAGE sql AS $$
WITH user_devices AS (
    SELECT
        d.id AS device_id,
        d.name as device_name
    FROM device d
    INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
    WHERE hu.id = userId
),
power_readings AS (
    SELECT 
        dr.device_id,
        dr.timeperiod + INTERVAL '3 hours' AS shifted_timeperiod,
        dr.value AS power_w,
        dr.price_during_reading AS price,
        LAG(dr.timeperiod + INTERVAL '3 hours') OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_shifted_timeperiod,
        LAG(dr.value) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_power_w,
        LAG(dr.price_during_reading) OVER (
            PARTITION BY dr.device_id ORDER BY dr.timeperiod
        ) AS prev_price
    FROM device_readings dr
    INNER JOIN user_devices ud ON dr.device_id = ud.device_id
    WHERE dr.type = 'DATA_TYPE_POWER'
),
time_intervals AS (
    SELECT 
        p.device_id,
        date_trunc('month', p.shifted_timeperiod) AS month_bucket,
        EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) AS delta_seconds,
        (p.power_w + p.prev_power_w) / 2.0 AS avg_power_w,
        (COALESCE(p.price, p.prev_price) + COALESCE(p.prev_price, p.price)) / 2.0 AS avg_price_val
    FROM power_readings p
    WHERE p.prev_shifted_timeperiod IS NOT NULL
      AND p.shifted_timeperiod >= date_trunc('year', chosen_date)
      AND p.shifted_timeperiod < date_trunc('year', chosen_date) + INTERVAL '1 year'
      AND EXTRACT(EPOCH FROM (p.shifted_timeperiod - p.prev_shifted_timeperiod)) <= 3600
),
monthly_calculated AS (
    SELECT 
        device_id,
        month_bucket,
        SUM(avg_power_w * (delta_seconds / 3600.0)) / 1000.0 AS kwh,
        AVG(avg_price_val) AS avg_price
    FROM time_intervals
    GROUP BY device_id, month_bucket
),
months_series AS (
    SELECT generate_series(
        date_trunc('year', chosen_date),
        date_trunc('year', chosen_date) + INTERVAL '11 months',
        INTERVAL '1 month'
    ) AS month_bucket
),
device_months AS (
    SELECT ud.device_id, ud.device_name, ms.month_bucket
    FROM user_devices ud
    CROSS JOIN months_series ms
),
monthly_complete AS (
    SELECT 
        dm.device_id,
        dm.device_name,
        dm.month_bucket,
        COALESCE(mc.kwh, 0.0) AS kwh,
        mc.avg_price
    FROM device_months dm
    LEFT JOIN monthly_calculated mc 
           ON dm.device_id = mc.device_id 
          AND dm.month_bucket = mc.month_bucket
)
SELECT 
    device_id,
    device_name,
    CASE 
        WHEN GROUPING(month_bucket) = 1 THEN 'TOTAL'
        ELSE TO_CHAR(month_bucket, 'YYYY-MM (Mon)')
    END AS timeperiod,
    ROUND(SUM(kwh)::numeric, 6) AS kwh_consumed,
    ROUND(AVG(avg_price)::numeric, 4) AS avg_price
FROM monthly_complete
GROUP BY device_id, device_name, ROLLUP(month_bucket)
ORDER BY device_id, month_bucket NULLS LAST;
$$;
