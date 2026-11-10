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
const connectionStatus = document.getElementById("connection-status");
const connectionLabel = document.getElementById("connection-label");
const connectButton = document.getElementById("serial-connect");
const connectButtonLabel = document.getElementById("serial-connect-label");

const localHosts = ["localhost", "127.0.0.1", "::1"];
const localDashboard = localHosts.includes(location.hostname);
const serialMode = localDashboard || location.protocol === "file:";
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

// Actualiza el indicador de conexión y activa o bloquea los botones.
function setConnection(connected, label) {
    connectionStatus.dataset.state = connected ? "connected" : "disconnected";
    connectButton.dataset.state = connected ? "connected" : "disconnected";
    connectionLabel.textContent = label;
    refreshButtonStates();
}

// Habilita los controles según el modo usado y la conexión actual.
function refreshButtonStates() {
    const ready = serialMode ? serialReady : connectionStatus.dataset.state === "connected";
    document.querySelectorAll(".controls button[type='submit']").forEach((button) => {
        button.disabled = !ready || pendingCommand !== null;
    });
    connectButton.disabled = !window.isSecureContext || !("serial" in navigator);
}

// Muestra el estado de un dispositivo y actualiza el aspecto de su tarjeta.
function showDeviceState(element, container, state, label) {
    element.textContent = label;
    container.dataset.state = state;
    const card = container.closest(".device-card");
    if (card) card.dataset.state = state;
}

// Limpia los datos y muestra que no hay información disponible.
function showUnavailableStatus() {
    showDeviceState(alarmStatus, alarmStatusContainer, "unknown", "Estado no disponible");
    showDeviceState(ledStatus, ledStatusContainer, "unknown", "Estado no disponible");
    showDeviceState(motorStatus, motorStatusContainer, "unknown", "Estado no disponible");
    sensorTemperature.textContent = "--.- °C";
    sensorHumidity.textContent = "--.- %";
    showDeviceState(sensorStatus, sensorStatusContainer, "unknown", "Lectura no disponible");
}

// Comprueba los datos recibidos y actualiza el panel con los estados.
function applyStatus(status) {
    const alarm = status.state;
    const alarmLabels = {
        apagada: "APAGADA",
        prendida: "PRENDIDA",
        disparada: "DISPARADA",
    };
    const deviceLabels = { prendido: "PRENDIDO", apagado: "APAGADO" };
    if (!Object.prototype.hasOwnProperty.call(alarmLabels, alarm) ||
        !Object.prototype.hasOwnProperty.call(deviceLabels, status.led) ||
        !Object.prototype.hasOwnProperty.call(deviceLabels, status.motor) ||
        typeof status.sensor_available !== "boolean" ||
        !Number.isInteger(status.temperature_tenths) ||
        !Number.isInteger(status.humidity_tenths)) {
        throw new Error("Respuesta de estado serial inválida.");
    }

    showDeviceState(alarmStatus, alarmStatusContainer, alarm, alarmLabels[alarm]);
    showDeviceState(ledStatus, ledStatusContainer, status.led, deviceLabels[status.led]);
    showDeviceState(motorStatus, motorStatusContainer, status.motor, deviceLabels[status.motor]);
    if (status.sensor_available) {
        sensorTemperature.textContent = (status.temperature_tenths / 10).toFixed(1) + " °C";
        sensorHumidity.textContent = (status.humidity_tenths / 10).toFixed(1) + " %";
        showDeviceState(sensorStatus, sensorStatusContainer, "available", "Sensor conectado");
    } else {
        sensorTemperature.textContent = "--.- °C";
        sensorHumidity.textContent = "--.- %";
        showDeviceState(sensorStatus, sensorStatusContainer, "unavailable", "Esperando primera lectura...");
    }
}

// Finaliza el comando pendiente y comunica si se completó o falló.
function finishCommand(error) {
    if (!pendingCommand) return;
    window.clearTimeout(pendingCommand.timeout);
    const command = pendingCommand;
    pendingCommand = null;
    refreshButtonStates();
    if (error) command.reject(error);
    else command.resolve();
}

// Interpreta cada línea recibida por UART: estado, confirmación o error.
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
            console.error("Respuesta de estado inválida:", error);
            serialReady = false;
            setConnection(false, "Respuesta serial inválida");
        }
        return;
    }
    if (line === "OK") finishCommand(null);
    else if (line.indexOf("ERR ") === 0) finishCommand(new Error(line.substring(4)));
}

// Envía una línea de texto al ESP32 por el puerto serial abierto.
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

// Lee continuamente las respuestas UART y las entrega para su interpretación.
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

// Cierra el puerto, detiene las consultas periódicas y limpia el panel.
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
        try { await serialReader.cancel(); } catch (error) { console.warn(error); }
    }
    if (port) {
        try { await port.close(); } catch (error) { console.warn("No se pudo cerrar el puerto:", error); }
    }
    connectButtonLabel.textContent = "Conectar por serial";
    setConnection(false, "Sin conexión serial");
    showUnavailableStatus();
}

