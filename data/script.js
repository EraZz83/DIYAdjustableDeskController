let holdTimer = null;
let activeDir = null;

async function api(path, body = {}) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}`);
  }
  await refresh();
}

function pulseMove(dir) {
  fetch('/api/move', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ direction: dir })
  }).catch(() => {});
}

function startHold(dir) {
  if (activeDir === dir) return;
  activeDir = dir;
  pulseMove(dir);
  clearInterval(holdTimer);
  holdTimer = setInterval(() => pulseMove(dir), 350);
}

function stopHold() {
  if (!activeDir) return;
  activeDir = null;
  clearInterval(holdTimer);
  holdTimer = null;
  api('/api/move', { direction: 'stop' }).catch(() => {});
}

function bindHold(id, dir) {
  const el = document.getElementById(id);
  el.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    startHold(dir);
  });
  ['pointerup', 'pointerleave', 'pointercancel'].forEach((ev) =>
    el.addEventListener(ev, stopHold)
  );
}

window.addEventListener('blur', stopHold);

bindHold('btnUp', 'up');
bindHold('btnDown', 'down');

document.getElementById('btnStop').addEventListener('click', () => {
  stopHold();
  api('/api/stop').catch(() => {});
});

document.getElementById('btnZero').addEventListener('click', () => {
  api('/api/zero').catch(() => {});
});

document.getElementById('btnSave').addEventListener('click', () => {
  const preset = Number(document.getElementById('p').value);
  const name = document.getElementById('name').value.trim();
  api('/api/presets/save', { preset, name }).catch(() => {});
});

const wifiModal = document.getElementById('wifiModal');
const btnOpenWifi = document.getElementById('btnOpenWifi');
const btnCloseWifi = document.getElementById('btnCloseWifi');

function openWifiModal() {
  wifiModal.classList.add('open');
  wifiModal.setAttribute('aria-hidden', 'false');
  loadWifiStatus();
}

function closeWifiModal() {
  wifiModal.classList.remove('open');
  wifiModal.setAttribute('aria-hidden', 'true');
}

btnOpenWifi.addEventListener('click', openWifiModal);
btnCloseWifi.addEventListener('click', closeWifiModal);

wifiModal.addEventListener('click', (e) => {
  if (e.target === wifiModal) {
    closeWifiModal();
  }
});

document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && wifiModal.classList.contains('open')) {
    closeWifiModal();
  }
});

document.getElementById('btnWifiSave').addEventListener('click', async () => {
  const ssid = document.getElementById('wifiSsid').value.trim();
  const password = document.getElementById('wifiPass').value;
  const feedback = document.getElementById('wifiFeedback');

  if (!ssid) {
    feedback.textContent = 'Bitte eine SSID eingeben.';
    return;
  }

  feedback.textContent = 'Speichere Zugangsdaten und starte ESP32 neu...';
  try {
    const res = await fetch('/api/wifi/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid, password })
    });
    if (!res.ok) {
      feedback.textContent = `Fehler (${res.status}) beim Speichern.`;
      return;
    }
    document.getElementById('wifiPass').value = '';
    setTimeout(loadWifiStatus, 4000);
  } catch (_) {
    feedback.textContent = 'ESP32 startet neu – gleich Verbindung prüfen.';
  }
});

async function loadWifiStatus() {
  try {
    const res = await fetch('/api/wifi');
    if (!res.ok) return;
    const w = await res.json();

    const modeLabel = w.apMode ? 'Setup-AP aktiv' : (w.connected ? 'Verbunden' : 'Getrennt');
    const rssiLabel = !w.apMode && w.connected ? ` · ${w.rssi} dBm` : '';
    document.getElementById('wifiStatus').textContent =
      `${modeLabel} · SSID: ${w.ssid || '-'} · IP: ${w.ip}${rssiLabel}`;

    const ssidInput = document.getElementById('wifiSsid');
    if (document.activeElement !== ssidInput && w.ssid) {
      ssidInput.value = w.ssid;
    }
  } catch (_) {}
}

async function refresh() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();

    document.getElementById('h').textContent = Number(data.heightCm).toFixed(1);

    const targetText =
      data.targetCm !== null && data.targetCm !== undefined
        ? ` → Ziel: ${Number(data.targetCm).toFixed(1)} cm`
        : '';
    document.getElementById('mode').textContent = `Modus: ${data.mode}${targetText}`;

    const box = document.getElementById('presets');
    box.replaceChildren();

    (data.presets || []).forEach((item) => {
      const row = document.createElement('div');
      row.className = 'preset';

      const info = document.createElement('div');
      const title = document.createElement('strong');
      title.textContent = item.name;

      const meta = document.createElement('span');
      meta.className = 'sub';
      meta.textContent = ` (Slot ${item.preset + 1} · ${Number(item.cm).toFixed(1)} cm)`;

      info.append(title, meta);

      const btn = document.createElement('button');
      btn.className = 'primary';
      btn.type = 'button';
      btn.textContent = 'Anfahren';
      btn.addEventListener('click', () => {
        api('/api/presets/goto', { preset: item.preset }).catch(() => {});
      });

      row.append(info, btn);
      box.appendChild(row);
    });
  } catch (err) {
    document.getElementById('mode').textContent = 'Verbindung getrennt...';
  }
}

refresh();
setInterval(refresh, 500);