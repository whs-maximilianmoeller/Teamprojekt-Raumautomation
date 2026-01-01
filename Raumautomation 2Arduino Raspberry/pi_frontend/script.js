// Konfiguration - Zeigt auf deine Raspberry Pi IP (falls nicht lokal)
const API_BASE = 'http://localhost:5001';

// --- Gauge Diagramme (Tacho) ---
const gaugeOptions = {
    rotation: -90,
    circumference: 180,
    cutout: '80%',
    plugins: {
        legend: { display: false },
        tooltip: { enabled: false }
    }
};

let tempGauge = new Chart(document.getElementById('tempGauge'), {
    type: 'doughnut',
    data: {
        labels: ['Temp', 'Max'],
        datasets: [{
            data: [0, 50],
            backgroundColor: ['#fd7e14', '#35393f'],
            borderWidth: 0
        }]
    },
    options: gaugeOptions
});

let humGauge = new Chart(document.getElementById('humGauge'), {
    type: 'doughnut',
    data: {
        labels: ['Hum', 'Max'],
        datasets: [{
            data: [0, 100],
            backgroundColor: ['#0dcaf0', '#35393f'],
            borderWidth: 0
        }]
    },
    options: gaugeOptions
});

// --- Verlaufs-Diagramm ---
let historyChart = new Chart(document.getElementById('historyChart'), {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Temperatur (°C)',
                data: [],
                borderColor: '#fd7e14',
                tension: 0.4,
                yAxisID: 'y'
            },
            {
                label: 'Feuchte (%)',
                data: [],
                borderColor: '#0dcaf0',
                tension: 0.4,
                yAxisID: 'y1'
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { labels: { color: '#ccc' } }
        },
        scales: {
            x: { ticks: { color: '#ccc' }, grid: { color: '#444' } },
            y: {
                type: 'linear', display: true, position: 'left',
                ticks: { color: '#fd7e14' }, grid: { color: '#444' }
            },
            y1: {
                type: 'linear', display: true, position: 'right',
                ticks: { color: '#0dcaf0' }, grid: { drawOnChartArea: false }
            }
        }
    }
});

// --- Logik ---
const statusBadge = document.getElementById('connection-status');
const targetTempSlider = document.getElementById('targetTempSlider');
const targetTempLabel = document.getElementById('targetTempLabel');
const targetHumSlider = document.getElementById('targetHumSlider');
const targetHumLabel = document.getElementById('targetHumLabel');

const modeTempBtn = document.getElementById('modeTemp');
const modeHumBtn = document.getElementById('modeHum');
const modeAutoBtn = document.getElementById('modeAuto');

const tempGroup = document.getElementById('tempControlGroup');
const humGroup = document.getElementById('humControlGroup');
const autoGroup = document.getElementById('autoControlGroup');

// State
let currentMode = 'TEMP';

function updateUIForMode(mode) {
    currentMode = mode;
    // Aktualisiere Buttons
    if (modeTempBtn) modeTempBtn.checked = (mode === 'TEMP');
    if (modeHumBtn) modeHumBtn.checked = (mode === 'HUM');
    if (modeAutoBtn) modeAutoBtn.checked = (mode === 'AUTO');

    // Aktualisiere Sichtbarkeit der Steuerungen
    if (mode === 'TEMP') {
        if (tempGroup) tempGroup.style.display = 'block';
        if (humGroup) humGroup.style.display = 'none';
        if (autoGroup) autoGroup.style.display = 'none';
    } else if (mode === 'HUM') {
        if (tempGroup) tempGroup.style.display = 'none';
        if (humGroup) humGroup.style.display = 'block';
        if (autoGroup) autoGroup.style.display = 'none';
    } else if (mode === 'AUTO') {
        if (tempGroup) tempGroup.style.display = 'none';
        if (humGroup) humGroup.style.display = 'none';
        if (autoGroup) autoGroup.style.display = 'block';
    }
}

