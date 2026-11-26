let holdTimer = null;
let activeDir = null;
let activeTargetId = null;
let lastPresetsJson = '';
let currentLimitMin = 50.0;
let currentLimitMax = 130.0;
let currentOffsetCm = 0.0;

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
  holdTimer = setInterval(() => pulseMove(dir), 250);
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

// Sollwert-Steuerung (Target Height)
const targetInput = document.getElementById('targetInput');
const btnStartTarget = document.getElementById('btnStartTarget');
const targetHint = document.getElementById('targetHint');

async function handleStartTarget() {
  const rawVal = parseFloat(targetInput.value);
  if (isNaN(rawVal)) {
    targetHint.textContent = 'Bitte eine gültige Zahl eingeben.';
    return;
  }

  let val = rawVal;
  if (val > currentLimitMax) {
    val = currentLimitMax;
    targetInput.value = val.toFixed(1);
    targetHint.textContent = `Auf oberes Limit (${val.toFixed(1)} cm) gesetzt`;
  } else if (val < currentLimitMin) {
    val = currentLimitMin;
    targetInput.value = val.toFixed(1);
    targetHint.textContent = `Auf unteres Limit (${val.toFixed(1)} cm) gesetzt`;
  } else {
    targetHint.textContent = '';
  }

  console.log(`Starte Fahrt zu Sollwert: ${val} cm`);
  updateMovingUI(true);
  try {
    await api('/api/target', { targetCm: val });
  } catch (err) {
    alert('Fehler beim Anfahren: ' + err.message);
    updateMovingUI(false);
  }
}

btnStartTarget.addEventListener('click', handleStartTarget);
targetInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') {
    e.preventDefault();
    handleStartTarget();
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
  btnAddPreset.disabled = isMoving;
  presetNameInput.disabled = isMoving;
  targetInput.disabled = isMoving;
  btnStartTarget.disabled = isMoving;

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
    btnGoto.className = 'btn btn-primary btn-goto';
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
    btnDel.className = 'btn btn-del';
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

    if (data.offsetCm !== undefined) currentOffsetCm = Number(data.offsetCm);
    if (data.limitMin !== undefined) currentLimitMin = Number(data.limitMin);
    if (data.limitMax !== undefined) currentLimitMax = Number(data.limitMax);

    targetInput.min = currentLimitMin;
    targetInput.max = currentLimitMax;

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

// Einstellungen Modal Handling (Zahnrad)
const settingsModal = document.getElementById('settingsModal');
const btnOpenSettings = document.getElementById('btnOpenSettings');
const btnCloseSettings = document.getElementById('btnCloseSettings');

const sensorOffsetInput = document.getElementById('sensorOffset');
const limitMinInput = document.getElementById('limitMin');
const limitMaxInput = document.getElementById('limitMax');
const btnSaveSettings = document.getElementById('btnSaveSettings');
const settingsFeedback = document.getElementById('settingsFeedback');

function openSettingsModal() {
  settingsModal.classList.add('open');
  settingsModal.setAttribute('aria-hidden', 'false');
  sensorOffsetInput.value = currentOffsetCm.toFixed(1);
  limitMinInput.value = currentLimitMin.toFixed(1);
  limitMaxInput.value = currentLimitMax.toFixed(1);
  settingsFeedback.textContent = '';
  loadWifiStatus();
}

function closeSettingsModal() {
  settingsModal.classList.remove('open');
  settingsModal.setAttribute('aria-hidden', 'true');
}

btnOpenSettings.addEventListener('click', openSettingsModal);
btnCloseSettings.addEventListener('click', closeSettingsModal);

settingsModal.addEventListener('click', (e) => {
  if (e.target === settingsModal) {
    closeSettingsModal();
  }
});

document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && settingsModal.classList.contains('open')) {
    closeSettingsModal();
  }
});

// Offset & Limiten gemeinsam speichern
btnSaveSettings.addEventListener('click', async () => {
  const offsetCm = parseFloat(sensorOffsetInput.value) || 0.0;
  const min = parseFloat(limitMinInput.value);
  const max = parseFloat(limitMaxInput.value);

  if (isNaN(min) || isNaN(max) || min >= max) {
    settingsFeedback.textContent = 'Fehler: Min-Limite muss kleiner als Max-Limite sein!';
    settingsFeedback.style.color = '#ff7582';
    return;
  }

  try {
    const res = await fetch('/api/limits', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ min, max, offsetCm })
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    currentOffsetCm = data.offsetCm !== undefined ? data.offsetCm : offsetCm;
    currentLimitMin = data.min;
    currentLimitMax = data.max;
    settingsFeedback.textContent = `Gespeichert! (Offset: ${currentOffsetCm.toFixed(1)} cm, Min: ${data.min.toFixed(1)} cm, Max: ${data.max.toFixed(1)} cm)`;
    settingsFeedback.style.color = '#54e38e';
    setTimeout(() => { if (settingsFeedback) settingsFeedback.textContent = ''; }, 3500);
    await refresh();
  } catch (err) {
    settingsFeedback.textContent = 'Fehler beim Speichern der Einstellungen.';
    settingsFeedback.style.color = '#ff7582';
  }
});

// WLAN Speichern
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
