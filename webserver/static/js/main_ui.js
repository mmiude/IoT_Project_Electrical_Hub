
let consumptionChartCardFullScreen = false
let deviceCardFullScreen = false
let settingsCardFullScreen = false

const consumptionChartData = {
    type: "bar",
    data: {
        labels: [],
        datasets: []
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            y: {
                beginAtZero: true
            }
        }
    }
}

const minutes = 15
setInterval(() => {
    window.location.reload()
}, minutes * 60 * 1000);


function ToggleCardSize(id) {
    const cards = [
        { id: "consumption_chart_card", fullScreen: consumptionChartCardFullScreen },
        { id: "devices_card", fullScreen: deviceCardFullScreen },
        { id: "settings_card", fullScreen: settingsCardFullScreen }
    ]
    const fullScreen = !cards[id].fullScreen

    for (let i = 0; i < cards.length; i++) {
        const cardObj = cards[i]
        const card = document.getElementById(cardObj.id)
        if (i == id) {
            const buttonIcon = document.getElementById(`${cardObj.id}_toggle_icon`)
            if (fullScreen) {
                buttonIcon.src = "/static/icons/minimize.png"
            } else {
                buttonIcon.src = "/static/icons/full_screen.png"
            }
        } else {
            if (fullScreen) card.style.display = "none"
            else card.style.display = "flex"
        }
    }
    return fullScreen
}

function LoadChart(labels, datasets) {
    const ctx = document.getElementById("consumption_chart")
    consumptionChartData.data.labels = labels
    consumptionChartData.data.datasets = datasets
    console.log(consumptionChartData)

    new Chart(ctx, consumptionChartData)
}

