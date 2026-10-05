const postgres = require("postgres");
// const config = require("../../config/app.config");
const config = require("./config/app.config")

// Initialize a single connection pool for the entire application
const sql = postgres({
    host: config.dbHost,
    port: config.dbPort,
    database: config.dbName,
    user: config.dbUser,
    password: config.dbPassword,
    max: 10 // Max connections in pool (adjust based on load)
});

