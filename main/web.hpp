#pragma once
// Small web server on port 80:
//   GET  /                     -> dashboard page (HTML + JS)
//   GET  /api/status           -> JSON with all live values
//   POST /api/relay?ch=1&on=1  -> switch a relay (ch 1..2, on 0/1)

namespace web {
bool Start();
}
