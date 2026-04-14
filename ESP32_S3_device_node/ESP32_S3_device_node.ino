/*
  ESP32_S3_device_node.ino
  -----------------------------------------
  功能：
  1. 连接 WiFi
  2. 连接树莓派 coordinator 的 WebSocket
  3. 接收 JSON 数组命令
  4. 控制两个风扇（PWM）
  5. 控制一条 WS2812B 灯带，逻辑分成两个 light
  6. 回传 ACK / 执行结果

  当前硬件规划：
  - 板子：ESP32-S3-DevKitC-1 v1.1
  - livingroom_fan -> GPIO4
  - bedroom_fan    -> GPIO5
  - WS2812B 灯带   -> GPIO6（数据线）
  - 灯带总灯珠数   -> 60
  - 前 30 颗       -> livingroom_light
  - 后 30 颗       -> bedroom_light

  依赖库：
  - ArduinoWebsockets
  - ArduinoJson
  - Adafruit NeoPixel
*/

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

using namespace websockets;

// =====================================================
// 一、WiFi 与 WebSocket 配置
// =====================================================

// ===== WiFi 配置 =====
const char* WIFI_SSID = "GL-AR300M-cc1";
const char* WIFI_PASSWORD = "goodlife";

// ===== 树莓派 coordinator 的 WebSocket 地址 =====
const char* WS_HOST = "192.168.8.228";
const uint16_t WS_PORT = 8001;
const char* WS_PATH = "/ws/device";

// WebSocket 客户端对象
WebsocketsClient webSocket;

// WebSocket 连接状态标志
bool wsConnected = false;

// 重连控制
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL_MS = 5000;

// =====================================================
// 二、硬件引脚与灯带配置
// =====================================================

// ===== 风扇引脚 =====
const int LIVINGROOM_FAN_PIN = 4;
const int BEDROOM_FAN_PIN = 5;

// ===== WS2812B 灯带引脚 =====
// 这里先按 GPIO6 预留，后面接线如果你改了，再改这个常量
const int LED_STRIP_PIN = 6;

// ===== WS2812B 灯带参数 =====
const int TOTAL_LED_COUNT = 60;

// 客厅灯：前 30 颗
const int LR_LIGHT_START = 0;
const int LR_LIGHT_END = 29;

// 卧室灯：后 30 颗
const int BR_LIGHT_START = 30;
const int BR_LIGHT_END = 59;