function DeviceAndHubModifyEvents(userId, hubId, access_token) {
    const deviceName = document.querySelectorAll('span[contenteditable]');
    const devicePriority = document.querySelectorAll('select[name="device_priority"]')
    const deviceOn = document.querySelectorAll('input[name="device_on"]')
    const thresholdInputs = document.querySelectorAll("input[name='threshold']")

    const apiRoute = `/api/send_command_to_hub?userId=${userId}&hubId=${hubId}`
    const reqData = {
        method: "POST",
        headers: {
            "Authorization": `Bearer ${access_token}`,
            "Content-Type": "application/json"
        }
    }

    deviceName.forEach((elem) => {
        let initialText = '';
        elem.addEventListener('focus', () => {
            initialText = elem.innerText;
        });

        // Check if value changed when user clicks out
        elem.addEventListener('blur', async () => {
            const newText = elem.innerText;
            const deviceId = elem.id.split("|")[1]
            
            if (newText !== initialText) {
                const yes = confirm(`Change device name to "${newText}"`)
                if (yes) {
                    reqData.body = JSON.stringify({
                        deviceId,
                        type: "DATA_TYPE_DEVICE_NAME",
                        data: {
                            value: 0,
                            value_int: 0,
                            command: "NONE",
                            c_value: newText
                        }
                        // command: "PLUG_NAME",
                        // deviceId,
                        // device_name: newText
                    })
                    console.log(reqData)
                    const req = await fetch(apiRoute, reqData)
                    const { error } = await req.json()
                    if (!error) {
                        window.location.reload()
                        return
                    }
                    alert("Failed")
                }
                elem.innerText = initialText
            }
        });

        // Blur on Enter key (optional)
        elem.addEventListener('keydown', (event) => {
            if (event.key === 'Enter') {
                event.preventDefault(); // Prevents adding a new line break
                elem.blur();
            }
        });
    })

    devicePriority.forEach((elem) => {
        const prevVal = elem.value
        const deviceId = elem.id.split("|")[1]

        const deviceOnElem = document.getElementById(`device_on|${deviceId}`)
        const deviceOnLabelElem = document.getElementById(`label_device_on|${deviceId}`)

        elem.addEventListener("change", async () => {
            const prioName = elem.value == 0 ? "HIGH" : elem.value == 1 ? "LOW" : "MED"
            const yes = confirm(`Change device priority to ${prioName}`)
            if (yes) {
                reqData.body = JSON.stringify({
                    deviceId,
                    type: "DATA_TYPE_PRIORITY",
                    data: {
                        value: 0,
                        value_int: elem.value,
                        command: "NONE",
                        c_value: "NONE"
                    }
                    // command: "PLUG_PRIORITY",
                    // deviceId,
                    // device_priority: elem.value
                })
                console.log(reqData)

                const req = await fetch(apiRoute, reqData)
                const { error } = await req.json()
                // if (!error && elem.value == 0) {
                //     // deviceOnElem.style = "pointer-events: none"
                //     // deviceOnLabelElem.style = "pointer-events: none"
                //     if (!deviceOnElem.checked) {
                //         deviceOnElem.click()
                //     }
                //     // deviceOnElem.checked = true
                //     window.location.reload()
                //     return
                if (!error) {
                    // deviceOnElem.style = "pointer-events: default"
                    // deviceOnLabelElem.style = "pointer-events: default"
                    window.location.reload()
                    return
                }
                alert("Failed")
            }
            elem.value = prevVal
        })
    })

    deviceOn.forEach((elem) => {
        const deviceId = elem.id.split("|")[1]
        console.log(deviceId)
        const devicePriorityElem = document.getElementById(`device_priority|${deviceId}`)
        // console.log(devicePriorityElem)
        const label = document.getElementById(`label_device_on|${deviceId}`)
        // if (devicePriorityElem.value == 0) {
        //     elem.style = "pointer-events: none"
        //     label.style = "pointer-events: none"
        // } else {
        elem.addEventListener("change", async () => {
            reqData.body = JSON.stringify({
                deviceId,
                type: "DATA_TYPE_COMMAND",
                data: {
                    value: 0,
                    value_int: 0,
                    command: elem.checked ? "PLUG_ON" : "PLUG_OFF",
                    c_value: "NONE" 
                }
                // command: elem.checked ? "PLUG_ON" : "PLUG_OFF",
                // deviceId
            })
            console.log(reqData)

            const req = await fetch(apiRoute, reqData)
            const { error } = await req.json()
            if (!error) {
                window.location.reload()
                // alert("Success")
                return
            }
            alert("Failed")
            elem.checked = !elem.checked
        })
        // }
    })

    thresholdInputs.forEach((elem) => {
        const prevVal = elem.value
        const elemId = elem.id
        const split = elemId.split("_")

        elem.addEventListener("change", async () => {
            const yes = confirm(`Change ${split[1]} devices price threshold to ${elem.value} (c/kWh)`)
            if (!yes) {
                return
            }

            reqData.body = JSON.stringify({
                deviceId: 0,
                type: `DATA_TYPE_${elemId}`,
                data: {
                    value: elem.value,
                    value_int: 0,
                    command: "NONE",
                    c_value: "NONE"
                }
                // command: elemId,
                // deviceId: 0,
                // threshold: elem.value
            })
            const req = await fetch(apiRoute, reqData)
            const { error } = await req.json()
            if (!error) {
                window.location.reload()
                return
            }
            alert("Failed")
            elem.value = prevVal
        })
    })
}

// function HubModifyEvents(hubId, access_token) {
//     const inputs = document.querySelectorAll("input[name='threshold']")
//     inputs.forEach((elem) => {
//         elem.addEventListener("change", async () => {
            
//         })
//     })
// }

// function getHourlyDataForToday(data) {
//   const today = new Date();
//   const hourlyData = Array.from({ length: 24 }, () => []);

//   for (const item of data) {
//     const date = new Date(item.timeperiod);

//     const isToday = 
//       date.getFullYear() === today.getFullYear() &&
//       date.getMonth() === today.getMonth() &&
//       date.getDate() === today.getDate();

//     if (isToday) {
//       const hour = date.getHours(); // 0 - 23 in local time
//       hourlyData[hour].push(item);
//     }
//   }

//   return hourlyData;
// }
// function getDailyDataForCurrentWeek(data) {
//   const now = new Date();
  
//   // Calculate Sunday at 00:00:00 UTC for the current week
//   const startOfWeek = new Date(Date.UTC(
//     now.getUTCFullYear(), 
//     now.getUTCMonth(), 
//     now.getUTCDate() - now.getUTCDay()
//   ));
  
//   const endOfWeek = new Date(startOfWeek);
//   endOfWeek.setUTCDate(startOfWeek.getUTCDate() + 7);

