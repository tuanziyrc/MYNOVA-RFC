/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

#include "WebService.h"
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include "DataStore.h"
#include "SystemSetting.h"
#include "RadioHelper.h"
#include "HAManager.h"
#include "ota_page.h"

extern DataStore dataStore;
extern SystemSetting systemSetting;
extern RadioHelper radioHelper;
extern HAManager haManager;
extern RemoteOTA remoteOTA;
void handleRequest(AsyncWebServerRequest *request){}
void handleUploadRequest(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){}

WebService::WebService():
server(80)
{
}

void WebService::init(WiFiManager *wifiMgr)
{
    pWiFiManager = wifiMgr;
    otaToken = dataStore.LoadOTAToken();
    if (otaToken.isEmpty()) {
        otaToken = "mynova-admin";   // 默认令牌，建议通过 /api/ota/settoken 修改
    }
    Serial.println("WebService: 创建AsyncWebServer实例，OTA Token " + (otaToken.length() ? String("已就绪") : String("未设置")));
}

void WebService::serverStart()
{
    Serial.println("WebService: 开始启动Web服务器");
    
    // 初始化LittleFS文件系统（失败不阻断 OTA / API）
    Serial.println("WebService: 挂载LittleFS文件系统");
    bool lfsOk = LittleFS.begin(true);
    if (!lfsOk) {
        Serial.println("WebService: LittleFS挂载失败，仅提供 /ota 升级页与 API");
    } else {
        Serial.println("WebService: LittleFS挂载成功");
        // 配置静态文件服务
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    }

    // ============ OTA 升级相关路由（独立于 LittleFS，必注册） ============
    // 本地网页上传固件
    server.on("/api/ota/upload", HTTP_POST,
        (ArRequestHandlerFunction)std::bind(&WebService::handleOTAUploadComplete, this, std::placeholders::_1),
        (ArUploadHandlerFunction)std::bind(&WebService::handleOTAUploadData, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
            std::placeholders::_5, std::placeholders::_6));
    // 远程更新：立即检查并升级（Token 鉴权）
    server.on("/api/ota/check", HTTP_POST,
        (ArRequestHandlerFunction)std::bind(&WebService::handleOTACheckRequest, this, std::placeholders::_1));
    // 修改 OTA Token（需当前 Token）
    server.on("/api/ota/settoken", HTTP_POST,
        (ArRequestHandlerFunction)std::bind(&WebService::handleOTASetTokenRequest, this, std::placeholders::_1));
    // 查询 OTA 状态（版本/运行分区/是否忙）
    server.on("/api/ota/status", HTTP_GET,
        (ArRequestHandlerFunction)std::bind(&WebService::handleOTAStatusRequest, this, std::placeholders::_1));
    // 升级页面（内置 HTML，无需 LittleFS）
    server.on("/ota", HTTP_GET,
        (ArRequestHandlerFunction)std::bind(&WebService::handleOTAUpdateForm, this, std::placeholders::_1));

    // 注册API路由
    Serial.println("WebService: 注册API路由");
    
    // WIFI相关接口
    server.on("/api/wifiscan", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleWIFIScanRequest, this, std::placeholders::_1));
    server.on("/api/wifisave", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleWIFISaveRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/wificlear", HTTP_DELETE, (ArRequestHandlerFunction)std::bind(&WebService::handleWIFIClearRequest, this, std::placeholders::_1));
    server.on("/api/wificonnect", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleWIFIConnectRequest, this, std::placeholders::_1));
    server.on("/api/wifienable", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleWIFIEnableRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/apwifiinfo", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleAPWIFIInfoRequest, this, std::placeholders::_1));
    server.on("/api/apsave", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleAPSaveRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/apenable", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleAPEnableRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    
    // 无线遥控数据管理接口
    server.on("/api/radiodata/list", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleRadioDataListRequest, this, std::placeholders::_1));
    server.on("/api/radiodata/add", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleRadioDataAddRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/radiodata/update", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleRadioDataUpdateRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/radiodata/delete", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleRadioDataDeleteRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/radiodata/send", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleRadioDataSendRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    
    // MQTT/HA配置接口
    server.on("/api/mqtt/config", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleMQTTConfigGetRequest, this, std::placeholders::_1));
    server.on("/api/mqtt/config", HTTP_POST, handleRequest, handleUploadRequest, (ArBodyHandlerFunction)std::bind(&WebService::handleMQTTConfigSaveRequest, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
    server.on("/api/mqtt/config", HTTP_DELETE, (ArRequestHandlerFunction)std::bind(&WebService::handleMQTTConfigClearRequest, this, std::placeholders::_1));
    server.on("/api/mqtt/connect", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleMQTTConnectRequest, this, std::placeholders::_1));
    server.on("/api/mqtt/status", HTTP_GET, (ArRequestHandlerFunction)std::bind(&WebService::handleMQTTStatusRequest, this, std::placeholders::_1));
    
    Serial.println("WebService: 启动HTTP服务器");
    server.begin();
    Serial.println("WebService: Web服务器启动成功，监听端口80");
}

void WebService::processRequest()
{
}

// ==================== OTA 相关实现 ====================

bool WebService::checkOTAToken(AsyncWebServerRequest *request)
{
    String t = "";
    if (request->hasParam("token")) t = request->getParam("token")->value();   // token 在 URL 查询串
    if (t.isEmpty() && request->hasHeader("X-OTA-Token")) {
        t = request->getHeader("X-OTA-Token")->value();                       // 或放在请求头
    }
    return t == otaToken;
}

void WebService::handleOTAUploadData(AsyncWebServerRequest *request, const String& filename,
                                     size_t index, uint8_t *data, size_t len, bool final)
{
    if (otaAborted) return;                 // 本次上传已中止

    if (index == 0) {
        otaAborted = false;
        OTA_busy = false;
        if (!checkOTAToken(request)) {      // 第一片先做鉴权
            otaAborted = true;
            request->send(401, "application/json", "{\"success\":false,\"msg\":\"Token 无效\"}");
            return;
        }
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            otaAborted = true;
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"msg\":\"Update.begin 失败(空间不足?)\"}");
            return;
        }
        OTA_busy = true;
        Serial.println("OTA upload start: " + filename);
    }

    if (len > 0) {
        if (Update.write(data, len) != len) {   // 必须校验写入字节数，防止静默丢数据变砖
            otaAborted = true;
            OTA_busy = false;
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"msg\":\"固件写入不完整\"}");
            return;
        }
    }
}

