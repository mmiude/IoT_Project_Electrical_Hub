const express = require("express")
const { LogHubToDB, RegisterHub, SendDeviceData } = require("../handlers/api/deviceRegistering")
const GetElectricityPrices = require("../handlers/api/electricityPrice")
const { ValidateAccessToken } = require("../middleware/auth")
const { WebSocketServer } = require('ws');
const Postgres = require("../services/database/postgres");


const APIRoutes = express.Router()

const wss = new WebSocketServer({ port: 8080, path: "/ws" })
const userSockets = new Map(); // Store userId -> socket mapping
wss.on("connection", (ws, req) => {
    const ip = req.headers['x-forwarded-for']?.split(',')[0].trim() || req.socket.remoteAddress;
    const fullUrl = new URL(req.url, `http://${req.headers.host}`);
    const hub = fullUrl.searchParams.get('hub');
    userSockets.set(hub, ws)

    console.log(`New connection from ${ip} hub ${hub}`)
    ws.send(`Hello user ${hub} from ${ip}`)

    ws.on('close', () => {
        console.log(`Connection closed: ${hub}`)
        userSockets.delete(hub); // Clean up on disconnect
    });
})

APIRoutes
    .post("/initial_log_to_db", LogHubToDB)
    .post("/register_hub/:id", ValidateAccessToken, RegisterHub)
    // .get("/test", ValidateAccessToken, (req, res) => {
    //     res.json({ success: true })
    // })
    .get("/get_electricity_prices", GetElectricityPrices)
    .post("/send_device_data", SendDeviceData)
    .post("/send_command_to_hub/:id", ValidateAccessToken, async (req, res) => {
        const { id } = req.params
        // TODO: verify hub signature
        // TODO: verify access token

        const { command, deviceId } = req.body
        console.log(`${command} ${deviceId}`)

        const targetWs = userSockets.get(id)
        console.log(targetWs.readyState)
        if (!targetWs || targetWs.readyState !== 1) {
            return res.json({ error: true })
        }


        let ws_success = true
        targetWs.send(`${command}|${deviceId}`, (error) => {
            console.log(error)
            if (error != null) ws_success = error;
        })

        if (!ws_success) {
            console.log("ws_error")
            return res.json({ error: true })
        }

        const pg = new Postgres()
        let values = {}
        let pg_success = false
        switch (command) {
            case "PLUG_ON":
                values = {
                    is_on: true
                }
                pg_success = await pg.update_device(deviceId, values)
                // console.log(pg_success)
                break
            case "PLUG_OFF":
                values = {
                    is_on: false
                }
                pg_success = await pg.update_device(deviceId, values)
                // console.log(pg_success)
                break
            case "PLUG_NAME":
                values = {
                    name: req.body.device_name
                }
                pg_success = await pg.update_device(deviceId, values)
                // console.log(pg_success)

                break
            case "PLUG_PRIORITY":
                values = {
                    priority: req.body.device_priority
                }
                pg_success = await pg.update_device(deviceId, values)
                // console.log(pg_success)
                break
            default:
                break
        }
        console.log(pg_success)

        if (!pg_success) {
            console.log("pg_error")
            return res.json({ error: true })
        }
        res.json({ error: false })
    })
    // .get("/test", async (req, res) => {
    //     // try {
    //         await test()
    //         res.send("OK")
    //     // } catch {
    //         // res.send("Error")
    //     // }
    // })

module.exports = APIRoutes