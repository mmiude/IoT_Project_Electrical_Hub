const express = require("express")
const googleOauth = require("../services/google/googleOauth");
const { googleOauthHandler } = require("../handlers/auth/googleOauth.handler");

const authenticationRoutes = express.Router();

authenticationRoutes
    .get("/google", (req, res) => {
        res.redirect(googleOauth.getGoogleOauthUrl())
    })
    .get("/sessions/oauth/google", googleOauthHandler)

module.exports = authenticationRoutes