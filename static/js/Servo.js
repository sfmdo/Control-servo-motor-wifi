document.addEventListener('DOMContentLoaded', function() {
    // --- CONFIGURACIÓN ---
    // !!! Reemplaza esta IP con la de tu ESP32 !!!
    const ESP32_IP = 'http://192.168.0.183'; 
    const STATUS_ENDPOINT = `${ESP32_IP}/status`;
    const CONTROL_ENDPOINT = `${ESP32_IP}/control`;
    const STATUS_POLL_INTERVAL = 2000; // Consultar estado cada 2 segundos

    // --- ELEMENTOS DEL DOM ---
    const wifiStatusEl = document.getElementById('wifi-status');
    const currentAngleEl = document.getElementById('current-angle');
    const currentModeEl = document.getElementById('current-mode');
    
    const angleSlider = document.getElementById('angle-slider');
    const sliderValueEl = document.getElementById('slider-value');
    const arrowIndicator = document.getElementById('arrow-indicator');
    const sendAngleBtn = document.getElementById('send-angle-btn');

    const sequenceInput = document.getElementById('sequence-input');
    const sendSequenceBtn = document.getElementById('send-sequence-btn');
    
    // Botones de modo especial
    const modeButtons = document.querySelectorAll('.btn[data-mode]');

    // --- LÓGICA ---

    // 1. Actualizar el estado periódicamente
    const fetchStatus = async () => {
        try {
            const response = await fetch(STATUS_ENDPOINT);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            const data = await response.json();
            
            // Actualizar la UI
            wifiStatusEl.textContent = data.wifiStatus || 'Conectado';
            wifiStatusEl.classList.add('status-ok');
            wifiStatusEl.classList.remove('status-error');

            currentAngleEl.innerHTML = `${data.angle} &deg;`;
            currentModeEl.textContent = data.mode;

        } catch (error) {
            console.error("Error al obtener estado:", error);
            wifiStatusEl.textContent = 'Desconectado';
            wifiStatusEl.classList.add('status-error');
            wifiStatusEl.classList.remove('status-ok');
            currentAngleEl.innerHTML = '-- &deg;';
            currentModeEl.textContent = '--';
        }
    };

    // 2. Enviar comandos de control al ESP32
    const sendCommand = async (params) => {
        try {
            const url = new URL(CONTROL_ENDPOINT);
            url.search = new URLSearchParams(params).toString();
            
            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            console.log(`Comando enviado: ${url.search}`);
            // Refrescar estado inmediatamente después de enviar un comando
            setTimeout(fetchStatus, 250);
        } catch (error) {
            console.error("Error al enviar comando:", error);
        }
    };

    //Lógica del slider de ángulo manual
    angleSlider.addEventListener('input', () => {
        const angle = angleSlider.value;
        sliderValueEl.textContent = angle;
        arrowIndicator.style.transform = `rotate(${angle - 90}deg)`;
    });

    sendAngleBtn.addEventListener('click', () => {
        const angle = angleSlider.value;
        sendCommand({ mode: 'manual', angle: angle });
    });

    //Lógica de los botones de modo
    modeButtons.forEach(button => {
        button.addEventListener('click', () => {
            const mode = button.getAttribute('data-mode');
            sendCommand({ mode: mode });
        });
    });

    //Lógica para enviar secuencia
    sendSequenceBtn.addEventListener('click', () => {
        const angles = sequenceInput.value.trim();
        if (angles) {
            // Validación simple para asegurar que son números y comas
            if (/^[\d\s,]+$/.test(angles)) {
                sendCommand({ mode: 'sequence', angles: angles });
            } else {
                alert("Formato de secuencia inválido. Usa números separados por comas. Ej: 0, 90, 180");
            }
        }
    });

    // Iniciar la rotación inicial de la flecha
    arrowIndicator.style.transform = `rotate(0deg)`;
    // Iniciar la consulta periódica del estado
    setInterval(fetchStatus, STATUS_POLL_INTERVAL);
    // Cargar el estado inicial al cargar la página
    fetchStatus();
});