//   // Initialize 7 empty arrays (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
//   const weeklyData = Array.from({ length: 7 }, () => []);

//   for (const item of data) {
//     const itemDate = new Date(item.timeperiod);

//     if (itemDate >= startOfWeek && itemDate < endOfWeek) {
//       const dayOfWeek = itemDate.getUTCDay(); // 0 to 6
//       weeklyData[dayOfWeek].push(item);
//     }
//   }

//   return weeklyData;
// }

// function getDailyDataForCurrentMonth(data) {
//   const now = new Date();
//   const year = now.getUTCFullYear();
//   const month = now.getUTCMonth(); // 0-indexed (0 = Jan, 11 = Dec)

//   // Calculate total days in the current month dynamically
//   const daysInMonth = new Date(Date.UTC(year, month + 1, 0)).getUTCDate();

//   // Initialize sub-arrays for each day of the month
//   const dailyData = Array.from({ length: daysInMonth }, () => []);

//   for (const item of data) {
//     const itemDate = new Date(item.timeperiod);

//     if (
//       itemDate.getUTCFullYear() === year &&
//       itemDate.getUTCMonth() === month
//     ) {
//       const dayOfMonth = itemDate.getUTCDate(); // 1 to 31
//       dailyData[dayOfMonth - 1].push(item);     // Map Day 1 to Index 0
//     }
//   }

//   return dailyData;
// }

// function getMonthlyDataForCurrentYear(data, targetYear = new Date().getUTCFullYear()) {
//   // Initialize 12 empty arrays (0 = Jan, 1 = Feb, ..., 11 = Dec)
//   const monthlyData = Array.from({ length: 12 }, () => []);

//   for (const item of data) {
//     const itemDate = new Date(item.timeperiod);

//     if (itemDate.getUTCFullYear() === targetYear) {
//       const month = itemDate.getUTCMonth(); // 0 to 11
//       monthlyData[month].push(item);
//     }
//   }

//   return monthlyData;
// }

// function ProcessDeviceReadingsData(data)
// {
//     const processedData = []
//     for (let i = 0; i < data.length; i++) {
//         if (data[i].length == 0) {
//             processedData.push(0)
//             continue;
//         }

//         let sum = 0
//         for (let j = 0; j < data[i].length; j++) {
//             if (data[i][j].type == "DATA_TYPE_POWER") {
//                 sum += data[i][j].value
//             }
//         }
//         processedData.push(sum)
//     }
//     return processedData
// }

async function fetchDeviceReadings(userId, timeData, date, access_token) {
    const req = await fetch(
        `/api/get_device_readings?userId=${userId}&timeData=${timeData}&date=${date}`,
        {
            method: "GET",
            headers: {
                "Authorization": `Bearer ${access_token}`,
                "Content-Type": "application/json"
            }
        }
    )
    const { error, data } = await req.json()
    return { error, data }
}

function GetLabelsForChart(data, timeData) {
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

    const labels = data
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

    return { labels, entriesCount }
}

async function GetDataForChartGroupByDevice(userId, timeData, date, group_by_price, access_token) {
    const { error, data } = await fetchDeviceReadings(userId, timeData, date, access_token)
    if (error) {
        return
    }
    // console.log(data)
    const { labels, entriesCount } = GetLabelsForChart(data, timeData)

    const deviceReadingsGrouped = [];
    let currentGroup = [];
    for (const item of data) {
        // Once we hit "TOTAL", push the current group and reset
        if (item.timeperiod === "TOTAL") {
            deviceReadingsGrouped.push(currentGroup);
            currentGroup = [];
            continue
        }
        currentGroup.push(item);
    }

    // Catch any trailing objects if the list doesn't end in "TOTAL"
    let datasets = []
    for (const deviceReadingGroup of deviceReadingsGrouped) {
        let dataSetData = Array.from({ length: entriesCount }, () => ({ kWh: 0.0, price: 0.0 }))
        const dataset = {
            label: "",
            borderWidth: 1
        }
        for (let i = 0; i < deviceReadingGroup.length; i++) {
            const deviceReading = deviceReadingGroup[i]
            if (dataset.label == "") {
                dataset.label = group_by_price ?
                    `${deviceReading.device_name} (eur)` : 
                    `${deviceReading.device_name} (kWh)`
            }
            dataSetData[i].kWh = deviceReading.kwh_consumed
            if (deviceReading.avg_price) {
                dataSetData[i].price = deviceReading.avg_price
            }
        }
        dataset.data = dataSetData.map((val) => group_by_price ? val.kWh * val.price : val.kWh)
        datasets.push(dataset)
    }
    return { labels, datasets }
}

