#include "Network.h"
#include <WiFi.h>
#include <Preferences.h>
#include <atomic>
#include <WebServer.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <errno.h>
#include <esp_task_wdt.h>
#include "secrets.h"
#include "identity.h"
#include "Page.h"
#include "../telemetry/Bridge.h"
namespace {
std::atomic<int> disconnectReason{-1};
WebServer web(80);
String token;
uint32_t lease = 0;
int serverFd = -1, clientFd = -1;
bool logConnected = false;
uint32_t httpMaxUs = 0;
bool leaseExpired = false;
// 会话令牌仅用于运行时控制租约；有效期1500ms，不是固件内置密码。
bool authorized() {
    return token.length() && web.arg("token") == token && uint32_t(millis() - lease) < 1500;
}
// 网络核所有者：HTTP、NVS配置、TCP发送；只经桥传命令/读遥测，不执行控制算法。
void task(void *) {
    Serial.println("[BOOT] network task; wifi begin");
    esp_task_wdt_add(nullptr);
    // 沿用原固件保存的配置，禁止用默认凭据覆盖用户NVS；秘密不输出。
    Preferences prefs;
    String ssid = STA_SSID, pass = STA_PASS;
    bool saved = false;
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
            disconnectReason.store(info.wifi_sta_disconnected.reason);
    });
    bool ap = WiFi.softAP(AP_SSID, AP_PSW);
    Serial.printf("[BOOT] ap ready=%d ip=%s\n", ap, WiFi.softAPIP().toString().c_str());
    if (prefs.begin("wifi_cfg", true)) {
        if (prefs.isKey("ssid")) {
            saved = true;
            ssid = prefs.getString("ssid", "");
            pass = prefs.getString("pass", "");
            if (prefs.getBool("static", false)) {
                IPAddress ip, gw, sn, dns;
                if (ip.fromString(prefs.getString("ip", "")) &&
                    gw.fromString(prefs.getString("gw", "")) &&
                    sn.fromString(prefs.getString("sn", ""))) {
                    if (!dns.fromString(prefs.getString("dns", "")))
                        dns = gw;
                    Serial.printf("[BOOT] static ip accepted=%d\n", WiFi.config(ip, gw, sn, dns));
                } else
                    Serial.println("[BOOT] invalid saved static ip; DHCP");
            }
        }
        prefs.end();
    }
    Serial.printf("[BOOT] sta config=%s enabled=%d\n", saved ? "saved" : "default",
                  ssid.length() > 0);
    if (ssid.length())
        WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("[BOOT] wifi begin returned");
    web.on("/", HTTP_GET, [] { web.send_P(200, "text/html; charset=utf-8", page); });
    web.on("/claim", HTTP_POST, [] {
        if (token.length() && uint32_t(millis() - lease) < 1500) {
            bridge::event(2);
            web.send(409, "text/plain", "owner active");
            return;
        }
        token = String(esp_random(), HEX) + String(esp_random(), HEX);
        lease = millis();
        leaseExpired = false;
        bridge::beat();
        bridge::event(1);
        web.send(200, "text/plain", token);
    });
    web.on("/heartbeat", HTTP_POST, [] {
        if (!authorized()) {
            bridge::event(3);
            web.send(403, "text/plain", "session expired");
            return;
        }
        lease = millis();
        bridge::beat();
        web.send(200, "text/plain", "ok");
    });
    web.on("/command", HTTP_POST, [] {
        if (!authorized()) {
            bridge::event(6, 403);
            web.send(403, "text/plain", "session expired");
            return;
        }
        String c = web.arg("code");
        if (c.length() != 1 || c[0] < '0' || c[0] > '4') {
            bridge::event(6, 400);
            web.send(400, "text/plain", "bad command");
            return;
        }
        bool ok = bridge::command(c[0] - '0');
        bridge::event(ok ? 5 : 6, c[0] - '0');
        web.send(ok ? 202 : 503, "text/plain",
                 ok ? "queued; inspect actual mode" : "queue full; stop latched");
    });
    web.on("/status", HTTP_GET, [] {
        auto s = bridge::status();
        String j = "{\"mode\":";
        j += s.frame.mode[0];
        j += ",\"reason\":";
        j += s.frame.reason[0];
        j += ",\"voltage\":";
        j += s.frame.sensors[6];
        j += ",\"percent_estimate\":";
        j += s.percent;
        j += ",\"age_ms\":";
        j += uint32_t(micros() - s.frame.time_us[0]) / 1000;
        j += ",\"drops\":";
        j += bridge::drops();
        j += ",\"log_connected\":";
        j += logConnected ? "true" : "false";
        j += ",\"model\":\"" MODEL_SHA "\",\"firmware\":\"" FIRMWARE_SHA
             "\",\"model_identity\":" MODEL_IDENTITY_JSON "}";
        web.send(200, "application/json", j);
    });
    // 只读固定环；最多32条，整数源时钟；非有限观测编码为null。
    web.on("/diagnostics", HTTP_GET, [] {
        uint32_t total = bridge::eventTotal(), cursor = 0;
        String j = "{\"total\":" + String(total) +
                   ",\"overwritten\":" + String(total > 32 ? total - 32 : 0) +
                   ",\"http_max_us\":" + String(httpMaxUs) + ",\"events\":[";
        bridge::Event e;
        unsigned count = 0;
        auto number = [](float v) { return isfinite(v) ? String(v, 5) : String("null"); };
        while (count < 32 && bridge::eventAfter(cursor, e) && e.id <= total) {
            if (count++)
                j += ",";
            cursor = e.id;
            j += "{\"id\":" + String(e.id) + ",\"at_ms\":" + String(e.atMs) +
                 ",\"source_us\":" + String(e.sourceUs) + ",\"kind\":" + String(e.kind) +
                 ",\"detail\":" + String(e.detail) + ",\"mode\":" + String(e.mode) +
                 ",\"reason\":" + String(e.reason) + ",\"cycle_us\":" + String(e.cycleUs) +
                 ",\"pitch\":" + number(e.pitch) + ",\"roll\":" + number(e.roll) +
                 ",\"accZ\":" + number(e.accZ) + ",\"battery\":" + number(e.battery) + "}";
        }
        auto first = bridge::firstFault();
        j += "],\"first_fault\":{\"id\":" + String(first.id) + ",\"at_ms\":" + String(first.atMs) +
             ",\"source_us\":" + String(first.sourceUs) + ",\"reason\":" + String(first.reason) +
             ",\"cycle_us\":" + String(first.cycleUs) + ",\"pitch\":" + number(first.pitch) +
             ",\"roll\":" + number(first.roll) + ",\"accZ\":" + number(first.accZ) +
             ",\"battery\":" + number(first.battery) + "}}";
        web.send(200, "application/json", j);
    });
    web.begin();
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9871);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverFd, (sockaddr *)&addr, sizeof(addr)) < 0 || listen(serverFd, 1) < 0) {
        close(serverFd);
        serverFd = -1;
    } else
        fcntl(serverFd, F_SETFL, O_NONBLOCK);
    static wire::Frame pending;
    size_t offset = 0;
    bool has = false;
    uint32_t progress = 0;
    String header;
    int lastWifi = -1, lastReason = -1;
    uint32_t lastRetry = millis(), lastLog = 0;
    Serial.printf("[BOOT] servers http=80 log=%d\n", serverFd);
    for (;;) {
        int ws = int(WiFi.status()), dr = disconnectReason.load();
        // 只在通信核限频记录变化；发送空间不足就延后，不阻塞控制核。
        if ((ws != lastWifi || dr != lastReason) && uint32_t(millis() - lastLog) >= 1000 &&
            Serial.availableForWrite() >= 128) {
            lastWifi = ws;
            lastReason = dr;
            lastLog = millis();
            Serial.printf("[NET] status=%d reason=%d ip=%s\n", ws, dr,
                          WiFi.localIP().toString().c_str());
        }
        if (ssid.length() && ws != WL_CONNECTED && uint32_t(millis() - lastRetry) >= 15000) {
            lastRetry = millis();
            WiFi.reconnect();
        }
        uint32_t httpStart = micros();
        web.handleClient();
        uint32_t httpUs = micros() - httpStart;
        if (httpUs > httpMaxUs)
            httpMaxUs = httpUs;
        if (token.length() && !leaseExpired && uint32_t(millis() - lease) >= 1500) {
            leaseExpired = true;
            bridge::event(4, uint32_t(millis() - lease));
        }
        if (serverFd >= 0 && clientFd < 0) {
            clientFd = accept(serverFd, nullptr, nullptr);
            if (clientFd >= 0) {
                fcntl(clientFd, F_SETFL, O_NONBLOCK);
                logConnected = true;
                has = false;
                offset = 0;
                progress = millis();
                header =
                    String("{\"protocol\":1,\"model\":\"") + MODEL_SHA + "\",\"firmware\":\"" +
                    FIRMWARE_SHA +
                    "\",\"parameters\":\"direct-wheel-voltage-v1; 63-64-64-4 tanh; history=3; U_model=7.4*clip(a,-1,1) V; no wheel LPF/P; hip=.12 rad; control_dt=10ms; substep_dt=1ms\"}\n";
            }
        }
        if (clientFd >= 0) {
            const uint8_t *data = nullptr;
            size_t len = 0;
            if (header.length()) {
                data = (const uint8_t *)header.c_str();
                len = header.length();
            } else {
                if (!has) {
                    has = bridge::pop(pending);
                    offset = 0;
                }
                if (has) {
                    data = (const uint8_t *)&pending;
                    len = sizeof(pending);
                }
            }
            if (data) {
                int n = send(clientFd, data + offset, len - offset, MSG_DONTWAIT);
                if (n > 0) {
                    offset += n;
                    progress = millis();
                    if (offset == len) {
                        offset = 0;
                        if (header.length())
                            header = "";
                        else
                            has = false;
                    }
                } else if (n == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
                    close(clientFd);
                    clientFd = -1;
                    logConnected = false;
                }
            }
            if (clientFd >= 0 && uint32_t(millis() - progress) > 1500) {
                close(clientFd);
                clientFd = -1;
                logConnected = false;
            }
        }
        esp_task_wdt_reset();
        vTaskDelay(1);
    }
}
} // namespace
// 把通信任务固定至核0；返回任务创建结果，不代表Wi-Fi已连接。
bool startNetwork() {
    return xTaskCreatePinnedToCore(task, "network", 8192, nullptr, 1, nullptr, 0) == pdPASS;
}
