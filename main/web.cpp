#include "web.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "events.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io.hpp"
#include "mqtt_link.hpp"
#include "ota.hpp"
#include "schedule.hpp"
#include "status.hpp"
#include "wifi.hpp"

namespace web {
namespace {

const char* TAG = "web";

// The whole dashboard: one page, no external files or internet needed.
const char kIndexHtml[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Controller</title>
<style>
 *{box-sizing:border-box}
 body{margin:0;font-family:system-ui,Segoe UI,Arial;background:#0f172a;color:#e2e8f0}
 header{padding:14px 20px;background:#1e293b;display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
 h1{font-size:20px;margin:0} h1 small{font-size:12px;color:#94a3b8;font-weight:400;margin-left:8px}
 h2{font-size:14px;color:#94a3b8;text-transform:uppercase;letter-spacing:.06em;margin:22px 20px 0}
 .dot{width:10px;height:10px;border-radius:50%;background:#ef4444;display:inline-block;margin-right:6px}
 .ok{background:#22c55e!important}
 .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:14px;padding:14px 20px}
 .card{background:#1e293b;border-radius:12px;padding:16px}
 .label{font-size:12px;color:#94a3b8;text-transform:uppercase;letter-spacing:.05em}
 .value{font-size:22px;font-weight:600;margin-top:6px}
 .sub{font-size:13px;color:#94a3b8;margin-top:4px}
 button{margin-top:12px;width:100%;padding:11px;border:0;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;background:#334155;color:#e2e8f0}
 button.on{background:#22c55e;color:#052e16} button.warn{background:#b45309}
 input{width:100%;padding:9px;margin-top:8px;border-radius:8px;border:1px solid #334155;background:#0f172a;color:#e2e8f0;font-size:15px}
 .row{display:flex;gap:8px;align-items:center} .row input[type=checkbox]{width:auto;margin:0}
 #banner{display:none;margin:14px 20px 0;padding:12px 16px;border-radius:10px;background:#b45309}
 ul{list-style:none;margin:0;padding:0;font-family:ui-monospace,Consolas,monospace;font-size:13px;max-height:260px;overflow:auto}
 li{padding:5px 0;border-bottom:1px solid #334155}
 #msg{color:#22c55e;font-size:13px;margin-top:8px;min-height:16px}
 footer{padding:16px 20px 24px;color:#64748b;font-size:12px}
</style></head><body>
<header><h1>Smart Controller<small id="ver"></small></h1>
<span><span id="dot" class="dot"></span><span id="conn">offline</span></span></header>
<div id="banner">Setup mode: board is not on your WiFi yet. Enter your WiFi in <b>Settings</b> below.</div>

<div class="grid">
 <div class="card"><div class="label">Relay 1</div><div class="value" id="r1">-</div><div class="sub" id="s1"></div><button id="b1" onclick="toggle(1)">-</button></div>
 <div class="card"><div class="label">Relay 2</div><div class="value" id="r2">-</div><div class="sub" id="s2"></div><button id="b2" onclick="toggle(2)">-</button></div>
 <div class="card"><div class="label">Inputs</div><div class="value" id="in">-</div><div class="sub">GND on input = active, toggles its relay</div></div>
 <div class="card"><div class="label">Time</div><div class="value" id="time">-</div><div class="sub" id="up"></div></div>
 <div class="card"><div class="label">Network</div><div class="value" id="ip">-</div><div class="sub" id="net"></div></div>
 <div class="card"><div class="label">System</div><div class="value" id="sys">-</div><div class="sub" id="fw"></div></div>
</div>

<h2>Schedules (daily)</h2>
<div class="grid">
 <div class="card"><div class="label">Relay 1 timer</div>
  <div class="row" style="margin-top:10px"><input type="checkbox" id="e1"><label for="e1">Enabled</label></div>
  <div class="row"><input type="time" id="on1"><input type="time" id="off1"></div>
  <div class="sub">ON time &nbsp;/&nbsp; OFF time</div><button onclick="saveSched(1)">Save</button></div>
 <div class="card"><div class="label">Relay 2 timer</div>
  <div class="row" style="margin-top:10px"><input type="checkbox" id="e2"><label for="e2">Enabled</label></div>
  <div class="row"><input type="time" id="on2"><input type="time" id="off2"></div>
  <div class="sub">ON time &nbsp;/&nbsp; OFF time</div><button onclick="saveSched(2)">Save</button></div>
</div>

<h2>Event log</h2>
<div class="grid"><div class="card" style="grid-column:1/-1"><ul id="log"></ul></div></div>

<h2>Settings</h2>
<div class="grid">
 <div class="card"><div class="label">WiFi</div>
  <input id="ssid" placeholder="WiFi name"><input id="pass" type="password" placeholder="Password">
  <button onclick="saveWifi()">Save &amp; reboot</button></div>
 <div class="card"><div class="label">Firmware update (OTA)</div>
  <input type="file" id="bin" accept=".bin"><button onclick="upload()">Upload</button>
  <div class="sub">build\smart_controller.bin</div></div>
 <div class="card"><div class="label">Device</div><div class="sub" id="dev"></div>
  <button class="warn" onclick="reboot()">Reboot</button><div id="msg"></div></div>
</div>
<footer>ESP32-S3 &middot; ESP-IDF &middot; FreeRTOS &middot; REST API: <a style="color:#94a3b8" href="/api/status">/api/status</a></footer>

<script>
let s=null,schedLoaded=false;
const $=id=>document.getElementById(id);
const msg=t=>{$('msg').textContent=t;};
function fmtUp(t){const d=Math.floor(t/86400),h=Math.floor(t%86400/3600),m=Math.floor(t%3600/60);return 'uptime '+(d?d+'d ':'')+h+'h '+m+'m';}
function draw(){
 $('ver').textContent='v'+s.version+' · '+s.id;
 for(let i=0;i<2;i++){const on=s.relays[i],n=i+1,sc=s.schedule[i];
  $('r'+n).textContent=on?'ON':'OFF';$('b'+n).textContent=on?'Turn OFF':'Turn ON';$('b'+n).className=on?'on':'';
  $('s'+n).textContent=sc.enabled?('Timer '+sc.on+' - '+sc.off):'No timer';}
 $('in').textContent=s.inputs.map(v=>v?'ACTIVE':'idle').join(' / ');
 $('time').textContent=s.time.length>8?s.time.slice(11):s.time;
 $('up').textContent=(s.time.length>8?s.time.slice(0,10)+' · ':'')+fmtUp(s.uptime_s);
 $('ip').textContent=s.ap_mode&&s.ip==='0.0.0.0'?'192.168.4.1':s.ip;
 $('net').textContent=(s.ssid||'no WiFi')+' · '+s.rssi+' dBm · MQTT '+(s.mqtt?'online':'offline');
 $('sys').textContent=s.chip_temp_c.toFixed(1)+' °C';
 $('fw').textContent=Math.round(s.free_heap/1024)+' KB free RAM';
 $('dev').textContent='ID '+s.id+' · firmware '+s.version+' · slot '+s.partition;
 $('banner').style.display=s.ap_mode?'block':'none';
 if(!schedLoaded){for(let i=0;i<2;i++){const n=i+1,sc=s.schedule[i];
  $('e'+n).checked=sc.enabled;$('on'+n).value=sc.on;$('off'+n).value=sc.off;}
  if(!$('ssid').value)$('ssid').value=s.ssid;schedLoaded=true;}
}
async function poll(){
 try{const r=await fetch('/api/status');s=await r.json();draw();$('dot').className='dot ok';$('conn').textContent='online';}
 catch(e){$('dot').className='dot';$('conn').textContent='offline';}
}
async function loadLog(){
 try{const r=await fetch('/api/events');const ev=await r.json();
  $('log').innerHTML=ev.map(e=>'<li>'+e.replace(/</g,'&lt;')+'</li>').join('');}catch(e){}
}
async function post(url,body){const r=await fetch(url,{method:'POST',body});if(!r.ok)throw new Error(await r.text());return r;}
async function toggle(ch){if(!s)return;await post('/api/relay?ch='+ch+'&on='+(s.relays[ch-1]?0:1));poll();loadLog();}
async function saveSched(ch){
 const q=new URLSearchParams({ch:ch,en:$('e'+ch).checked?1:0,on:$('on'+ch).value,off:$('off'+ch).value});
 try{await post('/api/schedule?'+q);msg('Relay '+ch+' timer saved');schedLoaded=false;poll();loadLog();}catch(e){msg('Error: '+e.message);}
}
async function saveWifi(){
 if(!$('ssid').value){msg('Enter WiFi name');return;}
 const b=new URLSearchParams({ssid:$('ssid').value,pass:$('pass').value});
 try{await post('/api/wifi',b);msg('Saved. Rebooting... reconnect to your WiFi and open the new IP.');}catch(e){msg('Error: '+e.message);}
}
async function upload(){
 const f=$('bin').files[0];if(!f){msg('Choose a .bin file');return;}
 msg('Uploading '+Math.round(f.size/1024)+' KB...');
 try{const r=await post('/api/ota',f);msg(await r.text());}catch(e){msg('Error: '+e.message);}
}
async function reboot(){if(confirm('Reboot the board?')){await post('/api/reboot');msg('Rebooting...');}}
poll();loadLog();setInterval(poll,1500);setInterval(loadLog,4000);
</script></body></html>)HTML";

// "%3A" -> ':', '+' -> ' ' (browsers encode form values like this)
std::string UrlDecode(const char* in) {
    std::string out;
    for (const char* p = in; *p; ++p) {
        if (*p == '+') {
            out += ' ';
        } else if (*p == '%' && p[1] && p[2]) {
            const char hex[3] = {p[1], p[2], 0};
            out += static_cast<char>(std::strtol(hex, nullptr, 16));
            p += 2;
        } else {
            out += *p;
        }
    }
    return out;
}

//! Read one key from a "a=1&b=2" string (URL query or form body).
bool Param(const char* query, const char* key, std::string& out) {
    char raw[100];
    if (httpd_query_key_value(query, key, raw, sizeof(raw)) != ESP_OK) return false;
    out = UrlDecode(raw);
    return true;
}

bool Query(httpd_req_t* req, char* buf, size_t size) {
    return httpd_req_get_url_query_str(req, buf, size) == ESP_OK;
}

esp_err_t SendJson(httpd_req_t* req, const std::string& json) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t BadRequest(httpd_req_t* req, const char* why) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, why);
    return ESP_FAIL;
}

void RebootTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

// ---- handlers ----------------------------------------------------------

esp_err_t IndexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t StatusHandler(httpd_req_t* req) { return SendJson(req, status::Json()); }

esp_err_t EventsHandler(httpd_req_t* req) { return SendJson(req, events::ToJsonArray()); }

esp_err_t RelayHandler(httpd_req_t* req) {
    char q[64];
    std::string ch;
    std::string on;
    if (!Query(req, q, sizeof(q)) || !Param(q, "ch", ch) || !Param(q, "on", on)) {
        return BadRequest(req, "use ?ch=1&on=1");
    }
    const int channel = std::atoi(ch.c_str());
    if (channel < 1 || channel > io::kRelayCount) return BadRequest(req, "ch must be 1 or 2");
    io::RelaySet(channel - 1, std::atoi(on.c_str()) != 0, "web");
    mqtt::PublishNow();
    return SendJson(req, status::Json());
}

esp_err_t ScheduleHandler(httpd_req_t* req) {
    char q[128];
    std::string ch, en, on, off;
    if (!Query(req, q, sizeof(q)) || !Param(q, "ch", ch) || !Param(q, "en", en) ||
        !Param(q, "on", on) || !Param(q, "off", off)) {
        return BadRequest(req, "use ?ch=1&en=1&on=18:00&off=23:00");
    }
    const int channel = std::atoi(ch.c_str());
    const schedule::Rule rule{std::atoi(en.c_str()) != 0,
                              schedule::ParseHHMM(on.c_str()),
                              schedule::ParseHHMM(off.c_str())};
    if (!schedule::Set(channel - 1, rule)) return BadRequest(req, "invalid channel or time");
    events::Add("Relay %d timer %s %s-%s", channel, rule.enabled ? "ON" : "off",
                on.c_str(), off.c_str());
    return SendJson(req, schedule::ToJson());
}

esp_err_t WifiHandler(httpd_req_t* req) {
    char body[200] = {};
    if (req->content_len >= sizeof(body)) return BadRequest(req, "too long");
    int got = 0;
    while (got < static_cast<int>(req->content_len)) {
        const int n = httpd_req_recv(req, body + got, req->content_len - got);
        if (n <= 0) return BadRequest(req, "read error");
        got += n;
    }
    std::string ssid;
    std::string pass;
    if (!Param(body, "ssid", ssid) || ssid.empty() || ssid.size() > 32) {
        return BadRequest(req, "WiFi name missing");
    }
    Param(body, "pass", pass);
    if (!pass.empty() && pass.size() < 8) return BadRequest(req, "password min 8 characters");
    net::SaveCredentials(ssid, pass);
    httpd_resp_sendstr(req, "Saved, rebooting");
    xTaskCreate(RebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

esp_err_t RebootHandler(httpd_req_t* req) {
    events::Add("Reboot requested from web");
    httpd_resp_sendstr(req, "Rebooting");
    xTaskCreate(RebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

void Register(httpd_handle_t server, const char* uri, httpd_method_t method,
              esp_err_t (*handler)(httpd_req_t*)) {
    httpd_uri_t u{};
    u.uri = uri;
    u.method = method;
    u.handler = handler;
    httpd_register_uri_handler(server, &u);
}

}  // namespace

bool Start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;        // OTA handler uses a 2 KB buffer
    config.lru_purge_enable = true;  // drop old sockets instead of refusing
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Web server failed to start");
        return false;
    }
    Register(server, "/", HTTP_GET, IndexHandler);
    Register(server, "/api/status", HTTP_GET, StatusHandler);
    Register(server, "/api/events", HTTP_GET, EventsHandler);
    Register(server, "/api/relay", HTTP_POST, RelayHandler);
    Register(server, "/api/schedule", HTTP_POST, ScheduleHandler);
    Register(server, "/api/wifi", HTTP_POST, WifiHandler);
    Register(server, "/api/ota", HTTP_POST, ota::UploadHandler);
    Register(server, "/api/reboot", HTTP_POST, RebootHandler);
    ESP_LOGI(TAG, "Web server running on port 80");
    return true;
}

}  // namespace web
