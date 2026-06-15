const express = require("express")
const jwt = require('jsonwebtoken');
const postgres = require("postgres")
const path = require("path")
const deviceSign = require("./deviceSign")

const app = express()
app.use(express.json());

const sql = postgres({
    host: "localhost",
    port: 5432,
    database: "spikestriker",
    user: "admin",
    password: "admin"
})

async function hub_id_logged(hub_id) {
    const found_hub_id = await sql`
        SELECT * FROM valid_hub_ids
        WHERE hub_id = ${hub_id}
    `
    return found_hub_id.count > 0
}

async function log_hub_id(hub_id) {
    const hub = await sql`
        INSERT INTO valid_hub_ids (hub_id)
        VALUES (${hub_id})
    `
}

async function create_hub(hub_id, signature) {
    const hub = await sql`
        INSERT INTO hub (id, signature)
        VALUES (${hub_id}, ${signature})
    `
}

app.get("/", (req, res) => {
    res.send("Hello world: " + req.query.id)
})

app.get("/register_hub", (req, res) => {
    res.sendFile(path.resolve("./index.html"))
})
app.post("/register_hub/:id", async (req, res) => {
    const { id } = req.params

    try {
        const valid_hub = await hub_id_logged(id)
        if (!valid_hub) {
            res.status(406).send("Invalid hub id")
            return
        }
    
        const signature = deviceSign.sign(id)
        await create_hub(id, signature)
        res.send("OK")
    } catch (error) {
        res.status(500).send(error)
    }

})

app.post("/send", (req, res) => {
    console.log(req.body)
    res.send("OK")
})

app.post("/initial_log_to_db", async (req, res) => {
    const auth = req.get("Authorization")
    const token = auth.replace("Bearer ", "")

    try {
        const decoded = jwt.verify(token, "a-very-secret-and-secure-secret-token")
        const logged = await hub_id_logged(decoded.device)
        if (logged) {
            res.send("OK")
        } else {
            await log_hub_id(decoded.device)
            res.send("OK")
        }
    } catch (error) {
        res.status(500).send(error)
    }
})

app.listen(3000, () => {
    console.log("Running on port: 3000")
})