void WebService::handleOTAUploadComplete(AsyncWebServerRequest *request)
{
    if (otaAborted) {                    // 上传过程中已回复过错误
        otaAborted = false;
        return;
    }
    if (Update.isRunning() && Update.end(true)) {
        OTA_busy = false;
        request->send(200, "application/json", "{\"success\":true,\"msg\":\"固件已写入，设备即将重启\"}");
        delay(1000);
        ESP.restart();
    } else {
        OTA_busy = false;
        Update.printError(Serial);
        request->send(500, "application/json", "{\"success\":false,\"msg\":\"固件校验失败或未收到数据\"}");
    }
}

void WebService::handleOTAStatusRequest(AsyncWebServerRequest *request)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["running"] = running ? String(running->label) : String("?");
    doc["busy"]    = (bool)OTA_busy;
    doc["hasToken"]= otaToken.length() > 0;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void WebService::handleOTACheckRequest(AsyncWebServerRequest *request)
{
    if (!checkOTAToken(request)) {
        request->send(401, "application/json", "{\"success\":false,\"msg\":\"Token 无效\"}");
        return;
    }
    bool ok = remoteOTA.checkNow();
    JsonDocument doc;
    doc["success"] = ok;
    doc["msg"] = ok ? "发现新版本，开始升级，设备将重启" : String(remoteOTA.lastError());
    String out;
    serializeJson(doc, out);
    request->send(ok ? 200 : 400, "application/json", out);
}

void WebService::handleOTASetTokenRequest(AsyncWebServerRequest *request)
{
    if (!checkOTAToken(request)) {
        request->send(401, "application/json", "{\"success\":false,\"msg\":\"当前 Token 无效\"}");
        return;
    }
    if (!request->hasParam("newtoken")) {
        request->send(400, "application/json", "{\"success\":false,\"msg\":\"缺少 newtoken 参数\"}");
        return;
    }
    String nt = request->getParam("newtoken")->value();
    if (nt.length() < 4) {
        request->send(400, "application/json", "{\"success\":false,\"msg\":\"Token 至少 4 位\"}");
        return;
    }
    otaToken = nt;
    dataStore.SaveOTAToken(otaToken);
    request->send(200, "application/json", "{\"success\":true,\"msg\":\"Token 已更新\"}");
}