async function GetDataForChart(userId, timeData, date, group_by_price, access_token) {
    let chartData = []
    let label = ""
    let labels = []

    switch (timeData) {
        case "chart_data_day":
            label = group_by_price ?
                "Money spent on electricity today per hour (eur)" :
                "Hourly consumption for today (kWh)"
            break
        case "chart_data_week":
            label = group_by_price ?
                "Money spent on electricity every day for the week (eur)" :
                "Daily consumption for the week (kWh)"
            break
        case "chart_data_month":
            label = group_by_price ? 
                "Money spent on electricity every day for the month (eur)" :
                "Daily consumption for the month (kWh)"
            break
        case "chart_data_year":
            label = group_by_price ?
                "Money spent on electricity every month for the year (eur)" :
                "Monthly consumption for the year (kWh)"
            break
    }

    const { error, data } = await fetchDeviceReadings(userId, timeData, date, access_token)

    if (!error) {
        // let prevIndex = 0
        // let firstFoundIndex = 0;
        // let index = 0
        // let count = 0;
        // while (index >= 0) {
        //     prevIndex = index
        //     index = data.findIndex((e, i) => (i > index && e.timeperiod == "TOTAL"))
        //     if (prevIndex == 0) firstFoundIndex = index
        //     if (index >= 0) count++;
        // }
        // const entriesCount = (data.length / count) - 1
    
        // labels = data
        //     .slice(0, firstFoundIndex)
        //     .map((e) => {
        //         const splitted = e.timeperiod.split(" ")
        //         if (timeData == "chart_data_day") {
        //             return splitted[1]
        //         }
        //         if (timeData == "chart_data_week" || timeData == "chart_data_year") {
        //             return splitted[1].slice(1, -1)
        //         }
        //         if (timeData == "chart_data_month") {
        //             return splitted[0].split("-")[2]
        //         }
        //         return "invalid"
        //     })
        const labelData = GetLabelsForChart(data, timeData)
        labels = labelData.labels
        chartData = Array.from({ length: labelData.entriesCount }, () => ({ kWh: 0.0, price: 0.0 }))
    
        let iterations = 0;
        for (let i = 0; i < data.length; i++) {
            const obj = data[i]
            if (obj.timeperiod == "TOTAL") {
                iterations++
                continue
            }
            
            const cDataIdx = i - labelData.entriesCount * iterations - iterations
            chartData[cDataIdx].kWh += parseFloat(obj.kwh_consumed)
            if (obj.avg_price) {
                chartData[cDataIdx].price += parseFloat(obj.avg_price)
            }
        }
        for (let i = 0; i < chartData.length; i++) {
            chartData[i].price /= iterations
        }
    }

    const dataset = {
        label,
        data: chartData.map((value) => group_by_price ? value.kWh * value.price : value.kWh),
        borderWidth: 1
    }
    return { labels, datasets : [dataset] }
}

