const temperatures = [];
const humidities = [];
const localHosts = ["localhost", "127.0.0.1", "::1"];
const serialMode = location.protocol === "file:" || localHosts.includes(location.hostname);
const encoder = new TextEncoder();
const decoder = new TextDecoder();

let serialPort = null;
let serialReader = null;
let serialConnected = false;
let serialReady = false;
let receivedText = "";
let pendingCommand = null;
let statusTimer = null;
let statusInterval = null;
let statusRequestPending = false;
let wifiRequestPending = false;

const elements = {
    connection: document.getElementById("connection-status"),
    connectionLabel: document.getElementById("connection-label"),
    serialButton: document.getElementById("serial-connect"),
    menu: document.getElementById("menu"),
    menuButton: document.getElementById("menu-boton"),
    sensitivity: document.getElementById("sensibilidad"),
    sensitivityValue: document.getElementById("sensibilidad-valor"),
};

function refreshButtonStates() {
    const ready = serialMode
        ? serialReady
        : elements.connection.dataset.state === "connected";
    document.querySelectorAll(".botones button[type='submit']").forEach((button) => {
        button.disabled = !ready || pendingCommand !== null;
    });
}

function setConnection(connected, label) {
    elements.connection.dataset.state = connected ? "connected" : "disconnected";
    elements.connectionLabel.textContent = label;
    refreshButtonStates();
}

function showDeviceState(id, state, label) {
    const status = document.getElementById(id);
    const card = status.closest(".estado-card");
    status.textContent = label;
    card.dataset.state = state;
}

function showUnavailableStatus(label) {
    showDeviceState("alarm-status", "unknown", label);
    showDeviceState("led-status", "unknown", label);
    showDeviceState("motor-status", "unknown", label);
    document.getElementById("sensor-temperature").innerHTML = "--.- <small>°C</small>";
    document.getElementById("sensor-humidity").innerHTML = "--.- <small>%</small>";
    showDeviceState("sensor-status", "unknown", label);
}

function applyStatus(status) {
    const alarmLabels = {
        apagada: "APAGADA",
        prendida: "PRENDIDA",
        disparada: "DISPARADA",
    };
    const deviceLabels = { prendido: "PRENDIDO", apagado: "APAGADO" };
    if (!Object.prototype.hasOwnProperty.call(alarmLabels, status.state) ||
        !Object.prototype.hasOwnProperty.call(deviceLabels, status.led) ||
        !Object.prototype.hasOwnProperty.call(deviceLabels, status.motor) ||
        typeof status.sensor_available !== "boolean" ||
        !Number.isInteger(status.temperature_tenths) ||
        !Number.isInteger(status.humidity_tenths)) {
        throw new Error("Respuesta de estado inválida.");
    }

    showDeviceState("alarm-status", status.state, alarmLabels[status.state]);
    showDeviceState("led-status", status.led, deviceLabels[status.led]);
    showDeviceState("motor-status", status.motor, deviceLabels[status.motor]);
    document.getElementById("alarm-control-card").dataset.state = status.state;

    if (status.sensor_available) {
        updateReadings(status);
    } else {
        document.getElementById("sensor-card").dataset.state = "unknown";
        document.getElementById("sensor-status").textContent = "ESPERANDO LECTURA";
        document.getElementById("sensor-temperature").innerHTML = "--.- <small>°C</small>";
        document.getElementById("sensor-humidity").innerHTML = "--.- <small>%</small>";
    }
}

function drawChart(id, values, color) {
    const canvas = document.getElementById(id);
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    if (width === 0 || height === 0 || values.length < 2) return;

    const ratio = window.devicePixelRatio || 1;
    canvas.width = width * ratio;
    canvas.height = height * ratio;
    const context = canvas.getContext("2d");
    context.scale(ratio, ratio);
    context.clearRect(0, 0, width, height);

    let minimum = values[0];
    let maximum = values[0];
    for (let i = 1; i < values.length; i++) {
        minimum = Math.min(minimum, values[i]);
        maximum = Math.max(maximum, values[i]);
    }

    const range = Math.max(1, maximum - minimum);
    context.beginPath();
    values.forEach((value, index) => {
        const x = index * width / (values.length - 1);
        const y = height - (value - minimum) / range * (height - 8) - 4;
        if (index === 0) context.moveTo(x, y);
        else context.lineTo(x, y);
    });
    context.strokeStyle = color;
    context.lineWidth = 2;
    context.stroke();
}

