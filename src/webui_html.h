#pragma once

// The single-page web UI (ADR-0006). GENERATED from web/ by
// scripts/gen_webui.py — edit the sources there, not this file.
static const char kWebUiHtml[] = R"SFERICWEBUI(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>sferic</title>
<style>
:root { color-scheme: dark }
* { box-sizing: border-box }
body { font-family: system-ui, sans-serif; margin: 0; background: #111; color: #ddd }
header { padding: 12px 16px; background: #1b1b1b; border-bottom: 1px solid #333 }
header b { color: #6cf; font-size: 18px }
header span { color: #888; font-size: 13px }
main { padding: 12px 16px; max-width: 640px; margin: 0 auto }

.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 8px; margin: 8px 0 16px }
.stat { background: #1b1b1b; border: 1px solid #333; border-radius: 8px; padding: 8px 10px }
.stat small { color: #888; display: block }

h3 { margin: 16px 0 6px; color: #9ad; font-size: 14px; text-transform: uppercase; letter-spacing: .04em }

.card { background: #1b1b1b; border: 1px solid #333; border-radius: 8px; padding: 10px 12px; margin: 6px 0 }
.row { display: flex; align-items: center; gap: 10px }
.row + .row { margin-top: 8px }
.row .m { flex: 1; min-width: 0 }
.row .m b { display: block }
.row .m small { color: #888 }
label { color: #9ad; font-size: 13px; min-width: 84px }

.sw { width: 44px; height: 24px; border-radius: 12px; background: #444; position: relative; flex: none; cursor: pointer; transition: .15s }
.sw.on { background: #2a7 }
.sw::after { content: ""; position: absolute; top: 2px; left: 2px; width: 20px; height: 20px; border-radius: 50%; background: #fff; transition: .15s }
.sw.on::after { left: 22px }

a.btn, button { background: #2a6; color: #fff; border: 0; border-radius: 8px; padding: 10px 14px; text-decoration: none; font: inherit; cursor: pointer }
button.sm { padding: 6px 12px; font-size: 13px }
button.ghost { background: #333; color: #cde }
button:disabled { opacity: .4; cursor: default }

select { background: #222; color: #ddd; border: 1px solid #444; border-radius: 6px; padding: 6px 8px; font: inherit }

pre.out { margin: 8px 0 0; padding: 8px 10px; background: #0c0c0c; border: 1px solid #333; border-radius: 6px;
  color: #9f9; font-size: 12px; white-space: pre-wrap; word-break: break-word; overflow-x: auto }
pre.out.err { color: #f99 }
</style>
</head>
<body>
<header><b>sferic</b> <span id="name"></span></header>
<main>
  <div class="grid" id="stats"></div>

  <h3>Display</h3>
  <div class="card">
    <div class="row"><label for="sleep">Sleep</label><select id="sleep"></select></div>
    <div class="row"><label for="bright">Brightness</label><select id="bright"></select></div>
  </div>

  <h3>Capture</h3>
  <a class="btn" id="dl" href="/api/capture.pcap">download capture.pcap</a>

  <div id="tools"></div>
</main>
<script>
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
</script>
</body>
</html>
)SFERICWEBUI";