void WebService::handleOTAUpdateForm(AsyncWebServerRequest *request)
{
    request->send_P(200, "text/html", OTA_PAGE_HTML);
}

void WebService::handleWIFIScanRequest(AsyncWebServerRequest *request)
{
    // 使用WiFiManager进行异步扫描
    if (pWiFiManager) {
        if (!pWiFiManager->isScanComplete()) {
            pWiFiManager->startScan();
        }
        request->send(200, "application/json", pWiFiManager->getScanResultJson());
    } else {
        request->send(200, "application/json", "{\"result\":\"failed\",\"message\":\"WiFiManager not initialized\"}");
    }
}

void WebService::handleWIFISaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr =(char*)data;
  
    // 打印原始POST数据
    Serial.println(jsonStr);
    // 解析JSON数据
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"JSON parsing failed\"}");
        return;
    }
    // 处理解析后的JSON数据
    const char* ssid = doc["SSID"];
    const char* pwd = doc["PWD"];
    
    // 使用WiFiManager保存配置
    if (pWiFiManager) {
        pWiFiManager->saveConfig(ssid, pwd);
    }
    
    // 返回响应
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

void WebService::handleWIFIClearRequest(AsyncWebServerRequest *request)
{
    // 使用WiFiManager清除配置
    if (pWiFiManager) {
        pWiFiManager->clearConfig();
        pWiFiManager->disconnect();
    }
    
    // 返回响应
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

void WebService::handleWIFIConnectRequest(AsyncWebServerRequest *request)
{
    // 使用WiFiManager连接
    if (pWiFiManager) {
        String ssid, password;
        if (pWiFiManager->loadConfig(ssid, password)) {
            bool connected = pWiFiManager->connectToWiFi(ssid, password);
            if (connected) {
                request->send(200, "application/json", "{\"result\":\"OK\",\"connected\":true}");
                return;
            }
        }
    }
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}


// 无线遥控数据管理接口实现
void WebService::handleRadioDataListRequest(AsyncWebServerRequest *request)
{   
    JsonDocument doc;
    JsonArray dataArray = doc["data"].to<JsonArray>();
    
    // 读取所有数据位置（1-100）
    for (int i = 1; i <= 100; i++) {
        RadioData radioData = dataStore.ReadData(i);
        
        // 只返回有效数据（名称不为空）
        if (radioData.name.length() > 0) {
            JsonObject item = dataArray.add<JsonObject>();
            item["index"] = i;
            item["name"] = radioData.name;
            item["data"] = (unsigned long)radioData.rcData.data;
            item["bitLength"] = radioData.rcData.bitLength;
            item["protocol"] = radioData.rcData.protocal;
            item["pulseLength"] = radioData.rcData.pulseLength;
            item["freqType"] = (int)radioData.rcData.freqType;
        }
    }
    
    doc["result"] = "OK";
    doc["count"] = dataArray.size();
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void WebService::handleRadioDataAddRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Add RadioData: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"JSON parsing failed\"}");
        return;
    }
    
    // 查找第一个空位置
    int emptyIndex = -1;
    for (int i = 1; i <= 100; i++) {
        RadioData existingData = dataStore.ReadData(i);
        if (existingData.name.length() == 0) {
            emptyIndex = i;
            break;
        }
    }
    
    if (emptyIndex == -1) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"No empty slot available\"}");
        return;
    }
    
    // 创建新数据
    RadioData newData;
    newData.name = doc["name"].as<String>();
    newData.rcData.data = doc["data"].as<unsigned long>();
    newData.rcData.bitLength = doc["bitLength"].as<unsigned int>();
    newData.rcData.protocal = doc["protocol"].as<unsigned int>();
    newData.rcData.pulseLength = doc["pulseLength"].as<uint16_t>();
    newData.rcData.freqType = (FreqType)doc["freqType"].as<int>();
    
    // 保存数据
    dataStore.SaveData(emptyIndex, newData);
    
    JsonDocument responseDoc;
    responseDoc["result"] = "OK";
    responseDoc["index"] = emptyIndex;
    
    String output;
    serializeJson(responseDoc, output);
    request->send(200, "application/json", output);
}

