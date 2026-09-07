const dotenv = require("dotenv")
dotenv.config()

const config = {
    googleOauthClientId: process.env.GOOGLE_OAUTH_CLIENT_ID,
    googleOauthClientSecret: process.env.GOOGLE_OAUTH_CLIENT_SECRET,
    googleOauthRedirectURL: "http://localhost:3000/auth/sessions/oauth/google",
    dbHost: process.env.DB_HOST,
    dbPort: process.env.DB_PORT,
    dbName: process.env.DB_NAME,
    dbUser: process.env.DB_USER,
    dbPassword: process.env.DB_PASSWORD,
    accessTokenTtl: '30m',
    refreshTokenTtl: '1y',
}

module.exports = config