
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