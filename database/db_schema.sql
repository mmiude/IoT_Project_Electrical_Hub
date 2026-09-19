CREATE TABLE "hub"(
    "id" VARCHAR(255) PRIMARY KEY,
    "signature" BYTEA NOT NULL
);

CREATE TABLE "device"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "hub_id" VARCHAR(255) NOT NULL,
    "name" VARCHAR(255) NOT NULL,
    "priority" VARCHAR(255) CHECK ("priority" IN ('LOW', 'MED', 'HIGH')) NOT NULL DEFAULT 'HIGH',
    "is_on" BOOLEAN NOT NULL,
    CONSTRAINT "device_hub_id_foreign" FOREIGN KEY ("hub_id") REFERENCES "hub"("id")
);

CREATE TABLE "device_readings"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "device_id" BIGINT NOT NULL,
    "power_w" DOUBLE PRECISION,
    "energy_J" DOUBLE PRECISION,
    "voltage_V" DOUBLE PRECISION,
    "current_A" DOUBLE PRECISION,
    -- "timeperiod_from" TIMESTAMP(0) WITHOUT TIME ZONE NOT NULL,
    "timeperiod" TIMESTAMP(0) WITHOUT TIME ZONE NOT NULL,
    CONSTRAINT "device_readings_device_id_foreign" FOREIGN KEY ("device_id") REFERENCES "device"("id")
);

CREATE TABLE "hub_user"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "name" VARCHAR(255) NOT NULL,
    "email" VARCHAR(255) NOT NULL,
    "profile_picture" VARCHAR(255),
    "hub_id" VARCHAR(255),
    CONSTRAINT "hub_user_hub_id_foreign" FOREIGN KEY ("hub_id") REFERENCES "hub"("id")
);

CREATE TABLE "valid_hub_ids"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "hub_id" VARCHAR(255) NOT NULL
);