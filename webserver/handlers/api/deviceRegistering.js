const config = require("../../config/app.config");
const pg = require("../../services/database/postgres");
const { verifyJwt } = require("../../utils");
const { sign } = require("../../deviceSign");

async function LogHubToDB(req, res) {
    const auth = req.get("Authorization") || "";
    const token = auth.replace("Bearer ", "");

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret);
        if (!valid) {
            return res.status(401).send("Invalid token");
        }

        const logged = await pg.hub_created(decodedJwt.hub);
        if (!logged) {
            await pg.create_hub(decodedJwt.hub);
        }

        return res.send("OK");
    } catch (error) {
        console.error("LogHubToDB error:", error);
        return res.status(500).send("Internal Server Error");
    }
}

async function RegisterHub(req, res) {
    const { userId, hubId } = req.query;

    try {
        const valid_hub = await pg.hub_created(hubId);
        if (!valid_hub) {
            return res.status(404).send("Invalid hub id");
        }

        const signature = sign(hubId);
        const created = await pg.sign_hub_to_user(userId, hubId, signature);

        return res.status(created ? 200 : 500).send(created ? "OK" : "ERROR");
    } catch (error) {
        console.error("RegisterHub error:", error);
        return res.status(500).send("Internal Server Error");
    }
}

async function SendDeviceData(req, res) {
    const auth = req.get("Authorization") || "";
    const token = auth.replace("Bearer ", "");

    try {
        const { valid, decodedJwt } = verifyJwt(token, config.deviceJWTSecret);
        if (!valid) {
            return res.status(401).send("Invalid token");
        }
        const hubId = decodedJwt.hub;

        const valid_hub = await pg.hub_created(hubId);
        if (!valid_hub) {
            return res.status(404).send("Hub not found");
        }

        const { device_id, type, value, value_int, command, flag, c_value } = req.body;
        let message = "Error";
        let values = {};
        let success = false;
        let device_update = false;

        switch (type) {
            case "DATA_TYPE_SET_ON": // 2
                values = {
                    is_on: flag == 1
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    const on_off = values.is_on ? "ON" : "OFF"
                    message = `Device ${device_id} turned ${on_off}`;   
                }
                break

            case "DATA_TYPE_DEVICE_NAME": // 3
                const found = await pg.find_device(device_id)
                if (found) {
                    values = {
                        name: c_value
                    }
                    success = await pg.update_device(device_id, values)
                    if (success) {
                        message = `Device ${device_id} name updated to ${c_value}.`
                    }
                    break
                }

                success = await pg.create_device(device_id, c_value, hubId);
                if (success) {
                    message = `Device ${device_id} added with name "${c_value}".`;
                }
                break;

            case "DATA_TYPE_ONLINE_STATE":
                values = {
                    online: flag == 1
                }
                success = await pg.update_device(device_id, values)
                if (success) {
                    const online = values.online ? "online" : "offline"
                    message = `Device ${device_id} is ${online}.`
                }
                break

            case "DATA_TYPE_DEVICE_LEFT":
                console.log(type)
                success = await pg.delete_device(device_id);
                if (success) {
                    message = `Device ${device_id} removed.`
                }
                break;

            case "DATA_TYPE_PRIORITY": // 6
                values = {
                    priority: value_int == 0 ? "HIGH" : value_int == 1 ? "LOW" : "MED"
                };
                success = await pg.update_device(device_id, values);
                if (success) {
                    message = `Device ${device_id} priority updated to ${values.priority}.`;
                }
                break;

            case "DATA_TYPE_ELEC_PRICE":
                values = { electricity_price: value };
                success = await pg.update_hub(hubId, values);
                if (success) {
                    message = `Hub ${hubId} electricity price updated to ${values.electricity_price}.`;
                }
                break;

            case "DATA_TYPE_THRESHOLD_MED": // 7
                values = { threshold_med: value };
                success = await pg.update_hub(hubId, values);
                if (success) {
                    message = `Hub ${hubId} MED devices threshold updated to ${values.threshold_med}.`;
                }
                break;

            case "DATA_TYPE_THRESHOLD_LOW": // 8
                values = { threshold_low: value };
                success = await pg.update_hub(hubId, values);
                if (success) {
                    message = `Hub ${hubId} LOW devices threshold updated to ${values.threshold_low}.`;
                }
                break;

            default: // 12
                device_update = true;
                if (value > 0) {
                    success = await pg.put_device_reading(device_id, type, value);
                    message = "Updated"
                } else {
                    success = true;
                    message = "No need to update"
                }
                break;
        }

        if (!success) {
            return res.status(500).send(message);
        }

        if (!device_update) {
            const logged = await pg.log_to_hub(hubId, message);
            if (!logged) {
                return res.status(500).send("log error");
            }
        }

        return res.status(200).send(message);
    } catch (error) {
        console.error("SendDeviceData error:", error);
        return res.status(500).send("Internal Server Error");
    }
}

module.exports = { LogHubToDB, RegisterHub, SendDeviceData };