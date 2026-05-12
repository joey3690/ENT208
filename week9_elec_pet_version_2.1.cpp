#include <M5Unified.h>
#include <math.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>

// ==================== WiFi 配置 ====================
// 请改成你自己的 WiFi 信息
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ==================== 心知天气 API 配置 ====================
// 请改成你自己的 API Key
const char* WEATHER_API_KEY = "YOUR_WEATHER_API_KEY";
const char* WEATHER_CITY    = "Taicang";

// ==================== NTP 时间配置 ====================
const char* NTP_SERVER      = "pool.ntp.org";
const long  GMT_OFFSET_SEC  = 8 * 3600;
const int   DAYLIGHT_OFFSET_SEC = 0;

// ==================== 显示缓冲 ====================
M5Canvas canvas(&M5.Display);

// ==================== 表情枚举 ====================
enum Expression {
    IDLE,
    SMILE,
    YAWN,
    LOOK_LEFT,
    LOOK_RIGHT,
    DIZZY,
    SHOCK,
    HEAD_TOUCH
};

// ==================== 全局状态变量 ====================
Expression currentExp = IDLE;
Expression previousExp = IDLE;

// 动作计时器
unsigned long lastActionTime = 0;
unsigned long actionStartTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastToggleTime = 0;
int nextIdleAnim = 0;
int lookDirection = -1;
int dizzyFrame = 0;
bool isInAction = false;

// 惊吓状态
bool shockActive = false;
unsigned long shockStartTime = 0;
const unsigned long SHOCK_DURATION = 1000;

// 拖拽交互状态
bool isSliding = false;
int touchStartX = 0;
int touchStartY = 0;
unsigned long touchStartTime = 0;
int slideOffset = 0;
const int SLIDE_THRESHOLD = 30;
const unsigned long TAP_MAX_DURATION = 400;

int lastSlideEyeOffset = 999;
unsigned long lastSlideDrawTime = 0;
const unsigned long SLIDE_DRAW_INTERVAL = 35;

// 倒立检测
bool upsideDown = false;
unsigned long upsideDownStart = 0;
const unsigned long UPSIDE_DOWN_DELAY = 500;
const float UPSIDE_THRESHOLD = -0.8f;
const float RECOVER_THRESHOLD = -0.3f;

// 陀螺仪眼珠跟随
float smoothAccX = 0;
float smoothAccY = 0;
int oldPupilLX = 0;
int oldPupilLY = 0;
int oldPupilRX = 0;
int oldPupilRY = 0;

// 麦克风录音缓冲
static constexpr size_t record_length = 256;
static int16_t rec_data[record_length];
const int MIC_THRESHOLD = 60000;

// PIR 人体感应
#define PIR_PIN 36
bool isTouchingHead = false;
unsigned long headTouchStart = 0;
const unsigned long HEAD_TOUCH_DURATION = 2000;

// 温湿度传感器
#define SHT30_ADDR 0x44
float envTemperature = 0.0;
float envHumidity = 0.0;
unsigned long lastEnvRead = 0;
const unsigned long ENV_READ_INTERVAL = 5000;

struct WeatherData {
    String conditionText;
    float temperature;
    int code;
};

WeatherData weatherInfo;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_FETCH_INTERVAL = 600000;

struct tm timeInfo;

// ==================== Web 服务器 ====================
WebServer server(80);
String deviceIP = "";

bool qrShowing = false;
bool qrDrawn = false;
unsigned long qrShowStart = 0;
const unsigned long QR_DISPLAY_DURATION = 10000;

unsigned long btnBPressStart = 0;
bool btnBLongPressTriggered = false;
const unsigned long LONG_PRESS_TIME = 2000;

bool remoteControlActive = false;
unsigned long remoteControlLastCmd = 0;
const unsigned long REMOTE_TIMEOUT = 10000;

// ==================== 函数声明 ====================
bool readSHT30(float &t, float &h);
void connectWiFi();
void fetchWeather();
bool checkMicrophonePeak();
bool checkShake();
bool checkButton();
void updateUpsideDown();
void updateSmoothAccel();
void getPupilOffset(int &offX, int &offY);
void updateHeadTouch();

void pushCanvas();
void drawWeatherIcon(int code, int x, int y);
void drawStatusBar();
void fullDrawExpression(Expression exp, int eyeOffset = 0);
void updateIdlePupil(Expression exp);