// Pide al ESP32 su estado actual y muestra un aviso si no responde.
async function requestSerialStatus() {
    if (!serialConnected || pendingCommand || statusRequestPending) return;
    statusRequestPending = true;
    window.clearTimeout(statusTimer);
    statusTimer = window.setTimeout(() => {
        statusRequestPending = false;
        serialReady = false;
        setConnection(false, "Puerto abierto; ESP32 sin respuesta");
        showUnavailableStatus();
    }, 3000);
    try {
        await writeSerialLine("STATUS");
    } catch (error) {
        await closeSerial(error.message);
    }
}

// Envía una orden al ESP32 y espera su confirmación.
function sendSerialCommand(command) {
    if (!serialConnected || !serialReady || pendingCommand) {
        return Promise.reject(new Error("Primero conecta el puerto y espera la respuesta del ESP32."));
    }
    return new Promise((resolve, reject) => {
        pendingCommand = {
            resolve,
            reject,
            timeout: window.setTimeout(() => {
                finishCommand(new Error("El ESP32 no confirmó el comando."));
            }, 5000),
        };
        writeSerialLine(command).catch((error) => finishCommand(error));
    });
}

// Abre el selector del navegador y conecta el ESP32 por UART.
async function connectSerial() {
    if (!window.isSecureContext || !("serial" in navigator)) {
        setConnection(false, "Serial requiere Chrome o Edge en localhost");
        return;
    }
    connectButton.disabled = true;
    connectButtonLabel.textContent = "Abriendo puerto...";
    connectionLabel.textContent = "Selecciona el puerto serial...";
    try {
        serialPort = await navigator.serial.requestPort();
        await serialPort.open({ baudRate: 9600, dataBits: 8, stopBits: 1, parity: "none" });
        serialConnected = true;
        serialReady = false;
        receivedText = "";
        connectButtonLabel.textContent = "Desconectar serial";
        setConnection(false, "Puerto abierto; esperando ESP32...");
        void readSerial(serialPort);
        await requestSerialStatus();
        statusInterval = window.setInterval(requestSerialStatus, 1000);
    } catch (error) {
        console.error("No se pudo abrir el puerto serial:", error);
        await closeSerial("No se pudo abrir el puerto serial.");
    }
}

// Conecta o desconecta el puerto al pulsar el botón del panel.
connectButton.addEventListener("click", async () => {
    if (serialConnected) await closeSerial("Puerto serial desconectado.");
    else await connectSerial();
});

// Convierte cada formulario en una orden serial o una petición HTTP.
document.querySelectorAll(".controls form").forEach((form) => {
    form.addEventListener("submit", async (event) => {
        event.preventDefault();
        const card = form.closest(".device-card");
        const status = card && card.querySelector(".device-status [role='status']");
        if (!status) return;

        const command = form.action.split("/").slice(-2).map((part) => part.toUpperCase()).join(" ");
        const buttons = card.querySelectorAll(".controls button[type='submit']");
        buttons.forEach((button) => { button.disabled = true; });
        status.textContent = "Enviando comando...";
        try {
            if (serialMode) {
                await sendSerialCommand(command);
            } else {
                const response = await fetch(form.action, { method: form.method });
                if (!response.ok) throw new Error("HTTP " + response.status);
                await refreshWifiStatus();
            }
        } catch (error) {
            status.textContent = "No se pudo enviar el comando.";
            console.error("Error al enviar " + command + ":", error);
        } finally {
            refreshButtonStates();
            if (serialMode) await requestSerialStatus();
            else void refreshWifiStatus();
        }
    });
});

// Consulta periódicamente los estados cuando el panel se usa por Wi-Fi.
async function refreshWifiStatus() {
    if (wifiRequestPending) return;
    wifiRequestPending = true;
    try {
        const response = await fetch("/alarm/status", { cache: "no-store" });
        if (!response.ok) throw new Error("HTTP " + response.status);
        applyStatus(await response.json());
        setConnection(true, "ESP32 conectado");
    } catch (error) {
        showUnavailableStatus();
        setConnection(false, "Sin conexión");
    } finally {
        wifiRequestPending = false;
    }
}

// Inicializa el panel según se haya abierto localmente o desde el ESP32.
if (serialMode) {
    connectionLabel.textContent = "Conecta el ESP32 por serial";
    showUnavailableStatus();
    if (!window.isSecureContext || !("serial" in navigator)) {
        connectButtonLabel.textContent = "Serial no disponible";
        connectButton.title = "Abre este dashboard en Chrome o Edge desde http://localhost.";
    }
} else {
    connectButton.disabled = true;
    connectButton.title = "Conecta por serial desde el dashboard local en localhost.";
    void refreshWifiStatus();
    window.setInterval(refreshWifiStatus, 1000);
}
refreshButtonStates();