void WebService::handleRadioDataUpdateRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Update RadioData: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"JSON parsing failed\"}");
        return;
    }
    
    int dataIndex = doc["index"].as<int>();
    if (dataIndex < 1 || dataIndex > 100) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid index\"}");
        return;
    }
    
    // 更新数据
    RadioData updateData;
    updateData.name = doc["name"].as<String>();
    updateData.rcData.data = doc["data"].as<unsigned long>();
    updateData.rcData.bitLength = doc["bitLength"].as<unsigned int>();
    updateData.rcData.protocal = doc["protocol"].as<unsigned int>();
    updateData.rcData.pulseLength = doc["pulseLength"].as<uint16_t>();
    updateData.rcData.freqType = (FreqType)doc["freqType"].as<int>();
    
    dataStore.SaveData(dataIndex, updateData);
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

void WebService::handleRadioDataDeleteRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Delete RadioData: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"JSON parsing failed\"}");
        return;
    }
    
    int dataIndex = doc["index"].as<int>();
    if (dataIndex < 1 || dataIndex > 100) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid index\"}");
        return;
    }
    
    // 创建空数据来覆盖
    RadioData emptyData;
    emptyData.name = "";
    emptyData.rcData.data = 0;
    emptyData.rcData.bitLength = 0;
    emptyData.rcData.protocal = 0;
    emptyData.rcData.pulseLength = 0;
    emptyData.rcData.freqType = FREQ_315;
    
    dataStore.SaveData(dataIndex, emptyData);
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

void WebService::handleRadioDataGetRequest(AsyncWebServerRequest *request)
{
    if (!request->hasParam("index")) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Missing index parameter\"}");
        return;
    }
    
    int dataIndex = request->getParam("index")->value().toInt();
    if (dataIndex < 1 || dataIndex > 100) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid index\"}");
        return;
    }
    
    RadioData radioData = dataStore.ReadData(dataIndex);
    
    JsonDocument doc;
    doc["result"] = "OK";
    doc["index"] = dataIndex;
    doc["name"] = radioData.name;
    doc["data"] = (unsigned long)radioData.rcData.data;
    doc["bitLength"] = radioData.rcData.bitLength;
    doc["protocol"] = radioData.rcData.protocal;
    doc["pulseLength"] = radioData.rcData.pulseLength;
    doc["freqType"] = (int)radioData.rcData.freqType;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}
// 新增的API处理函数 - 添加到WebService.cpp末尾

// WiFi开关控制
void WebService::handleWIFIEnableRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("WiFi Enable: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    bool wifiEnable = doc["WIFI_ENABLE"].as<bool>();
    systemSetting.setWifiEnabled(wifiEnable, true);
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

// 获取AP和WiFi配置信息
void WebService::handleAPWIFIInfoRequest(AsyncWebServerRequest *request)
{
    SystemConfig config = systemSetting.getConfig();
    
    JsonDocument doc;
    doc["AP_SSID"] = config.APName;
    doc["AP_PWD"] = config.APPassword;
    doc["AP_ENABLE"] = config.APEnabled;
    doc["WIFI_SSID"] = config.WifiName;
    doc["WIFI_PASSWORD"] = config.WifiPassword;
    doc["WIFI_ENABLE"] = config.WifiEnabled;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

// 保存AP配置
void WebService::handleAPSaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Save AP Config: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    String apSSID = doc["AP_SSID"].as<String>();
    String apPWD = doc["AP_PWD"].as<String>();
    
    systemSetting.setAPConfig(apSSID, apPWD, true);
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

// AP开关控制
void WebService::handleAPEnableRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("AP Enable: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    bool apEnable = doc["AP_ENABLE"].as<bool>();
    systemSetting.setAPEnabled(apEnable, true);
    
    request->send(200, "application/json", "{\"result\":\"OK\"}");
}

// 发送遥控信号
void WebService::handleRadioDataSendRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Send Radio Signal: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    int dataIndex = doc["index"].as<int>();
    
    if (dataIndex < 1 || dataIndex > 100) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid index\"}");
        return;
    }
    
    // 从DataStore读取数据
    RadioData radioData = dataStore.ReadData(dataIndex);
    
    if (radioData.name.length() == 0) {
        request->send(404, "application/json", "{\"result\":\"failed\",\"message\":\"Data not found\"}");
        return;
    }
    
    // 发送信号
    Serial.print("发送信号: ");
    Serial.print(radioData.name);
    Serial.print(" | 数据: ");
    Serial.print((unsigned long)radioData.rcData.data);
    Serial.print(" | 频率: ");
    Serial.println(radioData.rcData.freqType == FREQ_315 ? "315MHz" : "433MHz");
    
    radioHelper.SendData(radioData.rcData);
    
    request->send(200, "application/json", "{\"result\":\"OK\",\"message\":\"Signal sent successfully\"}");
}