void drawIdleFace();
void drawSmileFace();
void drawYawnFace();
void drawLookDirection(int dir);
void drawDizzyFrame(int frame);
void drawShockFace();
void drawHeadTouchFace();
void drawBlink();
void drawQRCodeOnce();

void runSmileHold(unsigned long durationMs);

Expression getNextExpression(Expression e);
Expression getPrevExpression(Expression e);

void handleRoot();
void handleSmile();
void handleYawn();
void handleDizzy();
void handleShock();
void handleIdle();
void runRemoteMode();

// ==================== 工具函数 ====================
void pushCanvas() {
    canvas.pushSprite(0, 0);
}

// ==================== 传感器实现 ====================
bool readSHT30(float &t, float &h) {
    Wire.beginTransmission(SHT30_ADDR);
    Wire.write(0x2C);
    Wire.write(0x06);
    if (Wire.endTransmission() != 0) return false;
    delay(100);
    Wire.requestFrom(SHT30_ADDR, 6);
    if (Wire.available() != 6) return false;
    uint8_t data[6];
    for (int i = 0; i < 6; i++) {
        data[i] = Wire.read();
    }
    uint16_t rawTemp = (data[0] << 8) | data[1];
    uint16_t rawHumi = (data[3] << 8) | data[4];
    t = -45.0 + 175.0 * rawTemp / 65535.0;
    h = 100.0 * rawHumi / 65535.0;
    return true;
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int timeout = 40;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(500);
        timeout--;
    }
    if (WiFi.status() == WL_CONNECTED) {
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    }
}

void fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) return;
    WiFiClient client;
    String url = "/v3/weather/now.json?key=";
    url += WEATHER_API_KEY;
    url += "&location=";
    url += WEATHER_CITY;
    url += "&language=zh-Hans&unit=c";

    if (!client.connect("api.seniverse.com", 80)) return;
    client.print("GET " + url + " HTTP/1.1\r\n");
    client.print("Host: api.seniverse.com\r\n");
    client.print("Connection: close\r\n\r\n");
    delay(500);

    String payload = "";
    while (client.available()) {
        payload += client.readStringUntil('\n');
    }
    client.stop();

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) return;

    JsonObject root = doc["results"][0]["now"];
    weatherInfo.conditionText = root["text"].as<String>();
    weatherInfo.temperature = root["temperature"].as<float>();
    String text = weatherInfo.conditionText;

    if (text == "晴") weatherInfo.code = 0;
    else if (text == "多云") weatherInfo.code = 1;
    else if (text == "阴") weatherInfo.code = 2;
    else if (text.indexOf("雨") != -1) weatherInfo.code = 3;
    else if (text.indexOf("雪") != -1) weatherInfo.code = 4;
    else weatherInfo.code = 1;
}

bool checkMicrophonePeak() {
    if (M5.Mic.record(rec_data, record_length, 16000)) {
        int16_t minVal = 32767;
        int16_t maxVal = -32768;
        for (size_t i = 0; i < record_length; i++) {
            if (rec_data[i] < minVal) minVal = rec_data[i];
            if (rec_data[i] > maxVal) maxVal = rec_data[i];
        }
        return (maxVal - minVal > MIC_THRESHOLD);
    }
    return false;
}

bool checkShake() {
    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
        float mag = sqrt(ax * ax + ay * ay + az * az);
        return (mag > 2.5);
    }
    return false;
}

bool checkButton() {
    return M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed();
}

void updateUpsideDown() {
    float ax, ay, az;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
    if (az < UPSIDE_THRESHOLD) {
        if (upsideDownStart == 0) {
            upsideDownStart = millis();
        } else if (millis() - upsideDownStart > UPSIDE_DOWN_DELAY) {
            upsideDown = true;
        }
    } else if (az > RECOVER_THRESHOLD) {
        upsideDown = false;
        upsideDownStart = 0;
        if (currentExp == DIZZY && !isInAction) {
            currentExp = IDLE;
            drawIdleFace();
        }
    }
}

void updateSmoothAccel() {
    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
        smoothAccX = smoothAccX * 0.9f + ax * 0.1f;
        smoothAccY = smoothAccY * 0.9f + ay * 0.1f;
    }
}