// 创建灯带对象
Adafruit_NeoPixel strip(TOTAL_LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// =====================================================
// 三、PWM 配置（用于两个风扇）
// =====================================================

// ESP32 Arduino 下 LEDC PWM 配置
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;  // 8位，占空比范围 0~255

// 给两个风扇分配 PWM 通道
const int LR_FAN_PWM_CHANNEL = 0;
const int BR_FAN_PWM_CHANNEL = 1;

// 最大 duty（8位）
const int PWM_MAX_DUTY = 255;

// =====================================================
// 四、设备状态结构体
// =====================================================

// 灯状态
struct LightState {
  bool power;
  int brightness;   // 0~100
  int colorTemp;    // 2700~6500
};

// 风扇状态
struct FanState {
  bool power;
  int speed;        // 0~100
};

// 整体状态
struct DeviceStateStore {
  LightState livingroomLight;
  LightState bedroomLight;
  FanState livingroomFan;
  FanState bedroomFan;
};

// 全局状态
DeviceStateStore g_state = {
  {false, 100, 4000},   // livingroomLight
  {false, 100, 4000},   // bedroomLight
  {false, 0},           // livingroomFan
  {false, 0}            // bedroomFan
};

// =====================================================
// 五、工具函数
// =====================================================

/*
  把 brightness 百分比（0~100）转换成 PWM duty（0~255）
*/
int percentToDuty(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return (percent * PWM_MAX_DUTY) / 100;
}

/*
  限制 brightness 范围
*/
int clampBrightness(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

/*
  限制 color temperature 范围
*/
int clampColorTemp(int value) {
  if (value < 2700) return 2700;
  if (value > 6500) return 6500;
  return value;
}

/*
  通过设备名判断是不是 light
*/
bool isLightDevice(const String& device) {
  return device == "livingroom_light" || device == "bedroom_light";
}

/*
  通过设备名判断是不是 fan
*/
bool isFanDevice(const String& device) {
  return device == "livingroom_fan" || device == "bedroom_fan";
}

/*
  根据 color_temp + brightness 粗略生成 RGB
  说明：
  - 这不是严格色温算法
  - 当前阶段做的是“工程上可用的近似”
  - 2700K 更暖，6500K 更冷
*/
void colorTempToRGB(int colorTemp, int brightness, uint8_t &r, uint8_t &g, uint8_t &b) {
  colorTemp = clampColorTemp(colorTemp);
  brightness = clampBrightness(brightness);

  // 用 2700K ~ 6500K 映射一个 0~1 的比例
  float t = (float)(colorTemp - 2700) / (6500.0 - 2700.0);

  // 暖色时红高蓝低，冷色时蓝高红略低
  float rf = 255.0;
  float gf = 160.0 + 95.0 * t;
  float bf = 60.0 + 195.0 * t;

  // brightness 缩放
  float scale = brightness / 100.0;
  r = (uint8_t)(rf * scale);
  g = (uint8_t)(gf * scale);
  b = (uint8_t)(bf * scale);
}

/*
  打印当前所有设备状态到串口
*/
void printStateToSerial() {
  Serial.println("========== DEVICE STATE ==========");

  Serial.print("[LR Light] power=");
  Serial.print(g_state.livingroomLight.power);
  Serial.print(" brightness=");
  Serial.print(g_state.livingroomLight.brightness);
  Serial.print(" colorTemp=");
  Serial.println(g_state.livingroomLight.colorTemp);

  Serial.print("[BR Light] power=");
  Serial.print(g_state.bedroomLight.power);
  Serial.print(" brightness=");
  Serial.print(g_state.bedroomLight.brightness);
  Serial.print(" colorTemp=");
  Serial.println(g_state.bedroomLight.colorTemp);

  Serial.print("[LR Fan] power=");
  Serial.print(g_state.livingroomFan.power);
  Serial.print(" speed=");
  Serial.println(g_state.livingroomFan.speed);

  Serial.print("[BR Fan] power=");
  Serial.print(g_state.bedroomFan.power);
  Serial.print(" speed=");
  Serial.println(g_state.bedroomFan.speed);

  Serial.println("==================================");
}

// =====================================================
// 六、硬件初始化
// =====================================================

/*
  初始化两个风扇的 PWM
*/
void initFanPWM() {
  // 在 ESP32 Arduino core 3.x 中，新的写法是把 PWM 直接附着到引脚
  // ledcAttach(pin, freq, resolution)
  bool lrOk = ledcAttach(LIVINGROOM_FAN_PIN, PWM_FREQ, PWM_RESOLUTION);
  bool brOk = ledcAttach(BEDROOM_FAN_PIN, PWM_FREQ, PWM_RESOLUTION);

  if (!lrOk) {
    Serial.println("[HW] Failed to attach PWM to livingroom fan pin");
  }
  if (!brOk) {
    Serial.println("[HW] Failed to attach PWM to bedroom fan pin");
  }

  // 默认占空比设为 0，表示风扇初始关闭
  ledcWrite(LIVINGROOM_FAN_PIN, 0);
  ledcWrite(BEDROOM_FAN_PIN, 0);

  Serial.println("[HW] Fan PWM initialized");
}

/*
  初始化灯带
*/
void initLedStrip() {
  strip.begin();
  strip.show();   // 全灭
  Serial.println("[HW] WS2812B strip initialized");
}

/*
  初始化所有硬件
*/
void initHardware() {
  initFanPWM();
  initLedStrip();
}

// =====================================================
// 七、灯控制函数
// =====================================================

/*
  把某一段灯带设置成指定颜色
*/
void setLightSegmentColor(int startIdx, int endIdx, uint8_t r, uint8_t g, uint8_t b) {
  for (int i = startIdx; i <= endIdx; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

/*
  按设备名刷新某个 light 的显示效果
  这里统一根据：
  - power
  - brightness
  - colorTemp
  来决定灯带实际颜色
*/
void refreshLightByDevice(const String& device) {
  uint8_t r, g, b;

  if (device == "livingroom_light") {
    if (!g_state.livingroomLight.power || g_state.livingroomLight.brightness <= 0) {
      setLightSegmentColor(LR_LIGHT_START, LR_LIGHT_END, 0, 0, 0);
      return;
    }

    colorTempToRGB(
      g_state.livingroomLight.colorTemp,
      g_state.livingroomLight.brightness,
      r, g, b
    );
    setLightSegmentColor(LR_LIGHT_START, LR_LIGHT_END, r, g, b);
  }
  else if (device == "bedroom_light") {
    if (!g_state.bedroomLight.power || g_state.bedroomLight.brightness <= 0) {
      setLightSegmentColor(BR_LIGHT_START, BR_LIGHT_END, 0, 0, 0);
      return;
    }

    colorTempToRGB(
      g_state.bedroomLight.colorTemp,
      g_state.bedroomLight.brightness,
      r, g, b
    );
    setLightSegmentColor(BR_LIGHT_START, BR_LIGHT_END, r, g, b);
  }
}

/*
  灯开
*/
void lightOn(const String& device) {
  if (device == "livingroom_light") {
    g_state.livingroomLight.power = true;
    if (g_state.livingroomLight.brightness <= 0) {
      g_state.livingroomLight.brightness = 100;
    }
  } else if (device == "bedroom_light") {
    g_state.bedroomLight.power = true;
    if (g_state.bedroomLight.brightness <= 0) {
      g_state.bedroomLight.brightness = 100;
    }
  }

  refreshLightByDevice(device);

  Serial.print("[LIGHT] ON -> ");
  Serial.println(device);
}

/*
  灯关
*/
void lightOff(const String& device) {
  if (device == "livingroom_light") {
    g_state.livingroomLight.power = false;
  } else if (device == "bedroom_light") {
    g_state.bedroomLight.power = false;
  }

  refreshLightByDevice(device);

  Serial.print("[LIGHT] OFF -> ");
  Serial.println(device);
}

/*
  设置亮度
*/
void setLightBrightness(const String& device, int brightness) {
  brightness = clampBrightness(brightness);

  if (device == "livingroom_light") {
    g_state.livingroomLight.brightness = brightness;
    g_state.livingroomLight.power = (brightness > 0);
  } else if (device == "bedroom_light") {
    g_state.bedroomLight.brightness = brightness;
    g_state.bedroomLight.power = (brightness > 0);
  }

  refreshLightByDevice(device);

  Serial.print("[LIGHT] BRIGHTNESS -> ");
  Serial.print(device);
  Serial.print(" = ");
  Serial.println(brightness);
}

/*
  设置色温
*/
void setLightColorTemp(const String& device, int colorTemp) {
  colorTemp = clampColorTemp(colorTemp);

  if (device == "livingroom_light") {
    g_state.livingroomLight.colorTemp = colorTemp;
  } else if (device == "bedroom_light") {
    g_state.bedroomLight.colorTemp = colorTemp;
  }

  refreshLightByDevice(device);

  Serial.print("[LIGHT] COLOR_TEMP -> ");
  Serial.print(device);
  Serial.print(" = ");
  Serial.println(colorTemp);
}

/*
  brighten：默认 +10
*/
void brightenLight(const String& device) {
  if (device == "livingroom_light") {
    setLightBrightness(device, g_state.livingroomLight.brightness + 10);
  } else if (device == "bedroom_light") {
    setLightBrightness(device, g_state.bedroomLight.brightness + 10);
  }
}

/*
  dim：默认 -10
*/
void dimLight(const String& device) {
  if (device == "livingroom_light") {
    setLightBrightness(device, g_state.livingroomLight.brightness - 10);
  } else if (device == "bedroom_light") {
    setLightBrightness(device, g_state.bedroomLight.brightness - 10);
  }
}

// =====================================================
// 八、风扇控制函数
// =====================================================

/*
  设置风扇速度（0~100）
*/
void setFanSpeed(const String& device, int speed) {
  speed = clampBrightness(speed);  // speed 同样限制在 0~100
  int duty = percentToDuty(speed);

  if (device == "livingroom_fan") {
    g_state.livingroomFan.speed = speed;
    g_state.livingroomFan.power = (speed > 0);
    ledcWrite(LIVINGROOM_FAN_PIN, duty);
  }
  else if (device == "bedroom_fan") {
    g_state.bedroomFan.speed = speed;
    g_state.bedroomFan.power = (speed > 0);
    ledcWrite(BEDROOM_FAN_PIN, duty);
  }

  Serial.print("[FAN] SPEED -> ");
  Serial.print(device);
  Serial.print(" = ");
  Serial.print(speed);
  Serial.print("%, duty=");
  Serial.println(duty);
}


/*
  风扇开：默认 100%
*/
void fanOn(const String& device) {
  setFanSpeed(device, 100);
}

/*
  风扇关：0%
*/
void fanOff(const String& device) {
  setFanSpeed(device, 0);
}

// =====================================================
// 九、回传 ACK / 结果
// =====================================================

/*
  向 coordinator 回传一个简单 ACK
  这里先保持结构简单，后面你可以扩展成更完整的执行结果上报
*/
void sendAck(const String& device, const String& action, const String& status) {
  if (!wsConnected) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "ack";
  doc["device"] = device;
  doc["action"] = action;
  doc["status"] = status;

  String out;
  serializeJson(doc, out);

  webSocket.send(out);

  Serial.print("[WS] Sent ACK: ");
  Serial.println(out);
}

// =====================================================
// 十、命令执行入口
// =====================================================

/*
  执行单条 command object
  输入 JSON 对象格式对应你的 command schema：
  {
    "device": "...",
    "location": "...",
    "action": "...",
    "parameters": {...}
  }
*/
void handleSingleCommand(JsonObject command) {
  String device = command["device"] | "";
  String location = command["location"] | "";
  String action = command["action"] | "";

  JsonObject parameters = command["parameters"].as<JsonObject>();

  Serial.println("------ COMMAND START ------");
  Serial.print("device   = "); Serial.println(device);
  Serial.print("location = "); Serial.println(location);
  Serial.print("action   = "); Serial.println(action);
  Serial.println("---------------------------");

  // 先判断设备是否合法
  if (!isLightDevice(device) && !isFanDevice(device)) {
    Serial.println("[CMD] Unsupported device");
    sendAck(device, action, "unsupported_device");
    return;
  }

  // ========== light ==========
  if (isLightDevice(device)) {
    if (action == "on") {
      lightOn(device);
      sendAck(device, action, "ok");
    }
    else if (action == "off") {
      lightOff(device);
      sendAck(device, action, "ok");
    }
    else if (action == "brighten") {
      brightenLight(device);
      sendAck(device, action, "ok");
    }
    else if (action == "dim") {
      dimLight(device);
      sendAck(device, action, "ok");
    }
    else if (action == "set_brightness") {
      int brightness = parameters["brightness"] | 100;
      setLightBrightness(device, brightness);
      sendAck(device, action, "ok");
    }
    else if (action == "set_color_temp") {
      int colorTemp = parameters["color_temp"] | 4000;
      setLightColorTemp(device, colorTemp);
      sendAck(device, action, "ok");
    }
    else {
      Serial.println("[CMD] Unsupported light action");
      sendAck(device, action, "unsupported_action");
    }
    return;
  }

  // ========== fan ==========
  if (isFanDevice(device)) {
    if (action == "on") {
      fanOn(device);
      sendAck(device, action, "ok");
    }
    else if (action == "off") {
      fanOff(device);
      sendAck(device, action, "ok");
    }
    else {
      // 根据你的 schema，fan 当前只支持 on / off
      // 但因为你要求 PWM speed 直接写进去，这里加一个扩展入口：
      // 如果 coordinator 以后发 set_speed，我们也能执行
      if (action == "set_speed") {
        int speed = parameters["speed"] | 100;
        setFanSpeed(device, speed);
        sendAck(device, action, "ok");
      } else {
        Serial.println("[CMD] Unsupported fan action");
        sendAck(device, action, "unsupported_action");
      }
    }
    return;
  }
}

/*
  处理服务器发来的消息
  当前约定：
  - 如果是 ack: 开头，表示 coordinator 对 hello 的回应
  - 如果是 JSON 数组，则按 command array 解析
*/
void handleIncomingMessage(const String& msg) {
  Serial.print("[WS] Received: ");
  Serial.println(msg);

  // 1. 先兼容最开始的 hello -> ack 文本通信
  if (msg.startsWith("ack:")) {
    Serial.println("[WS] Text ACK received from coordinator");
    return;
  }

  // 2. 尝试按 JSON 数组解析
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.print("[JSON] Parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("[JSON] Message is not a JSON array");
    return;
  }

  JsonArray arr = doc.as<JsonArray>();

  for (JsonObject command : arr) {
    handleSingleCommand(command);
  }

  printStateToSerial();
}

// =====================================================
// 十一、WiFi 与 WebSocket 连接逻辑
// =====================================================

/*
  组装 WebSocket URL
*/
String buildWebSocketURL() {
  String url = "ws://";
  url += WS_HOST;
  url += ":";
  url += String(WS_PORT);
  url += WS_PATH;
  return url;
}

/*
  连接 WiFi
*/
void connectToWiFi() {
  Serial.println("[WiFi] Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] WiFi connected");
    Serial.print("[WiFi] Local IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] WiFi connection failed");
  }
}

/*
  连接 WebSocket
*/
void connectToWebSocket() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WS] WiFi not connected, skip WebSocket connect");
    return;
  }

  String wsURL = buildWebSocketURL();
  Serial.print("[WS] Connecting to: ");
  Serial.println(wsURL);

  webSocket.onMessage([](WebsocketsMessage message) {
    handleIncomingMessage(message.data());
  });

  webSocket.onEvent([](WebsocketsEvent event, String data) {
    switch (event) {
      case WebsocketsEvent::ConnectionOpened:
        wsConnected = true;
        Serial.println("[WS] WebSocket connected");

        // 连接成功后先发一个 hello，验证最基础通信
        webSocket.send("hello from esp32");
        Serial.println("[WS] Sent hello from esp32");
        break;

      case WebsocketsEvent::ConnectionClosed:
        wsConnected = false;
        Serial.println("[WS] WebSocket disconnected");
        break;

      case WebsocketsEvent::GotPing:
        Serial.println("[WS] Got Ping");
        break;

      case WebsocketsEvent::GotPong:
        Serial.println("[WS] Got Pong");
        break;
    }
  });

  bool ok = webSocket.connect(wsURL);

  if (!ok) {
    wsConnected = false;
    Serial.println("[WS] WebSocket connect failed");
  }
}

// =====================================================
// 十二、setup / loop
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("========================================");
  Serial.println("ESP32-S3 Device Node Starting...");
  Serial.println("Devices:");
  Serial.println("- livingroom_light  -> WS2812B LEDs 0~29");
  Serial.println("- bedroom_light     -> WS2812B LEDs 30~59");
  Serial.println("- livingroom_fan    -> GPIO4 PWM");
  Serial.println("- bedroom_fan       -> GPIO5 PWM");
  Serial.println("========================================");

  initHardware();
  connectToWiFi();
  connectToWebSocket();

  printStateToSerial();
}

void loop() {
  // WiFi 掉线则尝试重连
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] WiFi lost, reconnecting...");
    connectToWiFi();
  }

  // WiFi 正常时，维护 WebSocket 连接
  if (WiFi.status() == WL_CONNECTED) {
    if (wsConnected) {
      webSocket.poll();
    } else {
      unsigned long now = millis();
      if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
        lastReconnectAttempt = now;
        connectToWebSocket();
      }
    }
  }

  delay(10);
}