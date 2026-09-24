
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
    // new Chart(ctx, {
    //     type: 'bar',
    //     data: {
    //     labels: ['Red', 'Blue', 'Yellow', 'Green', 'Purple', 'Orange'],
    //     datasets: [{
    //         label: '# of Votes',
    //         data: [12, 19, 3, 5, 2, 3],
    //         borderWidth: 1
    //     }]
    //     },
    //     options: {
    //     scales: {
    //         y: {
    //         beginAtZero: true
    //         }
    //     }
    //     }
    // });
    consumptionChartData.data.labels = labels
    consumptionChartData.data.datasets = datasets
    console.log(consumptionChartData)

    new Chart(ctx, consumptionChartData)
}

function DeviceModifyEvents(user_hub, access_token) {
    const deviceName = document.querySelectorAll('span[contenteditable]');
    const devicePriority = document.querySelectorAll('select[name="device_priority"]')
    const deviceOn = document.querySelectorAll('input[name="device_on"]')

    const apiRoute = `/api/send_command_to_hub/${user_hub}`
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
                        command: "PLUG_NAME",
                        deviceId,
                        device_name: newText
                    })
                    console.log(reqData)
                    const req = await fetch(apiRoute, reqData)
                    const { error } = await req.json()
                    if (!error) {
                        alert("Success")
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
            const yes = confirm(`Change device priority to ${elem.value}`)
            if (yes) {
                reqData.body = JSON.stringify({
                    command: "PLUG_PRIORITY",
                    deviceId,
                    device_priority: elem.value
                })
                console.log(reqData)

                const req = await fetch(apiRoute, reqData)
                const { error } = await req.json()
                if (!error && elem.value == "HIGH") {
                    deviceOnElem.style = "pointer-events: none"
                    deviceOnLabelElem.style = "pointer-events: none"
                    if (!deviceOnElem.checked) {
                        deviceOnElem.click()
                    }
                    // deviceOnElem.checked = true
                    alert("Success")
                    return
                } else if (!error) {
                    deviceOnElem.style = "pointer-events: default"
                    deviceOnLabelElem.style = "pointer-events: default"
                    alert("Success")
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
        console.log(devicePriorityElem)
        const label = document.getElementById(`label_device_on|${deviceId}`)
        if (devicePriorityElem.value == "HIGH") {
            elem.style = "pointer-events: none"
            label.style = "pointer-events: none"
        } else {
            elem.addEventListener("change", async () => {
                reqData.body = JSON.stringify({
                    command: elem.checked ? "PLUG_ON" : "PLUG_OFF",
                    deviceId
                })
                console.log(reqData)
    
                const req = await fetch(apiRoute, reqData)
                const { error } = await req.json()
                if (!error) {
                    // alert("Success")
                    return
                }
                alert("Failed")
                elem.checked = !elem.checked
            })
        }
    })
}

function getHourlyDataForToday(data) {
  const today = new Date();
  const hourlyData = Array.from({ length: 24 }, () => []);

  for (const item of data) {
    const date = new Date(item.timeperiod);

    const isToday = 
      date.getFullYear() === today.getFullYear() &&
      date.getMonth() === today.getMonth() &&
      date.getDate() === today.getDate();

    if (isToday) {
      const hour = date.getHours(); // 0 - 23 in local time
      hourlyData[hour].push(item);
    }
  }

  return hourlyData;
}
function getDailyDataForCurrentWeek(data) {
  const now = new Date();
  
  // Calculate Sunday at 00:00:00 UTC for the current week
  const startOfWeek = new Date(Date.UTC(
    now.getUTCFullYear(), 
    now.getUTCMonth(), 
    now.getUTCDate() - now.getUTCDay()
  ));
  
  const endOfWeek = new Date(startOfWeek);
  endOfWeek.setUTCDate(startOfWeek.getUTCDate() + 7);

  // Initialize 7 empty arrays (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
  const weeklyData = Array.from({ length: 7 }, () => []);

  for (const item of data) {
    const itemDate = new Date(item.timeperiod);

    if (itemDate >= startOfWeek && itemDate < endOfWeek) {
      const dayOfWeek = itemDate.getUTCDay(); // 0 to 6
      weeklyData[dayOfWeek].push(item);
    }
  }

  return weeklyData;
}

