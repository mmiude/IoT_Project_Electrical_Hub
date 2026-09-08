const jwt = require('jsonwebtoken');
const config = require("./config/app.config");

const { jwtSecret } = config;

// interface VerifyJwtReturntypes {
//     valid: boolean;
//     decodedJwt: any;
// }

function signJwt(object, options = {}) {
    return jwt.sign(object, jwtSecret, {
        ...options,
        algorithm: 'HS256',
    });
}

function verifyJwt(token) {
    try {
        const decoded = jwt.verify(token, jwtSecret);
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
