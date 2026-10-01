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
        const logged = await pg.hub_created(decodedJwt.hub)
        if (!logged) {
            await pg.create_hub(decodedJwt.hub)
        }
        await pg.sql.end()
        res.send("OK")
    } catch (error) {
        res.status(500).send(error)
    }
}

async function RegisterHub(req, res) {
    const { userId, hubId } = req.query
    console.log(userId, hubId)
    // const { userId } = req.session

    try {
        const pg = new Postgres()
        const valid_hub = await pg.hub_created(hubId)
        if (!valid_hub) {
            await pg.sql.end()
            return res.status(404).send("Invalid hub id")
        }
        console.log("Valid hub")
    
        const signature = sign(hubId)
        console.log(signature)
        const created = await pg.sign_hub_to_user(userId, hubId, signature)
        await pg.sql.end()
        res.status(created ? 200 : 500).send(created ? "OK" : "ERROR")
    } catch (error) {
        res.status(500).send(error)
    }
}

async function SendDeviceData(req, res) {
    const auth = req.get("Authorization")
    const token = auth.replace("Bearer ", "")

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret)
        if (!valid) {
            return res.status(401).send("Invalid token")
        }
        const hubId = decodedJwt.hub

        const pg = new Postgres()
        const valid_hub = await pg.hub_created(hubId)
        if (!valid_hub) {
            await pg.sql.end()
            return res.status(404).send("Hub not found")
        }

        const { device_id, type, value, value_int, flag, command, c_value }
            = req.body
        let message = "Error"
        let values = {}
        console.log(`${device_id} - ${type}`)
        let success = false;
        switch (type) {
            case "DATA_TYPE_COMMAND":
                if (command != "PLUG_ON" || command != "PLUG_OFF") {
                    break
                }
                values = {
                    is_on: command == "PLUG_ON"
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    message = `Device ${device_id} turned ${command.split("_")[1]}`
                }
                console.log(message)
                break
            case "DATA_TYPE_DEVICE_ADDED":
                // handle device join
                console.log(decodedJwt)
                success = await pg.create_device(device_id, c_value, hubId)
                if (success) {
                    message = `Device ${device_id} added with name "${c_value}".`
                }
                console.log(message)
                break
            case "DATA_TYPE_DEVICE_LEFT":
                // handle device left
                break
            case "DATA_TYPE_PRIORITY":
                values = {
                    priority:
                        value_int == 0 ? "HIGH" :
                        value_int == 1 ? "MED" : "LOW"
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    message = `Device ${device_id} priority updated to ${values.priority}.`
                }
                console.log(message)
                break
            case "DATA_TYPE_ELEC_PRICE":
                values = {
                    electricity_price: value
                }
                success = await pg.update_hub(hubId, values)
                if (success) {
                    message = `Hub ${hubId} electricity price updated to ${values.electricity_price}.`
                }
                console.log(message)
                break
            case "DATA_TYPE_THRESHOLD_MED":
                values = {
                    threshold_med: value
                }
                success = await pg.update_hub(hubId, values)
                if (success) {
                    message = `Hub ${hubId} MED devices threshold updated to ${values.threshold_med}.`
                }
                console.log(message)
                break
            case "DATA_TYPE_THRESHOLD_LOW":
                values = {
                    threshold_low: value
                }
                success = await pg.update_hub(hubId, values)
                if (success) {
                    message = `Hub ${hubId} LOW devices threshold updated to ${values.threshold_low}.`
                }
                console.log(message)
                break
            default:
                // handle electricity readings
                console.log(`${type} ${value}`)
                success = await pg.put_device_reading(device_id, type, value)
                // if (success) {
                //     message = `Inserted ${type}. Value: ${value}`
                // }
                // console.log(message)
                break
        }
        if (!success) {
            await pg.sql.end()
            return res.status(500).send(message)
        }

        const logged = await pg.log_to_hub(hubId, message);
        await pg.sql.end()
        if (!logged) {
            return res.status(500).send("log error")
        }
        res.status(200).send(message)

    } catch (error) {
        res.status(500).send(error)
    }
}

module.exports = { LogHubToDB, RegisterHub, SendDeviceData }