/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

#ifndef __WEBSERVICE__H__
#define __WEBSERVICE__H__
#include <Arduino.h>
#include "ESPAsyncWebServer.h"
#include <DNSServer.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <Update.h>

#include "WiFiManager.h"
#include "DataStore.h"
#include "RemoteOTA.h"

class WebService
{
public:
    WebService();
    void init(WiFiManager *wifiMgr);
    void serverStart();
    void processRequest();

    // WIFI相关接口
    void handleWIFIScanRequest(AsyncWebServerRequest *request);
    void handleWIFISaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleWIFIClearRequest(AsyncWebServerRequest *request);
    void handleWIFIConnectRequest(AsyncWebServerRequest *request);
    void handleWIFIEnableRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleAPWIFIInfoRequest(AsyncWebServerRequest *request);
    void handleAPSaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleAPEnableRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    // 无线遥控数据管理接口
    void handleRadioDataListRequest(AsyncWebServerRequest *request);
    void handleRadioDataAddRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleRadioDataUpdateRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleRadioDataDeleteRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleRadioDataGetRequest(AsyncWebServerRequest *request);
    void handleRadioDataSendRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    
    // MQTT/HA配置接口
    void handleMQTTConfigGetRequest(AsyncWebServerRequest *request);
    void handleMQTTConfigSaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleMQTTConfigClearRequest(AsyncWebServerRequest *request);
    void handleMQTTConnectRequest(AsyncWebServerRequest *request);
    void handleMQTTStatusRequest(AsyncWebServerRequest *request);

    // OTA 网页上传接口（本地手动刷机）
    void handleOTAUploadComplete(AsyncWebServerRequest *request);
    void handleOTAUploadData(AsyncWebServerRequest *request, const String& filename,
                            size_t index, uint8_t *data, size_t len, bool final);
    // OTA 状态 / 远程检查 / 令牌管理 / 升级页
    void handleOTAStatusRequest(AsyncWebServerRequest *request);
    void handleOTACheckRequest(AsyncWebServerRequest *request);
    void handleOTASetTokenRequest(AsyncWebServerRequest *request);
    void handleOTAUpdateForm(AsyncWebServerRequest *request);
    bool checkOTAToken(AsyncWebServerRequest *request);

private:
    AsyncWebServer server;
    WiFiManager *pWiFiManager;

    String otaToken;          // OTA 访问令牌（存于 Preferences，默认 mynova-admin）
    bool otaAborted = false;  // 本次上传已因鉴权/写入失败而中止
};

#endif