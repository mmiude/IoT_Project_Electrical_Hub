const session = require('express-session');
const dotenv = require('dotenv');
dotenv.config();

const refreshTokenCookieOptions = {
    maxAge: 3.154e10,
    httpOnly: true,
    sameSite: 'lax',
    secure: false,
};

const sessionConfig = {
    secret: process.env.SESSION_SECRET,
    resave: false,
    saveUninitialized: true,
    cookie: {
        secure: false,
        httpOnly: true,
    },
};
const newSession = session(sessionConfig)

module.exports = { newSession, refreshTokenCookieOptions }