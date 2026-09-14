const config = require("../../config/app.config");

async function GetElectricityPrices(req, res) {
    const electricityPriceRequest = await fetch("https://api.porssisahko.net/v2/latest-prices.json")
    if (electricityPriceRequest.status != 200) {
        return res.status(electricityPriceRequest.status).send(electricityPriceRequest.statusText)
    }
    const electricityPrices = await electricityPriceRequest.json()
    const now = new Date()
    const prices = electricityPrices.prices
        .filter(p => new Date(p.startDate) >= now)
        .map(p => p.price)
    console.log(prices)
    res.status(200).send(prices.join(","))
}

module.exports = GetElectricityPrices