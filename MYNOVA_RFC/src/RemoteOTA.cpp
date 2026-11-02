/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

#include "RemoteOTA.h"
#include <WiFi.h>
#include <ArduinoJson.h>

volatile bool OTA_busy = false;

void RemoteOTA::begin(const String& url, unsigned long intervalMs)
{
    versionUrl = url;
    this->intervalMs = intervalMs;
    lastCheck = 0;
    bootChecked = false;
    pendingReboot = false;
    attemptedVersion = "";
    Serial.printf("RemoteOTA: 版本检查地址=%s, 间隔=%lus (启动60s后首查)\n",
                  url.length() ? url.c_str() : "(未设置)", intervalMs / 1000);
}

// 逐段数值比较版本号，支持 "1.1" < "1.2" < "1.10"
int RemoteOTA::versionCompare(const String& a, const String& b)
{
    int i = 0, j = 0;
    while (i < (int)a.length() || j < (int)b.length()) {
        int na = 0, nb = 0;
        while (i < (int)a.length() && a[i] != '.') na = na * 10 + (a[i++] - '0');
        if (i < (int)a.length()) i++;
        while (j < (int)b.length() && b[j] != '.') nb = nb * 10 + (b[j++] - '0');
        if (j < (int)b.length()) j++;
        if (na != nb) return na < nb ? -1 : 1;
    }
    return 0;
}

bool RemoteOTA::doUpdate(const String& firmwareUrl, const String& md5)
{
    HTTPClient http;
    http.setTimeout(60000);
    http.begin(firmwareUrl);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        lastErr = "下载失败, HTTP " + String(code);
        http.end();
        return false;
    }

    int len = http.getSize();
    if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        lastErr = "Update.begin 失败(空间不足?)";
        Update.printError(Serial);
        http.end();
        return false;
    }
    if (md5.length() == 32) Update.setMD5(md5.c_str());

    Update.onProgress([](size_t done, size_t total) {
        if (total) Serial.printf("RemoteOTA: 下载 %u/%u (%u%%)\n", done, total, done * 100 / total);
    });

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);
    http.end();

    if (len > 0 && written != (size_t)len) {
        lastErr = "写入字节数不符";
        Update.printError(Serial);
        return false;
    }
    if (Update.end(true) && Update.isFinished()) {
        Serial.println("RemoteOTA: 固件校验通过，准备重启");
        pendingReboot = true;
        rebootAt = millis() + 1500;     // 留出时间让响应/日志发出
        return true;
    }
    lastErr = "Update.end 校验失败";
    Update.printError(Serial);
    return false;
}

bool RemoteOTA::checkNow()
{
    if (OTA_busy)          { lastErr = "已有 OTA 进行中"; return false; }
    if (WiFi.status() != WL_CONNECTED) { lastErr = "WiFi 未连接"; return false; }
    if (versionUrl.length() == 0)      { lastErr = "未配置版本地址"; return false; }

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(versionUrl);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        lastErr = "获取版本信息失败, HTTP " + String(code);
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        lastErr = "版本 JSON 解析失败";
        return false;
    }
    String remoteVer = doc["version"] | "";
    String url       = doc["url"]     | "";
    String md5       = doc["md5"]     | "";
    if (remoteVer.isEmpty() || url.isEmpty()) {
        lastErr = "版本信息不完整";
        return false;
    }

    Serial.printf("RemoteOTA: 本地 V%s / 远程 V%s\n", FIRMWARE_VERSION, remoteVer.c_str());
    if (versionCompare(remoteVer, String(FIRMWARE_VERSION)) <= 0) {
        lastErr = "已是最新";
        return false;
    }
    if (remoteVer == attemptedVersion) {
        lastErr = "该版本此前已尝试但失败，跳过以避免变砖";
        return false;
    }
    attemptedVersion = remoteVer;   // 标记，避免反复刷同一坏版本
    OTA_busy = true;
    bool ok = doUpdate(url, md5);
    OTA_busy = false;
    return ok;
}

void RemoteOTA::update()
{
    unsigned long now = millis();
    if (!bootChecked) {
        if (now < 60000) return;   // 启动后等 60s 让 WiFi 连上
        bootChecked = true;
    }
    if (now - lastCheck < intervalMs) return;
    lastCheck = now;
    checkNow();
}

void RemoteOTA::loopEnd()
{
    if (pendingReboot && (long)(millis() - rebootAt) >= 0) {
        Serial.println("RemoteOTA: 重启以应用新固件");
        delay(200);
        ESP.restart();
    }
}