function updateReadings(status) {
    const temperature = status.temperature_tenths / 10;
    const humidity = status.humidity_tenths / 10;
    document.getElementById("sensor-temperature").innerHTML =
        temperature.toFixed(1) + " <small>°C</small>";
    document.getElementById("sensor-humidity").innerHTML =
        humidity.toFixed(1) + " <small>%</small>";
    showDeviceState("sensor-status", "available", "CONECTADO");
    document.getElementById("sensor-card").dataset.state = "available";

    temperatures.push(temperature);
    humidities.push(humidity);
    if (temperatures.length > 30) temperatures.shift();
    if (humidities.length > 30) humidities.shift();
    drawChart("temperature-chart", temperatures, "#64dfb0");
    drawChart("humidity-chart", humidities, "#7dc8fa");
}

function finishCommand(error) {
    if (!pendingCommand) return;
    window.clearTimeout(pendingCommand.timeout);
    const command = pendingCommand;
    pendingCommand = null;
    refreshButtonStates();
    if (error) command.reject(error);
    else command.resolve();
}

function handleSerialLine(value) {
    const line = value.trim();
    if (!line) return;

    if (line.charAt(0) === "{") {
        statusRequestPending = false;
        window.clearTimeout(statusTimer);
        statusTimer = null;
        try {
            applyStatus(JSON.parse(line));
            serialReady = true;
            setConnection(true, "ESP32 conectado por serial");
        } catch (error) {
            console.error("Respuesta serial inválida:", error);
            serialReady = false;
            setConnection(false, "Respuesta serial inválida");
            showUnavailableStatus("ESTADO INVÁLIDO");
        }
    } else if (line === "OK") {
        finishCommand(null);
    } else if (line.indexOf("ERR ") === 0) {
        finishCommand(new Error(line.substring(4)));
    }
}

async function writeSerialLine(line) {
    if (!serialPort || !serialPort.writable) {
        throw new Error("El puerto serial no está abierto.");
    }
    const writer = serialPort.writable.getWriter();
    try {
        await writer.write(encoder.encode(line + "\r\n"));
    } finally {
        writer.releaseLock();
    }
}

async function readSerial(port) {
    try {
        while (serialPort === port && port.readable) {
            serialReader = port.readable.getReader();
            try {
                while (true) {
                    const result = await serialReader.read();
                    if (result.done) break;
                    receivedText += decoder.decode(result.value, { stream: true });
                    const lines = receivedText.split(/\r?\n/);
                    receivedText = lines.pop() || "";
                    lines.forEach(handleSerialLine);
                }
            } finally {
                serialReader.releaseLock();
                serialReader = null;
            }
        }
    } catch (error) {
        if (serialConnected) console.error("Error leyendo el puerto serial:", error);
    } finally {
        if (serialPort === port) await closeSerial("Conexión serial cerrada.");
    }
}

async function closeSerial(reason) {
    const port = serialPort;
    serialPort = null;
    serialConnected = false;
    serialReady = false;
    receivedText = "";
    statusRequestPending = false;
    window.clearTimeout(statusTimer);
    window.clearInterval(statusInterval);
    statusTimer = null;
    statusInterval = null;
    finishCommand(new Error(reason));

    if (serialReader) {
        try {
            await serialReader.cancel();
        } catch (error) {
            console.warn("No se pudo detener la lectura serial:", error);
        }
    }
    if (port) {
        try {
            await port.close();
        } catch (error) {
            console.warn("No se pudo cerrar el puerto:", error);
        }
    }

    elements.serialButton.textContent = "Conectar por serial";
    setConnection(false, reason);
    showUnavailableStatus("SIN CONEXIÓN");
}

async function requestSerialStatus() {
    if (!serialConnected || pendingCommand || statusRequestPending) return;
    statusRequestPending = true;
    window.clearTimeout(statusTimer);
    statusTimer = window.setTimeout(() => {
        statusRequestPending = false;
        serialReady = false;
        setConnection(false, "Puerto abierto; ESP32 sin respuesta");
        showUnavailableStatus("SIN CONEXIÓN");
    }, 3000);

    try {
        await writeSerialLine("STATUS");
    } catch (error) {
        await closeSerial(error.message);
    }
}

