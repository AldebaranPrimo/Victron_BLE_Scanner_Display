#ifndef CONFIG_HTML_H
#define CONFIG_HTML_H

// HTML page for configuration portal
// Uses %PLACEHOLDER% markers for template substitution.
//
// Device rows are NOT hard-coded here: the static template carries a single
// %DEVICE_ROWS% placeholder, and config_portal.cpp::buildDeviceRow() emits one
// <details> block per slot (count follows MAX_DEVICES). This keeps a single
// source of truth for the device count and avoids N-fold HTML copy-paste.
static const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Victron BLE Gateway</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:10px;max-width:480px;margin:0 auto}
h1{color:#00d4aa;font-size:1.3em;text-align:center;margin:10px 0}
h2{color:#00d4aa;font-size:1.1em;margin:15px 0 8px;border-bottom:1px solid #333;padding-bottom:4px}
label{display:block;margin:6px 0 2px;font-size:.85em;color:#aaa}
input,select{width:100%;padding:8px;border:1px solid #444;border-radius:4px;background:#16213e;color:#e0e0e0;font-size:.9em}
input:focus,select:focus{border-color:#00d4aa;outline:none}
.row{display:flex;gap:8px}
.row>*{flex:1}
details{background:#16213e;border:1px solid #333;border-radius:6px;margin:8px 0;padding:8px}
summary{cursor:pointer;color:#00d4aa;font-weight:bold;font-size:.95em}
summary::marker{color:#00d4aa}
.dis details{opacity:.5}
.chk{display:flex;align-items:center;gap:6px;margin:6px 0}
.chk input{width:auto}
.chk label{margin:0}
button{width:100%;padding:12px;background:#00d4aa;color:#1a1a2e;border:none;border-radius:6px;font-size:1em;font-weight:bold;cursor:pointer;margin:15px 0}
button:hover{background:#00b894}
.info{font-size:.75em;color:#666;margin-top:2px}
.msg{background:#00d4aa22;border:1px solid #00d4aa;border-radius:6px;padding:10px;text-align:center;margin:10px 0;display:none}
</style>
</head>
<body>
<h1>Victron BLE Gateway</h1>
<div class="msg" id="msg">Configurazione salvata! Riavvio in corso...</div>
<form method="POST" action="/save" id="frm">

<h2>WiFi</h2>
<label>SSID</label>
<input name="wifi_ssid" value="%WIFI_SSID%" required maxlength="32">
<label>Password</label>
<input type="password" name="wifi_pass" value="%WIFI_PASS%" maxlength="64">

<h2>MQTT</h2>
<label>Broker (IP o hostname)</label>
<input name="mqtt_broker" value="%MQTT_BROKER%" required maxlength="64">
<div class="row">
<div><label>Porta</label>
<input type="number" name="mqtt_port" value="%MQTT_PORT%" min="1" max="65535"></div>
<div><label>Intervallo pub. (s)</label>
<input type="number" name="mqtt_interval" value="%MQTT_INTERVAL%" min="1" max="300"></div>
</div>
<label>Topic base</label>
<input name="mqtt_topic" value="%MQTT_TOPIC%" maxlength="32">
<div class="row">
<div><label>Username</label>
<input name="mqtt_user" value="%MQTT_USER%" maxlength="32"></div>
<div><label>Password</label>
<input type="password" name="mqtt_pass" value="%MQTT_PASS%" maxlength="32"></div>
</div>

<h2>Dispositivi Victron</h2>
<div class="info">Il <b>Nome</b> di ogni dispositivo diventa il segmento del topic MQTT: <code>victron/&lt;nome&gt;/state</code>. Per il primo MPPT lascia <code>mppt</code> (retrocompatibilita). / The device <b>Name</b> is the MQTT topic segment.</div>

%DEVICE_ROWS%

<button type="submit">Salva e Riavvia</button>
</form>
<script>
document.getElementById('frm').addEventListener('submit',function(e){
e.preventDefault();
fetch('/save',{method:'POST',body:new FormData(this)}).then(r=>{
if(r.ok){document.getElementById('msg').style.display='block';
setTimeout(function(){location.reload()},5000);}
});
});
</script>
</body>
</html>
)rawliteral";

#endif
