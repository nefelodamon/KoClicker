#pragma once

#define PAGE_UPDATE_VERSION "1.3"

String buildUpdateHtml() {
  String html;
  html.reserve(2048);
  html += F("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>KoClicker Firmware Update</title><style>"
            "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Arial,sans-serif;margin:20px;background:#111;color:#ddd;}"
            "h1{font-weight:600;margin:0 0 10px 0}"
            ".card{padding:12px 14px;border:1px solid #333;border-radius:8px;margin:10px 0;background:#191919;}"
            "label{display:block;font-size:13px;color:#aaa;margin-bottom:6px}"
            ".row{display:flex;gap:8px;margin-top:10px}"
            ".btn{padding:8px 12px;border-radius:6px;border:0;cursor:pointer;font-weight:600}"
            ".btn.primary{background:#5fb3ff;color:#041827}"
            ".status{font-size:13px;color:#aaa;margin-top:10px;min-height:18px}"
            ".status.ok{color:#4caf50} .status.err{color:#f44336}"
            "#bar-wrap{display:none;background:#2b2b2b;border-radius:6px;margin-top:10px;overflow:hidden}"
            "#bar{height:18px;width:0%;background:#5fb3ff;border-radius:6px;text-align:center;line-height:18px;color:#041827;font-size:12px;font-weight:600;transition:width 0.2s}"
            ".path{font-family:ui-monospace,Menlo,Consolas,monospace;color:#aaa}"
            "a{color:#5fb3ff;text-decoration:none} a:hover{text-decoration:underline}"
            "</style>");

  html += F("<h1>KoClicker Firmware Update</h1>");
  html += F("<div class='card'><strong>Current version:</strong> <span class='path'>");
  html += KoClickerVersion;
  html += F("</span></div>");
  html += F("<div style='font-size:11px;color:#555;margin-bottom:4px'>Uploader v" PAGE_UPDATE_VERSION "</div>");

  html += F("<div class='card'>"
            "<label for='bin'>Select firmware file (.bin)</label>"
            "<input type='file' id='bin' accept='.bin' style='color:#ddd;margin-bottom:4px'>"
            "<div class='row'><button class='btn primary' onclick='upload()'>Upload</button></div>"
            "<div id='bar-wrap'><div id='bar'>0%</div></div>"
            "<div class='status' id='status'></div>"
            "</div>");

  html += F("<div style='margin-top:8px;font-size:12px;color:#aaa'>Back to <a href='/'>index</a></div>"
            "<script>"
            "function upload(){"
            "  const file=document.getElementById('bin').files[0];"
            "  if(!file){alert('Select a .bin file first');return;}"
            "  const st=document.getElementById('status');"
            "  st.className='status';st.textContent='Uploading\u2026';"
            "  let timerStarted=false;"
            "  function startCountdown(){"
            "    let secs=10;"
            "    st.className='status ok';"
            "    st.textContent='Update complete! Redirecting in '+secs+'s\u2026';"
            "    const cd=setInterval(function(){"
            "      secs--;"
            "      if(secs<=0){clearInterval(cd);window.location.href='/';}"
            "      else{st.textContent='Update complete! Redirecting in '+secs+'s\u2026';}"
            "    },1000);"
            "  }"
            "  const xhr=new XMLHttpRequest();"
            "  xhr.open('POST','/update',true);"
            "  xhr.upload.onprogress=function(e){"
            "    if(!e.lengthComputable)return;"
            "    const pct=Math.round(e.loaded/e.total*100);"
            "    document.getElementById('bar-wrap').style.display='block';"
            "    document.getElementById('bar').style.width=pct+'%';"
            "    document.getElementById('bar').textContent=pct+'%';"
            "    if(pct===100&&!timerStarted){"
            "      timerStarted=true;"
            "      st.className='status';let fd=0;"
            "      const fi=setInterval(function(){fd=(fd+1)%4;st.textContent='Flashing firmware'+'.'.repeat(fd+1);},500);"
            "      setTimeout(function(){clearInterval(fi);startCountdown();},5000);"
            "    }"
            "  };"
            "  xhr.onload=function(){"
            "    if(xhr.status!==200&&!timerStarted){st.className='status err';st.textContent='Upload failed: '+xhr.responseText;}"
            "  };"
            "  xhr.onerror=function(){"
            "    if(!timerStarted){st.className='status err';st.textContent='Connection error.';}"
            "  };"
            "  xhr.send(file);"
            "}"
            "</script>");
  return html;
}
