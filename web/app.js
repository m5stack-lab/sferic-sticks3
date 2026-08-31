// sferic web UI (ADR-0006). Vanilla, no external assets — the station's
// SoftAP has no internet.

const $ = (id) => document.getElementById(id);
const j = async (u, o) => (await fetch(u, o)).json();
const esc = (s) => String(s).replace(/[&<>]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]));

const SLEEP_OPTS = [
  [0, "Never"], [15000, "15s"], [30000, "30s"],
  [60000, "1m"], [120000, "2m"], [300000, "5m"],
];
const BRIGHT_OPTS = [20, 40, 60, 80, 100];

function stat(k, v) {
  return `<div class=stat><small>${k}</small>${v}</div>`;
}

async function refreshStatus() {
  const s = await j("/api/status");
  $("name").textContent = `${s.name} ${s.version} (${s.build})`;
  const c = s.capture, d = s.display || {};
  $("stats").innerHTML =
    stat("uptime", s.uptime_s + "s") +
    stat("heap", (s.heap / 1024 | 0) + " KB") +
    stat("battery", s.batt + "%" + (s.on_usb ? " (USB)" : "")) +
    stat("screen", (d.awake ? "on" : "asleep") + " · " + (d.brightness ?? "?") + "%") +
    stat("capture", c.frames + " fr / " + (c.used / 1024 | 0) + " KB" +
      (c.recording ? " REC" : c.full ? " FULL" : ""));
  $("dl").style.display = c.used > 24 ? "inline-block" : "none";
  syncDisplayControls(d);
}

let displayTouched = 0;  // don't stomp a just-changed <select> from a poll
function syncDisplayControls(d) {
  if (Date.now() - displayTouched < 4000) return;
  if (d.sleep_ms != null) $("sleep").value = String(d.sleep_ms);
  if (d.brightness != null) $("bright").value = String(d.brightness);
}

function buildDisplayControls() {
  $("sleep").innerHTML = SLEEP_OPTS.map(([v, l]) => `<option value=${v}>${l}</option>`).join("");
  $("bright").innerHTML = BRIGHT_OPTS.map((v) => `<option value=${v}>${v}%</option>`).join("");
  const push = (k, el) => {
    displayTouched = Date.now();
    fetch(`/api/display?${k}=${encodeURIComponent(el.value)}`, { method: "POST" });
  };
  $("sleep").onchange = (e) => push("sleep_ms", e.target);
  $("bright").onchange = (e) => push("brightness", e.target);
}

async function runTool(id, out, btn) {
  btn.disabled = true;
  out.className = "out";
  out.textContent = "running…";
  try {
    const r = await fetch("/api/tools/run?id=" + encodeURIComponent(id));
    const body = await r.json();
    const payload = r.ok ? body.result : body;
    out.classList.toggle("err", !r.ok);
    out.textContent = JSON.stringify(payload, null, 2);
  } catch (e) {
    out.className = "out err";
    out.textContent = String(e);
  } finally {
    btn.disabled = false;
  }
}

async function refreshTools() {
  const ts = await j("/api/tools");
  const by = {};
  ts.forEach((t) => (by[t.toolset] = by[t.toolset] || []).push(t));
  const wrap = $("tools");
  wrap.innerHTML = "";
  for (const g in by) {
    const h = document.createElement("h3");
    h.textContent = g;
    wrap.appendChild(h);
    by[g].forEach((t) => wrap.appendChild(toolCard(t)));
  }
}

function toolCard(t) {
  const card = document.createElement("div");
  card.className = "card";
  const row = document.createElement("div");
  row.className = "row";
  row.innerHTML =
    `<div class=m><b>${esc(t.name)}</b><small>${esc(t.summary)}</small></div>` +
    (t.has_run ? `<button class="sm run" ${t.enabled ? "" : "disabled"}>Run</button>` : "") +
    `<div class="sw ${t.enabled ? "on" : ""}"></div>`;
  card.appendChild(row);

  row.querySelector(".sw").onclick = async () => {
    await fetch(`/api/tools/set?id=${encodeURIComponent(t.id)}&on=${t.enabled ? 0 : 1}`, { method: "POST" });
    refreshTools();
  };
  const runBtn = row.querySelector(".run");
  if (runBtn) {
    const out = document.createElement("pre");
    out.className = "out";
    out.hidden = true;
    card.appendChild(out);
    runBtn.onclick = () => { out.hidden = false; runTool(t.id, out, runBtn); };
  }
  return card;
}

buildDisplayControls();
refreshStatus();
refreshTools();
setInterval(refreshStatus, 2000);
