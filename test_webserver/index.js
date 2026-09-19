const express = require("express")
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

app.get("/", /*UserLoggedIn,*/ (req, res) => {
    // console.log(req.access_token)
    // const data = { user: req.session.name, access_token: req.access_token }
    const data = { user: "cool user", access_token: "test_token" }
    res.render("main", { data })
    // res.send(`Logged in as ${req.session.name}`)
})

app.get("/login", (req, res) => {
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
//         const valid_hub = await hub_id_logged(id)
//         if (!valid_hub) {
//             res.status(406).send("Invalid hub id")
//             return
//         }
    
//         const signature = deviceSign.sign(id)
//         await create_hub(id, signature)
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
//         const logged = await hub_id_logged(decoded.device)
//         if (logged) {
//             res.send("OK")
//         } else {
//             await log_hub_id(decoded.device)
//             res.send("OK")
//         }
//     } catch (error) {
//         res.status(500).send(error)
//     }
// })

app.listen(3000, "0.0.0.0", () => {
    console.log("Running on port: 3000")
})