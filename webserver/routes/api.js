const express = require("express")
const { LogHubToDB, RegisterHub, SendDeviceData } = require("../handlers/api/deviceRegistering")
const GetElectricityPrices = require("../handlers/api/electricityPrice")
const { ValidateAccessToken } = require("../middleware/auth")
const { verify } = require("../deviceSign")
const { WebSocketServer, errorMonitor } = require('ws');
const pg = require("../services/database/postgres");


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

    // const intervalId = setInterval(() => {
    //     console.log("Sending test data")
    //     ws.send("PLUG_OFF|123")
    // }, 5000)

    ws.on('close', () => {
        console.log(`Connection closed: ${hub}`)
        userSockets.delete(hub); // Clean up on disconnect
        // clearInterval(intervalId)
    });

    // while (true) {
    //     setTimeout()
    // }
})

APIRoutes
    .post("/initial_log_to_db", LogHubToDB)
    .post("/register_hub", ValidateAccessToken, RegisterHub)
    .post("/delete_hub_from_user", ValidateAccessToken, async (req, res) => {
        const { userId } = req.query
        try {
            // const pg = new Postgres()
            const deleted = await pg.delete_hub_from_user(userId)
            // await pg.sql.end()
            if (deleted) {
                return res.status(200).send("OK")
            }
            res.status(500).send("Error")
        } catch (error) {
            res.status(500).send(error)
        }
    })
    .get("/get_device_readings", ValidateAccessToken, async (req, res) => {
        const { userId, timeData, date } = req.query
        try {
            const jsonData = { error: false, data: [] }
            switch (timeData) {
                case "chart_data_day":
                    jsonData.data = await pg.get_hourly_energy_by_date(userId, date)
                    break;
                case "chart_data_week":
                    jsonData.data = await pg.get_daily_energy_by_week(userId, date)
                    break;
                case "chart_data_month":
                    jsonData.data = await pg.get_daily_energy_by_month(userId, date)
                    break;
                case "chart_data_year":
                    jsonData.data = await pg.get_monthly_energy_by_year(userId, date)
                    break;
                default:
                    jsonData.error = true
                    jsonData.data = "Invalid timeData"
                    break;
            }
            res.status(jsonData.error ? 406 : 200).json(jsonData)
        } catch (error) {
            res.status(500).json({ error: true, data: error })
        }
    })
    .post("/delete_hub_log", ValidateAccessToken, async (req, res) => {
        const { logId } = req.query
        try {
            // const pg = new Postgres()
            const deleted = await pg.delete_hub_log(logId)
            // await pg.sql.end()
            if (deleted) {
                return res.status(200).send("OK")
            }
            res.status(500).send("Error")
        } catch (error) {
            res.status(500).send(error)
        }
    })
    .post("/delete_hub_logs", ValidateAccessToken, async (req, res) => {
        const { hubId } = req.query
        try {
            // const pg = new Postgres()
            const deleted = await pg.delete_hub_logs(hubId)
            // await pg.sql.end()
            if (deleted) {
                return res.status(200).send("OK")
            }
            res.status(200).send("No logs to delete")
        } catch (error) {
            res.status(500).send(error)
        }
    })
    // .get("/test", ValidateAccessToken, (req, res) => {
    //     res.json({ success: true })
    // })
    .get("/get_electricity_prices", GetElectricityPrices)
    .post("/send_device_data", SendDeviceData)
    .post("/send_command_to_hub", ValidateAccessToken, async (req, res) => {
        try {
            const { userId, hubId } = req.query
            const hub_signature = await pg.get_user_hub_signature(userId)
            if (hub_signature == null) {
                // await pg.sql.end()
                return res.json({ error: true })
            }
            if (!verify(hub_signature, hubId)) {
                // await pg.sql.end()
                console.log("Signature verification error")
                return res.json({ error: true })
            }


            // const { command, deviceId } = req.body
            const { deviceId, type, data } = req.body
            console.log(`${type} ${deviceId}`)

            const targetWs = userSockets.get(hubId)
            // console.log(`Websocket readystate: ${targetWs.readyState}`)
            if (!targetWs || targetWs.readyState !== 1) {
                // await pg.sql.end()
                return res.json({ error: true })
            }


            let ws_success = true
            const webSocketPayload = `${deviceId}|${type}|${data.value}|${data.value_int}|${data.command}|${data.c_value}`

            targetWs.send(webSocketPayload, (error) => {
                console.log(error)
                if (error != null) ws_success = error;
            })

            if (!ws_success) {
                console.log("ws_error")
                // await pg.sql.end()
                return res.json({ error: true })
            }

            let values = {}
            let pg_success = false
            let log_message = ""
            switch (type) {
                case "DATA_TYPE_COMMAND":
                    switch (data.command) {
                        case "PLUG_ON":
                            values = {
                                is_on: true
                            }
                            pg_success = await pg.update_device(deviceId, values)
                            if (pg_success) {
                                log_message = `Device ${deviceId} turned ON.`
                            }
                            // console.log(pg_success)
                            break
                        case "PLUG_OFF":
                            values = {
                                is_on: false
                            }
                            pg_success = await pg.update_device(deviceId, values)
                            if (pg_success) {
                                log_message = `Device ${deviceId} turned OFF.`
                            }
                            // console.log(pg_success)
                            break
                        default:
                            break
                    }
                    break;
                case "DATA_TYPE_DEVICE_NAME":
                    values = {
                        name: data.c_value
                    }
                    pg_success = await pg.update_device(deviceId, values)
                    if (pg_success) {
                        log_message = `Device ${deviceId} name updated to "${values.name}".`
                    }
                    // console.log(pg_success)

                    break
                case "DATA_TYPE_PRIORITY":
                    console.log(data.value_int)
                    values = {
                        // priority: data.value_int == 0 ? "HIGH" : data.value_int == 1 ? "MED" : "LOW"
                        priority: data.value_int == 0 ? "HIGH": data.value_int == 1 ? "LOW" : "MED"
                    }
                    pg_success = await pg.update_device(deviceId, values)
                    if (pg_success) {
                        log_message = `Device ${deviceId} priority updated to ${values.priority}.`
                    }
                    // console.log(pg_success)
                    break
                case "DATA_TYPE_THRESHOLD_MED":
                    values = {
                        threshold_med: data.value
                    }
                    pg_success = await pg.update_hub(hubId, values)
                    if (pg_success) {
                        log_message = `Hub ${hubId} MED devices threshold updated to ${values.threshold_med}.`
                    }
                    break
                case "DATA_TYPE_THRESHOLD_LOW":
                    values = {
                        threshold_low: data.value
                    }
                    pg_success = await pg.update_hub(hubId, values)
                    if (pg_success) {
                        log_message = `Hub ${hubId} LOW devices threshold updated to ${values.threshold_low}.`
                    }
                    break
                default:
                    break
            }
            console.log(pg_success)

            if (!pg_success) {
                console.log("pg_error")
                // await pg.sql.end()
                return res.json({ error: true })
            }
            const logged = await pg.log_to_hub(hubId, log_message);
            // await pg.sql.end()
            if (!logged) {
                console.log("log error")
                res.json({ error: true })
            }
            res.json({ error: false })
        }
        catch (error) {
            res.json({ error: true })
        }
        // const { userId, hubId } = req.query
        // // TODO: verify hub signature
        // // const pg = new Postgres()
        // const hub_signature = await pg.get_user_hub_signature(userId)
        // if (hub_signature == null) {
        //     // await pg.sql.end()
        //     return res.json({ error: true })
        // }
        // if (!verify(hub_signature, hubId)) {
        //     // await pg.sql.end()
        //     console.log("Signature verification error")
        //     return res.json({ error: true })
        // }


        // // const { command, deviceId } = req.body
        // const { deviceId, type, data } = req.body
        // console.log(`${type} ${deviceId}`)

        // const targetWs = userSockets.get(hubId)
        // // console.log(`Websocket readystate: ${targetWs.readyState}`)
        // if (!targetWs || targetWs.readyState !== 1) {
        //     // await pg.sql.end()
        //     return res.json({ error: true })
        // }


        // let ws_success = true
        // const webSocketPayload = `${deviceId}|${type}|${data.value}|${data.value_int}|${data.command}|${data.c_value}`

        // targetWs.send(webSocketPayload, (error) => {
        //     console.log(error)
        //     if (error != null) ws_success = error;
        // })

        // if (!ws_success) {
        //     console.log("ws_error")
        //     // await pg.sql.end()
        //     return res.json({ error: true })
        // }

        // let values = {}
        // let pg_success = false
        // let log_message = ""
        // switch (type) {
        //     case "DATA_TYPE_COMMAND":
        //         switch (data.command) {
        //             case "PLUG_ON":
        //                 values = {
        //                     is_on: true
        //                 }
        //                 pg_success = await pg.update_device(deviceId, values)
        //                 if (pg_success) {
        //                     log_message = `Device ${deviceId} turned ON.`
        //                 }
        //                 // console.log(pg_success)
        //                 break
        //             case "PLUG_OFF":
        //                 values = {
        //                     is_on: false
        //                 }
        //                 pg_success = await pg.update_device(deviceId, values)
        //                 if (pg_success) {
        //                     log_message = `Device ${deviceId} turned OFF.`
        //                 }
        //                 // console.log(pg_success)
        //                 break
        //             default:
        //                 break
        //         }
        //         break;
        //     case "DATA_TYPE_DEVICE_NAME":
        //         values = {
        //             name: data.c_value
        //         }
        //         pg_success = await pg.update_device(deviceId, values)
        //         if (pg_success) {
        //             log_message = `Device ${deviceId} name updated to "${values.name}".`
        //         }
        //         // console.log(pg_success)

        //         break
        //     case "DATA_TYPE_PRIORITY":
        //         console.log(data.value_int)
        //         values = {
        //             // priority: data.value_int == 0 ? "HIGH" : data.value_int == 1 ? "MED" : "LOW"
        //             priority: data.value_int == 0 ? "HIGH": data.value_int == 1 ? "LOW" : "MED"
        //         }
        //         pg_success = await pg.update_device(deviceId, values)
        //         if (pg_success) {
        //             log_message = `Device ${deviceId} priority updated to ${values.priority}.`
        //         }
        //         // console.log(pg_success)
        //         break
        //     case "DATA_TYPE_THRESHOLD_MED":
        //         values = {
        //             threshold_med: data.value
        //         }
        //         pg_success = await pg.update_hub(hubId, values)
        //         if (pg_success) {
        //             log_message = `Hub ${hubId} MED devices threshold updated to ${values.threshold_med}.`
        //         }
        //         break
        //     case "DATA_TYPE_THRESHOLD_LOW":
        //         values = {
        //             threshold_low: data.value
        //         }
        //         pg_success = await pg.update_hub(hubId, values)
        //         if (pg_success) {
        //             log_message = `Hub ${hubId} LOW devices threshold updated to ${values.threshold_low}.`
        //         }
        //         break
        //     default:
        //         break
        // }
        // console.log(pg_success)

        // if (!pg_success) {
        //     console.log("pg_error")
        //     // await pg.sql.end()
        //     return res.json({ error: true })
        // }
        // const logged = await pg.log_to_hub(hubId, log_message);
        // // await pg.sql.end()
        // if (!logged) {
        //     console.log("log error")
        //     res.json({ error: true })
        // }
        // res.json({ error: false })
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