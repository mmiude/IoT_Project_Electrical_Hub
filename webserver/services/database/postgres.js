const postgres = require("postgres");
const config = require("../../config/app.config");

// Initialize a single connection pool for the entire application
const sql = postgres({
    host: config.dbHost,
    port: config.dbPort,
    database: config.dbName,
    user: config.dbUser,
    password: config.dbPassword,
    max: 10 // Max connections in pool (adjust based on load)
});

class Postgres {
    constructor() {
        this.sql = sql;
    }

    async hub_created(hub_id) {
        const found_hub = await this.sql`
            SELECT 1 FROM hub WHERE id = ${hub_id}
        `;
        return found_hub.count > 0;
    }

    async create_hub(hub_id) {
        const q = await this.sql`
            INSERT INTO hub (id)
            VALUES (${hub_id})
            RETURNING id
        `;
        return q.count > 0;
    }

    async sign_hub_to_user(userId, hub_id, signature) {
        const q = await this.sql`
            UPDATE hub_user
            SET
                hub_id = ${hub_id},
                hub_signature = ${signature}
            WHERE id = ${userId}
            RETURNING id
        `;
        return q.count > 0;
    }

    async delete_hub_from_user(userId) {
        const q = await this.sql`
            UPDATE hub_user
            SET hub_id = NULL, hub_signature = NULL
            WHERE id = ${userId}
            RETURNING id
        `;
        return q.count > 0;
    }

    async find_or_create_user(name, email, profilePicture) {
        const [existingUser] = await this.sql`
            SELECT id, name, email FROM hub_user
            WHERE email = ${email}
        `;
        if (existingUser) return existingUser;

        const [newUser] = await this.sql`
            INSERT INTO hub_user (name, email, profile_picture)
            VALUES (${name}, ${email}, ${profilePicture})
            RETURNING id, name, email
        `;
        return newUser || null;
    }

    async find_device(device_id) {
        const q = await this.sql`
            SELECT id FROM device
            WHERE id = ${device_id}
        `
        return q.count > 0
    }

    async create_device(device_id, name, hub_id) {
        const q = await this.sql`
            INSERT INTO device (id, hub_id, name, is_on, online)
            VALUES (${device_id}, ${hub_id}, ${name}, false, true)
            RETURNING id
        `;
        return q.count > 0;
    }

    async update_hub(hubId, values) {
        if (!values || Object.keys(values).length === 0) return false;

        const q = await this.sql`
            UPDATE hub
            SET ${this.sql(values)}
            WHERE id = ${hubId}
            RETURNING id
        `;
        return q.count > 0;
    }

    async update_device(device_id, values) {
        if (!values || Object.keys(values).length === 0) return false;

        const q = await this.sql`
            UPDATE device
            SET ${this.sql(values)}
            WHERE id = ${device_id}
            RETURNING id
        `;
        return q.count > 0;
    }

    async put_device_reading(device_id, type, value) {
        const q = await this.sql`
            INSERT INTO device_readings (device_id, type, value, timeperiod)
            VALUES (${device_id}, ${type}, ${value}, ${new Date().toISOString()})
            RETURNING device_id
        `;
        return q.count > 0;
    }

    async get_user_data(userId) {
        const [userData] = await this.sql`
            SELECT
                hu.hub_id,
                hu.profile_picture,
                h.threshold_low,
                h.threshold_med,
                h.electricity_price
            FROM hub_user hu
            LEFT JOIN hub h ON hu.hub_id = h.id
            WHERE hu.id = ${userId}
        `;

        if (!userData) {
            return {
                hub_id: null,
                profile_picture: null,
                threshold_low: null,
                threshold_med: null,
                electricity_price: null
            };
        }

        return userData;
    }

    async get_user_hub_signature(userId) {
        const [row] = await this.sql`
            SELECT hub_signature FROM hub_user WHERE id = ${userId}
        `;
        return row ? row.hub_signature : null;
    }

    async get_user_devices(userId) {
        return await this.sql`
            SELECT d.id, d.hub_id, d.name, d.priority, d.is_on, d.online
            FROM device d
            INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
            WHERE hu.id = ${userId}
        `;
    }

    async get_user_devices_readings(userId) {
        return await this.sql`
            SELECT
                dr.id,
                dr.device_id,
                dr.type,
                dr.value,
                dr.timeperiod + INTERVAL '3 hours' as timeperiod
            FROM device_readings dr
            INNER JOIN device d ON dr.device_id = d.id
            INNER JOIN hub_user hu ON d.hub_id = hu.hub_id
            WHERE hu.id = ${userId}
        `;
    }

    async get_hourly_energy_by_date(userId, date) {
        const q = await sql`
            SELECT * FROM get_hourly_energy_by_date(${date}, ${userId})
        `
        return q
    }
    async get_daily_energy_by_week(userId, date) {
        const q = await sql`
            SELECT * FROM get_daily_energy_by_week(${date}, ${userId})
        `
        return q
    }
    async get_daily_energy_by_month(userId, date) {
        const q = await sql`
            SELECT * FROM get_daily_energy_by_month(${date}, ${userId})
        `
        return q
    }
    async get_monthly_energy_by_year(userId, date) {
        const q = await sql`
            SELECT * FROM get_monthly_energy_by_year(${date}, ${userId})
        `
        return q
    }

    async get_user_hub_logs(userId) {
        return await this.sql`
            SELECT hl.id, hl.log_text, hl.timestamp
            FROM hub_logs hl
            INNER JOIN hub_user hu ON hl.hub_id = hu.hub_id
            WHERE hu.id = ${userId}
            ORDER BY hl.timestamp DESC
        `;
    }

    async delete_hub_log(logId) {
        const deleted = await this.sql`
            DELETE FROM hub_logs WHERE id = ${logId}
        `;
        return deleted.count > 0;
    }
    async delete_hub_logs(hubId) {
        const deleted = await this.sql`
            DELETE FROM hub_logs WHERE hub_id = ${hubId}
        `
        return deleted.count > 0
    }

    async delete_device(deviceId) {
        await this.sql`
            DELETE FROM device_readings WHERE device_id = ${deviceId}
        `
        const deleted = await this.sql`
            DELETE FROM device WHERE id = ${deviceId}
        `
        return deleted.count > 0

    }

    async log_to_hub(hubId, text) {
        const q = await this.sql`
            INSERT INTO hub_logs (hub_id, log_text, timestamp)
            VALUES (${hubId}, ${text}, ${new Date().toISOString()})
            RETURNING id
        `;
        return q.count > 0;
    }
}

// Export a singleton instance
module.exports = new Postgres();