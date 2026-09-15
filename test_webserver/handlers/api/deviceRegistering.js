const config = require("../../config/app.config");
const Postgres = require("../../services/database/postgres");
const { verifyJwt } = require("../../utils");
const { sign } = require("../../deviceSign")

async function LogDevicetoDB(req, res) {
    const auth = req.get("Authorization")
    const token = auth.replace("Bearer ", "")

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret)
        if (!valid) {
            return res.status(500).send("Invalid token")
        }

        const pg = new Postgres()
        const logged = await pg.hub_id_logged(decodedJwt.device)
        if (!logged) {
            await pg.log_hub_id(decodedJwt.device)
        }
        await pg.sql.end()
        res.send("OK")
    } catch (error) {
        res.status(500).send(error)
    }
}

async function RegisterHub(req, res) {
    const { id } = req.params

    try {
        const pg = new Postgres()
        const valid_hub = await pg.hub_id_logged(id)
        if (!valid_hub) {
            await pg.sql.end()
            return res.status(406).send("Invalid hub id")
        }
    
        const signature = sign(id)
        await pg.create_hub(id, signature)
        await pg.sql.end()
        res.send("OK")
    } catch (error) {
        res.status(500).send(error)
    }
}

module.exports = { LogDevicetoDB, RegisterHub }