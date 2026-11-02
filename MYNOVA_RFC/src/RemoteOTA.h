/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

#ifndef REMOTE_OTA_H
#define REMOTE_OTA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "Version.h"

// 共享的 OTA 忙标志：本地网页升级与远程自动升级互斥，避免并发写分区
extern volatile bool OTA_busy;

class RemoteOTA {
public:
    // versionUrl: 远端 version.json 地址，例如 "http://192.168.1.100:8000/version.json"
    // intervalMs: 自动检查间隔（默认 24 小时）
    void begin(const String& versionUrl, unsigned long intervalMs = 24UL * 3600 * 1000);
    void setVersionUrl(const String& url) { versionUrl = url; }

    bool checkNow();                 // 立即检查一次（手动 / API 调用）
    void update();                   // 在 loop() 中周期调用（启动 60s 后首查 + 周期检查）
    void loopEnd();                  // 在 loop() 末尾调用，处理待重启

    static bool isUpdating() { return OTA_busy; }
    const char* lastError() const { return lastErr.c_str(); }

private:
    bool doUpdate(const String& firmwareUrl, const String& md5);
    int  versionCompare(const String& a, const String& b);

    String  versionUrl;
    unsigned long intervalMs   = 24UL * 3600 * 1000;
    unsigned long lastCheck    = 0;
    bool    bootChecked        = false;
    bool    pendingReboot      = false;
    unsigned long rebootAt     = 0;
    String  attemptedVersion;       // 已尝试过（失败）的版本，避免反复刷坏固件
    String  lastErr            = "";
};

#endif
