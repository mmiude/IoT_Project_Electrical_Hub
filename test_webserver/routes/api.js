const express = require("express")
const { LogDevicetoDB, RegisterHub } = require("../handlers/api/deviceRegistering")
const GetElectricityPrices = require("../handlers/api/electricityPrice")
const { ValidateAccessToken } = require("../middleware/auth")

const APIRoutes = express.Router()

APIRoutes
    .post("/initial_log_to_db", LogDevicetoDB)
    .post("/register_hub/:id", ValidateAccessToken, RegisterHub)
    .get("/test", ValidateAccessToken, (req, res) => {
        res.json({ success: true })
    })
    .get("/get_electricity_prices", GetElectricityPrices)
    .post("/send_device_data", (req, res) => {
        console.log(req.headers)
        console.log(req.body)
        res.send("OK")
    })

module.exports = APIRoutes