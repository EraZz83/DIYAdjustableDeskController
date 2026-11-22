let holdTimer = null;
let activeDir = null;
let activeTargetId = null;
let lastPresetsJson = '';

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
  console.log(`Button gedrückt: ${dir.toUpperCase()} (Halten gestartet)`);
  pulseMove(dir);
  clearInterval(holdTimer);
  holdTimer = setInterval(() => pulseMove(dir), 350);
}

function stopHold() {
  if (!activeDir) return;
  console.log(`Button losgelassen: ${activeDir.toUpperCase()} (Halten beendet)`);
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

// Stopp-Button
const btnStop = document.getElementById('btnStop');
btnStop.addEventListener('click', () => {
  console.log('Button geklickt: STOPP');
  stopHold();
  activeTargetId = null;
  updateMovingUI(false);
  api('/api/stop').catch(() => {});
});

// Nullpunkt-Button (dauerhaft speichern)
const btnZero = document.getElementById('btnZero');
btnZero.addEventListener('click', async () => {
  console.log('Button geklickt: NULLPUNKT');
  const ok = confirm('Möchtest du die aktuelle Höhe als neuen Nullpunkt (0.0 cm) dauerhaft speichern?');
  if (!ok) return;
  try {
    await api('/api/zero');
  } catch (err) {
    alert('Fehler beim Setzen des Nullpunkts: ' + err.message);
  }
});

// Preset hinzufügen
const presetNameInput = document.getElementById('presetName');
const btnAddPreset = document.getElementById('btnAddPreset');

async function handleAddPreset() {
  const name = presetNameInput.value.trim();
  console.log('Button geklickt: PRESET SPEICHERN', { name });
  try {
    await api('/api/presets/add', { name });
    presetNameInput.value = '';
  } catch (err) {
    alert('Fehler beim Speichern des Presets: ' + err.message);
  }
}

btnAddPreset.addEventListener('click', handleAddPreset);
presetNameInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    e.preventDefault();
    handleAddPreset();
  }
});

// Moving State UI Handling
function updateMovingUI(isMoving, targetId = null) {
  if (isMoving) {
    document.body.classList.add('is-moving');
  } else {
    document.body.classList.remove('is-moving');
    activeTargetId = null;
  }

  // Stopp bleibt IMMER aktiv
  btnStop.disabled = false;

  // Manuelle Tasten & Inputs sperren / freigeben
  document.getElementById('btnUp').disabled = isMoving;
  document.getElementById('btnDown').disabled = isMoving;
  btnZero.disabled = isMoving;
  btnAddPreset.disabled = isMoving;
  presetNameInput.disabled = isMoving;

  // Preset Buttons aktualisieren
  document.querySelectorAll('.preset').forEach((row) => {
    const rowId = Number(row.dataset.id);
    const gotoBtn = row.querySelector('.btn-goto');
    const delBtn = row.querySelector('.btn-del');

    if (delBtn) delBtn.disabled = isMoving;

    if (gotoBtn) {
      gotoBtn.disabled = isMoving;
      if (isMoving && targetId !== null && rowId === targetId) {
        row.classList.add('active-target');
        gotoBtn.textContent = 'Fährt... ⏳';
      } else {
        row.classList.remove('active-target');
        gotoBtn.textContent = 'Anfahren';
      }
    }
  });
}

// Preset-Liste rendern
function renderPresets(presets, isMoving, targetCm) {
  const box = document.getElementById('presets');
  box.replaceChildren();

  if (!presets || presets.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'preset-empty';
    empty.textContent = 'Noch keine Presets vorhanden. Stelle eine Höhe ein und speichere sie ab.';
    box.appendChild(empty);
    return;
  }

  presets.forEach((item) => {
    const row = document.createElement('div');
    row.className = 'preset';
    row.dataset.id = item.id;

    const info = document.createElement('div');
    info.className = 'preset-info';

    const title = document.createElement('strong');
    title.className = 'preset-name';
    title.textContent = item.name;

    const meta = document.createElement('span');
    meta.className = 'preset-cm';
    meta.textContent = `${Number(item.cm).toFixed(1)} cm`;

    info.append(title, meta);

    const actions = document.createElement('div');
    actions.className = 'preset-actions';

    const btnGoto = document.createElement('button');
    btnGoto.className = 'primary btn-goto';
    btnGoto.type = 'button';
    btnGoto.textContent = 'Anfahren';
    btnGoto.disabled = isMoving;

    btnGoto.addEventListener('click', async () => {
      console.log('Button geklickt: ANFAHREN', item);
      activeTargetId = item.id;
      updateMovingUI(true, item.id);
      try {
        await api('/api/presets/goto', { id: item.id });
      } catch (err) {
        updateMovingUI(false);
      }
    });

    const btnDel = document.createElement('button');
    btnDel.className = 'danger btn-del';
    btnDel.type = 'button';
    btnDel.title = 'Preset löschen';
    btnDel.innerHTML = '&times;';
    btnDel.disabled = isMoving;

    btnDel.addEventListener('click', async () => {
      console.log('Button geklickt: PRESET LÖSCHEN', item);
      const ok = confirm(`Preset "${item.name}" wirklich löschen?`);
      if (!ok) return;
      try {
        await api('/api/presets/delete', { id: item.id });
      } catch (err) {
        alert('Fehler beim Löschen: ' + err.message);
      }
    });

    actions.append(btnGoto, btnDel);
    row.append(info, actions);
    box.appendChild(row);
  });
}

// Status & Polling
async function refresh() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();

    document.getElementById('h').textContent = Number(data.heightCm).toFixed(1);

    const isMoving = Boolean(data.moving);
    const targetText =
      data.targetCm !== null && data.targetCm !== undefined
        ? ` → Ziel: ${Number(data.targetCm).toFixed(1)} cm`
        : '';
    document.getElementById('mode').textContent = `Modus: ${data.mode}${targetText}`;

    // Wenn Ziel vorgegeben ist, aktives Preset bestimmen
    if (isMoving && activeTargetId === null && data.targetCm !== null && data.presets) {
      const match = data.presets.find(
        (p) => Math.abs(p.cm - data.targetCm) < 0.2
      );
      if (match) activeTargetId = match.id;
    }

    // Presets nur neu aufbauen, wenn sich die Daten geändert haben
    const currentJson = JSON.stringify(data.presets || []);
    if (currentJson !== lastPresetsJson) {
      lastPresetsJson = currentJson;
      renderPresets(data.presets || [], isMoving, data.targetCm);
    }

    updateMovingUI(isMoving, activeTargetId);
  } catch (err) {
    document.getElementById('mode').textContent = 'Verbindung getrennt...';
  }
}

// WLAN Modal Handling
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

refresh();
setInterval(refresh, 500);