function sendSerialCommand(command) {
    if (!serialConnected || !serialReady || pendingCommand) {
        return Promise.reject(new Error("Conecta el puerto y espera la respuesta del ESP32."));
    }

    return new Promise((resolve, reject) => {
        pendingCommand = {
            resolve,
            reject,
            timeout: window.setTimeout(() => {
                finishCommand(new Error("El ESP32 no confirmó el comando."));
            }, 5000),
        };
        refreshButtonStates();
        writeSerialLine(command).catch((error) => finishCommand(error));
    });
}

async function connectSerial() {
    if (!window.isSecureContext || !("serial" in navigator)) {
        setConnection(false, "Web Serial requiere Chrome o Edge en localhost");
        return;
    }

    elements.serialButton.disabled = true;
    elements.serialButton.textContent = "Abriendo puerto...";
    try {
        serialPort = await navigator.serial.requestPort();
        await serialPort.open({
            baudRate: 9600,
            dataBits: 8,
            stopBits: 1,
            parity: "none",
        });
        serialConnected = true;
        serialReady = false;
        receivedText = "";
        elements.serialButton.textContent = "Desconectar serial";
        setConnection(false, "Puerto abierto; esperando ESP32...");
        void readSerial(serialPort);
        await requestSerialStatus();
        statusInterval = window.setInterval(requestSerialStatus, 1000);
    } catch (error) {
        console.error("No se pudo conectar por serial:", error);
        await closeSerial("No se pudo abrir el puerto serial.");
    } finally {
        elements.serialButton.disabled = false;
    }
}

async function refreshWifiStatus() {
    if (wifiRequestPending) return;
    wifiRequestPending = true;
    try {
        const response = await fetch("/alarm/status", { cache: "no-store" });
        if (!response.ok) throw new Error("HTTP " + response.status);
        applyStatus(await response.json());
        setConnection(true, "ESP32 conectado por Wi-Fi");
    } catch (error) {
        console.error("No se pudo consultar el ESP32:", error);
        setConnection(false, "Sin conexión Wi-Fi");
        showUnavailableStatus("SIN CONEXIÓN");
    } finally {
        wifiRequestPending = false;
    }
}

elements.menuButton.addEventListener("click", () => {
    const open = elements.menu.classList.toggle("abierto");
    elements.menuButton.setAttribute("aria-expanded", String(open));
});

elements.menu.querySelectorAll("a").forEach((link) => {
    link.addEventListener("click", () => {
        elements.menu.classList.remove("abierto");
        elements.menuButton.setAttribute("aria-expanded", "false");
    });
});

elements.sensitivity.addEventListener("input", (event) => {
    elements.sensitivityValue.value = event.target.value + "%";
});

elements.serialButton.addEventListener("click", async () => {
    if (serialConnected) await closeSerial("Puerto serial desconectado.");
    else await connectSerial();
});

document.querySelectorAll(".botones form").forEach((form) => {
    form.addEventListener("submit", async (event) => {
        event.preventDefault();
        const card = form.closest(".dispositivo");
        const buttons = card.querySelectorAll("button[type='submit']");
        const originalText = Array.from(buttons, (button) => button.textContent);
        buttons.forEach((button) => {
            button.disabled = true;
            button.textContent = "Enviando...";
        });

        const command = form.action.split("/").slice(-2)
            .map((part) => part.toUpperCase()).join(" ");
        try {
            if (serialMode) {
                await sendSerialCommand(command);
            } else {
                const response = await fetch(form.action, { method: form.method });
                if (!response.ok) throw new Error("HTTP " + response.status);
                await refreshWifiStatus();
            }
        } catch (error) {
            console.error("No se pudo enviar " + command + ":", error);
        } finally {
            buttons.forEach((button, index) => {
                button.textContent = originalText[index];
            });
            refreshButtonStates();
            if (serialMode) await requestSerialStatus();
            else void refreshWifiStatus();
        }
    });
});

window.addEventListener("resize", () => {
    drawChart("temperature-chart", temperatures, "#64dfb0");
    drawChart("humidity-chart", humidities, "#7dc8fa");
});

if (serialMode) {
    elements.serialButton.hidden = false;
    setConnection(false, "Conecta el ESP32 por serial");
    showUnavailableStatus("ESPERANDO CONEXIÓN");
    if (!window.isSecureContext || !("serial" in navigator)) {
        elements.serialButton.disabled = true;
        elements.serialButton.title = "Abre este panel en Chrome o Edge desde localhost.";
    }
} else {
    elements.serialButton.hidden = true;
    setConnection(false, "Conectando por Wi-Fi...");
    void refreshWifiStatus();
    window.setInterval(refreshWifiStatus, 1000);
}
