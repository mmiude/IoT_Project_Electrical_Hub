const config = require("../../config/app.config")
const googleOauth = require("../../services/google/googleOauth")
const Postgres = require("../../services/database/postgres")

async function googleOauthHandler(req, res) {
    const { refreshTokenTtl } = config;
    const session = req.session
    const googleOauthCode = req.query.code

    const googleOauthTokens = await googleOauth.getGoogleOauthTokens(googleOauthCode)
    if (googleOauthTokens.error) {
        res.send("Error")
        return
    }

    const { id_token, access_token } = googleOauthTokens.data
    const googleUser = await googleOauth.getGoogleUserData(id_token, access_token)
    if (googleUser.error || (!googleUser.error && !googleUser.data.verified_email)) {
        res.send("Error")
        return
    }

    const { email, given_name, family_name, picture } = googleUser.data
    

}
