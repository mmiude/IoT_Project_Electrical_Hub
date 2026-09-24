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

    async hub_id_logged(hub_id) {
        const found_hub_id = await this.sql`
            SELECT * FROM valid_hub_ids
            WHERE hub_id = ${hub_id}
        `
        // console.log(`Hub ids ${found_hub_id.count}`)
        return found_hub_id.count > 0
    }

    async log_hub_id(hub_id) {
        const q = await this.sql`
            INSERT INTO valid_hub_ids (hub_id)
            VALUES (${hub_id})
            RETURNING hub_id
        `
        return q.count > 0
    }

    async create_hub(hub_id, signature) {
        const q = await this.sql`
            INSERT INTO hub (id, signature)
            VALUES (${hub_id}, ${signature})
            RETURNING hub_id
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

    async create_device(device_id, hub_id) {
        const q = await this.sql`
            INSERT INTO device
            (id, hub_id, is_on)
            VALUES
            (${device_id}, ${hub_id}, false)
            RETURNING id
        `
        return q.count > 0
    }

    async update_device(device_id, values) {
        if (!values || Object.keys(values).length === 0) {
            return;
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
        console.log(d)
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
        const user_data = await this.sql`
            SELECT hub_id, profile_picture
            FROM hub_user
            WHERE id = ${userId}
        `
        if (user_data.count < 1) return null
        console.log(user_data)
        const { hub_id, profile_picture } = user_data[0]
        return { hub_id, profile_picture }
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
}

module.exports = Postgres