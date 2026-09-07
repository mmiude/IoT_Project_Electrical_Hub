const express = require("express")
const googleOauth = require("../services/google/googleOauth")

const authenticationRoutes = express.Router();

authenticationRoutes
    .get("/google", (req, res) => {
        res.status(301).redirect(googleOauth.getGoogleOauthUrl())
    })
    .get("/sessions/oauth/google", async (req, res) => {
        await googleOauth.getGoogleOauthTokens(req.query.code)
        res.send("yaaah")
    })

module.exports = authenticationRoutes