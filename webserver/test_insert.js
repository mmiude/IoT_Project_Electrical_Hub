const postgres = require("postgres");
// const config = require("../../config/app.config");
const config = require("./config/app.config");

// Initialize a single connection pool for the entire application
const sql = postgres({
    host: config.dbHost,
    port: config.dbPort,
    database: config.dbName,
    user: config.dbUser,
    password: config.dbPassword,
    max: 10 // Max connections in pool (adjust based on load)
});

async function insert_reading(deviceId, type, date) {
    const q = await sql`
        INSERT INTO device_readings
        (device_id, type, value, timeperiod)
        VALUES (${deviceId}, ${type}, 123.4, ${date})
    `
    return q.count > 0
}
async function get_hourly_energy_by_date(userId, date) {
    const q = await sql`
        SELECT * FROM get_hourly_energy_by_date(${date}, ${userId})
    `
    // console.log(`get_hourly_energy_by_date: ${q.count} ${JSON.stringify(q)}`)
    return q
}
async function get_daily_energy_by_week(userId, date) {
    const q = await sql`
        SELECT * FROM get_daily_energy_by_week(${date}, ${userId})
    `
    return q
}
async function get_daily_energy_by_month(userId, date) {
    const q = await sql`
        SELECT * FROM get_daily_energy_by_month(${date}, ${userId})
    `
    return q
}
async function get_monthly_energy_by_year(userId, date) {
    const q = await sql`
        SELECT * FROM get_monthly_energy_by_year(${date}, ${userId})
    `
    return q
}

function subtractMins(date, mins) {
    const nd = new Date(date.toISOString())
    nd.setMinutes(nd.getMinutes() - mins);
    return nd;
}
function subtractHours(date, hours) {
    const nd = new Date(date.toISOString())
    nd.setHours(nd.getHours() - hours);
    return nd;
}
function subtractDays(date, days) {
    const nd = new Date(date.toISOString())
    nd.setDate(nd.getDate() - days);
    return nd;
}
function subtractMonths(date, months) {
    const nd = new Date(date.toISOString())
    nd.setMonth(nd.getMonth() - months);
    return nd;
}
function subtractYears(date, years) {
    const nd = new Date(date.toISOString())
    nd.setFullYear(nd.getFullYear() - years);
    return nd;
}

const d = new Date()
async function test3() {

    const timeData = "chart_data_year"
    const date = subtractMonths(d, 8)
    const data = await get_monthly_energy_by_year(1, date.toISOString())

    let prevIndex = 0
    let firstFoundIndex = 0;
    let index = 0
    let count = 0;
    while (index >= 0) {
        prevIndex = index
        index = data.findIndex((e, i) => (i > index && e.timeperiod == "TOTAL"))
        if (prevIndex == 0) firstFoundIndex = index
        if (index >= 0) count++;
    }
    const entriesCount = (data.length / count) - 1

    const chartDataLabels = data
        .slice(0, firstFoundIndex)
        .map((e) => {
            const splitted = e.timeperiod.split(" ")
            if (timeData == "chart_data_day") {
                return splitted[1]
            }
            if (timeData == "chart_data_week" || timeData == "chart_data_year") {
                return splitted[1].slice(1, -1)
            }
            if (timeData == "chart_data_month") {
                return splitted[0].split("-")[2]
            }
            return "invalid"
        })
    const chartData = new Array(entriesCount).fill(0.0)

    let iterations = 0;
    for (let i = 0; i < data.length; i++) {
        const obj = data[i]
        if (obj.timeperiod == "TOTAL") {
            iterations++
            continue
        }
        
        chartData[i - entriesCount * iterations - iterations] += parseFloat(obj.kwh_consumed)
    }
    console.log(chartDataLabels)
    console.log(chartData)

    // const chartData = new Array((data.length / count) - 1).fill(
    //     data.forEach((e, i) => {

    //     })
    // )
    // const filtered = data.filter((val) => {
    //     val.hour_period != "TOTAL"
    // })
    // console.log(chartData)
    // for (let i = 0; i < 4; i++) {
    //     const s = await insert_reading(123, 'DATA_TYPE_POWER', subtractMins(d, (i+1) * 15))
    //     console.log(s)
    // }
}
const users = [
    {
        id: 1,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
    {
        id: 2,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
]
const devices = [
    {
        id: 123,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
    {
        id: 1234,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
    {
        id: 321,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
    {
        id: 4321,
        hourData: [
            subtractHours(d, 1),
            subtractHours(d, 2),
            subtractHours(d, 3)
        ],
        dayData: [
            subtractDays(d, 1),
            subtractDays(d, 2),
            subtractDays(d, 3),
        ],
        weekData: [
            subtractDays(d, 7),
            subtractDays(d, 7 * 2),
            subtractDays(d, 7 * 3),
        ],
        monthData: [
            subtractMonths(d, 1),
            subtractMonths(d, 2),
            subtractMonths(d, 3),
        ],
        yearData: [
            subtractYears(d, 1),
            subtractYears(d, 2),
            subtractYears(d, 3),
        ]
    },
]
// const devicesMapArr = []
async function test1() {
    for (let i = 0; i < users.length; i++) {
        const data = [
            // users[i].hourData,
            users[i].dayData,
            users[i].weekData,
            users[i].monthData,
            users[i].yearData
        ]
    
        for (let j = 0; j < data.length; j++) {
            for (let k = 0; k < data[j].length; k++) {
                if (j == 0) {
                    await get_hourly_energy_by_date(users[i].id, data[j][k].toISOString())
                }
                else if (j == 1) {
                    await get_daily_energy_by_week(users[i].id, data[j][k].toISOString())
                }
                else if (j == 2) {
                    await get_daily_energy_by_month(users[i].id, data[j][k].toISOString())
                }
                else if (j == 3) {
                    await get_monthly_energy_by_year(users[i].id, data[j][k].toISOString())
                }
            //     await 
            //     // const success = await insert_reading(devices[i].id, 'DATA_TYPE_POWER', data[j][k])
            //     // if (success) {
            //     //     console.log("Success")
            //     // } else {
            //     //     console.log("Fail")
            //     // }
            }
        }
    
    }
    console.log("DONE")
}
async function test2() {
    for (let i = 0; i < devices.length; i++) {
        const data = [
            devices[i].hourData,
            devices[i].dayData,
            devices[i].weekData,
            devices[i].monthData,
            devices[i].yearData
        ]
    
        for (let j = 0; j < data.length; j++) {
            for (let k = 0; k < data[j].length; k++) {
                const success = await insert_reading(devices[i].id, 'DATA_TYPE_POWER', data[j][k])
                if (success) {
                    console.log("Success")
                } else {
                    console.log("Fail")
                }
            }
        }
    
    }
    console.log("DONE")
}
// console.log(new Date(d.toISOString()))
// console.log(subtractHours(d, 1))
test2()
// test1()