function getDailyDataForCurrentMonth(data) {
  const now = new Date();
  const year = now.getUTCFullYear();
  const month = now.getUTCMonth(); // 0-indexed (0 = Jan, 11 = Dec)

  // Calculate total days in the current month dynamically
  const daysInMonth = new Date(Date.UTC(year, month + 1, 0)).getUTCDate();

  // Initialize sub-arrays for each day of the month
  const dailyData = Array.from({ length: daysInMonth }, () => []);

  for (const item of data) {
    const itemDate = new Date(item.timeperiod);

    if (
      itemDate.getUTCFullYear() === year &&
      itemDate.getUTCMonth() === month
    ) {
      const dayOfMonth = itemDate.getUTCDate(); // 1 to 31
      dailyData[dayOfMonth - 1].push(item);     // Map Day 1 to Index 0
    }
  }

  return dailyData;
}

function getMonthlyDataForCurrentYear(data, targetYear = new Date().getUTCFullYear()) {
  // Initialize 12 empty arrays (0 = Jan, 1 = Feb, ..., 11 = Dec)
  const monthlyData = Array.from({ length: 12 }, () => []);

  for (const item of data) {
    const itemDate = new Date(item.timeperiod);

    if (itemDate.getUTCFullYear() === targetYear) {
      const month = itemDate.getUTCMonth(); // 0 to 11
      monthlyData[month].push(item);
    }
  }

  return monthlyData;
}

function ProcessDeviceReadingsData(data)
{
    const processedData = []
    for (let i = 0; i < data.length; i++) {
        if (data[i].length == 0) {
            processedData.push(0)
            continue;
        }

        let sum = 0
        for (let j = 0; j < data[i].length; j++) {
            if (data[i][j].type == "DATA_TYPE_ENERGY") {
                sum += data[i][j].value
            }
        }
        processedData.push(sum)
    }
    return processedData
}

function GetDataForChart(type, device_readings) {
    let data = []
    let label = ""
    let labels = []
    switch (type) {
        case "chart_data_day":
            label = "Hourly consumption for today (kWh)"
            data = ProcessDeviceReadingsData(
                getHourlyDataForToday(device_readings)
            )
            for (let i = 0; i < data.length; i++) {
                if (i < 10) labels.push(`0${i}`)
                else labels.push(`${i}`)
            }
            break
        case "chart_data_week":
            label = "Daily consumption for the week (kWh)"
            labels = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
            data = ProcessDeviceReadingsData(
                getDailyDataForCurrentWeek(device_readings)
            )
            break
        case "chart_data_month":
            label = "Daily consumption for the month (kWh)"
            data = ProcessDeviceReadingsData(
                getDailyDataForCurrentMonth(device_readings)
            )
            for (let i = 1; i <= data.length; i++) {
                labels.push(`${i}`)
            }
            break
        case "chart_data_year":
            label = "Monthly consumption for the year (kWh)"
            labels = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
            data = ProcessDeviceReadingsData(
                getMonthlyDataForCurrentYear(device_readings)
            )
            break
    }
    const dataset = {
        label,
        data,
        borderWidth: 1

    }
    return { labels, dataset }
}

function LoadChartDataBy(device_readings) {
    const buttons = document.querySelectorAll('button[name="show_chart_data"]')
    const activeButton = "py-1 bg-[#12005e] text-white flex-1 rounded-md m-0.5 shadow-sm cursor-pointer"
    const inactiveButton = "py-1 flex-1 hover:opacity-80 transition-opacity cursor-pointer"

    let showDataBy = localStorage.getItem("show_chart_data")
    if (showDataBy == null) {
        showDataBy = "chart_data_day"
    }
    const dataButton = document.getElementById(showDataBy)
    dataButton.className = activeButton

    const { labels, dataset } = GetDataForChart(showDataBy, device_readings)
    LoadChart(labels, [dataset])

    // console.log(buttons.length)
    buttons.forEach((elem) => {
        elem.addEventListener("click", (event) => {
            localStorage.setItem("show_chart_data", event.target.id)
            const { labels, dataset } = GetDataForChart(event.target.id, device_readings)
            const chart = Chart.getChart("consumption_chart")
            if (chart) {
                chart.destroy()
            }
            LoadChart(labels, [dataset])

            event.target.className = activeButton
            buttons.forEach((elem) => {
                if (elem.id != event.target.id) {
                    elem.className = inactiveButton
                }
            })
        })
    })
}

function DeleteHub(hub_id) {
    const yes = confirm("Are you sure you want delete your hub?")
    if (yes) {
        alert("Deleted")
    }
}
