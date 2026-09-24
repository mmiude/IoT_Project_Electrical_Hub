const config = require("../../config/app.config")
const googleOauth = require("../../services/google/googleOauth")
const Postgres = require("../../services/database/postgres");
const { signJwt } = require("../../utils");
const { refreshTokenCookieOptions } = require("../../config/auth.config");

async function googleOauthHandler(req, res) {
    const { refreshTokenTtl } = config;
    const session = req.session
    const googleOauthCode = req.query.code

    const googleOauthTokens = await googleOauth.getGoogleOauthTokens(googleOauthCode)
    if (googleOauthTokens.error) {
        return res.send("Error")
    }

    const { id_token, access_token } = googleOauthTokens.data
    const googleUser = await googleOauth.getGoogleUserData(id_token, access_token)
    if (googleUser.error || (!googleUser.error && !googleUser.data.verified_email)) {
        return res.send("Error")
    }

    const { email, given_name, family_name, picture } = googleUser.data
    const userName = `${given_name} ${family_name}`
    
    const pg = new Postgres()
    const pgUserData = await pg.find_or_create_user(userName, email, picture)
    await pg.sql.end()
    if (pgUserData == null) {
        return res.send("Error")
    }
    // console.log(userId)

    session.userId = pgUserData.id
    session.name = pgUserData.name
    session.email = pgUserData.email

    const userData = { userId: pgUserData.id, userName: pgUserData.name, email: pgUserData.email }
    const refreshToken = signJwt(
        { userData, session: session.id },
        config.jwtSecret,
        { expiresIn: refreshTokenTtl }
    );

    res.cookie("refresh_token", refreshToken, refreshTokenCookieOptions)
    req.session.save((err) => {
        if (err) {
            console.error("Session save error:", err);
            return res.status(500).send("Error saving session");
        }
        return res.redirect("/")
    });
}

module.exports = { googleOauthHandler }