let by_price = localStorage.getItem("by_price") == "true"
let chart_data_by_date = localStorage.getItem("chart_data_by_date")
let group_by_device = localStorage.getItem("group_by_device") == "true"
async function LoadChartDataBy(userId, access_token) {
    const buttons = document.querySelectorAll('button[name="show_chart_data"]')
    const activeButton = "py-1 bg-[#12005e] text-white flex-1 rounded-md m-0.5 shadow-sm cursor-pointer"
    const inactiveButton = "py-1 flex-1 hover:opacity-80 transition-opacity cursor-pointer"
    const kwh_or_eurSwitch = document.getElementById("switch-component-on_kwh_eur")
    const dateSelector = document.getElementById("chart_data_by_date")
    const groupByDeviceSelector = document.getElementById("group_by_device")

    kwh_or_eurSwitch.addEventListener("change", (event) => {
        by_price = event.target.checked
        localStorage.setItem("by_price", by_price)
        window.location.reload()
    })
    dateSelector.addEventListener("change", (event) => {
        chart_data_by_date = event.target.value
        localStorage.setItem("chart_data_by_date", chart_data_by_date)
        window.location.reload()
    })
    groupByDeviceSelector.addEventListener("change", (event) => {
        group_by_device = event.target.checked
        localStorage.setItem("group_by_device", group_by_device)
        window.location.reload()
    })

    kwh_or_eurSwitch.checked = by_price
    groupByDeviceSelector.checked = group_by_device


    if (chart_data_by_date == null) {
        chart_data_by_date = new Date().toLocaleDateString("en-CA")
    }
    dateSelector.value = chart_data_by_date

    let showDataBy = localStorage.getItem("show_chart_data")
    if (showDataBy == null) {
        showDataBy = "chart_data_day"
    }
    const dataButton = document.getElementById(showDataBy)
    dataButton.className = activeButton

    let dataForChart = { labels: [], datasets: [] }
    if (group_by_device) {
        dataForChart = await GetDataForChartGroupByDevice(userId, showDataBy, chart_data_by_date, by_price, access_token)
    } else {
        dataForChart = await GetDataForChart(userId, showDataBy, chart_data_by_date, by_price, access_token)
    }
    const { labels, datasets } = dataForChart
    LoadChart(labels, datasets)

    // console.log(buttons.length)
    buttons.forEach((elem) => {
        elem.addEventListener("click", async (event) => {
            localStorage.setItem("show_chart_data", event.target.id)
            let dataForChart = { labels: [], datasets: [] }
            if (group_by_device) {
                dataForChart = await GetDataForChartGroupByDevice(userId, event.target.id, chart_data_by_date, by_price, access_token)
            } else {
                dataForChart = await GetDataForChart(userId, event.target.id, chart_data_by_date, by_price, access_token)
            }
            // const { labels, datasets } = await GetDataForChart(userId, event.target.id, chart_data_by_date, by_price, access_token)
            const { labels, datasets } = dataForChart
            const chart = Chart.getChart("consumption_chart")
            if (chart) {
                chart.destroy()
            }
            LoadChart(labels, datasets)

            event.target.className = activeButton
            buttons.forEach((elem) => {
                if (elem.id != event.target.id) {
                    elem.className = inactiveButton
                }
            })
        })
    })
}

async function DeleteHub(userId, access_token) {
    const yes = confirm("Are you sure you want delete your hub?")
    if (yes) {
        const req = await fetch(`/api/delete_hub_from_user?userId=${userId}`, {
            method: "POST",
            headers: {
                "Authorization": `Bearer ${access_token}`
            }
        })
        if (req.status != 200) {
            const message = await req.text()
            alert(`Error deleting hub:\n${message}`)
        } else {
            window.location.reload()
        }
    }
}

async function RegisterHub(userId, access_token) {
    let hubId = prompt("Enter your hub id:")
    if (hubId == null || hubId == "") {
        return
    }
    const req = await fetch(`/api/register_hub?userId=${userId}&hubId=${hubId}`, {
        method: "POST",
        headers: {
            "Authorization": `Bearer ${access_token}`
        }
    })
    if (req.status != 200) {
        const message = await req.text()
        alert(`Error registering hub:\n${message}`)
    } else {
        window.location.reload()
    }
}

async function DeleteLog(logId, access_token) {
    const req = await fetch(`/api/delete_hub_log?logId=${logId}`, {
        method: "POST",
        headers: {
            "Authorization": `Bearer ${access_token}`
        }
    })
    if (req.status != 200) {
        const message = await req.text()
        alert(`Error deleting log:\n${message}`)
    } else {
        window.location.reload()
    }
}

async function DeleteLogs(hubId, access_token) {
    const req = await fetch(`/api/delete_hub_logs?hubId=${hubId}`, {
        method: "POST",
        headers: {
            "Authorization": `Bearer ${access_token}`
        }
    })
    const message = await req.text()
    if (req.status != 200) {
        alert(`Error deleting logs:\n${message}`)
    } else if (message == "OK") {
        window.location.reload()
    }
}
