#include "web.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "io.hpp"
#include "time_sync.hpp"
#include "wifi.hpp"

namespace web {
namespace {

const char* TAG = "web";

// The whole dashboard: one page, no external files or internet needed.
// JavaScript asks /api/status every second and redraws the cards.
const char kIndexHtml[] = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Controller</title>
<style>
 body{margin:0;font-family:system-ui,Segoe UI,Arial;background:#0f172a;color:#e2e8f0}
 header{padding:16px 20px;background:#1e293b;display:flex;justify-content:space-between;align-items:center}
 h1{font-size:20px;margin:0} .dot{width:10px;height:10px;border-radius:50%;background:#ef4444;display:inline-block;margin-right:6px}
 .ok{background:#22c55e!important}
 main{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px;padding:18px}
 .card{background:#1e293b;border-radius:12px;padding:16px}
 .label{font-size:12px;color:#94a3b8;text-transform:uppercase;letter-spacing:.05em}
 .value{font-size:24px;font-weight:600;margin-top:6px}
 button{margin-top:12px;width:100%;padding:12px;border:0;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;background:#334155;color:#e2e8f0}
 button.on{background:#22c55e;color:#052e16}
 footer{padding:0 20px 20px;color:#64748b;font-size:12px}
</style></head><body>
<header><h1>Smart Controller</h1><span><span id="dot" class="dot"></span><span id="conn">offline</span></span></header>
<main>
 <div class="card"><div class="label">Relay 1</div><div class="value" id="r1">-</div><button id="b1" onclick="toggle(1)">-</button></div>
 <div class="card"><div class="label">Relay 2</div><div class="value" id="r2">-</div><button id="b2" onclick="toggle(2)">-</button></div>
 <div class="card"><div class="label">Input 1 / Input 2</div><div class="value" id="in">-</div></div>
 <div class="card"><div class="label">Time</div><div class="value" id="time">-</div></div>
 <div class="card"><div class="label">WiFi</div><div class="value" id="wifi">-</div></div>
 <div class="card"><div class="label">Chip temp / Free RAM</div><div class="value" id="sys">-</div></div>
</main>
<footer>ESP32-S3 &middot; ESP-IDF &middot; uptime <span id="up">-</span></footer>
<script>
let s={relays:[false,false]};
const $=id=>document.getElementById(id);
function fmtUp(t){const h=Math.floor(t/3600),m=Math.floor(t%3600/60);return h+'h '+m+'m '+(t%60)+'s';}
function draw(){
 for(let i=0;i<2;i++){const on=s.relays[i];$('r'+(i+1)).textContent=on?'ON':'OFF';
  const b=$('b'+(i+1));b.textContent=on?'Turn OFF':'Turn ON';b.className=on?'on':'';}
 $('in').textContent=s.inputs.map(v=>v?'ACTIVE':'idle').join(' / ');
 $('time').textContent=s.time;
 $('wifi').textContent=s.ip+'  ('+s.rssi+' dBm)';
 $('sys').textContent=s.chip_temp_c.toFixed(1)+' C / '+Math.round(s.free_heap/1024)+' KB';
 $('up').textContent=fmtUp(s.uptime_s);
}
async function poll(){
 try{const r=await fetch('/api/status');s=await r.json();draw();
  $('dot').className='dot ok';$('conn').textContent='online';}
 catch(e){$('dot').className='dot';$('conn').textContent='offline';}
}
async function toggle(ch){
 const on=s.relays[ch-1]?0:1;
 await fetch('/api/relay?ch='+ch+'&on='+on,{method:'POST'});poll();
}
poll();setInterval(poll,1000);
</script></body></html>)HTML";

esp_err_t IndexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t StatusHandler(httpd_req_t* req) {
    char json[512];
    std::snprintf(
        json, sizeof(json),
        "{\"ip\":\"%s\",\"rssi\":%d,\"time\":\"%s\",\"uptime_s\":%" PRId64
        ",\"free_heap\":%" PRIu32 ",\"chip_temp_c\":%.1f,"
        "\"relays\":[%s,%s],\"inputs\":[%s,%s]}",
        net::WifiIp().c_str(), net::WifiRssi(), clk::TimeNow().c_str(),
        esp_timer_get_time() / 1000000, esp_get_free_heap_size(),
        io::ChipTempC(), io::RelayGet(0) ? "true" : "false",
        io::RelayGet(1) ? "true" : "false",
        io::InputActive(0) ? "true" : "false",
        io::InputActive(1) ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t RelayHandler(httpd_req_t* req) {
    char query[64];
    char ch[8];
    char on[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "ch", ch, sizeof(ch)) != ESP_OK ||
        httpd_query_key_value(query, "on", on, sizeof(on)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "use ?ch=1&on=1");
        return ESP_FAIL;
    }
    const int channel = std::atoi(ch);
    if (channel < 1 || channel > io::kRelayCount) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ch must be 1 or 2");
        return ESP_FAIL;
    }
    io::RelaySet(channel - 1, std::atoi(on) != 0);
    return StatusHandler(req);  // answer with the new state
}

}  // namespace

bool Start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Web server failed to start");
        return false;
    }
    httpd_uri_t index{};
    index.uri = "/";
    index.method = HTTP_GET;
    index.handler = IndexHandler;
    httpd_register_uri_handler(server, &index);

    httpd_uri_t status{};
    status.uri = "/api/status";
    status.method = HTTP_GET;
    status.handler = StatusHandler;
    httpd_register_uri_handler(server, &status);

    httpd_uri_t relay{};
    relay.uri = "/api/relay";
    relay.method = HTTP_POST;
    relay.handler = RelayHandler;
    httpd_register_uri_handler(server, &relay);

    ESP_LOGI(TAG, "Web server running on port 80");
    return true;
}

}  // namespace web
