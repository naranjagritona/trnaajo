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
        }
    });
});