// ==================== MQTT/HA配置接口实现 ====================

void WebService::handleMQTTConfigGetRequest(AsyncWebServerRequest *request)
{
    MQTTMode mode;
    String server;
    uint16_t port;
    String username;
    String password;
    
    JsonDocument doc;
    
    if (dataStore.LoadMQTTConfig(mode, server, port, username, password)) {
        doc["result"] = "OK";
        doc["mode"] = (mode == MQTTMode::AUTO) ? "auto" : "manual";
        doc["server"] = server;
        doc["port"] = port;
        doc["username"] = username;
        // 不返回密码，用******表示
        doc["password"] = (password.length() > 0) ? "******" : "";
        doc["hasConfig"] = true;
    } else {
        doc["result"] = "OK";
        doc["hasConfig"] = false;
        doc["mode"] = "auto";
        doc["server"] = "";
        doc["port"] = 1883;
        doc["username"] = "";
        doc["password"] = "";
    }
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void WebService::handleMQTTConfigSaveRequest(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    String jsonStr = String((char*)data).substring(0, len);
    Serial.println("Save MQTT Config: " + jsonStr);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    const char* modeStr = doc["mode"] | "auto";
    const char* server = doc["server"] | "";
    uint16_t port = doc["port"] | 1883;
    const char* username = doc["username"] | "";
    const char* password = doc["password"] | "";
    
    // 转换mode字符串为枚举
    MQTTMode mode = (strcmp(modeStr, "manual") == 0) ? MQTTMode::MANUAL : MQTTMode::AUTO;
    
    // 验证：如果是手动模式，服务器地址不能为空
    if (mode == MQTTMode::MANUAL && strlen(server) == 0) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Manual mode requires server address\"}");
        return;
    }
    
    // 保存MQTT配置
    if (dataStore.SaveMQTTConfig(mode, server, port, username, password)) {
        request->send(200, "application/json", "{\"result\":\"OK\",\"message\":\"MQTT config saved successfully\"}");
    } else {
        request->send(500, "application/json", "{\"result\":\"failed\",\"message\":\"Failed to save MQTT config\"}");
    }
}

void WebService::handleMQTTConfigClearRequest(AsyncWebServerRequest *request)
{
    // 断开MQTT连接
    haManager.disconnect();
    
    // 清除配置
    dataStore.ClearMQTTConfig();
    
    request->send(200, "application/json", "{\"result\":\"OK\",\"message\":\"MQTT config cleared\"}");
}

void WebService::handleMQTTConnectRequest(AsyncWebServerRequest *request)
{
    // 检查WiFi连接状态
    if (!pWiFiManager || !pWiFiManager->isConnected()) {
        request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"WiFi not connected\"}");
        return;
    }
    
    // 自动发现并连接到MQTT服务器
    if (haManager.connectAuto()) {
        request->send(200, "application/json", "{\"result\":\"OK\",\"message\":\"Auto-discovered and connected to MQTT broker\",\"connected\":true}");
    } else {
        request->send(500, "application/json", "{\"result\":\"failed\",\"message\":\"Failed to discover or connect to MQTT broker\",\"connected\":false}");
    }
}

void WebService::handleMQTTStatusRequest(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    
    doc["connected"] = haManager.isConnected();
    doc["hasConfig"] = haManager.hasSavedConfig();
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}
