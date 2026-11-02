/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

// 升级页面 HTML（由 WebService 的 /ota 路由直接返回，无需 LittleFS）
// 与 data/ota.html 内容保持一致
static const char OTA_PAGE_HTML[] = R"OTAHTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MYNOVA RFC 固件升级</title>
<style>
  body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:#0f1420;color:#e6e6e6;margin:0;padding:24px;}
  .card{max-width:520px;margin:0 auto;background:#1a2233;border-radius:12px;padding:24px;box-shadow:0 4px 20px rgba(0,0,0,.4);}
  h1{font-size:20px;margin:0 0 4px;}
  .sub{color:#8aa;font-size:13px;margin-bottom:18px;}
  label{display:block;font-size:13px;color:#9fb3c8;margin:14px 0 6px;}
  input[type=text],input[type=file]{width:100%;box-sizing:border-box;padding:10px;border-radius:8px;border:1px solid #2f3b52;background:#0f1420;color:#e6e6e6;}
  button{margin-top:16px;width:100%;padding:12px;border:0;border-radius:8px;background:#2f81f7;color:#fff;font-size:15px;cursor:pointer;}
  button.sec{background:#324a6d;}
  button:disabled{opacity:.5;cursor:not-allowed;}
  .bar{height:14px;background:#0f1420;border-radius:8px;overflow:hidden;margin-top:14px;border:1px solid #2f3b52;}
  .bar > div{height:100%;width:0;background:linear-gradient(90deg,#2f81f7,#39d0d8);transition:width .2s;}
  #status{margin-top:14px;font-size:13px;white-space:pre-wrap;color:#9fb3c8;min-height:18px;}
  .ok{color:#39d0d8;} .err{color:#ff6b6b;}
</style>
</head>
<body>
<div class="card">
  <h1>MYNOVA RFC 固件升级</h1>
  <div class="sub" id="ver">当前版本：加载中…</div>

  <label>访问令牌 (Token)</label>
  <input type="text" id="token" placeholder="与设备一致的 OTA Token">

  <label>本地固件文件 (.bin)</label>
  <input type="file" id="file" accept=".bin">

  <button id="uploadBtn">上传并升级</button>
  <div class="bar"><div id="prog"></div></div>

  <button class="sec" id="checkBtn">检查远程更新</button>
  <div id="status">就绪。</div>
</div>
<script>
const $ = id => document.getElementById(id);
function setStatus(msg, cls){ const s=$('status'); s.textContent=msg; s.className=cls||''; }

async function loadVer(){
  try{
    const r = await fetch('/api/ota/status');
    const j = await r.json();
    $('ver').textContent = '当前版本：V' + (j.version||'?') + '  |  运行分区：' + (j.running||'?');
  }catch(e){ $('ver').textContent='当前版本：未知'; }
}
$('uploadBtn').onclick = ()=>{
  const f = $('file').files[0];
  const tk = $('token').value.trim();
  if(!f){ setStatus('请先选择 .bin 文件','err'); return; }
  if(!tk){ setStatus('请先填写 Token','err'); return; }
  const fd = new FormData();
  fd.append('firmware', f);
  const xhr = new XMLHttpRequest();
  xhr.open('POST','/api/ota/upload?token='+encodeURIComponent(tk));
  xhr.upload.onprogress = e=>{
    if(e.lengthComputable){ const p=e.loaded/e.total*100; $('prog').style.width=p+'%'; setStatus('上传中 '+p.toFixed(0)+'%'); }
  };
  xhr.onload = ()=>{
    if(xhr.status===200){ setStatus('升级成功，设备即将重启…','ok'); }
    else { setStatus('升级失败 ('+xhr.status+')：'+xhr.responseText,'err'); }
  };
  xhr.onerror = ()=> setStatus('网络错误','err');
  setStatus('开始上传…');
  xhr.send(fd);
};
$('checkBtn').onclick = async ()=>{
  const tk = $('token').value.trim();
  if(!tk){ setStatus('请先填写 Token','err'); return; }
  setStatus('正在检查远程更新…');
  try{
    const r = await fetch('/api/ota/check',{method:'POST',headers:{'X-OTA-Token':tk}});
    const j = await r.json();
    if(j.success) setStatus('发现新版本，开始升级，设备将重启…','ok');
    else setStatus('检查结果：'+(j.msg||'未知'), j.msg==='已是最新'?'ok':'err');
  }catch(e){ setStatus('请求失败：'+e.message,'err'); }
};
loadVer();
</script>
</body>
</html>)OTAHTML";
