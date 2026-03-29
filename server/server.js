const express = require("express")
const path = require("path")

const app = express()
app.use(express.json())

app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "static", "index.html"))
})
app.get("/emulator", (req, res) => {
    res.sendFile(path.join(__dirname, "static", "emulator.html"))
})
app.get("/emulator.js", (req, res) => {
    res.sendFile(path.join(__dirname, "..", "emulator", "emulator.js"))
})
app.get("/emulator.wasm", (req, res) => {
    res.type('application/wasm')
    res.sendFile(path.join(__dirname, "..", "emulator", "emulator.wasm"))
})
app.get("/payload.bin", (req, res) => {
    res.type("application/octet-stream")
    res.sendFile(path.join(__dirname, "..", "test", "payload.bin"))
})

app.listen(8080)