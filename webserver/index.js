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
const Postgres = require("./services/database/postgres");

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

// const wss = new WebSocketServer({ port: 8080, path: "/ws" })
// const userSockets = new Map(); // Store userId -> socket mapping
// wss.on("connection", (ws, req) => {
//     const ip = req.headers['x-forwarded-for']?.split(',')[0].trim() || req.socket.remoteAddress;
//     const fullUrl = new URL(req.url, `http://${req.headers.host}`);
//     const hub = fullUrl.searchParams.get('hub');
//     userSockets.set(hub, ws)

//     console.log(`New connection from ${ip} hub ${hub}`)
//     ws.send(`Hello user ${hub} from ${ip}`)

//     ws.on('close', () => {
//         userSockets.delete(hub); // Clean up on disconnect
//     });
// })


// app.get("/test_ws", (req, res) => {
//     const { hub } = req.query
//     console.log(hub)
//     const targetWs = userSockets.get(hub)
//     if (targetWs && targetWs.readyState === 1) {
//         targetWs.send("This is a test")
//         return res.send("OK")
//     }
//     res.send("Fail")
// })

app.get("/", UserLoggedIn, async (req, res) => {
    // console.log(req.access_token)
    const { userId, name, email } = req.session
    const pg = new Postgres()
    const {
        hub_id,
        profile_picture,
        threshold_low,
        threshold_med,
        electricity_price
    } = await pg.get_user_data(userId)
    const devices = await pg.get_user_devices(userId)
    const device_readings = await pg.get_user_devices_readings(userId)
    const logs = await pg.get_user_hub_logs(userId)
    await pg.sql.end()

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
            device_readings,
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
// app.get("/googleLogin", (req, res) => {
//     res.status(301).redirect(googleOauth.getGoogleOauthUrl())
// })

// app.get("/register_hub", (req, res) => {
//     res.sendFile(path.resolve("./templates/register_hub.html"))
// })
// app.post("/register_hub/:id", async (req, res) => {
//     const { id } = req.params

//     try {
//         const valid_hub = await hub_created(id)
//         if (!valid_hub) {
//             res.status(406).send("Invalid hub id")
//             return
//         }
    
//         const signature = deviceSign.sign(id)
//         await sign_hub_to_user(id, signature)
//         res.send("OK")
//     } catch (error) {
//         res.status(500).send(error)
//     }

// })

// app.post("/initial_log_to_db", async (req, res) => {
//     const auth = req.get("Authorization")
//     const token = auth.replace("Bearer ", "")

//     try {
//         const decoded = jwt.verify(token, "a-very-secret-and-secure-secret-token")
//         const logged = await hub_created(decoded.device)
//         if (logged) {
//             res.send("OK")
//         } else {
//             await create_hub(decoded.device)
//             res.send("OK")
//         }
//     } catch (error) {
//         res.status(500).send(error)
//     }
// })


app.listen(3000, "0.0.0.0", () => {
    console.log("Running on port: 3000")
})
// const server = http.createServer(app)

// server.listen(3000, "0.0.0.0", () => {
//     console.log("Running on port: 3000")
// })