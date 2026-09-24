CREATE TABLE "hub"(
    "id" VARCHAR(255) PRIMARY KEY,
    "signature" BYTEA NOT NULL
);

CREATE TABLE "device"(
    "id" BIGINT PRIMARY KEY,
    "hub_id" VARCHAR(255) NOT NULL,
    "name" VARCHAR(255),
    "priority" VARCHAR(255) CHECK ("priority" IN ('LOW', 'MED', 'HIGH')) NOT NULL DEFAULT 'HIGH',
    "is_on" BOOLEAN NOT NULL,
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
    "device_id" BIGINT NOT NULL,
    "type" VARCHAR(255) CHECK ("type" IN ('DATA_TYPE_POWER', 'DATA_TYPE_ENERGY', 'DATA_TYPE_VOLTAGE', 'DATA_TYPE_CURRENT')) NOT NULL DEFAULT 'DATA_TYPE_POWER',
    "value" DOUBLE PRECISION,
    "timeperiod" TIMESTAMP(0) WITHOUT TIME ZONE,
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