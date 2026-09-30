const jwt = require('jsonwebtoken');
const config = require("./config/app.config");

const { jwtSecret } = config;

function signJwt(object, secret, options = {}) {
    return jwt.sign(object, secret, {
        ...options,
        algorithm: 'HS256',
    });
}

function verifyJwt(token, secret) {
    try {
        const decoded = jwt.verify(token, secret);
        return {
            valid: true,
            decodedJwt: decoded,
        };
    } catch (error) {
        return {
            valid: false,
            decodedJwt: {},
        };
    }
}

module.exports = { signJwt, verifyJwt };
