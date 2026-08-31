const crypto = require("crypto")
const fs = require("fs")
const path = require('path');

const KEY_DIR = "./keys"

function sign(data) {
    const privateKey = fs.readFileSync(path.join(KEY_DIR + "/private_key.pem"), "utf-8")
    const dataBuffer = Buffer.isBuffer(data) ? data : Buffer.from(data);

    const signature = crypto.sign(
        'sha256',
        dataBuffer,
        {
            key: privateKey,
            padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
            saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_LENGTH
        }
    );

    return signature;
}

function verify(signature, deviceId) {
    const publicKey = fs.readFileSync(path.join(KEY_DIR + "/public_key.pem"), "utf-8")
    
    try {
        const deviceIdBuffer = Buffer.isBuffer(deviceId) ? deviceId : Buffer.from(deviceId);

        return crypto.verify(
            'sha256',
            deviceIdBuffer,
            {
                key: publicKey,
                padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_LENGTH
            },
            signature
        );
    } catch (error) {
        return false;
    }
}

module.exports = { sign, verify }