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
  const user = Number(document.getElementById('u').value);
  const preset = Number(document.getElementById('p').value);
  const name = document.getElementById('name').value.trim();
  api('/api/presets/save', { user, preset, name }).catch(() => {});
});

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
      meta.textContent = ` (User ${item.user + 1} · Slot ${item.preset + 1} · ${Number(item.cm).toFixed(1)} cm)`;

      info.append(title, meta);

      const btn = document.createElement('button');
      btn.className = 'primary';
      btn.textContent = 'Anfahren';
      btn.addEventListener('click', () => {
        api('/api/presets/goto', { user: item.user, preset: item.preset }).catch(() => {});
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