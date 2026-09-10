const config = require("../config/app.config")
const { verifyJwt, signJwt } = require("../utils")

function ValidateRefreshToken(token, session) {
    const { valid, decodedJwt } = verifyJwt(token, config.jwtSecret)
    if (!valid || (valid && session.id != decodedJwt.session)) {
        return { valid: false, decodedJwt: null }
    }
    return { valid, decodedJwt }
}

function ValidateAccessToken(req, res, next) {
    const refreshToken = req.cookies["refresh_token"]
    const session = req.session

    const { valid, decodedJwt } = ValidateRefreshToken(refreshToken, session)
    if (!valid) {
        return res.status(401).json({
            unauthorized: true,
            message: "No valid refresh token or session"
        })
    }

    const authHeader = req.headers.authorization;
    if (authHeader && authHeader.startsWith('Bearer ')) {
        const accessToken = authHeader.split(' ')[1];
        const decodedAccessToken = verifyJwt(accessToken, refreshToken)
        if (!decodedAccessToken.valid || decodedAccessToken.decodedJwt.userData.userId != decodedJwt.userData.userId) {
            return res.status(401).json({ 
                nauthorized: true,
                message: 'Authorization token is invalid'
            });
        }
        next()
    } else {
        res.status(401).json({ 
            nauthorized: true,
            message: 'Authorization token missing'
        });
    }

}

function UserLoggedIn(req, res, next) {
    const refreshToken = req.cookies["refresh_token"]
    const session = req.session

    const { valid, decodedJwt } = ValidateRefreshToken(refreshToken, session)
    if (!valid) {
        return res.redirect("/login")
    }
    const accessToken = signJwt({ userData: decodedJwt.userData}, refreshToken, { expiresIn: config.accessTokenTtl })
    req.access_token = accessToken

    next()

}

module.exports = { UserLoggedIn, ValidateAccessToken }