void getPupilOffset(int &offX, int &offY) {
    offX = constrain((int)(smoothAccX * 18), -19, 19);
    offY = constrain((int)(smoothAccY * 18), -19, 19);
}

void updateHeadTouch() {
    static bool lastPir = false;
    bool pir = digitalRead(PIR_PIN);
    if (pir && !lastPir) {
        isTouchingHead = true;
        headTouchStart = millis();
    }
    lastPir = pir;
}

// ==================== 绘图函数 ====================
void drawWeatherIcon(int code, int x, int y) {
    canvas.fillRect(x, y, 24, 24, BLACK);
    switch (code) {
        case 0:
            canvas.fillCircle(x + 10, y + 10, 7, WHITE);
            for (int i = 0; i < 8; i++) {
                float angle = i * 3.14159 * 2 / 8;
                int lx = x + 10 + 10 * cos(angle);
                int ly = y + 10 + 10 * sin(angle);
                canvas.drawLine(x + 10 + 8 * cos(angle), y + 10 + 8 * sin(angle), lx, ly, WHITE);
            }
            break;
        case 1:
            canvas.fillCircle(x + 7, y + 8, 5, WHITE);
            canvas.fillCircle(x + 13, y + 8, 6, WHITE);
            canvas.fillRoundRect(x + 5, y + 9, 12, 7, 3, WHITE);
            break;
        case 2:
            canvas.fillCircle(x + 7, y + 8, 5, 0x7BEF);
            canvas.fillCircle(x + 13, y + 8, 6, 0x7BEF);
            canvas.fillRoundRect(x + 5, y + 9, 12, 7, 3, 0x7BEF);
            break;
        case 3:
            canvas.fillCircle(x + 7, y + 5, 5, WHITE);
            canvas.fillCircle(x + 13, y + 5, 6, WHITE);
            canvas.fillRoundRect(x + 5, y + 6, 12, 7, 3, WHITE);
            canvas.drawLine(x + 7, y + 15, x + 4, y + 19, WHITE);
            canvas.drawLine(x + 11, y + 15, x + 8, y + 19, WHITE);
            canvas.drawLine(x + 15, y + 15, x + 12, y + 19, WHITE);
            break;
        case 4:
            canvas.fillCircle(x + 7, y + 5, 5, WHITE);
            canvas.fillCircle(x + 13, y + 5, 6, WHITE);
            canvas.fillRoundRect(x + 5, y + 6, 12, 7, 3, WHITE);
            for (int i = 0; i < 3; i++) {
                canvas.drawPixel(x + 5 + i * 5, y + 15, WHITE);
                canvas.drawPixel(x + 7 + i * 5, y + 16, WHITE);
                canvas.drawPixel(x + 9 + i * 5, y + 17, WHITE);
            }
            break;
        default:
            canvas.drawString("?", x, y);
            break;
    }
}