async function updateLive() {
    try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 1000); // 1 Sekunde Timeout

        const response = await fetch(`${API_BASE}/api/live`, { signal: controller.signal });
        clearTimeout(timeoutId);

        if (!response.ok) throw new Error('Network response was not ok');

        const data = await response.json();

        // Status Aktualisierung
        statusBadge.className = 'badge bg-success';
        statusBadge.textContent = 'Online';

        // Aktualisiere Gauges
        const t = data.temperature || 0;
        document.getElementById('tempValue').textContent = `${t.toFixed(1)} °C`;
        tempGauge.data.datasets[0].data = [t, 50 - t];
        tempGauge.update();

        const h = data.humidity || 0;
        document.getElementById('humValue').textContent = `${h.toFixed(1)} %`;
        humGauge.data.datasets[0].data = [h, 100 - h];
        humGauge.update();

        // Aktualisiere Lüfter
        const f = data.fan_speed || 0;
        const fPercent = Math.round((f / 255) * 100);
        document.getElementById('fanSpeedValue').textContent = `${fPercent} %`;
        document.getElementById('fanProgressBar').style.width = `${fPercent}%`;

        // Aktualisiere Steuerungslogik (Sync vom Server wenn nicht gerade gezogen wird?)
        if (data.control_mode && data.control_mode !== currentMode) {
            updateUIForMode(data.control_mode);
        }

        // Aktualisiere Auto-Anzeigewerte
        const tTemp = data.target_temperature || 22.0;
        const tHum = data.target_humidity || 50.0;
        if (document.getElementById('autoTempDisplay')) document.getElementById('autoTempDisplay').textContent = `${tTemp.toFixed(1)} °C`;
        if (document.getElementById('autoHumDisplay')) document.getElementById('autoHumDisplay').textContent = `${tHum.toFixed(1)} %`;

    } catch (error) {
        console.warn('Fetch error (Entering Demo Mode):', error);
        statusBadge.className = 'badge bg-warning text-dark';
        statusBadge.textContent = 'Demo Mode (Backend Unavailable)';

        // Mock Daten (Demo Modus)
        const t = 22.5 + Math.random() * 2 - 1;
        const h = 45.0 + Math.random() * 5 - 2.5;
        const f = 120 + Math.random() * 20 - 10;

        document.getElementById('tempValue').textContent = `${t.toFixed(1)} °C`;
        tempGauge.data.datasets[0].data = [t, 50 - t];
        tempGauge.update();

        document.getElementById('humValue').textContent = `${h.toFixed(1)} %`;
        humGauge.data.datasets[0].data = [h, 100 - h];
        humGauge.update();

        const fPercent = Math.round((f / 255) * 100);
        document.getElementById('fanSpeedValue').textContent = `${fPercent} %`;
        document.getElementById('fanProgressBar').style.width = `${fPercent}%`;

        // Mock Auto-Anzeigewerte
        if (document.getElementById('autoTempDisplay')) document.getElementById('autoTempDisplay').textContent = `22.0 °C`;
        if (document.getElementById('autoHumDisplay')) document.getElementById('autoHumDisplay').textContent = `50.0 %`;
    }
}

async function updateHistory() {
    try {
        const response = await fetch(`${API_BASE}/api/history?limit=20`);
        const data = await response.json();

        const labels = data.map(d => new Date(d.timestamp).toLocaleTimeString());
        const temps = data.map(d => d.temperature);
        const hums = data.map(d => d.humidity);

        historyChart.data.labels = labels;
        historyChart.data.datasets[0].data = temps;
        historyChart.data.datasets[1].data = hums;
        historyChart.update();

    } catch (e) {
        // Mock Verlauf
        if (historyChart.data.datasets[0].data.length === 0) {
            const labels = [];
            const temps = [];
            const hums = [];
            let now = new Date();
            for (let i = 0; i < 20; i++) {
                now = new Date(now.getTime() - 30000);
                labels.unshift(now.toLocaleTimeString());
                temps.unshift(20 + Math.random() * 5);
                hums.unshift(40 + Math.random() * 10);
            }
            historyChart.data.labels = labels;
            historyChart.data.datasets[0].data = temps;
            historyChart.data.datasets[1].data = hums;
            historyChart.update();
        }
    }
}

// --- Interaktion ---
function sendSettings(payload) {
    fetch(`${API_BASE}/api/settings`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    }).then(res => console.log("Sent Settings:", payload)).catch(e => console.log("Demo Mode: Setting ignored"));
}

// Schieberegler
let debounceTimer;
if (targetTempSlider) {
    targetTempSlider.addEventListener('input', (e) => {
        if (targetTempLabel) targetTempLabel.innerText = e.target.value;
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => { sendSettings({ target_temp: e.target.value }); }, 500);
    });
}

if (targetHumSlider) {
    targetHumSlider.addEventListener('input', (e) => {
        if (targetHumLabel) targetHumLabel.innerText = e.target.value;
        clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => { sendSettings({ target_hum: e.target.value }); }, 500);
    });
}

// Modi
if (modeTempBtn) modeTempBtn.addEventListener('change', () => { updateUIForMode('TEMP'); sendSettings({ control_mode: 'TEMP' }); });
if (modeHumBtn) modeHumBtn.addEventListener('change', () => { updateUIForMode('HUM'); sendSettings({ control_mode: 'HUM' }); });
if (modeAutoBtn) modeAutoBtn.addEventListener('change', () => { updateUIForMode('AUTO'); sendSettings({ control_mode: 'AUTO' }); });

// --- Initialisierung ---
updateUIForMode('TEMP'); // Standard
setInterval(updateLive, 2000);
setInterval(updateHistory, 10000);
updateLive();
updateHistory();
