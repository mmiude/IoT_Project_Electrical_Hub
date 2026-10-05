const express = require("express")
const http = require("http")
// const { WebSocketServer } = require('ws');
const jwt = require('jsonwebtoken');
const path = require("path")
const cookieParser = require("cookie-parser")
const deviceSign = require("./deviceSign")

const authenticationRoutes = require("./routes/auth")
const APIRoutes = require("./routes/api")

const googleOauth = require("./services/google/googleOauth");
const { verifyJwt } = require("./utils");
const { newSession } = require("./config/auth.config");
const { UserLoggedIn } = require("./middleware/auth");
const pg = require("./services/database/postgres");

const app = express()

app.set("view engine", "ejs")
app.set("views", path.join(__dirname, "views"))

app.use("/static", express.static(path.join(__dirname, "static")))
app.use(express.json());
app.use(express.urlencoded({ extended: true })); // support encoded bodies
app.use(cookieParser())
app.use(newSession);

app.use("/auth", authenticationRoutes)
app.use("/api", APIRoutes)

app.get("/", UserLoggedIn, async (req, res) => {
    const { userId, name, email } = req.session
    const {
        hub_id,
        profile_picture,
        threshold_low,
        threshold_med,
        electricity_price
    } = await pg.get_user_data(userId)
    const devices = await pg.get_user_devices(userId)
    const logs = await pg.get_user_hub_logs(userId)

    const data = {
        user: {
            userId,
            name,
            email,
            profile_picture,
            access_token: req.access_token
        },
        hub: {
            hub_id,
            threshold_low,
            threshold_med,
            electricity_price,
            devices,
            // device_readings,
            logs
        }
    }
    // console.log(data)
    res.render("main", { data })
})

app.get("/login", UserLoggedIn, (req, res) => {
    // res.sendFile(path.resolve("./templates/login.html"))
    res.render("login")
})


app.listen(3000, "0.0.0.0", () => {
    console.log("Running on port: 3000")
})
// const server = http.createServer(app)

// server.listen(3000, "0.0.0.0", () => {
//     console.log("Running on port: 3000")
// })