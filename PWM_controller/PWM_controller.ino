/*
  ESP32 PWM Generator - Web UI + WebSocket
  Compatible with ESP32 DevKit (classic ESP32) and ESP32-S3.

  Arduino IDE requirements:
    - ESP32 by Espressif Systems (Arduino core 2.x or 3.x)
    - WebSockets by Markus Sattler (Links2004/arduinoWebSockets)
    - ArduinoJson by Benoit Blanchon

  Network:
    SSID: ESP32-PWM
    Password: change-me-123
    URL: http://192.168.4.1
    WebSocket: ws://192.168.4.1:81

  PWM:
    Frequency: 5 Hz to 50 kHz
    Duty cycle: 0 to 100%
    GPIO options: 4, 18, 21 (GPIO 18 is the default)

  All control commands and state synchronization use WebSocket.
  HTTP is used only to deliver the initial web page.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <esp_arduino_version.h>
#include <math.h>

// -----------------------------------------------------------------------------
// User configuration
// -----------------------------------------------------------------------------

const char* AP_SSID = "ESP32-PWM";
const char* AP_PASSWORD = "change-me-123"; // At least 8 characters

static const uint32_t PWM_MIN_FREQUENCY = 5;
static const uint32_t PWM_MAX_FREQUENCY = 50000;
static const uint8_t PWM_MAX_RESOLUTION = 14; // Shared limit for ESP32 and ESP32-S3
static const uint32_t PWM_CLOCK_HZ = 80000000UL; // APB clock
static const uint8_t PWM_CHANNEL = 0; // Used only by Arduino-ESP32 2.x

// Only GPIOs intended to be common and generally usable on both boards are offered.
// Check your particular development board before connecting external circuitry.
static const uint8_t ALLOWED_PINS[] = {4, 18, 21};
static const size_t ALLOWED_PIN_COUNT = sizeof(ALLOWED_PINS) / sizeof(ALLOWED_PINS[0]);

// -----------------------------------------------------------------------------
// Global objects and state
// -----------------------------------------------------------------------------

WebServer httpServer(80);
WebSocketsServer webSocket(81);

struct PwmState {
  uint32_t frequency = 1000;
  float duty = 50.0f;
  bool enabled = false; // Start safely with the output disabled
  uint8_t pin = 18;
  uint8_t resolution = 11;
};

PwmState pwmState;
bool pwmAttached = false;

// -----------------------------------------------------------------------------
// Web interface
// -----------------------------------------------------------------------------

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#0b1020">
  <title>Pulse Lab | Gerador PWM</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #0b1020;
      --panel: rgba(20, 28, 49, .88);
      --panel-2: #151e34;
      --line: rgba(168, 190, 230, .13);
      --text: #f3f6ff;
      --muted: #8996b3;
      --cyan: #55d6e8;
      --violet: #9b8cff;
      --green: #54e0a1;
      --orange: #ffbd69;
      --danger: #ff7185;
      --radius: 24px;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0; min-height: 100vh; color: var(--text);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background:
        radial-gradient(ellipse at 12% 0%, rgba(79, 112, 220, .20), transparent 38%),
        radial-gradient(ellipse at 90% 18%, rgba(50, 197, 207, .12), transparent 32%), var(--bg);
    }
    button, select { font: inherit; }
    .shell { width: min(1120px, calc(100% - 36px)); margin: 0 auto; padding: 28px 0 42px; }
    .topbar { display:flex; align-items:center; justify-content:space-between; gap:16px; margin-bottom:30px; }
    .brand { display:flex; align-items:center; gap:13px; }
    .brand-mark {
      width:44px; height:44px; display:grid; place-items:center; border-radius:14px;
      background:linear-gradient(145deg, rgba(85,214,232,.22), rgba(155,140,255,.20));
      border:1px solid rgba(121,204,235,.27); box-shadow:0 8px 30px rgba(62,155,210,.12);
    }
    .brand-mark svg { width:25px; height:25px; }
    .brand-name { font-size:15px; letter-spacing:.17em; font-weight:800; }
    .brand-sub { color:var(--muted); font-size:11px; letter-spacing:.12em; margin-top:3px; }
    .connection {
      display:flex; align-items:center; gap:9px; border:1px solid var(--line); border-radius:999px;
      padding:9px 13px; color:var(--muted); font-size:12px; background:rgba(18,26,45,.72);
    }
    .led { width:8px; height:8px; border-radius:50%; background:var(--orange); box-shadow:0 0 12px currentColor; }
    .connection.online { color:#b8f7dc; border-color:rgba(84,224,161,.25); }
    .connection.online .led { background:var(--green); }
    .heading { margin:0 0 7px; font-size:clamp(25px,4vw,36px); letter-spacing:-.04em; }
    .intro { margin:0; color:var(--muted); font-size:14px; line-height:1.6; }
    .workspace { display:grid; grid-template-columns:1fr 1fr; gap:18px; margin-top:26px; }
    .panel {
      min-width:0; border:1px solid var(--line); border-radius:var(--radius);
      background:linear-gradient(145deg, rgba(23,33,57,.94), rgba(15,22,40,.94));
      box-shadow:0 18px 55px rgba(0,0,0,.19); padding:22px;
    }
    .panel-head { display:flex; justify-content:space-between; align-items:center; gap:12px; }
    .eyebrow { color:var(--muted); font-size:11px; font-weight:750; letter-spacing:.16em; text-transform:uppercase; }
    .unit-pill { color:#c8d1e8; font-size:11px; padding:6px 9px; border-radius:999px; background:rgba(255,255,255,.055); border:1px solid var(--line); }
    .dial-wrap { display:flex; justify-content:center; padding:18px 0 8px; }
    .dial {
      width:min(100%, 292px); aspect-ratio:1; position:relative; display:grid; place-items:center;
      border-radius:50%; touch-action:none; user-select:none; cursor:grab; outline:none;
    }
    .dial:active { cursor:grabbing; }
    .dial:focus-visible { filter:drop-shadow(0 0 10px rgba(85,214,232,.4)); }
    .dial svg { position:absolute; inset:0; width:100%; height:100%; overflow:visible; }
    .dial-track { fill:none; stroke:rgba(171,190,224,.13); stroke-width:8; stroke-dasharray:424 566; transform:rotate(-135deg); transform-origin:50% 50%; }
    .dial-progress { fill:none; stroke:var(--dial-color); stroke-width:8; stroke-linecap:round; stroke-dasharray:424 566; stroke-dashoffset:424; transform:rotate(-135deg); transform-origin:50% 50%; filter:drop-shadow(0 0 6px var(--dial-color)); }
    .dial-ticks { fill:none; stroke:rgba(176,192,223,.35); stroke-width:2; stroke-linecap:round; }
    .dial-core {
      position:relative; width:70%; aspect-ratio:1; border-radius:50%; display:flex; flex-direction:column;
      align-items:center; justify-content:center; text-align:center;
      background:radial-gradient(circle at 35% 25%, #273453, #141d33 69%);
      border:1px solid rgba(172,194,235,.17); box-shadow:inset 0 2px 14px rgba(255,255,255,.035), 0 18px 35px rgba(0,0,0,.28);
    }
    .needle {
      position:absolute; width:2px; height:38%; left:calc(50% - 1px); top:12%; border-radius:5px;
      transform-origin:50% 100%; transform:rotate(-135deg); background:var(--dial-color);
      box-shadow:0 0 12px var(--dial-color); pointer-events:none;
    }
    .needle::after {
      content:""; position:absolute; width:9px; height:9px; border-radius:50%; left:50%; top:-4px;
      transform:translateX(-50%); background:#fff; box-shadow:0 0 9px var(--dial-color);
    }
    .dial-value { font-size:clamp(29px,4vw,39px); font-weight:760; letter-spacing:-.055em; font-variant-numeric:tabular-nums; line-height:1.05; }
    .dial-unit { color:var(--muted); font-size:12px; margin-top:7px; letter-spacing:.06em; }
    .dial-hint { color:#697794; font-size:10px; letter-spacing:.12em; margin-top:17px; text-transform:uppercase; }
    .freq-card { --dial-color:var(--cyan); }
    .duty-card { --dial-color:var(--violet); }
    .range-labels { display:flex; justify-content:space-between; padding:0 14px; color:#74819d; font-size:11px; }
    .quick-row { display:flex; flex-wrap:wrap; justify-content:center; gap:8px; margin-top:15px; }
    .chip {
      padding:7px 11px; color:#b8c5e2; background:rgba(255,255,255,.045); border:1px solid var(--line);
      border-radius:999px; font-size:11px; cursor:pointer; transition:.18s ease;
    }
    .chip:hover { border-color:rgba(85,214,232,.45); color:#fff; transform:translateY(-1px); }
    .lower-grid { display:grid; grid-template-columns:1.15fr .85fr; gap:18px; margin-top:18px; }
    .control-panel { display:flex; flex-direction:column; gap:18px; }
    .output-row { display:flex; justify-content:space-between; align-items:center; gap:16px; }
    .output-title { font-size:14px; font-weight:700; }
    .output-caption { color:var(--muted); font-size:12px; margin-top:4px; }
    .toggle {
      position:relative; width:58px; height:32px; flex:0 0 auto; border:1px solid rgba(255,255,255,.12);
      border-radius:999px; background:#28334c; cursor:pointer; transition:.2s;
    }
    .toggle::after { content:""; position:absolute; top:4px; left:4px; width:22px; height:22px; border-radius:50%; background:#aab5cb; transition:.2s; }
    .toggle.on { background:rgba(84,224,161,.2); border-color:rgba(84,224,161,.55); }
    .toggle.on::after { left:30px; background:var(--green); box-shadow:0 0 12px rgba(84,224,161,.45); }
    .settings-row { display:grid; grid-template-columns:1fr 1fr; gap:12px; }
    .field label { display:block; color:var(--muted); font-size:11px; margin-bottom:8px; }
    select {
      width:100%; color:var(--text); background:#10182b; border:1px solid var(--line); border-radius:12px;
      padding:11px 12px; outline:none;
    }
    select:focus { border-color:rgba(85,214,232,.55); }
    .state-pill { display:inline-flex; align-items:center; gap:7px; font-size:11px; color:#aebbd7; }
    .state-pill .mini-dot { width:7px; height:7px; border-radius:50%; background:var(--danger); }
    .state-pill.active .mini-dot { background:var(--green); box-shadow:0 0 9px var(--green); }
    .metrics { display:grid; grid-template-columns:repeat(3,1fr); gap:10px; }
    .metric { padding:13px 12px; border:1px solid var(--line); border-radius:15px; background:rgba(7,13,27,.25); min-width:0; }
    .metric-label { color:var(--muted); font-size:10px; letter-spacing:.1em; text-transform:uppercase; }
    .metric-value { font-size:clamp(14px,2vw,19px); font-weight:700; margin-top:8px; font-variant-numeric:tabular-nums; white-space:nowrap; }
    .wave-panel { display:flex; flex-direction:column; justify-content:space-between; gap:14px; }
    .wave-box { min-height:126px; border:1px solid var(--line); border-radius:15px; background:rgba(5,11,24,.35); padding:13px; display:flex; align-items:center; }
    .wave-box svg { width:100%; height:auto; overflow:visible; }
    .wave-grid { stroke:rgba(151,171,208,.12); stroke-width:1; stroke-dasharray:3 5; }
    .wave-path { fill:none; stroke:var(--green); stroke-width:3; stroke-linejoin:round; stroke-linecap:round; filter:drop-shadow(0 0 5px rgba(84,224,161,.45)); }
    .footer { display:flex; justify-content:space-between; gap:12px; margin-top:20px; color:#66738f; font-size:11px; }
    .toast { position:fixed; left:50%; bottom:22px; transform:translate(-50%,20px); opacity:0; pointer-events:none; padding:11px 15px; border:1px solid var(--line); border-radius:12px; background:#1b2741; color:#fff; font-size:12px; box-shadow:0 12px 35px rgba(0,0,0,.35); transition:.2s; z-index:10; }
    .toast.show { opacity:1; transform:translate(-50%,0); }
    @media (max-width:760px) {
      .shell { width:min(100% - 24px, 560px); padding-top:18px; }
      .topbar { margin-bottom:23px; }
      .workspace { grid-template-columns:1fr; gap:13px; margin-top:20px; }
      .panel { padding:18px; border-radius:20px; }
      .dial { width:min(100%, 270px); }
      .lower-grid { grid-template-columns:1fr; gap:13px; margin-top:13px; }
      .footer { flex-direction:column; }
    }
    @media (max-width:390px) {
      .brand-name { font-size:13px; }
      .connection { padding:8px 10px; font-size:11px; }
      .metrics { gap:7px; }
      .metric { padding:11px 9px; }
      .metric-value { font-size:14px; }
    }
  </style>
</head>
<body>
    <div class="shell">
        <header class="topbar">
            <div class="brand">
                <div class="brand-mark" aria-hidden="true">
                    <svg viewBox="0 0 24 24" fill="none">
                        <path d="M2 13h4V6h5v12h5V9h6" stroke="#72e1ed" stroke-width="2" stroke-linecap="round"
                            stroke-linejoin="round" />
                        <circle cx="21" cy="9" r="1.5" fill="#9b8cff" />
                    </svg>
                </div>
                <div>
                    <div class="brand-name">PULSE LAB</div>
                    <div class="brand-sub">ESP32 · PWM GENERATOR</div>
                </div>
            </div>
            <div id="connection" class="connection"><span class="led"></span><span
                    id="connectionText">Conectando...</span>
            </div>
        </header>

        <section>
            <h1 class="heading">Controle do sinal</h1>
            <p class="intro">Ajuste frequência e duty cycle em tempo real. Todos os dispositivos conectados compartilham
                o
                mesmo estado.</p>
        </section>

        <main>
            <div class="workspace">
                <section class="panel freq-card">
                    <div class="panel-head"><span class="eyebrow">Frequência</span><span class="unit-pill">5 Hz — 50
                            kHz</span>
                    </div>
                    <div class="dial-wrap">
                        <div id="frequencyDial" class="dial" role="slider" tabindex="0" aria-label="Frequência"
                            aria-valuemin="5" aria-valuemax="50000">
                            <svg viewBox="0 0 220 220" aria-hidden="true">
                                <circle class="dial-track" cx="110" cy="110" r="90" />
                                <circle id="frequencyProgress" class="dial-progress" cx="110" cy="110" r="90" />
                                <g class="dial-ticks">
                                    <path d="M39 181l7-7M181 181l-7-7M110 20v10" />
                                </g>
                            </svg>
                            <div class="needle" id="frequencyNeedle"></div>
                            <div class="dial-core">
                                <div class="dial-value" id="frequencyValue">1.00</div>
                                <div class="dial-unit" id="frequencyUnit">kHz</div>
                                <div class="dial-hint">arraste para ajustar</div>
                            </div>
                        </div>
                    </div>
                    <div class="range-labels"><span>5 Hz</span><span>50 kHz</span></div>
                    <div class="quick-row">
                        <button class="chip" data-frequency="50">50 Hz</button>
                        <button class="chip" data-frequency="1000">1 kHz</button>
                        <button class="chip" data-frequency="10000">10 kHz</button>
                        <button class="chip" data-frequency="20000">20 kHz</button>
                    </div>
                </section>

                <section class="panel duty-card">
                    <div class="panel-head"><span class="eyebrow">Duty cycle</span><span class="unit-pill">0 —
                            100%</span></div>
                    <div class="dial-wrap">
                        <div id="dutyDial" class="dial" role="slider" tabindex="0" aria-label="Duty cycle"
                            aria-valuemin="0" aria-valuemax="100">
                            <svg viewBox="0 0 220 220" aria-hidden="true">
                                <circle class="dial-track" cx="110" cy="110" r="90" />
                                <circle id="dutyProgress" class="dial-progress" cx="110" cy="110" r="90" />
                                <g class="dial-ticks">
                                    <path d="M39 181l7-7M181 181l-7-7M110 20v10" />
                                </g>
                            </svg>
                            <div class="needle" id="dutyNeedle"></div>
                            <div class="dial-core">
                                <div class="dial-value" id="dutyValue">50.0</div>
                                <div class="dial-unit">por cento</div>
                                <div class="dial-hint">arraste para ajustar</div>
                            </div>
                        </div>
                    </div>
                    <div class="range-labels"><span>0%</span><span>100%</span></div>
                    <div class="quick-row">
                        <button class="chip" data-duty="0">0%</button>
                        <button class="chip" data-duty="25">25%</button>
                        <button class="chip" data-duty="50">50%</button>
                        <button class="chip" data-duty="75">75%</button>
                        <button class="chip" data-duty="100">100%</button>
                    </div>
                </section>
            </div>
            <div class="lower-grid">
                <section class="panel control-panel">
                    <div class="output-row">
                        <div>
                            <div class="output-title">Saída PWM</div>
                            <div class="output-caption" id="outputCaption">Desligada · nível baixo</div>
                        </div>
                        <button id="outputToggle" class="toggle" type="button" role="switch" aria-checked="false"
                            aria-label="Ligar ou desligar a saída"></button>
                    </div>
                    <div class="settings-row">
                        <div class="field">
                            <label for="pinSelect">GPIO de saída</label>
                            <select id="pinSelect">
                                <option value="4">Ventoinha 3 (GPIO 4)</option>
                                <option value="18" selected>Ventoinha 1 (GPIO 18)</option>
                                <option value="21">Ventoinha 2 (GPIO 21)</option>
                            </select></div>
                        <div class="field"><label>Estado do gerador</label>
                            <div id="generatorState" class="state-pill"><span
                                    class="mini-dot"></span><span>Desativado</span></div>
                        </div>
                    </div>
                    <div class="metrics">
                        <div class="metric">
                            <div class="metric-label">Período</div>
                            <div class="metric-value" id="periodValue">1.00 ms</div>
                        </div>
                        <div class="metric">
                            <div class="metric-label">Tempo ligado</div>
                            <div class="metric-value" id="highValue">500 µs</div>
                        </div>
                        <div class="metric">
                            <div class="metric-label">Tempo desligado</div>
                            <div class="metric-value" id="lowValue">500 µs</div>
                        </div>
                    </div>
                </section>

                <section class="panel wave-panel">
                    <div class="panel-head">
                        <span class="eyebrow">Prévia do sinal</span>
                        <span class="unit-pill" id="waveLabel">50% · 1 kHz</span>
                    </div>
                    <div class="wave-box">
                        <svg viewBox="0 0 320 100" role="img" aria-label="Representação ilustrativa do sinal PWM">
                            <path class="wave-grid"
                                d="M0 25H320M0 50H320M0 75H320M40 0V100M120 0V100M200 0V100M280 0V100" />
                            <path id="wavePath" class="wave-path" d="M0 76V24H120V76H160V24H280V76H320" />
                        </svg>
                    </div>
                    <div class="output-caption">Representação visual; use o osciloscópio para conferir o sinal elétrico
                        real.
                    </div>
                </section>
            </div>
        </main>
        <footer class="footer"><span>Pulse Lab · Controle sincronizado via WebSocket</span><span id="pinInfo">Saída:
                GPIO 18
                · lógica 3,3 V</span></footer>
    </div>
    <div id="toast" class="toast"></div>

    <script>
        const MIN_FREQ = 5;
        const MAX_FREQ = 50000;
        const state = { frequency: 1000, duty: 50, enabled: false, pin: 18 };
        let socket = null;
        let reconnectTimer = null;
        let toastTimer = null;
        let commandTimer = null;

        const $ = (id) => document.getElementById(id);

        function showToast(message) {
            $('toast').textContent = message;
            $('toast').classList.add('show');
            clearTimeout(toastTimer);
            toastTimer = setTimeout(() => $('toast').classList.remove('show'), 2300);
        }

        function setConnection(online, message) {
            $('connection').classList.toggle('online', online);
            $('connectionText').textContent = message;
        }

        function sendCommand(partial) {
            if (!socket || socket.readyState !== WebSocket.OPEN) {
                showToast('Sem conexão com o ESP32.');
                return;
            }
            socket.send(JSON.stringify({ type: 'set', ...partial }));
        }

        // Coalesce rapid dial movements, while keeping the interface visually immediate.
        function queueCommand(partial) {
            clearTimeout(commandTimer);
            commandTimer = setTimeout(() => sendCommand(partial), 45);
        }

        function frequencyToProgress(freq) {
            return Math.log(freq / MIN_FREQ) / Math.log(MAX_FREQ / MIN_FREQ);
        }
        function progressToFrequency(progress) {
            const raw = MIN_FREQ * Math.pow(MAX_FREQ / MIN_FREQ, progress);
            return Math.max(MIN_FREQ, Math.min(MAX_FREQ, Math.round(raw)));
        }
        function formatFrequency(freq) {
            if (freq >= 1000) {
                const khz = freq / 1000;
                return { value: khz >= 10 ? khz.toFixed(2) : khz.toFixed(2), unit: 'kHz' };
            }
            return { value: String(Math.round(freq)), unit: 'Hz' };
        }
        function formatTime(seconds) {
            if (seconds === 0) return '0';
            if (seconds >= 1) return seconds.toFixed(3) + ' s';
            if (seconds >= 0.001) return (seconds * 1000).toFixed(seconds < 0.01 ? 3 : 2) + ' ms';
            return (seconds * 1000000).toFixed(seconds < 0.00001 ? 1 : 0) + ' µs';
        }

        function updateDial(kind, progress) {
            progress = Math.max(0, Math.min(1, progress));
            const isFrequency = kind === 'frequency';
            const dial = $(isFrequency ? 'frequencyDial' : 'dutyDial');
            const progressCircle = $(isFrequency ? 'frequencyProgress' : 'dutyProgress');
            const needle = $(isFrequency ? 'frequencyNeedle' : 'dutyNeedle');
            const angle = -135 + progress * 270;
            dial.style.setProperty('--progress', progress);
            progressCircle.style.strokeDashoffset = String(424 * (1 - progress));
            needle.style.transform = `rotate(${angle}deg)`;
            if (isFrequency) {
                const f = progressToFrequency(progress);
                const formatted = formatFrequency(f);
                $('frequencyValue').textContent = formatted.value;
                $('frequencyUnit').textContent = formatted.unit;
                dial.setAttribute('aria-valuenow', String(f));
            } else {
                const duty = Math.round(progress * 1000) / 10;
                $('dutyValue').textContent = duty.toFixed(1);
                dial.setAttribute('aria-valuenow', duty.toFixed(1));
            }
        }

        function renderState() {
            const freqProgress = frequencyToProgress(state.frequency);
            updateDial('frequency', freqProgress);
            updateDial('duty', state.duty / 100);

            const period = 1 / state.frequency;
            const high = period * state.duty / 100;
            const low = period - high;
            $('periodValue').textContent = formatTime(period);
            $('highValue').textContent = formatTime(high);
            $('lowValue').textContent = formatTime(low);
            $('waveLabel').textContent = `${state.duty.toFixed(1)}% · ${formatFrequency(state.frequency).value} ${formatFrequency(state.frequency).unit}`;
            $('pinInfo').textContent = `Saída: GPIO ${state.pin} · lógica 3,3 V`;
            $('pinSelect').value = String(state.pin);

            const toggle = $('outputToggle');
            toggle.classList.toggle('on', state.enabled);
            toggle.setAttribute('aria-checked', String(state.enabled));
            $('outputCaption').textContent = !state.enabled ? 'Desligada · nível baixo' : (state.duty === 0 ? 'Ligada · nível baixo (0%)' : (state.duty === 100 ? 'Ligada · nível alto (100%)' : 'Ativa · sinal PWM'));
            $('generatorState').classList.toggle('active', state.enabled);
            $('generatorState').lastElementChild.textContent = state.enabled ? 'Ativo' : 'Desativado';

            // Illustrative one-period waveform, with a minimum visible pulse width.
            const x0 = 8, x1 = 312, yHigh = 23, yLow = 77;
            const edge = x0 + (x1 - x0) * state.duty / 100;
            let path;
            if (state.duty <= 0) path = `M${x0} ${yLow}H${x1}`;
            else if (state.duty >= 100) path = `M${x0} ${yHigh}H${x1}`;
            else path = `M${x0} ${yLow}V${yHigh}H${edge.toFixed(1)}V${yLow}H${x1}`;
            $('wavePath').setAttribute('d', path);
        }

        function applyServerState(message) {
            if (typeof message.frequency === 'number') state.frequency = message.frequency;
            if (typeof message.duty === 'number') state.duty = message.duty;
            if (typeof message.enabled === 'boolean') state.enabled = message.enabled;
            if (typeof message.pin === 'number') state.pin = message.pin;
            renderState();
        }

        function connectWebSocket() {
            clearTimeout(reconnectTimer);
            const host = location.hostname || '192.168.4.1';
            socket = new WebSocket(`ws://${host}:81/`);
            socket.onopen = () => setConnection(true, 'Conectado ao ESP32');
            socket.onmessage = (event) => {
                try {
                    const message = JSON.parse(event.data);
                    if (message.type === 'state') applyServerState(message);
                    if (message.type === 'error') showToast(message.message || 'Comando recusado.');
                } catch (error) { console.warn('Mensagem WebSocket inválida', error); }
            };
            socket.onclose = () => {
                setConnection(false, 'Reconectando...');
                reconnectTimer = setTimeout(connectWebSocket, 1200);
            };
            socket.onerror = () => { try { socket.close(); } catch (_) { } };
        }

        function attachDial(dialId, kind) {
            const dial = $(dialId);
            let dragging = false;
            function handlePointer(event) {
                const rect = dial.getBoundingClientRect();
                const dx = event.clientX - (rect.left + rect.width / 2);
                const dy = event.clientY - (rect.top + rect.height / 2);
                let angle = Math.atan2(dy, dx) * 180 / Math.PI;
                angle = Math.max(-135, Math.min(135, angle));
                const progress = (angle + 135) / 270;
                if (kind === 'frequency') {
                    state.frequency = progressToFrequency(progress);
                    updateDial('frequency', frequencyToProgress(state.frequency));
                    queueCommand({ frequency: state.frequency });
                } else {
                    state.duty = Math.round(progress * 1000) / 10;
                    updateDial('duty', state.duty / 100);
                    queueCommand({ duty: state.duty });
                }
                renderState();
            }
            dial.addEventListener('pointerdown', (event) => {
                dragging = true;
                dial.setPointerCapture(event.pointerId);
                handlePointer(event);
            });
            dial.addEventListener('pointermove', (event) => { if (dragging) handlePointer(event); });
            dial.addEventListener('pointerup', () => { dragging = false; });
            dial.addEventListener('pointercancel', () => { dragging = false; });
            dial.addEventListener('keydown', (event) => {
                if (!['ArrowLeft', 'ArrowDown', 'ArrowRight', 'ArrowUp', 'Home', 'End'].includes(event.key)) return;
                event.preventDefault();
                if (kind === 'frequency') {
                    let p = frequencyToProgress(state.frequency);
                    if (event.key === 'Home') p = 0;
                    else if (event.key === 'End') p = 1;
                    else p += (event.key === 'ArrowRight' || event.key === 'ArrowUp' ? 0.015 : -0.015);
                    state.frequency = progressToFrequency(Math.max(0, Math.min(1, p)));
                    updateDial('frequency', frequencyToProgress(state.frequency));
                    queueCommand({ frequency: state.frequency });
                } else {
                    if (event.key === 'Home') state.duty = 0;
                    else if (event.key === 'End') state.duty = 100;
                    else state.duty = Math.max(0, Math.min(100, state.duty + (event.key === 'ArrowRight' || event.key === 'ArrowUp' ? 1 : -1)));
                    updateDial('duty', state.duty / 100);
                    queueCommand({ duty: state.duty });
                }
                renderState();
            });
        }

        attachDial('frequencyDial', 'frequency');
        attachDial('dutyDial', 'duty');

        document.querySelectorAll('[data-frequency]').forEach(button => {
            button.addEventListener('click', () => {
                state.frequency = Number(button.dataset.frequency);
                renderState();
                sendCommand({ frequency: state.frequency });
            });
        });
        document.querySelectorAll('[data-duty]').forEach(button => {
            button.addEventListener('click', () => {
                state.duty = Number(button.dataset.duty);
                renderState();
                sendCommand({ duty: state.duty });
            });
        });
        $('outputToggle').addEventListener('click', () => sendCommand({ enabled: !state.enabled }));
        $('pinSelect').addEventListener('change', event => sendCommand({ pin: Number(event.target.value) }));

        renderState();
        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";

// -----------------------------------------------------------------------------
// PWM helpers - Arduino-ESP32 2.x and 3.x compatibility
// -----------------------------------------------------------------------------

uint8_t resolutionForFrequency(uint32_t frequency) {
  // Choose the greatest shared resolution whose divider can be >= 1.
  // The 5 Hz lower limit also keeps the 14-bit timer divider within range
  // when clocked from the 80 MHz APB clock.
  uint8_t bits = 1;
  while (bits < PWM_MAX_RESOLUTION) {
    const uint64_t nextDivisor = (uint64_t)frequency << (bits + 1);
    if (nextDivisor > PWM_CLOCK_HZ) break;
    bits++;
  }
  return bits;
}

bool isAllowedPin(uint8_t pin) {
  for (size_t i = 0; i < ALLOWED_PIN_COUNT; i++) {
    if (ALLOWED_PINS[i] == pin) return true;
  }
  return false;
}

void writeStaticLevel(uint8_t pin, bool high) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, high ? HIGH : LOW);
}

void detachPwmFromPin(uint8_t pin) {
  if (!pwmAttached) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(pin);
#else
  ledcDetachPin(pin);
#endif
  pwmAttached = false;
}

bool writePwmDuty(float duty) {
  const uint32_t maxCount = (1UL << pwmState.resolution) - 1UL;
  uint32_t count = (uint32_t)lroundf((duty / 100.0f) * (float)(1UL << pwmState.resolution));
  if (count > maxCount) count = maxCount;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ledcWrite(pwmState.pin, count);
#else
  ledcWrite(PWM_CHANNEL, count);
  return true;
#endif
}

bool attachOrUpdatePwm() {
  const uint8_t newResolution = resolutionForFrequency(pwmState.frequency);
  uint32_t actualFrequency = pwmState.frequency;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!pwmAttached) {
    if (!ledcAttach(pwmState.pin, pwmState.frequency, newResolution)) return false;
    pwmAttached = true;
    actualFrequency = ledcReadFreq(pwmState.pin);
  } else {
    actualFrequency = ledcChangeFrequency(pwmState.pin, pwmState.frequency, newResolution);
    if (actualFrequency == 0) return false;
  }
#else
  actualFrequency = ledcSetup(PWM_CHANNEL, pwmState.frequency, newResolution);
  if (actualFrequency == 0) return false;
  if (!pwmAttached) {
    ledcAttachPin(pwmState.pin, PWM_CHANNEL);
    pwmAttached = true;
  }
#endif

  pwmState.resolution = newResolution;
  if (actualFrequency > 0) pwmState.frequency = actualFrequency;
  return writePwmDuty(pwmState.duty);
}

bool applyOutput() {
  if (!pwmState.enabled || pwmState.duty <= 0.0f) {
    if (pwmAttached) return writePwmDuty(0.0f);
    writeStaticLevel(pwmState.pin, false);
    return true;
  }

  if (pwmState.duty >= 100.0f) {
    detachPwmFromPin(pwmState.pin);
    writeStaticLevel(pwmState.pin, true);
    return true;
  }

  return attachOrUpdatePwm();
}

bool moveOutputPin(uint8_t newPin) {
  if (!isAllowedPin(newPin)) return false;
  if (newPin == pwmState.pin) return true;

  const uint8_t oldPin = pwmState.pin;
  detachPwmFromPin(oldPin);
  writeStaticLevel(oldPin, false);
  pwmState.pin = newPin;
  writeStaticLevel(newPin, false);
  return true;
}

// -----------------------------------------------------------------------------
// WebSocket state synchronization
// -----------------------------------------------------------------------------

void buildStateJson(char* output, size_t outputSize) {
  StaticJsonDocument<256> doc;
  doc["type"] = "state";
  doc["frequency"] = pwmState.frequency;
  doc["duty"] = pwmState.duty;
  doc["enabled"] = pwmState.enabled;
  doc["pin"] = pwmState.pin;
  doc["resolution"] = pwmState.resolution;
  serializeJson(doc, output, outputSize);
}

void sendStateTo(uint8_t clientNumber) {
  char json[256];
  buildStateJson(json, sizeof(json));
  webSocket.sendTXT(clientNumber, json);
}

void broadcastState() {
  char json[256];
  buildStateJson(json, sizeof(json));
  webSocket.broadcastTXT(json);
}

void sendError(uint8_t clientNumber, const char* message) {
  StaticJsonDocument<160> doc;
  doc["type"] = "error";
  doc["message"] = message;
  char json[160];
  serializeJson(doc, json, sizeof(json));
  webSocket.sendTXT(clientNumber, json);
}

void handleSetCommand(uint8_t clientNumber, uint8_t* payload, size_t length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    sendError(clientNumber, "JSON inválido.");
    return;
  }

  if (strcmp(doc["type"] | "", "set") != 0) return;

  PwmState previous = pwmState;
  PwmState candidate = pwmState;

  if (doc.containsKey("frequency")) {
    if (!doc["frequency"].is<double>() && !doc["frequency"].is<int>()) {
      sendError(clientNumber, "Frequência inválida.");
      return;
    }
    const double value = doc["frequency"].as<double>();
    if (!isfinite(value) || value < PWM_MIN_FREQUENCY || value > PWM_MAX_FREQUENCY) {
      sendError(clientNumber, "Frequência fora da faixa de 5 Hz a 50 kHz.");
      return;
    }
    candidate.frequency = (uint32_t)lround(value);
  }

  if (doc.containsKey("duty")) {
    if (!doc["duty"].is<double>() && !doc["duty"].is<int>()) {
      sendError(clientNumber, "Duty cycle inválido.");
      return;
    }
    const double value = doc["duty"].as<double>();
    if (!isfinite(value) || value < 0.0 || value > 100.0) {
      sendError(clientNumber, "Duty cycle deve ficar entre 0 e 100%.");
      return;
    }
    candidate.duty = (float)value;
  }

  if (doc.containsKey("enabled")) {
    if (!doc["enabled"].is<bool>()) {
      sendError(clientNumber, "Estado da saída inválido.");
      return;
    }
    candidate.enabled = doc["enabled"].as<bool>();
  }

  if (doc.containsKey("pin")) {
    if (!doc["pin"].is<int>()) {
      sendError(clientNumber, "GPIO inválido.");
      return;
    }
    const int requestedPin = doc["pin"].as<int>();
    if (requestedPin < 0 || requestedPin > 48 || !isAllowedPin((uint8_t)requestedPin)) {
      sendError(clientNumber, "GPIO não permitido. Use GPIO 4, 18 ou 21.");
      return;
    }
    candidate.pin = (uint8_t)requestedPin;
  }

  // Apply the requested pin change before applying the rest of the state.
  if (candidate.pin != pwmState.pin) {
    if (!moveOutputPin(candidate.pin)) {
      sendError(clientNumber, "Não foi possível alterar o GPIO.");
      return;
    }
  }
  pwmState = candidate;

  if (!applyOutput()) {
    // Restore the last known state if the LEDC peripheral rejects a setting.
    if (pwmState.pin != previous.pin) {
      detachPwmFromPin(pwmState.pin);
      writeStaticLevel(pwmState.pin, false);
      pwmState.pin = previous.pin;
      writeStaticLevel(pwmState.pin, false);
    }
    pwmState = previous;
    applyOutput();
    sendError(clientNumber, "O periférico LEDC não aceitou essa configuração.");
  }

  broadcastState();
}

void webSocketEvent(uint8_t clientNumber, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      sendStateTo(clientNumber);
      break;
    case WStype_TEXT:
      handleSetCommand(clientNumber, payload, length);
      break;
    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Setup and main loop
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // Use the same 80 MHz APB clock on ESP32 and ESP32-S3.
  if (!ledcSetClockSource(LEDC_USE_APB_CLK)) {
    Serial.println("Aviso: não foi possível selecionar explicitamente o clock APB do LEDC.");
  }
#endif

  writeStaticLevel(pwmState.pin, false);
  pwmState.resolution = resolutionForFrequency(pwmState.frequency);

  WiFi.mode(WIFI_AP);
  WiFi.softAP( AP_SSID, AP_PASSWORD, 1, false, 4 );
  delay(150);

  Serial.println();
  Serial.println("====================================");
  Serial.println("       ESP32 PWM - PULSE LAB");
  Serial.println("====================================");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("IP:   "); Serial.println(WiFi.softAPIP());
  Serial.println("HTTP: http://192.168.4.1");
  Serial.println("WS:   ws://192.168.4.1:81");

  httpServer.on("/", HTTP_GET, []() {
    httpServer.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  httpServer.onNotFound([]() {
    httpServer.send(404, "text/plain; charset=utf-8", "Página não encontrada");
  });
  httpServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Servidor HTTP e WebSocket iniciado.");
}

void loop() {
  httpServer.handleClient();
  webSocket.loop();
  delay(1);
}