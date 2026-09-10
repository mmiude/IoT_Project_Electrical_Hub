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
        return found_hub_id.count > 0
    }

    async log_hub_id(hub_id) {
        const hub = await this.sql`
            INSERT INTO valid_hub_ids (hub_id)
            VALUES (${hub_id})
        `
    }

    async create_hub(hub_id, signature) {
        const hub = await this.sql`
            INSERT INTO hub (id, signature)
            VALUES (${hub_id}, ${signature})
        `
    }

    async find_or_create_user(name, email, profilePicture) {
        const user = await this.sql`
            SELECT id FROM hub_user
            WHERE email = ${email}
        `
        if (user.count > 0) {
            return user[0]["id"]
        }

        const newUser = await this.sql`
            INSERT INTO hub_user (name, email, profile_picture)
            VALUES (${name}, ${email}, ${profilePicture})
            RETURNING id
        `
        if (newUser.count <= 0) return null
        return newUser[0]["id"]
    }
}

module.exports = Postgres