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
    input,select{
      width:100%;border:1px solid rgba(223,241,255,.13);border-radius:15px;
      padding:14px 15px;color:var(--text);font:15px inherit;background:rgba(3,13,21,.34);
      outline:0;transition:.2s ease;appearance:none;
    }
    input:focus,select:focus{border-color:rgba(98,230,179,.58);box-shadow:0 0 0 4px rgba(98,230,179,.08);background:rgba(3,13,21,.47)}
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
    .auth-gate{position:fixed;z-index:50;inset:0;display:flex;align-items:center;justify-content:center;padding:20px;background:radial-gradient(700px 500px at 50% 10%,rgba(98,230,179,.13),transparent 65%),#07131f}
    .auth-gate[hidden],main[hidden],.confirm[hidden]{display:none!important}.auth-lock{font-size:27px}.auth-gate input{position:relative;margin-top:12px}.auth-gate .message{position:relative}.toggle-row{display:flex;gap:11px;align-items:flex-start;padding:13px 14px;margin-top:15px!important;border:1px solid rgba(223,241,255,.11);border-radius:15px;background:rgba(3,13,21,.3);cursor:pointer}.toggle-row input{width:18px;height:18px;margin:1px 0;accent-color:#65e9b5}.toggle-row span{font-size:12px;line-height:1.45}.toggle-row small{display:block;color:var(--muted);font-weight:400;margin-top:3px}
    .settings-group{margin-top:15px;padding:17px;border:1px solid rgba(223,241,255,.1);border-radius:18px;background:rgba(3,13,21,.22)}
    .settings-title{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:2px}.settings-title strong{font-size:13px}.settings-title small{color:var(--muted);font-size:10px;text-align:right}.settings-group>label:first-of-type{margin-top:13px}
    .lamp-shell{position:relative;overflow:hidden}.lamp-shell:before{content:"";position:absolute;width:150px;height:150px;right:-68px;top:-72px;border-radius:50%;background:radial-gradient(circle,var(--lamp-color,#ffd28a),transparent 68%);opacity:calc(var(--lamp-level,.55)*.32);filter:blur(5px);pointer-events:none}.lamp-controls{position:relative;display:grid;grid-template-columns:1fr 1fr;gap:0 12px}.lamp-controls[hidden]{display:none}.lamp-preview{display:flex;align-items:center;gap:9px;color:var(--muted);font-size:10px}.lamp-dots{display:flex;gap:4px}.lamp-dots i{width:8px;height:8px;border-radius:50%;background:var(--lamp-color,#ffd28a);box-shadow:0 0 11px var(--lamp-color,#ffd28a)}
    input[type=color]{height:47px;padding:6px;cursor:pointer}input[type=color]::-webkit-color-swatch-wrapper{padding:0}input[type=color]::-webkit-color-swatch{border:0;border-radius:10px}
    input[type=range]{height:47px;padding:0;background:transparent;accent-color:var(--pog)}.range-value{color:var(--pog);font:11px ui-monospace,SFMono-Regular,monospace}
    .security-card{padding:22px;margin-top:14px}.security-card .grid{margin-top:4px}.security-card .actions{align-items:center}.security-card .message{flex-basis:100%;margin-top:2px}.danger-action{color:#ffd1d1;border-color:rgba(255,130,130,.18)}
    footer{text-align:center;color:#6f8593;font-size:10px;letter-spacing:.08em;text-transform:uppercase;margin-top:20px}
    @media(max-width:520px){main{padding-top:25px}.brand{margin-bottom:28px}.hero{grid-template-columns:1fr 104px}.climate{height:108px;scale:.76}.glass{border-radius:22px}form{padding:20px}.grid{grid-template-columns:1fr}}
    @media(prefers-reduced-motion:reduce){*,*:before,*:after{animation:none!important;transition:none!important}}
  </style>
</head>
<body>
<div class="auth-gate" id="authGate">
  <div class="dialog">
    <div class="auth-lock" aria-hidden="true">⌁</div>
    <h2 id="authTitle">Sécurisation…</h2>
    <p id="authText">Vérification de l’accès au capteur.</p>
    <input id="adminPassword" type="password" maxlength="128" autocomplete="current-password" placeholder="Mot de passe administrateur">
    <input class="confirm" id="adminPasswordConfirm" type="password" maxlength="128" autocomplete="new-password" placeholder="Confirmer le mot de passe" hidden>
    <div class="message" id="authMessage" role="status"></div>
    <div class="actions"><button class="action emphasis" id="authSubmit" type="button">Continuer</button></div>
  </div>
</div>
<main id="app" hidden>
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
      <section class="settings-group">
        <div class="settings-title"><strong>POG Home</strong><small>Découverte automatique par défaut</small></div>
        <div class="grid">
          <div><label for="poghome">Adresse de secours</label><input id="poghome" name="poghome" placeholder="Détection automatique"></div>
          <div><label for="poghome_port">Port</label><input id="poghome_port" name="poghome_port" type="number" min="1" max="65535" value="8090"></div>
        </div>
      </section>
      <section class="settings-group">
        <div class="settings-title"><strong>Bus environnement</strong><small>BME280 · BME680 · SGP40 · SCD4x…</small></div>
        <div class="grid">
          <div><label for="sda">GPIO SDA</label><input id="sda" name="sda" type="number" min="0" max="48"></div>
          <div><label for="scl">GPIO SCL</label><input id="scl" name="scl" type="number" min="0" max="48"></div>
          <div><label for="period">Mesure toutes les (s)</label><input id="period" name="period" type="number" min="5" max="3600"></div>
          <div><label for="offset">Correction température (°C)</label><input id="offset" name="offset" type="number" min="-10" max="10" step=".1"></div>
        </div>
      </section>
      <section class="settings-group">
        <div class="settings-title"><strong>Radars de présence</strong><small>LD2410B et LD2450 auto-détectés</small></div>
        <div class="grid">
          <div><label for="radar_a_rx">Port A · RX ESP</label><input id="radar_a_rx" name="radar_a_rx" type="number" min="0" max="48"></div>
          <div><label for="radar_a_tx">Port A · TX ESP</label><input id="radar_a_tx" name="radar_a_tx" type="number" min="0" max="48"></div>
          <div><label for="radar_b_rx">Port B · RX ESP</label><input id="radar_b_rx" name="radar_b_rx" type="number" min="0" max="48"></div>
          <div><label for="radar_b_tx">Port B · TX ESP</label><input id="radar_b_tx" name="radar_b_tx" type="number" min="0" max="48"></div>
        </div>
      </section>
      <section class="settings-group lamp-shell" id="lampShell">
        <div class="settings-title"><strong>Lampe témoin</strong><div class="lamp-preview"><span id="lampHardware">4 LED · GPIO 7</span><span class="lamp-dots"><i></i><i></i><i></i><i></i></span></div></div>
        <label class="toggle-row" for="status_light_installed"><input id="status_light_installed" name="status_light_installed" type="checkbox"><span>Lampe adressable installée<small>Active les WS2812B et publie la lumière dans POG Home. Désactivé, aucune entité lumière ne remonte.</small></span></label>
        <div class="lamp-controls" id="lampControls" hidden>
          <label class="toggle-row" for="presence_light_auto" style="grid-column:1/-1"><input id="presence_light_auto" name="presence_light_auto" type="checkbox" checked><span>Allumer sur présence<small>La lampe suit automatiquement les radars connectés.</small></span></label>
          <div><label for="presence_light_color">Couleur de présence</label><input id="presence_light_color" name="presence_light_color" type="color" value="#ffd28a"></div>
          <div><label for="presence_light_brightness">Luminosité · <span class="range-value" id="brightnessValue">55 %</span></label><input id="presence_light_brightness" name="presence_light_brightness" type="range" min="0" max="100" value="55"></div>
          <div style="grid-column:1/-1"><label for="presence_light_hold">Maintien après la dernière présence (s)</label><input id="presence_light_hold" name="presence_light_hold" type="number" min="0" max="300" value="8"></div>
        </div>
      </section>
    </details>

    <button class="primary" id="save">Enregistrer les réglages</button>
    <div class="message" id="message" role="status"></div>
  </form>

  <section class="security-card glass" aria-labelledby="securityTitle">
    <div class="section-head"><h2 id="securityTitle">Accès au dashboard</h2><span class="hint">Session chiffrée localement</span></div>
    <div class="grid">
      <div><label for="currentAdminPassword">Mot de passe actuel</label><input id="currentAdminPassword" type="password" maxlength="128" autocomplete="current-password"></div>
      <div><label for="newAdminPassword">Nouveau mot de passe</label><input id="newAdminPassword" type="password" maxlength="128" minlength="8" autocomplete="new-password"></div>
    </div>
    <label for="confirmNewAdminPassword">Confirmer le nouveau mot de passe</label>
    <input id="confirmNewAdminPassword" type="password" maxlength="128" minlength="8" autocomplete="new-password">
    <div class="actions">
      <button class="action emphasis" id="changeAdminPassword" type="button">Changer le mot de passe</button>
      <button class="action danger-action" id="logout" type="button">Verrouiller maintenant</button>
      <div class="message" id="securityMessage" role="status"></div>
    </div>
  </section>

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
  let authMode="login",appStarted=false,statusLightPin=7;
  function getToken(){try{return sessionStorage.getItem("pogSensorToken")||""}catch(e){return ""}}
  function setToken(token){try{if(token)sessionStorage.setItem("pogSensorToken",token);else sessionStorage.removeItem("pogSensorToken")}catch(e){}}
  function showAuth(setup=false){authMode=setup?"setup":"login";$("authGate").hidden=false;$("app").hidden=true;$("authTitle").textContent=setup?"Protégeons POG Sensor":"POG Sensor verrouillé";$("authText").textContent=setup?"Créez le mot de passe administrateur de cet appareil.":"Saisissez le mot de passe pour accéder au dashboard.";$("adminPassword").value="";$("adminPasswordConfirm").value="";$("adminPasswordConfirm").hidden=!setup;$("authMessage").className="message";$("authSubmit").textContent=setup?"Créer le mot de passe":"Déverrouiller";setTimeout(()=>$("adminPassword").focus(),60)}
  function enterApp(){$("authGate").hidden=true;$("app").hidden=false;if(appStarted)return;appStarted=true;status();scan();pollUpdate(true);setInterval(()=>pollUpdate(true),2500)}
  async function bootstrapAuth(){try{const token=getToken(),headers=token?{"X-Auth-Token":token}:{},r=await fetch("/api/auth/status",{headers,cache:"no-store"}),d=await r.json();if(!d.hasPassword)return showAuth(true);if(d.authed)return enterApp();setToken("");showAuth(false)}catch(e){showAuth(false)}}
  async function submitAuth(){const password=$("adminPassword").value,confirm=$("adminPasswordConfirm").value;if(password.length<8)return authNote("Au moins 8 caractères.");if(authMode==="setup"&&password!==confirm)return authNote("Les mots de passe ne correspondent pas.");$("authSubmit").disabled=true;try{const endpoint=authMode==="setup"?"/api/auth/setup":"/api/auth/login",r=await fetch(endpoint,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({password})}),d=await r.json().catch(()=>({}));if(r.status===429)return authNote("Trop d’essais. Patientez 30 secondes.");if(!r.ok||!d.success)return authNote(authMode==="setup"?(d.error||"Création impossible."):"Mot de passe incorrect.");setToken(d.token);enterApp()}catch(e){authNote("Connexion impossible.")}finally{$("authSubmit").disabled=false}}
  function authNote(text){const target=$("authMessage");target.textContent=text;target.className="message show error"}
  async function authFetch(url,options={}){const headers=new Headers(options.headers||{}),token=getToken();if(token)headers.set("X-Auth-Token",token);const r=await fetch(url,{...options,headers});if(r.status===401){setToken("");showAuth(false)}return r}
  function securityNote(text,type="error"){const target=$("securityMessage");target.textContent=text;target.className=`message show ${type}`}
  function syncLampPanel(){
    const installed=$("status_light_installed").checked,color=$("presence_light_color").value,level=Number($("presence_light_brightness").value)||0;
    $("lampControls").hidden=!installed;$("brightnessValue").textContent=level+" %";
    $("lampShell").style.setProperty("--lamp-color",color);$("lampShell").style.setProperty("--lamp-level",String(level/100));
  }
  async function changeAdminPassword(){
    const current=$("currentAdminPassword").value,next=$("newAdminPassword").value,confirmNext=$("confirmNewAdminPassword").value;
    if(next.length<8)return securityNote("Le nouveau mot de passe doit contenir au moins 8 caractères.");
    if(next!==confirmNext)return securityNote("Les nouveaux mots de passe ne correspondent pas.");
    const button=$("changeAdminPassword");button.disabled=true;
    try{const d=await api("/api/auth/password",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({current_password:current,new_password:next})});setToken(d.token);$("currentAdminPassword").value="";$("newAdminPassword").value="";$("confirmNewAdminPassword").value="";securityNote("Mot de passe modifié. Les autres sessions ont été fermées.","ok")}
    catch(e){securityNote(e.message)}finally{button.disabled=false}
  }
  async function logout(){try{await authFetch("/api/auth/logout",{method:"POST"})}catch(e){}setToken("");showAuth(false)}
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
    try{const r=await authFetch("/api/networks",{cache:"no-store"}),d=await r.json();
      if(d.scanning){setTimeout(()=>{scanning=false;scan()},700);return}
      render(d.networks||[]);$("scan-state").textContent=`${(d.networks||[]).length} disponibles`;
    }catch(e){render([]);$("scan-state").textContent="Saisie manuelle"}finally{scanning=false}
  }
  async function status(){
    try{const d=await (await authFetch("/api/status",{cache:"no-store"})).json();
      $("sensor").textContent=d.sensor+(d.address?` · ${d.address}`:"")+(d.radar&&d.radar!=="aucun radar UART"?` · ${d.radar}`:"");
      $("identity").textContent=d.hw_id+" · "+d.poghome_status;
      $("device").classList.toggle("off",!d.sensor_online);
      for(const k of ["ssid","name","poghome","poghome_port","sda","scl","radar_a_rx","radar_a_tx","radar_b_rx","radar_b_tx","period","offset","presence_light_brightness","presence_light_color","presence_light_hold"])if(d[k]!==undefined)$(k).value=d[k];
      $("status_light_installed").checked=!!d.status_light_installed;
      $("presence_light_auto").checked=!!d.presence_light_auto;
      if(d.status_light_pin!==undefined)statusLightPin=Number(d.status_light_pin);
      if(d.status_light_pin!==undefined&&d.status_light_count!==undefined)$("lampHardware").textContent=`${d.status_light_count} LED · GPIO ${d.status_light_pin}`;
      syncLampPanel();
    }catch(e){}
  }
  async function api(url,options){
    const r=await authFetch(url,options),d=await r.json().catch(()=>({}));
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
    const data=new FormData();data.append("firmware",f);const request=new XMLHttpRequest();request.open("POST","/api/ota");const token=getToken();if(token)request.setRequestHeader("X-Auth-Token",token);$("updateProgress").style.display="block";
    request.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$("updateBar").style.width=p+"%";$("updateProgressText").textContent=`Envoi ${p} %`}};
    request.onload=()=>{let result={};try{result=JSON.parse(request.responseText)}catch(e){}if(request.status===401){setToken("");showAuth(false);return}if(result.ok){$("updateProgressText").textContent="Installé · redémarrage…";setTimeout(()=>location.reload(),8000)}else note(result.error||"La mise à jour a échoué.")};
    request.onerror=()=>note("La connexion a été interrompue.");request.send(data);
  }
  $("reveal").onclick=()=>{const p=$("password"),show=p.type==="password";p.type=show?"text":"password";$("reveal").textContent=show?"Masquer":"Voir"};
  $("updateCheck").onclick=checkUpdate;$("updateInstall").onclick=installUpdate;$("updateModalInstall").onclick=installUpdate;$("updateLater").onclick=dismissUpdate;$("otaInstall").onclick=installFile;
  $("status_light_installed").onchange=syncLampPanel;$("presence_light_color").oninput=syncLampPanel;$("presence_light_brightness").oninput=syncLampPanel;
  $("changeAdminPassword").onclick=changeAdminPassword;$("logout").onclick=logout;
  $("reboot").onclick=async()=>{if(!confirm("Redémarrer POG Sensor ?"))return;try{await api("/api/reboot",{method:"POST"})}catch(e){}};
  form.onsubmit=async e=>{
    e.preventDefault();const ssid=$("ssid").value.trim(),pass=$("password").value;
    if(!ssid)return note("Choisissez un réseau ou saisissez son nom.");
    if(pass&&pass.length<8)return note("Le mot de passe doit contenir au moins 8 caractères.");
    if($("sda").value===$("scl").value)return note("SDA et SCL doivent utiliser deux GPIO différents.");
    const pins=["sda","scl","radar_a_rx","radar_a_tx","radar_b_rx","radar_b_tx"].map(id=>$(id).value);
    if(new Set(pins).size!==pins.length)return note("Chaque liaison matérielle doit utiliser un GPIO différent.");
    if($("status_light_installed").checked&&pins.includes(String(statusLightPin)))return note(`Le GPIO ${statusLightPin} est réservé à la lampe témoin sur cette carte.`);
    $("save").disabled=true;$("save").textContent="Connexion en préparation…";msg.className="message";
    try{const body=new URLSearchParams(new FormData(form));const r=await authFetch("/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});
      const d=await r.json();if(!r.ok)throw new Error(d.error||"Configuration refusée.");
      note("Configuration enregistrée. Le capteur redémarre et rejoint votre réseau.","ok");$("save").textContent="Redémarrage…";
    }catch(err){note(err.message);$("save").disabled=false;$("save").textContent="Enregistrer les réglages"}
  };
  $("authSubmit").onclick=submitAuth;$("adminPassword").onkeydown=e=>{if(e.key==="Enter")submitAuth()};$("adminPasswordConfirm").onkeydown=e=>{if(e.key==="Enter")submitAuth()};syncLampPanel();bootstrapAuth();
</script>
</body>
</html>
)HTML";
