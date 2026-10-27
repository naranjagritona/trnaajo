const alarmStatus = document.getElementById("alarm-status");
const alarmStatusContainer = document.querySelector(".alarm-status");
const ledStatus = document.getElementById("led-status");
const ledStatusContainer = document.querySelector(".led-status");
const motorStatus = document.getElementById("motor-status");
const motorStatusContainer = document.querySelector(".motor-status");
const sensorStatus = document.getElementById("sensor-status");
const sensorStatusContainer = document.querySelector(".sensor-status");
const sensorTemperature = document.getElementById("sensor-temperature");
const sensorHumidity = document.getElementById("sensor-humidity");
let alarmStatusRequestPending = false;

async function refreshDeviceStates() {
    if (alarmStatusRequestPending) {
        return;
    }
    alarmStatusRequestPending = true;

    try {
        const response = await fetch("/alarm/status", { cache: "no-store" });
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const status = await response.json();
        if (!["apagada", "prendida", "disparada"].includes(status.state) ||
            !["prendido", "apagado"].includes(status.led) ||
            !["prendido", "apagado"].includes(status.motor) ||
            typeof status.sensor_available !== "boolean" ||
            !Number.isInteger(status.temperature_tenths) ||
            !Number.isInteger(status.humidity_tenths)) {
            throw new Error("Respuesta de estados inválida");
        }

        alarmStatus.textContent = `Estado: ${status.state}`;
        alarmStatusContainer.dataset.state = status.state;
        ledStatus.textContent = `Estado: ${status.led}`;
        ledStatusContainer.dataset.state = status.led;
        motorStatus.textContent = `Estado: ${status.motor}`;
        motorStatusContainer.dataset.state = status.motor;

        if (status.sensor_available) {
            sensorTemperature.textContent = `${(status.temperature_tenths / 10).toFixed(1)} °C`;
            sensorHumidity.textContent = `${(status.humidity_tenths / 10).toFixed(1)} %`;
            sensorStatus.textContent = "Sensor conectado";
            sensorStatusContainer.dataset.state = "available";
        } else {
            sensorTemperature.textContent = "--.- °C";
            sensorHumidity.textContent = "--.- %";
            sensorStatus.textContent = "Esperando primera lectura...";
            sensorStatusContainer.dataset.state = "unavailable";
        }
    } catch (error) {
        for (const [element, container] of [
            [alarmStatus, alarmStatusContainer],
            [ledStatus, ledStatusContainer],
            [motorStatus, motorStatusContainer],
        ]) {
            element.textContent = "Estado no disponible";
            container.dataset.state = "unknown";
        }
        sensorTemperature.textContent = "--.- °C";
        sensorHumidity.textContent = "--.- %";
        sensorStatus.textContent = "Lectura no disponible";
        sensorStatusContainer.dataset.state = "unknown";
        console.error("No se pudieron consultar los estados del ESP32:", error);
    } finally {
        alarmStatusRequestPending = false;
    }
}

refreshDeviceStates();
window.setInterval(refreshDeviceStates, 1000);

document.querySelectorAll(".controls form").forEach((form) => {
    form.addEventListener("submit", async (event) => {
        event.preventDefault();

        const button = form.querySelector('button[type="submit"]');
        const card = form.closest(".device-card");
        const status = card?.querySelector(".device-status span:last-child");
        const buttons = card?.querySelectorAll('button[type="submit"]');
        if (!button || !status || !buttons) {
            return;
        }

        const originalLabels = Array.from(buttons, (cardButton) => cardButton.textContent);
        buttons.forEach((cardButton) => {
            cardButton.disabled = true;
        });
        status.textContent = "Enviando comando...";

        try {
            const response = await fetch(form.action, {
                method: form.method,
                headers: { Accept: "text/html" },
            });
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }

            status.textContent = "Comando aplicado.";
        } catch (error) {
            status.textContent = "No se pudo enviar el comando.";
            console.error("Error al controlar el dispositivo:", error);
        } finally {
            buttons.forEach((cardButton, index) => {
                cardButton.disabled = false;
                cardButton.textContent = originalLabels[index];
            });
            refreshDeviceStates();
        }
    });
});