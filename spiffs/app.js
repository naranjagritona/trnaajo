// Guardamos las lecturas para dibujar los gráficos.
const temperaturas = [];
const humedades = [];
let consultaEnCurso = false;

// Busca un elemento de la página por su identificador.
function buscar(id) {
    return document.getElementById(id);
}

// Cambia el texto y el color del estado de un dispositivo.
function mostrarEstado(id, estado) {
    const texto = buscar(id);
    texto.textContent = estado;
    texto.closest(".estado-card").dataset.state = estado.toLowerCase();
}

// Dibuja una línea sencilla con los últimos valores del sensor.
function dibujarGrafico(id, valores, color) {
    const lienzo = buscar(id);
    const ancho = lienzo.clientWidth;
    const alto = lienzo.clientHeight;

    if (ancho === 0 || alto === 0 || valores.length < 2) {
        return;
    }

    lienzo.width = ancho * window.devicePixelRatio;
    lienzo.height = alto * window.devicePixelRatio;

    const lapiz = lienzo.getContext("2d");
    lapiz.scale(window.devicePixelRatio, window.devicePixelRatio);
    lapiz.clearRect(0, 0, ancho, alto);

    let valorMenor = valores[0];
    let valorMayor = valores[0];

    for (let i = 1; i < valores.length; i++) {
        if (valores[i] < valorMenor) valorMenor = valores[i];
        if (valores[i] > valorMayor) valorMayor = valores[i];
    }

    const rango = Math.max(1, valorMayor - valorMenor);
    lapiz.beginPath();

    for (let i = 0; i < valores.length; i++) {
        const x = i * ancho / (valores.length - 1);
        const y = alto - (valores[i] - valorMenor) / rango * (alto - 8) - 4;

        if (i === 0) {
            lapiz.moveTo(x, y);
        } else {
            lapiz.lineTo(x, y);
        }
    }

    lapiz.strokeStyle = color;
    lapiz.lineWidth = 2;
    lapiz.stroke();
}

// Consulta al ESP32 y actualiza los datos que aparecen en pantalla.
async function actualizarPanel() {
    if (consultaEnCurso) {
        return;
    }

    consultaEnCurso = true;

    try {
        const respuesta = await fetch("/alarm/status", { cache: "no-store" });

        if (!respuesta.ok) {
            throw new Error("No se pudo consultar el ESP32");
        }

        const datos = await respuesta.json();
        buscar("connection-status").dataset.state = "connected";
        buscar("connection-label").textContent = "ESP32 conectado";

        const textoAlarma = datos.state === "disparada"
            ? "DISPARADA"
            : datos.state.toUpperCase();

        mostrarEstado("alarm-status", textoAlarma);
        buscar("alarm-control-card").dataset.state = datos.state;
        mostrarEstado("led-status", datos.led.toUpperCase());
        mostrarEstado("motor-status", datos.motor.toUpperCase());

        if (datos.sensor_available) {
            actualizarLecturas(datos);
        } else {
            buscar("sensor-status").textContent = "ESPERANDO LECTURA";
            buscar("sensor-card").dataset.state = "unknown";
        }
    } catch (error) {
        buscar("connection-status").dataset.state = "disconnected";
        buscar("connection-label").textContent = "Sin conexión";
        mostrarEstado("alarm-status", "SIN CONEXIÓN");
        mostrarEstado("led-status", "SIN CONEXIÓN");
        mostrarEstado("motor-status", "SIN CONEXIÓN");
        buscar("sensor-status").textContent = "SIN CONEXIÓN";
        buscar("sensor-card").dataset.state = "unknown";
    }

    consultaEnCurso = false;
}

// Actualiza los números y agrega los valores nuevos a los gráficos.
function actualizarLecturas(datos) {
    const temperatura = datos.temperature_tenths / 10;
    const humedad = datos.humidity_tenths / 10;

    buscar("sensor-temperature").innerHTML = temperatura.toFixed(1) + " <small>°C</small>";
    buscar("sensor-humidity").innerHTML = humedad.toFixed(1) + " <small>%</small>";
    buscar("sensor-status").textContent = "CONECTADO";
    buscar("sensor-card").dataset.state = "available";

    temperaturas.push(temperatura);
    humedades.push(humedad);

    if (temperaturas.length > 30) temperaturas.shift();
    if (humedades.length > 30) humedades.shift();

    dibujarGrafico("temperature-chart", temperaturas, "#64dfb0");
    dibujarGrafico("humidity-chart", humedades, "#7dc8fa");
}

// Abre y cierra el menú para celulares.
buscar("menu-boton").addEventListener("click", function () {
    const menu = buscar("menu");
    const estaAbierto = menu.classList.toggle("abierto");
    buscar("menu-boton").setAttribute("aria-expanded", estaAbierto);
});

// Cierra el menú después de elegir una sección.
const enlacesMenu = document.querySelectorAll(".menu a");
for (const enlace of enlacesMenu) {
    enlace.addEventListener("click", function () {
        buscar("menu").classList.remove("abierto");
        buscar("menu-boton").setAttribute("aria-expanded", "false");
    });
}

// Muestra el número elegido en el control de sensibilidad.
buscar("sensibilidad").addEventListener("input", function (evento) {
    buscar("sensibilidad-valor").value = evento.target.value + "%";
});

// Envía los botones de encendido y apagado al ESP32.
const formularios = document.querySelectorAll(".botones form");
for (const formulario of formularios) {
    formulario.addEventListener("submit", async function (evento) {
        evento.preventDefault();

        const boton = formulario.querySelector("button");
        const textoOriginal = boton.textContent;
        boton.disabled = true;
        boton.textContent = "Enviando…";

        try {
            const respuesta = await fetch(formulario.action, {
                method: formulario.method
            });

            if (!respuesta.ok) {
                throw new Error("No se pudo enviar el comando");
            }

            await actualizarPanel();
            boton.textContent = "Listo";
        } catch (error) {
            boton.textContent = "Error al enviar";
        }

        window.setTimeout(function () {
            boton.textContent = textoOriginal;
            boton.disabled = false;
        }, 1000);
    });
}

// Consulta los estados al cargar la página y después cada segundo.
actualizarPanel();
window.setInterval(actualizarPanel, 1000);

// Redibuja los gráficos si cambia el tamaño de la ventana.
window.addEventListener("resize", function () {
    dibujarGrafico("temperature-chart", temperaturas, "#64dfb0");
    dibujarGrafico("humidity-chart", humedades, "#7dc8fa");
});
