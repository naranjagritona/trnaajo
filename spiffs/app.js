document.querySelectorAll(".controls form").forEach((form) => {
    form.addEventListener("submit", async (event) => {
        event.preventDefault();

        const button = form.querySelector('button[type="submit"]');
        const status = form.closest(".device-card")?.querySelector(".device-status span:last-child");
        if (!button || !status) {
            return;
        }

        const originalLabel = button.textContent;
        button.disabled = true;
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
            button.disabled = false;
            button.textContent = originalLabel;
        }
    });
});