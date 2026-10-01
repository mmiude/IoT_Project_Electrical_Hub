const postgres = require("postgres")
const config = require("../../config/app.config")

// const sql = postgres({
//     host: config.dbHost,
//     port: config.dbPort,
//     database: config.dbName,
//     user: config.dbUser,
//     password: config.dbPassword
// });

class Postgres {
    constructor() {
        this.sql = postgres({
            host: config.dbHost,
            port: config.dbPort,
            database: config.dbName,
            user: config.dbUser,
            password: config.dbPassword
        });
    }

    async hub_created(hub_id) {
        const found_hub = await this.sql`
            SELECT * FROM hub
            WHERE id = ${hub_id}
        `
        // console.log(`Hub ids ${found_hub.count}`)
        return found_hub.count > 0
    }

    async create_hub(hub_id) {
        const q = await this.sql`
            INSERT INTO hub (id)
            VALUES (${hub_id})
            RETURNING id
        `
        return q.count > 0
    }

    async sign_hub_to_user(userId, hub_id, signature) {
        // const q = await this.sql`
        //     INSERT INTO hub (id, signature)
        //     VALUES (${hub_id}, ${signature})
        //     RETURNING hub_id
        // `
        const q = await this.sql`
            UPDATE hub_user
            SET
                hub_id = ${hub_id},
                hub_signature = ${signature}
            WHERE
                id = ${userId}
            RETURNING
                id
        `
        return q.count > 0
    }

    async delete_hub_from_user(userId) {
        const q = await this.sql`
            UPDATE hub_user
            SET hub_id = NULL, hub_signature = NULL
            WHERE id = ${userId}
            RETURNING id
        `
        return q.count > 0
    }

    async find_or_create_user(name, email, profilePicture) {
        const user = await this.sql`
            SELECT id, name, email FROM hub_user
            WHERE email = ${email}
        `
        if (user.count > 0) {
            return user[0]
        }

        const newUser = await this.sql`
            INSERT INTO hub_user (name, email, profile_picture)
            VALUES (${name}, ${email}, ${profilePicture})
            RETURNING id, name, email
        `
        if (newUser.count <= 0) return null
        return newUser[0]
    }

    async create_device(device_id, name, hub_id) {
        const q = await this.sql`
            INSERT INTO device
            (id, hub_id, name, is_on)
            VALUES
            (${device_id}, ${hub_id}, ${name}, false)
            RETURNING id
        `
        return q.count > 0
    }

    async update_hub(hubId, values) {
        if (!values || Object.keys(values).length === 0) {
            return false;
        } 

        const q = await this.sql`
            UPDATE hub
            SET ${this.sql(values)}
            WHERE id = ${hubId}
            RETURNING id
        `
        return q.count > 0

    }
    
    async update_device(device_id, values) {
        if (!values || Object.keys(values).length === 0) {
            return false;
        }
        
        const q = await this.sql`
            UPDATE device
            SET ${ this.sql(values) }
            WHERE id = ${ device_id }
            RETURNING id
        `;
        console.log(q)
        return q.count > 0
    }

    async put_device_reading(device_id, type, value) {
        const d = new Date()
        const q = await this.sql`
            INSERT INTO device_readings
            (device_id, type, value, timeperiod)
            VALUES
            (${device_id}, ${type}, ${value}, ${d.toISOString()})
            RETURNING device_id
        `
        return q.count > 0
    }

    async get_user_data(userId) {
        const noData = {
            hub_id: null,
            profile_picture: null,
            threshold_low: null,
            threshold_med: null,
            electricity_price: null
        }

        const user_hub = await this.sql`
            SELECT hub_id
            FROM hub_user
            WHERE id = ${userId}
        `
        if (user_hub[0].hub_id == null) {
            const user_data = await this.sql`
                SELECT profile_picture
                FROM hub_user
                WHERE id = ${userId}
            `
            console.log(user_data)
            if (user_data.count < 1) {
                return noData
            }
            // console.log(user_data)
            const { profile_picture } = user_data[0]
            return {
                hub_id: null,
                profile_picture,
                threshold_low: null,
                threshold_med: null,
                electricity_price: null
            }
        } else {
            const user_data = await this.sql`
                SELECT
                    hu.hub_id,
                    hu.profile_picture,
                    h.threshold_low,
                    h.threshold_med,
                    h.electricity_price
                FROM
                    hub_user hu
                INNER JOIN
                    hub h
                ON
                    hu.hub_id = h.id
                WHERE
                    hu.id = ${userId}
            `
            if (user_data.count < 1) {
                return noData
            }
            // console.log(user_data)
            const {
                hub_id,
                profile_picture,
                threshold_low,
                threshold_med,
                electricity_price
            } = user_data[0]
            return {
                hub_id,
                profile_picture,
                threshold_low,
                threshold_med,
                electricity_price
            }
        }

    }

    async get_user_hub_signature(userId) {
        const signature = await this.sql`
            SELECT hub_signature
            FROM hub_user
            WHERE id = ${userId}
        `
        if (signature.count < 1) return null
        const { hub_signature } = signature[0]
        return hub_signature
    }

    async get_user_devices(userId) {
        const devices = await this.sql`
            SELECT
                d.id, d.hub_id, d.name, d.priority, d.is_on
            FROM
                device d
            INNER JOIN
                hub_user hu
            ON
                d.hub_id = hu.hub_id
            WHERE
                hu.id = ${userId}
        `
        return devices
    }

    async get_user_devices_readings(userId) {
        const device_readings = await this.sql`
            SELECT
                dr.id,
                dr.device_id,
                dr.type,
                dr.value,
                dr.timeperiod + INTERVAL '3 hours' as timeperiod
            FROM
                device_readings dr
            INNER JOIN
                device d
            ON
                dr.device_id = d.id
            INNER JOIN
                hub_user hu
            ON
                d.hub_id = hu.hub_id
            WHERE
                hu.id = ${userId}
        `
        return device_readings
    }

    async get_user_hub_logs(userId) {
        const logs = await this.sql`
            SELECT
                hl.id,
                hl.log_text,
                hl.timestamp
            FROM
                hub_logs hl
            INNER JOIN
                hub_user hu
            ON
                hl.hub_id = hu.hub_id
            WHERE
                hu.id = ${userId}
            ORDER BY
                hl.timestamp DESC;
        `
        return logs
    }

    async delete_hub_log(logId) {
        const deleted = await this.sql`
            DELETE FROM hub_logs
            WHERE id = ${logId}
        `
        return deleted.count > 0
    }

    async log_to_hub(hubId, text) {
        const d = new Date()
        const q = await this.sql`
            INSERT INTO hub_logs
            (hub_id, log_text, timestamp)
            VALUES
            (${hubId}, ${text}, ${d.toISOString()})
            RETURNING id
        `
        return q.count > 0
    }
}

module.exports = Postgres