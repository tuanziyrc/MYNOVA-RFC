/* 
* Copyright (c) 2026 Tomosawa 
* https://github.com/Tomosawa/ 
* All rights reserved 
*/

#include "Buzzer.h"
#include "SystemSetting.h"

extern SystemSetting systemSetting;

// 无源蜂鸣器（passive buzzer）内部无振荡电路，必须给 PWM 方波才会发声；
// 有源蜂鸣器（active buzzer）才是“给高电平即响”。硬件改为无源后改用 LEDC 输出方波驱动。
// 注：本工程所用 Arduino-ESP32 版本未提供新版 ledcAttach(pin,...) API，
//     故使用经典 API：ledcSetup(channel, freq, res) + ledcAttachPin(pin, channel) + ledcWrite(channel, duty)
#define BUZZER_PWM_FREQ       2700    // Hz：2~3kHz 听感清晰、对多数无源蜂鸣器通用性好
#define BUZZER_PWM_RES        8       // 8 位分辨率 -> duty 范围 0~255
#define BUZZER_DUTY_ON        128     // 50% 占空比 -> 持续方波，蜂鸣器发声
#define BUZZER_DUTY_OFF       0       // 0% 占空比 -> 静音
#define BUZZER_LEDC_CHANNEL   0       // 使用的 LEDC 通道（本工程无其他 PWM 占用）

Buzzer::Buzzer(int pin)
{
    this->pin = pin;
    this->buzzerTaskHandle = nullptr;
    this->buzzerQueue = nullptr;
    this->stopRequested = false;
    this->initialized = false;
}

Buzzer::~Buzzer() {
    // 停止常驻任务
    if (buzzerTaskHandle != nullptr) {
        vTaskDelete(buzzerTaskHandle);
        buzzerTaskHandle = nullptr;
    }
    
    // 删除队列
    if (buzzerQueue != nullptr) {
        vQueueDelete(buzzerQueue);
        buzzerQueue = nullptr;
    }
}

void Buzzer::init() {
    if (initialized) return;
    
    // 初始化GPIO
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    // 无源蜂鸣器：配置 LEDC 输出方波（有源蜂鸣器只需高/低电平开关）
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_PWM_FREQ, BUZZER_PWM_RES);
    ledcAttachPin(pin, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
    
    // 创建请求队列（最多缓存5个请求）
    buzzerQueue = xQueueCreate(5, sizeof(BuzzerRequest));
    
    // 创建常驻蜂鸣器任务
    xTaskCreate(
        buzzerTask,
        "BuzzerTask",
        2048,           // 常驻任务不需要太大堆栈
        this,           // 传递Buzzer实例
        2,              // 较低优先级
        &buzzerTaskHandle
    );
    
    initialized = true;
    Serial.println("Buzzer: 初始化完成（无源蜂鸣器 / LEDC 方波驱动）");
}

void Buzzer::beep(int duration)
{
    if (!initialized || !systemSetting.getBuzzerEnable()) return;
    
    BuzzerRequest request;
    request.command = BUZZER_CMD_BEEP;
    request.duration = duration;
    
    // 发送到队列（不等待，队列满则丢弃）
    xQueueSend(buzzerQueue, &request, 0);
}

void Buzzer::beepDouble()
{
    if (!initialized || !systemSetting.getBuzzerEnable()) return;
    
    BuzzerRequest request;
    request.command = BUZZER_CMD_DOUBLE;
    request.duration = 0;
    
    xQueueSend(buzzerQueue, &request, 0);
}

void Buzzer::beepLongShort()
{
    if (!initialized || !systemSetting.getBuzzerEnable()) return;
    
    BuzzerRequest request;
    request.command = BUZZER_CMD_LONG_SHORT;
    request.duration = 0;
    
    xQueueSend(buzzerQueue, &request, 0);
}

void Buzzer::beepSOS()
{
    if (!initialized || !systemSetting.getBuzzerEnable()) return;
    
    BuzzerRequest request;
    request.command = BUZZER_CMD_SOS;
    request.duration = 0;
    
    xQueueSend(buzzerQueue, &request, 0);
}

void Buzzer::stop()
{
    if (!initialized) return;
    
    stopRequested = true;
    
    // 清空队列
    xQueueReset(buzzerQueue);
    
    BuzzerRequest request;
    request.command = BUZZER_CMD_STOP;
    request.duration = 0;
    
    xQueueSend(buzzerQueue, &request, 0);
}

void Buzzer::on()
{
    if (!initialized || !systemSetting.getBuzzerEnable()) return;
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
}

void Buzzer::off()
{
    if (!initialized) return;
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
}

// ==================== 常驻蜂鸣器任务 ====================
void Buzzer::buzzerTask(void* pvParameters)
{
    Buzzer* buzzer = static_cast<Buzzer*>(pvParameters);
    BuzzerRequest request;
    
    while (true) {
        // 等待队列中的请求（永久等待）
        if (xQueueReceive(buzzer->buzzerQueue, &request, portMAX_DELAY) == pdPASS) {
            buzzer->stopRequested = false;
            
            switch (request.command) {
                case BUZZER_CMD_BEEP:
                    buzzer->executeBeep(request.duration);
                    break;
                case BUZZER_CMD_DOUBLE:
                    buzzer->executeDouble();
                    break;
                case BUZZER_CMD_LONG_SHORT:
                    buzzer->executeLongShort();
                    break;
                case BUZZER_CMD_SOS:
                    buzzer->executeSOS();
                    break;
                case BUZZER_CMD_STOP:
                    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
                    break;
            }
        }
    }
}

// ==================== 内部执行函数 ====================
void Buzzer::executeBeep(int duration)
{
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
    vTaskDelay(pdMS_TO_TICKS(duration));
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
}

void Buzzer::executeDouble()
{
    // 双短声: 滴-滴
    for (int i = 0; i < 2 && !stopRequested; i++) {
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
        vTaskDelay(pdMS_TO_TICKS(100));
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
        if (i < 1) vTaskDelay(pdMS_TO_TICKS(80));
    }
}

void Buzzer::executeLongShort()
{
    // 一长两短: 嘟---滴-滴
    if (stopRequested) return;
    
    // 长声
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
    vTaskDelay(pdMS_TO_TICKS(400));
    ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 两短声
    for (int i = 0; i < 2 && !stopRequested; i++) {
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
        vTaskDelay(pdMS_TO_TICKS(100));
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
        if (i < 1) vTaskDelay(pdMS_TO_TICKS(80));
    }
}

void Buzzer::executeSOS()
{
    // SOS: ... --- ... (短短短-长长长-短短短)
    
    // S (3个短音)
    for (int i = 0; i < 3 && !stopRequested; i++) {
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
        vTaskDelay(pdMS_TO_TICKS(150));
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (stopRequested) return;
    vTaskDelay(pdMS_TO_TICKS(200)); // 字母间隔
    
    // O (3个长音)
    for (int i = 0; i < 3 && !stopRequested; i++) {
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
        vTaskDelay(pdMS_TO_TICKS(400));
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (stopRequested) return;
    vTaskDelay(pdMS_TO_TICKS(200)); // 字母间隔
    
    // S (3个短音)
    for (int i = 0; i < 3 && !stopRequested; i++) {
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_ON);
        vTaskDelay(pdMS_TO_TICKS(150));
        ledcWrite(BUZZER_LEDC_CHANNEL, BUZZER_DUTY_OFF);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
