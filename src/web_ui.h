#pragma once

#include <Arduino.h>

static const char kSetupPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#07131f">
  <title>POG Sensor</title>
  <style>
    :root{
      --night:#07131f;--deep:#0b1d2b;--glass:rgba(225,242,255,.095);
      --line:rgba(223,241,255,.14);--text:#f2f8fc;--muted:#9fb2bf;
      --pog:#62e6b3;--ice:#a9d9ff;--iris:#9b8cff;--danger:#ff9d9d;
      --shadow:0 28px 80px rgba(0,0,0,.35);
    }
    *{box-sizing:border-box}
    html{min-height:100%;background:var(--night)}
    body{
      margin:0;min-height:100vh;color:var(--text);
      font-family:-apple-system,BlinkMacSystemFont,"SF Pro Display","Segoe UI",sans-serif;
      background:
        radial-gradient(900px 520px at 15% -10%,rgba(95,230,179,.16),transparent 62%),
        radial-gradient(760px 540px at 105% 30%,rgba(124,145,255,.19),transparent 64%),
        linear-gradient(150deg,var(--night),var(--deep));
      overflow-x:hidden;
    }
    body:before{
      content:"";position:fixed;inset:0;pointer-events:none;opacity:.18;
      background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 180 180' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='.75' numOctaves='3' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='.13'/%3E%3C/svg%3E");
      mix-blend-mode:soft-light;
    }
    main{width:min(680px,100%);margin:auto;padding:38px 18px 64px;position:relative}
    .brand{display:flex;align-items:center;gap:12px;margin:2px 2px 34px}
    .mark{width:38px;height:38px;border-radius:13px;display:grid;place-items:center;
      background:linear-gradient(145deg,rgba(139,255,216,.24),rgba(123,151,255,.16));
      border:1px solid rgba(222,255,245,.18);box-shadow:inset 0 1px rgba(255,255,255,.16)}
    .mark svg{width:22px;height:22px}
    .brand b{font-size:15px;letter-spacing:.01em}.brand span{display:block;color:var(--muted);font-size:11px;margin-top:2px}
    .hero{display:grid;grid-template-columns:1fr 144px;align-items:center;gap:22px;margin:0 2px 28px}
    .eyebrow{color:var(--pog);font-size:11px;font-weight:750;letter-spacing:.14em;text-transform:uppercase}
    h1{font-size:clamp(36px,9vw,58px);line-height:.98;letter-spacing:-.052em;margin:11px 0 14px;font-weight:690}
    .intro{color:#b8c8d2;line-height:1.55;margin:0;font-size:15px;max-width:430px}
    .climate{height:144px;position:relative;display:grid;place-items:center;filter:drop-shadow(0 20px 30px rgba(0,0,0,.3))}
    .orb{position:absolute;border-radius:50%;border:1px solid rgba(255,255,255,.22);backdrop-filter:blur(9px)}
    .orb.temp{width:112px;height:112px;background:linear-gradient(145deg,rgba(255,169,126,.25),rgba(255,111,133,.05));animation:float 7s ease-in-out infinite}
    .orb.hum{width:82px;height:82px;transform:translate(-31px,22px);background:linear-gradient(145deg,rgba(117,211,255,.29),rgba(91,120,255,.05));animation:float 8s ease-in-out -2s infinite}
    .orb.press{width:60px;height:60px;transform:translate(39px,34px);background:linear-gradient(145deg,rgba(114,255,198,.3),rgba(52,207,154,.05));animation:float 6s ease-in-out -1s infinite}
    @keyframes float{50%{translate:0 -6px;scale:1.025}}
    .glass{
      background:linear-gradient(145deg,rgba(231,245,255,.105),rgba(165,208,230,.055));
      border:1px solid var(--line);border-radius:26px;box-shadow:var(--shadow);
      backdrop-filter:blur(24px);-webkit-backdrop-filter:blur(24px);
    }
    .device{padding:16px 18px;display:flex;align-items:center;gap:13px;margin-bottom:14px}
    .dot{width:10px;height:10px;border-radius:50%;background:var(--pog);box-shadow:0 0 18px var(--pog)}
    .device.off .dot{background:#ffc477;box-shadow:0 0 18px rgba(255,196,119,.7)}
    .device strong{font-size:14px}.device small{display:block;color:var(--muted);margin-top:4px;font-size:11px;letter-spacing:.02em}
    form{padding:24px}
    .section-head{display:flex;justify-content:space-between;align-items:end;margin-bottom:16px}
    h2{font-size:19px;letter-spacing:-.025em;margin:0}.hint{font-size:11px;color:var(--muted)}
    .networks{display:grid;gap:8px;min-height:52px;margin-bottom:18px}
    .network{
      width:100%;display:flex;align-items:center;gap:12px;text-align:left;color:var(--text);
      padding:13px 14px;border:1px solid rgba(223,241,255,.1);border-radius:15px;
      background:rgba(4,15,24,.24);cursor:pointer;transition:.2s ease;
    }
    .network:hover,.network:focus-visible{background:rgba(189,231,255,.1);border-color:rgba(153,237,207,.28);outline:0;transform:translateY(-1px)}
    .network.active{border-color:rgba(98,230,179,.62);background:rgba(57,177,133,.12);box-shadow:inset 0 0 0 1px rgba(98,230,179,.08)}
    .wifi{width:19px;height:19px;color:var(--ice);flex:none}.network-name{font-size:14px;font-weight:600;flex:1;overflow:hidden;text-overflow:ellipsis}
    .lock{font-size:10px;color:var(--muted)}.signal{font:10px ui-monospace,SFMono-Regular,monospace;color:var(--muted)}
    .skeleton{height:48px;border-radius:15px;background:linear-gradient(100deg,rgba(255,255,255,.04) 20%,rgba(255,255,255,.11) 40%,rgba(255,255,255,.04) 60%);background-size:220% 100%;animation:shine 1.4s infinite}
    @keyframes shine{to{background-position-x:-220%}}
    label{display:block;color:#c7d4dc;font-size:12px;font-weight:600;margin:16px 2px 7px}
    input{
      width:100%;border:1px solid rgba(223,241,255,.13);border-radius:15px;
      padding:14px 15px;color:var(--text);font:15px inherit;background:rgba(3,13,21,.34);
      outline:0;transition:.2s ease;appearance:none;
    }
    input:focus{border-color:rgba(98,230,179,.58);box-shadow:0 0 0 4px rgba(98,230,179,.08);background:rgba(3,13,21,.47)}
    input::placeholder{color:#6e818e}
    .password{position:relative}.password input{padding-right:66px}.reveal{position:absolute;right:6px;top:6px;height:34px;border:0;border-radius:10px;background:rgba(255,255,255,.07);color:#b9cad4;padding:0 10px;cursor:pointer}
    details{margin-top:19px;border-top:1px solid rgba(223,241,255,.1);padding-top:16px}
    summary{cursor:pointer;color:#b9c9d3;font-size:12px;font-weight:650;list-style:none}
    summary::-webkit-details-marker{display:none}summary:after{content:"＋";float:right;color:var(--pog)}
    details[open] summary:after{content:"−"}.grid{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
    .primary{
      width:100%;border:0;border-radius:16px;margin-top:22px;padding:15px 18px;
      background:linear-gradient(120deg,#6af0bd,#7ce0c7);color:#062117;
      font:700 15px inherit;cursor:pointer;box-shadow:0 12px 34px rgba(44,215,154,.21);
      transition:transform .2s ease,filter .2s ease;
    }
    .primary:hover{filter:brightness(1.05);transform:translateY(-1px)}.primary:active{transform:scale(.99)}
    .primary:disabled{opacity:.55;cursor:wait;transform:none}
    .message{display:none;margin-top:14px;padding:12px 14px;border-radius:13px;font-size:12px;line-height:1.45}
    .message.show{display:block}.message.error{color:#ffd0d0;background:rgba(255,106,106,.1);border:1px solid rgba(255,140,140,.18)}
    .message.ok{color:#c9ffec;background:rgba(75,220,163,.1);border:1px solid rgba(98,230,179,.2)}
    .update-card{padding:22px;margin-top:14px;position:relative;overflow:hidden}
    .update-card:after{content:"";position:absolute;width:180px;height:180px;right:-92px;top:-110px;border-radius:50%;background:radial-gradient(circle,rgba(155,140,255,.22),transparent 70%);pointer-events:none}
    .update-top{position:relative;z-index:1;display:flex;align-items:center;gap:13px}
    .update-orb{width:42px;height:42px;flex:none;display:grid;place-items:center;border-radius:14px;background:linear-gradient(145deg,rgba(98,230,179,.2),rgba(155,140,255,.16));border:1px solid rgba(255,255,255,.13);color:var(--pog);font-size:20px}
    .update-copy{min-width:0;flex:1}.update-copy strong{font-size:14px}.update-copy small{display:block;color:var(--muted);font-size:11px;line-height:1.45;margin-top:4px}
    .version{align-self:flex-start;padding:5px 8px;border:1px solid var(--line);border-radius:999px;color:var(--muted);background:rgba(0,0,0,.18);font:10px ui-monospace,SFMono-Regular,monospace}
    .actions{position:relative;z-index:1;display:flex;flex-wrap:wrap;gap:8px;margin-top:16px}
    .action{border:1px solid rgba(223,241,255,.13);border-radius:13px;padding:10px 13px;color:var(--text);background:rgba(255,255,255,.065);font:650 12px inherit;cursor:pointer;text-decoration:none}
    .action.emphasis{border:0;color:#062117;background:linear-gradient(120deg,#6af0bd,#7ce0c7)}
    .action:hover{filter:brightness(1.12)}.action:disabled{opacity:.45;cursor:wait}
    .progress{display:none;margin-top:15px}.track{height:5px;overflow:hidden;border-radius:999px;background:rgba(255,255,255,.08)}.bar{width:0;height:100%;border-radius:inherit;background:linear-gradient(90deg,var(--pog),var(--ice));transition:width .25s ease}.progress small{display:block;color:var(--muted);font-size:10px;margin-top:7px}
    .manual{position:relative;z-index:1}.manual input{margin-top:12px;font-size:12px;padding:11px}.manual .action{margin-top:9px}
    .modal{display:none;position:fixed;z-index:20;inset:0;align-items:center;justify-content:center;padding:20px;background:rgba(3,9,15,.78);backdrop-filter:blur(22px);-webkit-backdrop-filter:blur(22px)}.modal.show{display:flex}
    .dialog{position:relative;overflow:hidden;width:min(420px,100%);padding:28px;border:1px solid rgba(255,255,255,.17);border-radius:28px;background:linear-gradient(155deg,rgba(28,47,59,.98),rgba(7,19,31,.98));box-shadow:0 35px 100px rgba(0,0,0,.6)}.dialog:before{content:"";position:absolute;width:230px;height:230px;right:-120px;top:-140px;border-radius:50%;background:radial-gradient(circle,rgba(155,140,255,.3),rgba(98,230,179,.08) 50%,transparent 70%)}.dialog h2{position:relative;margin:20px 0 9px;font-size:27px}.dialog p{position:relative;color:var(--muted);font-size:13px;line-height:1.55}.dialog .actions{margin-top:23px}
    footer{text-align:center;color:#6f8593;font-size:10px;letter-spacing:.08em;text-transform:uppercase;margin-top:20px}
    @media(max-width:520px){main{padding-top:25px}.brand{margin-bottom:28px}.hero{grid-template-columns:1fr 104px}.climate{height:108px;scale:.76}.glass{border-radius:22px}form{padding:20px}.grid{grid-template-columns:1fr}}
    @media(prefers-reduced-motion:reduce){*,*:before,*:after{animation:none!important;transition:none!important}}
  </style>
</head>
<body>
<main>
  <div class="brand">
    <div class="mark"><svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3.3a8.7 8.7 0 1 0 8.7 8.7" stroke="#65e9b5" stroke-width="2.2" stroke-linecap="round"/><circle cx="17.8" cy="6.1" r="2.3" fill="#a8d8ff"/></svg></div>
    <div><b>POG Sensor</b><span>Un climat qui reste chez vous.</span></div>
  </div>

  <section class="hero">
    <div>
      <div class="eyebrow">Première respiration</div>
      <h1>Relions<br>la pièce.</h1>
      <p class="intro">Choisissez le réseau de la maison. Le capteur rejoindra ensuite POG Home automatiquement.</p>
    </div>
    <div class="climate" aria-hidden="true"><i class="orb temp"></i><i class="orb hum"></i><i class="orb press"></i></div>
  </section>

  <div class="device glass off" id="device">
    <i class="dot"></i>
    <div><strong id="sensor">Recherche du capteur…</strong><small id="identity">Préparation de l’appareil</small></div>
  </div>

  <form class="glass" id="setup">
    <div class="section-head"><h2>Réseau Wi‑Fi</h2><span class="hint" id="scan-state">Recherche…</span></div>
    <div class="networks" id="networks"><div class="skeleton"></div><div class="skeleton"></div></div>
    <label for="ssid">Nom du réseau</label>
    <input id="ssid" name="ssid" maxlength="32" autocomplete="off" placeholder="Votre réseau">
    <label for="password">Mot de passe</label>
    <div class="password"><input id="password" name="password" type="password" maxlength="64" autocomplete="current-password" placeholder="8 caractères minimum"><button class="reveal" type="button" id="reveal">Voir</button></div>
    <label for="name">Nom dans POG Home</label>
    <input id="name" name="name" maxlength="48" value="Capteur POG" placeholder="Ex. Climat du salon">

    <details>
      <summary>Réglages avancés</summary>
      <label for="poghome">Adresse POG Home de secours</label>
      <input id="poghome" name="poghome" placeholder="Détection automatique">
      <div class="grid">
        <div><label for="sda">GPIO SDA</label><input id="sda" name="sda" type="number" min="0" max="48"></div>
        <div><label for="scl">GPIO SCL</label><input id="scl" name="scl" type="number" min="0" max="48"></div>
        <div><label for="period">Mesure toutes les</label><input id="period" name="period" type="number" min="5" max="3600"></div>
        <div><label for="offset">Correction °C</label><input id="offset" name="offset" type="number" min="-10" max="10" step=".1"></div>
      </div>
    </details>

    <button class="primary" id="save">Connecter le capteur</button>
    <div class="message" id="message" role="status"></div>
  </form>

  <section class="update-card glass" aria-labelledby="updateTitle">
    <div class="update-top">
      <div class="update-orb" aria-hidden="true">↓</div>
      <div class="update-copy"><strong id="updateTitle">Préparation des mises à jour…</strong><small id="updateText">POG Sensor vérifie les releases officielles lorsque le Wi‑Fi est disponible.</small></div>
      <span class="version" id="updateVersion">v—</span>
    </div>
    <div class="actions">
      <button class="action emphasis" id="updateInstall" type="button" style="display:none">Mettre à jour</button>
      <button class="action" id="updateCheck" type="button">Vérifier</button>
      <a class="action" id="updateRelease" href="#" target="_blank" rel="noopener" style="display:none">Notes</a>
      <button class="action" id="reboot" type="button">Redémarrer</button>
    </div>
    <div class="progress" id="updateProgress"><div class="track"><div class="bar" id="updateBar"></div></div><small id="updateProgressText"></small></div>
    <details class="manual">
      <summary>Installation manuelle d’un firmware .bin</summary>
      <input type="file" id="otaFile" accept=".bin">
      <button class="action" id="otaInstall" type="button">Installer le fichier</button>
    </details>
  </section>
  <footer>POG · Local par conception</footer>
</main>
<div class="modal" id="updateModal" role="dialog" aria-modal="true" aria-labelledby="updateModalTitle">
  <div class="dialog">
    <div class="update-orb" aria-hidden="true">↓</div>
    <h2 id="updateModalTitle">Une mise à jour est disponible.</h2>
    <p id="updateModalText">Une nouvelle version officielle de POG Sensor peut être installée.</p>
    <div class="actions"><button class="action" id="updateLater" type="button">Plus tard</button><button class="action emphasis" id="updateModalInstall" type="button">Installer</button></div>
  </div>
</div>
<script>
  const $=id=>document.getElementById(id), networks=$("networks"), form=$("setup"), msg=$("message");
  const wifiIcon=`<svg class="wifi" viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M3.5 9.4a13 13 0 0 1 17 0M6.8 13a8 8 0 0 1 10.4 0M10.2 16.5a2.8 2.8 0 0 1 3.6 0" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/><circle cx="12" cy="19" r="1.1" fill="currentColor"/></svg>`;
  let scanning=false;
  function note(text,type="error"){msg.textContent=text;msg.className=`message show ${type}`}
  function choose(ssid,button){$("ssid").value=ssid;document.querySelectorAll(".network").forEach(x=>x.classList.remove("active"));button?.classList.add("active");$("password").focus()}
  function render(list){
    networks.innerHTML="";
    if(!list.length){networks.innerHTML=`<div class="hint">Aucun réseau détecté. Vous pouvez saisir son nom.</div>`;return}
    list.forEach(n=>{const b=document.createElement("button");b.type="button";b.className="network";
      b.innerHTML=wifiIcon+`<span class="network-name"></span><span class="signal">${n.rssi} dBm</span><span class="lock">${n.secure?"FERMÉ":"OUVERT"}</span>`;
      b.querySelector(".network-name").textContent=n.ssid;b.onclick=()=>choose(n.ssid,b);networks.appendChild(b)})
  }
  async function scan(){
    if(scanning)return;scanning=true;$("scan-state").textContent="Recherche…";
    try{const r=await fetch("/api/networks",{cache:"no-store"}),d=await r.json();
      if(d.scanning){setTimeout(()=>{scanning=false;scan()},700);return}
      render(d.networks||[]);$("scan-state").textContent=`${(d.networks||[]).length} disponibles`;
    }catch(e){render([]);$("scan-state").textContent="Saisie manuelle"}finally{scanning=false}
  }
  async function status(){
    try{const d=await (await fetch("/api/status",{cache:"no-store"})).json();
      $("sensor").textContent=d.sensor+(d.address?` · ${d.address}`:"");
      $("identity").textContent=d.hw_id+" · "+d.poghome_status;
      $("device").classList.toggle("off",!d.sensor_online);
      for(const k of ["ssid","name","poghome","sda","scl","period","offset"])if(d[k]!==undefined)$(k).value=d[k];
    }catch(e){}
  }
  async function api(url,options){
    const r=await fetch(url,options),d=await r.json().catch(()=>({}));
    if(!r.ok)throw new Error(d.error||`HTTP ${r.status}`);
    return d;
  }
  let lastUpdate=null,updateCheckStarted=false;
  function dismissedVersion(){try{return localStorage.getItem("pogsensorUpdateLater")||""}catch(e){return ""}}
  function dismissUpdate(){if(lastUpdate?.latestVersion)try{localStorage.setItem("pogsensorUpdateLater",lastUpdate.latestVersion)}catch(e){}$("updateModal").classList.remove("show")}
  function renderUpdate(u,prompt=true){
    lastUpdate=u;const busy=["checking","downloading","verifying"].includes(u.phase);
    $("updateVersion").textContent="v"+(u.currentVersion||"—");
    $("updateCheck").disabled=busy;
    $("updateInstall").style.display=u.updateAvailable&&!busy?"inline-flex":"none";
    $("updateRelease").style.display=u.releaseUrl?"inline-flex":"none";
    if(u.releaseUrl)$("updateRelease").href=u.releaseUrl;
    if(u.phase==="checking"){
      $("updateTitle").textContent="Recherche de la dernière release…";$("updateText").textContent="Connexion sécurisée à POG-Projects sur GitHub.";
    }else if(u.phase==="available"){
      $("updateTitle").textContent=`POG Sensor ${u.latestVersion} est disponible`;$("updateText").textContent=`Version installée ${u.currentVersion} · firmware ${u.board} vérifié avant activation.`;
    }else if(u.phase==="downloading"){
      $("updateTitle").textContent="Téléchargement sécurisé…";$("updateText").textContent="Gardez le capteur alimenté pendant l’installation.";
    }else if(u.phase==="verifying"){
      $("updateTitle").textContent="Vérification du firmware…";$("updateText").textContent="Contrôle SHA‑256 avant redémarrage.";
    }else if(u.phase==="error"){
      $("updateTitle").textContent="Vérification impossible";$("updateText").textContent=u.error||"La release GitHub n’est pas accessible.";
    }else if(u.checked){
      $("updateTitle").textContent="POG Sensor est à jour";$("updateText").textContent=`Dernière release officielle : ${u.latestVersion||u.currentVersion}.`;
    }else{
      $("updateTitle").textContent="Mises à jour automatiques";$("updateText").textContent="Une vérification sera lancée dès que le réseau sera prêt.";
    }
    const moving=u.phase==="downloading"||u.phase==="verifying";
    $("updateProgress").style.display=moving?"block":"none";
    $("updateBar").style.width=(u.progress||0)+"%";
    $("updateProgressText").textContent=u.phase==="verifying"?"Empreinte vérifiée · activation…":`Téléchargement ${u.progress||0} %`;
    if(prompt&&u.updateAvailable&&u.phase==="available"&&dismissedVersion()!==u.latestVersion){
      $("updateModalTitle").textContent=`POG Sensor ${u.latestVersion} est disponible.`;
      $("updateModalText").textContent=`La version ${u.currentVersion} est installée. Le firmware officiel correspondant à cette carte est prêt.`;
      $("updateModal").classList.add("show");
    }
  }
  async function pollUpdate(prompt=true){
    try{const u=await api("/api/update");renderUpdate(u,prompt);
      if(u.phase==="idle"&&!updateCheckStarted){updateCheckStarted=true;await api("/api/update/check",{method:"POST"})}
    }catch(e){}
  }
  async function checkUpdate(){
    updateCheckStarted=true;
    renderUpdate({...lastUpdate,phase:"checking",currentVersion:lastUpdate?.currentVersion||"—"},false);
    try{await api("/api/update/check",{method:"POST"});setTimeout(()=>pollUpdate(false),500)}catch(e){note(e.message)}
  }
  async function installUpdate(){
    if(!lastUpdate?.updateAvailable)return;$("updateModal").classList.remove("show");$("updateInstall").disabled=true;
    $("updateProgress").style.display="block";$("updateProgressText").textContent="Préparation du téléchargement…";
    try{await api("/api/update/install",{method:"POST"})}catch(e){$("updateInstall").disabled=false;note(e.message)}
  }
  function installFile(){
    const f=$("otaFile").files[0];if(!f)return note("Choisissez un firmware .bin.");
    const data=new FormData();data.append("firmware",f);const request=new XMLHttpRequest();request.open("POST","/api/ota");$("updateProgress").style.display="block";
    request.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$("updateBar").style.width=p+"%";$("updateProgressText").textContent=`Envoi ${p} %`}};
    request.onload=()=>{let result={};try{result=JSON.parse(request.responseText)}catch(e){}if(result.ok){$("updateProgressText").textContent="Installé · redémarrage…";setTimeout(()=>location.reload(),8000)}else note(result.error||"La mise à jour a échoué.")};
    request.onerror=()=>note("La connexion a été interrompue.");request.send(data);
  }
  $("reveal").onclick=()=>{const p=$("password"),show=p.type==="password";p.type=show?"text":"password";$("reveal").textContent=show?"Masquer":"Voir"};
  $("updateCheck").onclick=checkUpdate;$("updateInstall").onclick=installUpdate;$("updateModalInstall").onclick=installUpdate;$("updateLater").onclick=dismissUpdate;$("otaInstall").onclick=installFile;
  $("reboot").onclick=async()=>{if(!confirm("Redémarrer POG Sensor ?"))return;try{await api("/api/reboot",{method:"POST"})}catch(e){}};
  form.onsubmit=async e=>{
    e.preventDefault();const ssid=$("ssid").value.trim(),pass=$("password").value;
    if(!ssid)return note("Choisissez un réseau ou saisissez son nom.");
    if(pass&&pass.length<8)return note("Le mot de passe doit contenir au moins 8 caractères.");
    if($("sda").value===$("scl").value)return note("SDA et SCL doivent utiliser deux GPIO différents.");
    $("save").disabled=true;$("save").textContent="Connexion en préparation…";msg.className="message";
    try{const body=new URLSearchParams(new FormData(form));const r=await fetch("/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
      const d=await r.json();if(!r.ok)throw new Error(d.error||"Configuration refusée.");
      note("Configuration enregistrée. Le capteur redémarre et rejoint votre réseau.","ok");$("save").textContent="Redémarrage…";
    }catch(err){note(err.message);$("save").disabled=false;$("save").textContent="Connecter le capteur"}
  };
  status();scan();pollUpdate(true);setInterval(()=>pollUpdate(true),2500);
</script>
</body>
</html>
)HTML";
