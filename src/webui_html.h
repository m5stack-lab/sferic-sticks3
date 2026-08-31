#pragma once

// The single-page web UI (ADR-0006). Self-contained, no external assets.
static const char kWebUiHtml[] = R"HTML(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>sferic</title>
<style>
:root{color-scheme:dark}
body{font-family:system-ui,sans-serif;margin:0;background:#111;color:#ddd}
header{padding:12px 16px;background:#1b1b1b;border-bottom:1px solid #333}
header b{color:#6cf;font-size:18px}
main{padding:12px 16px;max-width:640px;margin:0 auto}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px;margin:8px 0 16px}
.stat{background:#1b1b1b;border:1px solid #333;border-radius:8px;padding:8px 10px}
.stat small{color:#888;display:block}
h3{margin:16px 0 6px;color:#9ad}
.tool{display:flex;align-items:center;gap:10px;padding:8px 10px;background:#1b1b1b;border:1px solid #333;border-radius:8px;margin:6px 0}
.tool .m{flex:1;min-width:0}
.tool .m b{display:block}
.tool .m small{color:#888}
.sw{width:44px;height:24px;border-radius:12px;background:#444;position:relative;flex:none;cursor:pointer;transition:.15s}
.sw.on{background:#2a7}
.sw::after{content:"";position:absolute;top:2px;left:2px;width:20px;height:20px;border-radius:50%;background:#fff;transition:.15s}
.sw.on::after{left:22px}
a.btn,button{display:inline-block;background:#2a6;color:#fff;border:0;border-radius:8px;padding:10px 14px;text-decoration:none;font:inherit;cursor:pointer}
</style></head>
<body>
<header><b>sferic</b> <span id=name></span></header>
<main>
<div class=grid id=stats></div>
<a class=btn id=dl href="/api/capture.pcap">download capture.pcap</a>
<div id=tools></div>
</main>
<script>
async function j(u,o){const r=await fetch(u,o);return r.json()}
function stat(k,v){return `<div class=stat><small>${k}</small>${v}</div>`}
async function refreshStatus(){
 const s=await j('/api/status')
 document.getElementById('name').textContent=s.name+' '+s.version+' ('+s.build+')'
 const c=s.capture
 document.getElementById('stats').innerHTML=
  stat('uptime',s.uptime_s+'s')+stat('heap',(s.heap/1024|0)+' KB')+
  stat('battery',s.batt+'%'+(s.on_usb?' (USB)':''))+
  stat('capture',c.frames+' fr / '+(c.used/1024|0)+' KB'+(c.recording?' REC':c.full?' FULL':''))
 document.getElementById('dl').style.display=c.used>24?'inline-block':'none'
}
async function refreshTools(){
 const ts=await j('/api/tools'); const by={}
 ts.forEach(t=>(by[t.toolset]=by[t.toolset]||[]).push(t))
 let h=''
 for(const g in by){h+='<h3>'+g+'</h3>'
  by[g].forEach(t=>{h+=`<div class=tool><div class=m><b>${t.name}</b><small>${t.summary}</small></div>`
   +`<div class="sw ${t.enabled?'on':''}" data-id="${t.id}" data-on="${t.enabled?1:0}"></div></div>`})}
 document.getElementById('tools').innerHTML=h
 document.querySelectorAll('.sw').forEach(el=>el.onclick=async()=>{
  const on=el.dataset.on==='1'?0:1
  await fetch('/api/tools/set?id='+encodeURIComponent(el.dataset.id)+'&on='+on,{method:'POST'})
  refreshTools()})
}
refreshStatus();refreshTools();setInterval(refreshStatus,2000)
</script>
</body></html>)HTML";
