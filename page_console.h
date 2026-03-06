#pragma once

// ====== Console page (/console) with timestamps, auto-scroll, pause, scroll-to-bottom======
const char PAGE_CONSOLE[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<title>ESP32 Console</title>
<style>
  body { margin:0; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; background:#111; color:#eee; }
  #bar { display:flex; align-items:center; gap:12px; padding:8px 10px; background:#1b1b1b; font-size:14px; flex-wrap: wrap; }
  #out { white-space:pre-wrap; height:72vh; overflow:auto; padding:10px; border-top:1px solid #2a2a2a; border-bottom:1px solid #2a2a2a; }
  #in  { width:100%; box-sizing:border-box; padding:10px; border:none; outline:none; background:#222; color:#fff; }
  label { user-select:none; }
  a { color:#5fb3ff; text-decoration:none; }
  a:hover { text-decoration:underline; }
  .sep { opacity:.3; }
  .btn { padding:6px 10px; background:#2b2b2b; color:#eee; border:1px solid #444; border-radius:6px; cursor:pointer; }
  #scrollBottom { display:none; }
  #status.badge { padding:2px 8px; border:1px solid #444; border-radius:999px; font-size:12px; }
  #buf.badge    { padding:2px 8px; border:1px solid #444; border-radius:999px; font-size:12px; background:#2b2b2b; display:none; }
  #buf.warn     { border-color:#a66; color:#f99; }
</style>

<div id="bar">
  <a href="/">index</a>
  <span class="sep">|</span>
  <label><input type="checkbox" id="ts"> Timestamps</label>
  <label><input type="checkbox" id="autoscroll"> Auto-scroll</label>
  <label><input type="checkbox" id="pause"> Pause</label>
  <button id="scrollBottom" class="btn">Scroll to bottom</button>
  <button id="clear" class="btn">Clear</button>
  <span id="status" class="badge">Connecting...</span>
  <span id="buf" class="badge"></span>
</div>

<pre id="out"></pre>
<input id="in" placeholder="type command + Enter">

<script>
// --- DOM ---
const out    = document.getElementById("out");
const inp    = document.getElementById("in");
const stat   = document.getElementById("status");
const chkTs  = document.getElementById("ts");
const chkAS  = document.getElementById("autoscroll");
const chkP   = document.getElementById("pause");
const btnCl  = document.getElementById("clear");
const btnSB  = document.getElementById("scrollBottom");
const bufUI  = document.getElementById("buf");

// --- Settings (persist) ---
chkTs.checked = localStorage.getItem("console_ts") === "1";
chkAS.checked = localStorage.getItem("console_autoscroll") !== "0"; // default ON
chkP.checked  = localStorage.getItem("console_pause") === "1";

chkTs.addEventListener("change", () => localStorage.setItem("console_ts", chkTs.checked ? "1" : "0"));
chkAS.addEventListener("change", () => { localStorage.setItem("console_autoscroll", chkAS.checked ? "1" : "0"); updateScrollBtnVisibility(); });
chkP.addEventListener("change",  () => { localStorage.setItem("console_pause", chkP.checked ? "1" : "0"); if (!chkP.checked) flushPausedBuffer(); updateScrollBtnVisibility(); updateBufStatus(); });

// --- State ---
let lineStart = true; // start of logical line?
let ws;

// --- Pause buffer (FIFO) ---
let pausedBuffer = [];      // array of strings
let pausedBytes = 0;       // total bytes buffered
let trimmedBytesTotal = 0;
let trimmedChunks = 0;
// Production cap ~200000 (≈200 KB). Lower for quicker demo of trimming.
const PAUSE_MAX_BYTES = 200000;

// ---------- Utils ----------
function kb(n) { return (n/1024).toFixed(1); }
function isAtBottom() { return (out.scrollTop + out.clientHeight) >= (out.scrollHeight - 2); }
function scrollToBottom() { out.scrollTop = out.scrollHeight; }
function updateScrollBtnVisibility() {
  btnSB.style.display = (!chkAS.checked && !isAtBottom()) ? "inline-block" : "none";
}

function tsNow() {
  const d = new Date();
  const pad = (n, w=2) => String(n).padStart(w, "0");
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(),3)}`;
}

function updateBufStatus() {
  if (!chkP.checked) {
    bufUI.style.display = "none";
    bufUI.classList.remove("warn");
    return;
  }

  bufUI.style.display = "inline-block";
  bufUI.textContent = `Paused - buffering ${kb(pausedBytes)} KB, trimmed ${kb(trimmedBytesTotal)} KB (${trimmedChunks} drops)`;
  if (trimmedBytesTotal > 0) bufUI.classList.add("warn"); else bufUI.classList.remove("warn");
}

// ---------- Pause buffer ops ----------
function bufferWhilePaused(data) {
  pausedBuffer.push(data);
  pausedBytes += data.length;
  while (pausedBytes > PAUSE_MAX_BYTES && pausedBuffer.length > 0) {
    const dropped = pausedBuffer.shift();
    pausedBytes -= dropped.length;
    trimmedBytesTotal += dropped.length;
    trimmedChunks++;
  }

  updateBufStatus();
}
function flushPausedBuffer() {
  if (pausedBuffer.length === 0) return;
  const combined = pausedBuffer.join("");
  pausedBuffer = [];
  pausedBytes = 0;
  appendTextRender(combined);
  updateBufStatus();
}

// ---------- Rendering ----------
function appendTextRender(data) {
  const endsWithNL = data.endsWith("\n");
  const parts = data.split("\n");

  for (let i = 0; i < parts.length; i++) {
    const seg = parts[i];
    if (seg.length > 0) {
      if (chkTs.checked && lineStart) out.textContent += `[${tsNow()}] `;
      out.textContent += seg;
      lineStart = false;
    }
    if (i < parts.length - 1) {
      out.textContent += "\n";
      lineStart = true;
    }
  }
  if (endsWithNL) lineStart = true;

  if (chkAS.checked) scrollToBottom(); else updateScrollBtnVisibility();

  // Trim console to keep DOM fast
  if (out.textContent.length > 200000) out.textContent = out.textContent.slice(-150000);
}

function incoming(data) {
  // Called by WebSocket onmessage
  if (chkP.checked) bufferWhilePaused(data);
  else appendTextRender(data);
}
// ---------- WebSocket wiring ----------
function connect() {
  ws = new WebSocket(`ws://${location.host}/console-ws`);
  ws.onopen = () => { stat.textContent = "Connected"; };
  ws.onclose = () => { stat.textContent = "Disconnected. Reconnecting..."; setTimeout(connect, 1000); };
  ws.onerror = () => { stat.textContent = "Error"; };
  ws.onmessage = (e) => { incoming(e.data); };
}
connect();

// ---------- UI events ----------
inp.addEventListener("keydown", e => {
  if (e.key === "Enter" && ws.readyState === WebSocket.OPEN) {
    ws.send(inp.value);
    inp.value = "";
  }
});

btnCl.onclick = () => { out.textContent = ""; lineStart = true; updateScrollBtnVisibility(); };
btnSB.onclick = () => { scrollToBottom(); updateScrollBtnVisibility(); };
out.addEventListener("scroll", () => { if (!chkAS.checked) updateScrollBtnVisibility(); });
window.addEventListener("resize", updateScrollBtnVisibility);

// Initial UI state
updateScrollBtnVisibility();
updateBufStatus();
</script>
)HTML";
