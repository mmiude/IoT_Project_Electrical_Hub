const postgres = require("postgres")
const config = require("../../config/app.config")

const sql = postgres({
    host: config.dbHost,
    port: config.dbPort,
    database: config.dbName,
    user: config.dbUser,
    password: config.dbPassword
});

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
        const found_hub_id = await sql`
            SELECT * FROM valid_hub_ids
            WHERE hub_id = ${hub_id}
        `
        return found_hub_id.count > 0
    }

    async log_hub_id(hub_id) {
        const hub = await sql`
            INSERT INTO valid_hub_ids (hub_id)
            VALUES (${hub_id})
        `
    }

    async create_hub(hub_id, signature) {
        const hub = await sql`
            INSERT INTO hub (id, signature)
            VALUES (${hub_id}, ${signature})
        `
    }

    async find_or_create_user(email, name, profilePicture) {
        const user = await sql`
            SELECT * FROM
        `
    }
}

module.exports = Postgres