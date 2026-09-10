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

CREATE TABLE "power_consumption"(
    "id" BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "device_id" BIGINT NOT NULL,
    "consumption_kWh" DOUBLE PRECISION NOT NULL,
    "timeperiod_from" TIMESTAMP(0) WITHOUT TIME ZONE NOT NULL,
    "timeperiod_to" TIMESTAMP(0) WITHOUT TIME ZONE NOT NULL,
    CONSTRAINT "power_consumption_device_id_foreign" FOREIGN KEY ("device_id") REFERENCES "device"("id")
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