const express = require("express")
const { LogDevicetoDB, RegisterHub } = require("../handlers/api/deviceRegistering")
const { ValidateAccessToken } = require("../middleware/auth")

const APIRoutes = express.Router()

APIRoutes
    .post("/initial_log_to_db", LogDevicetoDB)
    .post("/register_hub/:id", ValidateAccessToken, RegisterHub)
    .get("/test", ValidateAccessToken, (req, res) => {
        res.json({ success: true })
    })

module.exports = APIRoutes