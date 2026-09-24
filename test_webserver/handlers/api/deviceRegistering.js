const config = require("../../config/app.config");
const Postgres = require("../../services/database/postgres");
const { verifyJwt } = require("../../utils");
const { sign } = require("../../deviceSign")

async function LogHubToDB(req, res) {
    const auth = req.get("Authorization")
    const token = auth.replace("Bearer ", "")

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret)
        if (!valid) {
            return res.status(401).send("Invalid token")
        }

        const pg = new Postgres()
        const logged = await pg.hub_id_logged(decodedJwt.hub)
        if (!logged) {
            await pg.log_hub_id(decodedJwt.hub)
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
            return res.status(404).send("Invalid hub id")
        }
    
        const signature = sign(id)
        const created = await pg.create_hub(id, signature)
        await pg.sql.end()
        res.status(created ? 200 : 500).send(created ? "OK" : "ERROR")
    } catch (error) {
        res.status(500).send(error)
    }
}

// async function test() {
//     const values = [
//     {
//         name: "test name" 
//     },
//     {
//         priority: "LOW"
//     },
//     {
//         is_on: true
//     },

//     {
//         name: "test name",
//         priority: "LOW"
//     },
//     {
//         priority: "LOW",
//         is_on: true
//     },
//     {
//         name: "test name",
//         is_on: true
//     },
//     {
//         name: "test name",
//         priority: "LOW",
//         is_on: true
//     }
//     ]

//     const pg = new Postgres()
//     for (let i = 0; i < values.length; i++) {
//         console.log("test " + i)
//         await pg.update_device(123, values[i])
//     }
// }

async function SendDeviceData(req, res) {
    const auth = req.get("Authorization")
    const token = auth.replace("Bearer ", "")

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret)
        if (!valid) {
            return res.status(401).send("Invalid token")
        }

        const pg = new Postgres()
        const valid_hub = await pg.hub_id_logged(decodedJwt.hub)
        if (!valid_hub) {
            await pg.sql.end()
            return res.status(404).send("Hub not found")
        }

        const { device_id, type, value, value_int, flag } = req.body
        let message = "Error"
        let values = {}
        console.log(`${device_id} - ${type}`)
        let success = false;
        switch (type) {
            case "DATA_TYPE_DEVICE_JOIN":
                // handle device join
                console.log(decodedJwt)
                success = await pg.create_device(device_id, decodedJwt.hub)
                if (success) {
                    message = `Device ${device_id} created.`
                }
                console.log(message)
                break
            case "DATA_TYPE_DEVICE_LEFT":
                // handle device left
                break
            case "DATA_TYPE_SET_ON":
                // handle set on
                values = {
                    is_on: true
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    message = `Device ${device_id} set on`
                }
                console.log(message)
                break
            case "DATA_TYPE_PRIORITY":
                // handle priority
                values = {
                    priority: value_int == 1 ? "HIGH" : "LOW"
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    message = `Device ${device_id} priority updated to ${values.priority}`
                }
                console.log(message)
                break
            case "DATA_TYPE_ONLINE_STATE":
                // handle online state
                break
            default:
                // handle electricity readings
                console.log(`${type} ${value}`)
                success = await pg.put_device_reading(device_id, type, value)
                if (success) {
                    message = `Inserted ${type}. Value: ${value}`
                }
                console.log(message)
                break
        }
        await pg.sql.end()
        res.status(200).send(message)

    } catch (error) {
        res.status(500).send(error)
    }
}

module.exports = { LogHubToDB, RegisterHub, SendDeviceData }