void drawStatusBar() {
    canvas.setTextSize(1);
    canvas.setTextColor(WHITE, BLACK);
    canvas.fillRect(0, 0, 320, 24, BLACK);
    canvas.fillRect(230, 210, 90, 30, BLACK);
    canvas.setCursor(240, 215);
    canvas.printf("%.1fC", envTemperature);
    canvas.setCursor(240, 225);
    canvas.printf("%.0f%%", envHumidity);

    if (WiFi.status() == WL_CONNECTED) {
        getLocalTime(&timeInfo);
        canvas.setCursor(220, 0);
        canvas.printf("%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
        drawWeatherIcon(weatherInfo.code, 260, 5);
        canvas.setCursor(5, 5);
        canvas.print("WiFi:●");
    } else {
        canvas.setCursor(260, 5);
        canvas.print("Off");
        canvas.setCursor(5, 5);
        canvas.print("WiFi:○");
    }
}

// 全屏重绘
void fullDrawExpression(Expression exp, int eyeOffset) {
    canvas.fillScreen(BLACK);

    int lx = 85 + eyeOffset;
    int rx = 235 + eyeOffset;
    int pupilOffX = 0;
    int pupilOffY = 0;

    // 只有 IDLE 表情需要跟随陀螺仪的眼珠坐标计算
    if (exp == IDLE) {
        getPupilOffset(pupilOffX, pupilOffY);
        oldPupilLX = lx + pupilOffX;
        oldPupilLY = 105 + pupilOffY;
        oldPupilRX = rx + pupilOffX;
        oldPupilRY = 105 + pupilOffY;
    } else {
        oldPupilLX = 0;
        oldPupilLY = 0;
        oldPupilRX = 0;
        oldPupilRY = 0;
    }

    switch (exp) {
        case IDLE:
            canvas.fillCircle(lx, 105, 25, WHITE);
            canvas.fillCircle(rx, 105, 25, WHITE);
            canvas.fillCircle(lx + pupilOffX, 105 + pupilOffY, 6, BLACK);
            canvas.fillCircle(rx + pupilOffX, 105 + pupilOffY, 6, BLACK);
            canvas.drawLine(110 + eyeOffset, 185, 210 + eyeOffset, 185, WHITE);
            break;

        case SMILE: {
            uint16_t blushColor = canvas.color565(255, 51, 51); // 专属二次元腮红红
            
            // 左眼 (纯白月牙，用黑圆切出弧度)
            canvas.fillCircle(lx, 105, 25, WHITE);
            canvas.fillCircle(lx, 118, 25, BLACK);
            
            // 右眼 (纯白月牙)
            canvas.fillCircle(rx, 105, 25, WHITE);
            canvas.fillCircle(rx, 118, 25, BLACK);
            
            // 左侧红晕双杠 (随面颊拖拽移动)
            canvas.fillRect(lx - 40, 145, 35, 6, blushColor);
            canvas.fillRect(lx - 45, 160, 35, 6, blushColor);
            
            // 右侧红晕双杠
            canvas.fillRect(rx + 5, 145, 35, 6, blushColor);
            canvas.fillRect(rx + 10, 160, 35, 6, blushColor);
            
            // 开心的小 v 嘴
            canvas.drawLine(150 + eyeOffset, 175, 160 + eyeOffset, 190, WHITE);
            canvas.drawLine(160 + eyeOffset, 190, 170 + eyeOffset, 175, WHITE);
            break;
        }

        case YAWN:
            canvas.drawLine(60 + eyeOffset, 115, 110 + eyeOffset, 115, WHITE);
            canvas.drawLine(210 + eyeOffset, 115, 260 + eyeOffset, 115, WHITE);
            canvas.drawCircle(160 + eyeOffset, 185, 15, WHITE);
            canvas.fillCircle(160 + eyeOffset, 185, 14, BLACK);
            break;

        case LOOK_LEFT:
        case LOOK_RIGHT: {
            int dir = (exp == LOOK_LEFT) ? -1 : 1;
            int off = dir * 15 + eyeOffset; 
            
            // 完美贴合的小眼珠结构
            canvas.drawLine(65 + off, 105, 105 + off, 105, WHITE);
            canvas.fillCircle(85 + dir * 15 + off, 113, 8, WHITE);
            
            canvas.drawLine(215 + off, 105, 255 + off, 105, WHITE);
            canvas.fillCircle(235 + dir * 15 + off, 113, 8, WHITE);
            
            int cx = 160 + off;
            int cy = 175;
            canvas.drawLine(cx - 24, cy - 5, cx - 12, cy + 10, WHITE);
            canvas.drawLine(cx - 12, cy + 10, cx, cy - 2, WHITE);
            canvas.drawLine(cx, cy - 2, cx + 12, cy + 10, WHITE);
            canvas.drawLine(cx + 12, cy + 10, cx + 24, cy - 5, WHITE);
            break;
        }

        case DIZZY: {
            float rotOffset = dizzyFrame * 0.4f;
            int eyes[2] = {lx, rx};
            for (int i = 0; i < 2; i++) {
                int eyeCx = eyes[i];
                float prevX = -1;
                float prevY = -1;
                for (float theta = 0; theta <= 18.8; theta += 0.6) {
                    float r = 1.6 * theta;
                    int x = (int)(eyeCx + r * cos(theta + rotOffset));
                    int y = (int)(105 + r * sin(theta + rotOffset));
                    if (prevX >= 0) canvas.drawLine((int)prevX, (int)prevY, x, y, WHITE);
                    prevX = x;
                    prevY = y;
                }
            }
            int waveOffset = (int)(8 * sin(dizzyFrame * 0.5));
            int pts[5][2] = {{110, 185}, {135, 175}, {160, 195}, {185, 175}, {210, 185}};
            for (int i = 0; i < 4; i++) {
                canvas.drawLine(pts[i][0] + eyeOffset, pts[i][1] + waveOffset, 
                                pts[i + 1][0] + eyeOffset, pts[i + 1][1] + waveOffset, WHITE);
            }
            break;
        }

        case SHOCK:
            canvas.fillCircle(lx, 105, 28, WHITE);
            canvas.fillCircle(lx, 105, 5, BLACK);
            canvas.fillCircle(rx, 105, 28, WHITE);
            canvas.fillCircle(rx, 105, 5, BLACK);
            canvas.drawCircle(160 + eyeOffset, 185, 18, WHITE);
            canvas.fillCircle(160 + eyeOffset, 185, 16, BLACK);
            break;

        case HEAD_TOUCH: {
            canvas.drawLine(lx - 15, 95, lx + 15, 95, WHITE);
            canvas.drawLine(rx - 15, 95, rx + 15, 95, WHITE);
            int hx = lx - 20;
            int hy = 85;
            canvas.fillCircle(hx - 3, hy, 4, WHITE);
            canvas.fillCircle(hx + 3, hy, 4, WHITE);
            canvas.fillTriangle(hx - 5, hy + 2, hx + 5, hy + 2, hx, hy + 10, WHITE);
            canvas.drawArc(160 + eyeOffset, 190, 12, 13, 210, 330, WHITE);
            break;
        }
    }

    drawStatusBar();
    pushCanvas();
}

// 局部更新瞳孔 (仅限 IDLE 状态，避免微笑时无效计算重绘)
void updateIdlePupil(Expression exp) {
    int pupilOffX = 0;
    int pupilOffY = 0;
    getPupilOffset(pupilOffX, pupilOffY);

    int lx = 85;
    int rx = 235;

    int newLX = lx + pupilOffX;
    int newLY = 105 + pupilOffY;
    int newRX = rx + pupilOffX;
    int newRY = 105 + pupilOffY;

    if (oldPupilLX == 0 && oldPupilRX == 0) {
        fullDrawExpression(exp, 0);
        return;
    }
    
    // 只处理带眼球的状态
    if (exp == IDLE) {
        // 覆盖旧瞳孔
        canvas.fillCircle(oldPupilLX, oldPupilLY, 7, WHITE);
        canvas.fillCircle(oldPupilRX, oldPupilRY, 7, WHITE);
        
        // 绘制新瞳孔
        canvas.fillCircle(newLX, newLY, 6, BLACK);
        canvas.fillCircle(newRX, newRY, 6, BLACK);

        oldPupilLX = newLX;
        oldPupilLY = newLY;
        oldPupilRX = newRX;
        oldPupilRY = newRY;
    }

    drawStatusBar();
    pushCanvas();
}

void drawIdleFace() { fullDrawExpression(IDLE, 0); }
void drawSmileFace() { fullDrawExpression(SMILE, 0); }
void drawYawnFace() { fullDrawExpression(YAWN, 0); }
void drawLookDirection(int dir) { currentExp = (dir == -1) ? LOOK_LEFT : LOOK_RIGHT; fullDrawExpression(currentExp, 0); }
void drawDizzyFrame(int frame) { dizzyFrame = frame; fullDrawExpression(DIZZY, 0); }
void drawShockFace() { fullDrawExpression(SHOCK, 0); }
void drawHeadTouchFace() { fullDrawExpression(HEAD_TOUCH, 0); }

void drawBlink() {
    canvas.fillRect(55, 75, 70, 65, BLACK);
    canvas.fillRect(205, 75, 70, 65, BLACK);
    canvas.drawLine(60, 105, 110, 105, WHITE);
    canvas.drawLine(210, 105, 260, 105, WHITE);
    pushCanvas();
    delay(100);
    
    canvas.fillRect(55, 75, 70, 65, BLACK);
    canvas.fillRect(205, 75, 70, 65, BLACK);
    
    int pupilOffX = 0;
    int pupilOffY = 0;
    getPupilOffset(pupilOffX, pupilOffY);
    
    canvas.fillCircle(85, 105, 25, WHITE);
    canvas.fillCircle(235, 105, 25, WHITE);
    canvas.fillCircle(85 + pupilOffX, 105 + pupilOffY, 6, BLACK);
    canvas.fillCircle(235 + pupilOffX, 105 + pupilOffY, 6, BLACK);
    
    oldPupilLX = 85 + pupilOffX;
    oldPupilLY = 105 + pupilOffY;
    oldPupilRX = 235 + pupilOffX;
    oldPupilRY = 105 + pupilOffY;
    
    drawStatusBar();
    pushCanvas();
}

void drawQRCodeOnce() {
    M5.Display.fillScreen(BLACK);
    String qrText = "http://" + WiFi.localIP().toString() + "/";
    M5.Display.qrcode(qrText, 10, 10, 200, 4);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(10, 230);
    M5.Display.print("Scan (10s)");
}

// 抚摸微笑逻辑 (移除了无效的陀螺仪眼球计算更新，更加省电平滑)
void runSmileHold(unsigned long durationMs) {
    isInAction = true;
    currentExp = SMILE;
    previousExp = SMILE;
    fullDrawExpression(SMILE, 0);
    actionStartTime = millis();

    while (millis() - actionStartTime < durationMs) {
        M5.update();
        server.handleClient();
        updateSmoothAccel();

        if (checkShake()) break;
        
        // 持续抚摸可重置并延长微笑时间
        if (checkButton() || M5.Touch.getDetail().wasPressed()) {
            actionStartTime = millis(); 
        }
        
        delay(50);
    }

    currentExp = IDLE;
    isInAction = false;
    drawIdleFace();
    previousExp = IDLE;
    lastActionTime = millis(); 
}

Expression getNextExpression(Expression e) {
    switch (e) {
        case IDLE: return SMILE;
        case SMILE: return YAWN;
        case YAWN: return DIZZY;
        case DIZZY: return IDLE;
        default: return IDLE;
    }
}

Expression getPrevExpression(Expression e) {
    switch (e) {
        case IDLE: return DIZZY;
        case SMILE: return IDLE;
        case YAWN: return SMILE;
        case DIZZY: return YAWN;
        default: return IDLE;
    }
}

// ==================== Web 服务器处理 ====================
void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Emoji Remote</title>
<style>
body { background:#222; color:#fff; text-align:center; font-family:Arial,sans-serif; }
h1 { margin:20px 0; }
button { width:80%; max-width:300px; padding:15px; margin:10px auto; font-size:1.2em; border:none; border-radius:12px; background:#4CAF50; color:#fff; display:block; cursor:pointer; }
button:active { background:#388E3C; }
</style>
</head>
<body>
<h1>M5Stack Remote</h1>
<button onclick="fetch('/smile')">Smile</button>
<button onclick="fetch('/yawn')">Yawn</button>
<button onclick="fetch('/dizzy')">Dizzy</button>
<button onclick="fetch('/shock')">Shock</button>
<button onclick="fetch('/idle')" style="background:#555;">Idle</button>
<p>Tap to change face</p>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleSmile() { currentExp = SMILE; remoteControlActive = true; remoteControlLastCmd = millis(); server.send(200, "text/plain", "ok"); }
void handleYawn() { currentExp = YAWN; remoteControlActive = true; remoteControlLastCmd = millis(); server.send(200, "text/plain", "ok"); }
void handleDizzy() { currentExp = DIZZY; dizzyFrame = 0; remoteControlActive = true; remoteControlLastCmd = millis(); server.send(200, "text/plain", "ok"); }
void handleShock() { currentExp = SHOCK; remoteControlActive = true; remoteControlLastCmd = millis(); server.send(200, "text/plain", "ok"); }
void handleIdle() { currentExp = IDLE; remoteControlActive = false; server.send(200, "text/plain", "ok"); }

void runRemoteMode() {
    Expression lastRemoteExp = currentExp;
    unsigned long lastRemoteDraw = 0;
    fullDrawExpression(currentExp, 0);

    while (remoteControlActive) {
        M5.update();
        server.handleClient();
        unsigned long now = millis();
        if (now - remoteControlLastCmd > REMOTE_TIMEOUT) {
            remoteControlActive = false;
            break;
        }
        if (currentExp != lastRemoteExp || now - lastRemoteDraw >= 150) {
            fullDrawExpression(currentExp, 0);
            lastRemoteExp = currentExp;
            lastRemoteDraw = now;
        }
        delay(30);
    }
    currentExp = IDLE;
    fullDrawExpression(IDLE, 0);
    previousExp = IDLE;
    lastActionTime = millis(); 
}

// ==================== 初始化 ====================
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    
    // 初始化画布
    canvas.setColorDepth(16);
    canvas.createSprite(320, 240);
    canvas.setTextSize(1);
    canvas.setTextColor(WHITE, BLACK);

    // 开启麦克风和陀螺仪
    M5.Mic.begin();
    M5.Imu.begin();
    
    pinMode(PIR_PIN, INPUT);
    Wire.begin();

    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        fetchWeather();
        deviceIP = WiFi.localIP().toString();
        server.on("/", handleRoot);
        server.on("/smile", handleSmile);
        server.on("/yawn", handleYawn);
        server.on("/dizzy", handleDizzy);
        server.on("/shock", handleShock);
        server.on("/idle", handleIdle);
        server.begin();
    }

    drawIdleFace();
    previousExp = IDLE;
    lastActionTime = millis();
    lastBlinkTime = millis();
}

// ==================== 主循环 ====================
void loop() {
    M5.update();
    unsigned long now = millis();
    server.handleClient();

    if (remoteControlActive) {
        runRemoteMode();
        return;
    }

    // ---- 长按 B 键显示二维码 ----
    if (M5.BtnB.isPressed()) {
        if (btnBPressStart == 0) btnBPressStart = now;
    } else {
        if (btnBPressStart > 0) {
            if (now - btnBPressStart >= LONG_PRESS_TIME && !btnBLongPressTriggered) {
                btnBLongPressTriggered = true;
                if (WiFi.status() == WL_CONNECTED) {
                    qrShowing = true;
                    qrDrawn = false;
                    qrShowStart = now;
                    lastActionTime = now;
                }
            }
            btnBPressStart = 0;
        }
        btnBLongPressTriggered = false;
    }

    if (qrShowing) {
        if (now - qrShowStart >= QR_DISPLAY_DURATION) {
            qrShowing = false;
            qrDrawn = false;
            drawIdleFace();
            lastActionTime = now;
        } else {
            if (!qrDrawn) {
                drawQRCodeOnce();
                qrDrawn = true;
            }
            delay(30);
            return;
        }
    }

    // ---- 环境数据更新 ----
    if (now - lastEnvRead >= ENV_READ_INTERVAL) {
        lastEnvRead = now;
        float t, h;
        if (readSHT30(t, h)) {
            envTemperature = t;
            envHumidity = h;
        }
    }
    if (WiFi.status() == WL_CONNECTED && (now - lastWeatherFetch >= WEATHER_FETCH_INTERVAL)) {
        lastWeatherFetch = now;
        fetchWeather();
    }

    // ---- 传感器检测 ----
    updateSmoothAccel();
    updateUpsideDown();
    updateHeadTouch();

    // ---- 声音惊吓逻辑 ----
    static unsigned long lastMicCheck = 0;
    if (now - lastMicCheck >= 80) {
        lastMicCheck = now;
        if (!shockActive && checkMicrophonePeak()) {
            shockActive = true;
            shockStartTime = now;
            currentExp = SHOCK;
            drawShockFace();
            lastActionTime = now; 
        }
    }

    if (shockActive) {
        if (now - shockStartTime >= SHOCK_DURATION) {
            shockActive = false;
            currentExp = IDLE;
            drawIdleFace();
            previousExp = IDLE;
            lastActionTime = now; 
        }
        delay(10);
        return;
    }

    // ---- 倒立眩晕 ----
    if (upsideDown) {
        if (currentExp != DIZZY) {
            currentExp = DIZZY;
            dizzyFrame = 0;
        }
        drawDizzyFrame(dizzyFrame);
        dizzyFrame++;
        delay(10);
        lastActionTime = now;
        return;
    }

    // ---- 摇晃眩晕 ----
    if (checkShake() && !isInAction && !isSliding) {
        isInAction = true;
        actionStartTime = now;
        dizzyFrame = 0;
        currentExp = DIZZY;

        while (millis() - actionStartTime < 3000) {
            M5.update();
            server.handleClient();
            drawDizzyFrame(dizzyFrame);
            dizzyFrame++;
            delay(20);
        }
        currentExp = IDLE;
        isInAction = false;
        drawIdleFace();
        previousExp = IDLE; 
        lastActionTime = millis();
    }

    // ---- 触摸脸颊拖拽交互 ----
    auto touch = M5.Touch.getDetail();

    if (touch.wasPressed()) {
        touchStartX = touch.x;
        touchStartY = touch.y;
        touchStartTime = now;
        isSliding = true;
        slideOffset = 0;
        lastSlideEyeOffset = 999;
        lastSlideDrawTime = 0;
        lastActionTime = now; 
    }

    if (touch.isPressed() && isSliding) {
        lastActionTime = now; 
        slideOffset = touch.x - touchStartX;
        int eyeOffset = constrain(slideOffset / 3, -20, 20); 
        if (eyeOffset != lastSlideEyeOffset && now - lastSlideDrawTime >= SLIDE_DRAW_INTERVAL) {
            fullDrawExpression(currentExp, eyeOffset);
            lastSlideEyeOffset = eyeOffset;
            lastSlideDrawTime = now;
        }
        return;
    }

    if (touch.wasReleased() && isSliding) {
        isSliding = false;
        unsigned long touchDuration = now - touchStartTime;

        if (abs(slideOffset) >= SLIDE_THRESHOLD) {
            if (slideOffset > 0) {
                currentExp = getNextExpression(currentExp);
            } else {
                currentExp = getPrevExpression(currentExp);
            }
            fullDrawExpression(currentExp, 0);
            previousExp = currentExp;
            lastActionTime = now;
        } 
        else if (touchDuration < TAP_MAX_DURATION && !isInAction) {
            runSmileHold(3000);
        } else {
            lastActionTime = now;
        }
    }

    if (checkButton() && !isInAction && !isSliding) {
        runSmileHold(3000);
    }

    // ---- 10秒完美待机逻辑 (时间已修改) ----
    if (currentExp == IDLE && !isInAction && !isSliding && !qrShowing && !remoteControlActive && (now - lastActionTime >= 10000)) {
        isInAction = true;
        bool interruptedByTouchOrBtn = false; 

        if (nextIdleAnim == 0) {
            currentExp = YAWN;
            fullDrawExpression(YAWN, 0);
            actionStartTime = now;

            while (millis() - actionStartTime < 2000) {
                M5.update();
                server.handleClient();
                if (checkShake()) break;
                if (checkButton() || M5.Touch.getDetail().wasPressed()) {
                    interruptedByTouchOrBtn = true;
                    break;
                }
                delay(50);
            }
            nextIdleAnim = 1;
        } else {
            lookDirection = (random(2) == 0) ? -1 : 1;
            drawLookDirection(lookDirection);
            actionStartTime = now;
            lastToggleTime = now;

            while (millis() - actionStartTime < 4000) {
                M5.update();
                server.handleClient();
                if (checkShake()) break;
                if (checkButton() || M5.Touch.getDetail().wasPressed()) {
                    interruptedByTouchOrBtn = true;
                    break;
                }

                if (millis() - lastToggleTime >= 1000) {
                    lookDirection *= -1;
                    drawLookDirection(lookDirection);
                    lastToggleTime = millis();
                }
                delay(50);
            }
            nextIdleAnim = 0;
        }

        currentExp = IDLE;
        isInAction = false;
        drawIdleFace();
        previousExp = IDLE;
        lastActionTime = millis();

        if (interruptedByTouchOrBtn) {
            runSmileHold(3000);
        }
    }

    // ---- 随机眨眼 ----
    if (!isInAction && currentExp == IDLE && !isSliding && !qrShowing && (now - lastBlinkTime >= 200)) {
        if (random(1, 16) == 1) {
            drawBlink();
        }
        lastBlinkTime = now;
    }

    // ---- 日常局部更新 ----
    if (!isInAction && !shockActive && !isSliding && !qrShowing && !remoteControlActive) {
        if (currentExp == IDLE) {
            if (currentExp != previousExp) {
                fullDrawExpression(currentExp, 0);
                previousExp = currentExp;
            } else {
                updateIdlePupil(currentExp); 
            }
        } else {
            if (currentExp != previousExp) {
                fullDrawExpression(currentExp, 0);
                previousExp = currentExp;
            }
        }
    }

    delay(30);
}