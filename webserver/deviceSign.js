const crypto = require("crypto")
const fs = require("fs")
const path = require('path');

const KEY_DIR = "./keys"

const PRIVATE_KEY = fs.readFileSync(path.join(KEY_DIR, "private_key.pem"), "utf-8");
const PUBLIC_KEY = fs.readFileSync(path.join(KEY_DIR, "public_key.pem"), "utf-8");

function sign(data) {
    // const privateKey = fs.readFileSync(path.join(KEY_DIR + "/private_key.pem"), "utf-8")
    const dataBuffer = Buffer.isBuffer(data) ? data : Buffer.from(data);

    const signature = crypto.sign(
        'sha256',
        dataBuffer,
        {
            key: PRIVATE_KEY,
            padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
            saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_LENGTH
        }
    );

    return signature;
}

function verify(signature, hubId) {
    // const publicKey = fs.readFileSync(path.join(KEY_DIR + "/public_key.pem"), "utf-8")
    if (!signature || !hubId) return false;

    try {
        const hubIdBuffer = Buffer.isBuffer(hubId) ? hubId : Buffer.from(hubId);
        const signatureBuffer = Buffer.isBuffer(signature) ? signature : Buffer.from(signature);

        return crypto.verify(
            'sha256',
            hubIdBuffer,
            {
                key: PUBLIC_KEY,
                padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                saltLength: crypto.constants.RSA_PSS_SALTLEN_MAX_LENGTH
            },
            signatureBuffer
        );
    } catch (error) {
        return false;
    }
}

module.exports = { sign, verify }