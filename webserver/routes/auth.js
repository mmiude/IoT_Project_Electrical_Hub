const express = require("express")
const googleOauth = require("../services/google/googleOauth");
const { googleOauthHandler } = require("../handlers/auth/googleOauth");

const authenticationRoutes = express.Router();

authenticationRoutes
    .get("/google", (req, res) => {
        res.redirect(googleOauth.getGoogleOauthUrl())
    })
    .get("/sessions/oauth/google", googleOauthHandler)
    .get("/logout", (req, res) => {
        req.session.destroy((err) => {
            if (err) {
                return res.status(500).send(
                    "<h1>Could not log out</h1><a href='/'> Go back</a>"
                )
            }
            res.clearCookie("connect.sid")
            res.clearCookie("refresh_token")
            res.redirect("/login")
        })
    })

module.exports = authenticationRoutes