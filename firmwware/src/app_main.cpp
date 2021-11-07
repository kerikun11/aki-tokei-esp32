/**
 * @brief   ESP32 AKI-TOKEI-ESP32
 * @author  kerikun11 (GitHub: kerikun11)
 * @date    2021.10.09
 */

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp32-hal-log.h>

/**
 * @brief 7セグLEDの各桁のアノードのピン番号
 */
const int pins_led_anode[4] = {12, 13, 14, 15};
/**
 * @brief 7セグLEDの各バーのカソードのピン番号
 */
const int pins_led_cathode[7] = {2, 4, 5, 16, 17, 18, 19};
const int pin_led_colon = 25; //**< @brief コロンのLEDのピン
const int pin_button = 0;     //**< @brief ボタンピン

/**
 * @brief NTPにより現在時刻を合わせる関数
 */
void setTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    log_e("Failed to obtain time");
    return;
  }
  time_t t = time(NULL);
  log_i("Time: %s", ctime(&t));
}

/**
 * @brief WiFiイベントが発生したときによばれる関数
 * @details WiFiに接続されたら時刻合わせをする
 */
void WiFiEvent(WiFiEvent_t event) {
  log_i("[WiFi-event] event: %d\n", event);
  switch (event) {

  case SYSTEM_EVENT_STA_GOT_IP:
    setTime(); //< 時刻合わせ
    break;
  default:
    break;
  }
}

/**
 * @brief OTA処理をするタスクの関数
 * @details FreeRTOSにより実行
 */
void ota_task(void *arg) {
  ArduinoOTA.setHostname("AKI-TOKEI");
  ArduinoOTA.begin();
  portTickType xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, 1 / portTICK_RATE_MS);
    ArduinoOTA.handle();
  }
}

/**
 * @brief smartConfig処理をするタスクの関数
 * @details FreeRTOSにより実行，ボタンが押されたらsmartConfig処理を実行
 */
void smartConfig_task(void *arg) {
  pinMode(pin_button, INPUT_PULLUP);
  portTickType xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, 1 / portTICK_RATE_MS);
    if (digitalRead(pin_button) == LOW) {
      log_i("WiFi.beginSmartConfig();");
      WiFi.beginSmartConfig();
      while (!WiFi.smartConfigDone()) {
        delay(100);
      }
    }
  }
}

/**
 * @brief 7セグLEDの表示処理関数
 * @details I/Oピンのドライブ関数
 */
static void print_column(int8_t col) {
  for (int i = 0; i < 4; i++)
    digitalWrite(pins_led_anode[i], col == i);
}
/**
 * @brief 7セグLEDの表示処理関数
 * @details I/Oピンのドライブ関数
 */
static void print_pattern(uint8_t pat) {
  for (int i = 0; i < 7; i++)
    digitalWrite(pins_led_cathode[i], (pat >> i) & 1);
}

/**
 * @brief コロンを点滅させるタスクの関数
 * @details FreeRTOSにより実行
 */
void colon_task(void *arg) {
  pinMode(pin_led_colon, OUTPUT);
  portTickType xLastWakeTime;
  xLastWakeTime = xTaskGetTickCount();
  while (1) {
    for (int i = 0; i < 50; i++) {
      digitalWrite(pin_led_colon, HIGH);
      vTaskDelayUntil(&xLastWakeTime, 1 / portTICK_RATE_MS);
      digitalWrite(pin_led_colon, LOW);
      vTaskDelayUntil(&xLastWakeTime, 9 / portTICK_RATE_MS);
    }
    digitalWrite(pin_led_colon, LOW);
    vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_RATE_MS);
  }
}

/** 7セグLEDのある1文字を表示する関数
  @brief  7セグLEDのある1文字を表示する関数
  @param col 何桁目を表示するか選択
  @param c 表示する文字(char)
*/
void print_7seg(uint8_t col, char c) {
  // BGACDEHF
  static uint8_t pattern[10] = {
      0b00111111, // 0
      0b00000110, // 1
      0b01011011, // 2
      0b01001111, // 3
      0b01100110, // 4
      0b01101101, // 5
      0b01111101, // 6
      0b00100111, // 7
      0b01111111, // 8
      0b01101111, // 9
  };
  uint8_t pat;
  switch (c) {
  case ' ':
    pat = 0b00000000;
    break;
  case ':':
    pat = 0b10100000;
    break;
  case '-':
    pat = 0b01000000;
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    pat = pattern[c - '0'];
    break;
  default:
    pat = 0b00000010;
    break;
  }
  print_column(-1);
  print_pattern(pat);
  print_column(col);
}
/**
 * @brief ダイナミクス点灯処理の関数
 * @details この関数を定期的に呼ぶと7セグLEDがダイナミック点灯する
 */
void dynamic(void) {
  static uint8_t dynamic_counter = 1;
  static struct tm *t_st;
  time_t t = time(NULL);
  t_st = localtime(&t);
  switch (dynamic_counter) {
  case 1:
    print_7seg(0, (t_st->tm_hour / 10) ? ('0' + (t_st->tm_hour / 10)) : (' '));
    break;
  case 2:
    print_7seg(1, '0' + (t_st->tm_hour % 10));
    break;
  case 3:
    print_7seg(2, '0' + (t_st->tm_min / 10));
    break;
  case 4:
    print_7seg(3, '0' + (t_st->tm_min % 10));
    dynamic_counter = 0;
    break;
  default:
    dynamic_counter = 0;
    break;
  }
  dynamic_counter++;
}

/**
 * @brief 7セグLEDをダイナミック点灯するタスクの関数
 * @details FreeRTOSにより実行
 */
void dynamic_task(void *arg) {
  for (int i = 0; i < 4; i++)
    pinMode(pins_led_anode[i], OUTPUT);
  for (int i = 0; i < 7; i++)
    pinMode(pins_led_cathode[i], OUTPUT);

  portTickType xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, 2 / portTICK_RATE_MS);
    dynamic();
  }
}

/**
 * @brief 定期的に時刻合わせをするタスクの関数
 * @details FreeRTOSにより実行
 */
void time_task(void *arg) {
  portTickType xLastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&xLastWakeTime, 1000 * 60 * 60 / portTICK_RATE_MS);
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin();
      WiFi.waitForConnectResult();
    }
    setTime();
  }
}

/**
 * @brief 初期化関数
 */
void setup() {
  Serial.begin(115200);
  log_i("Hello, this is ESP32.");

  // イベントハンドラの割り当て
  WiFi.onEvent(WiFiEvent);

  // 前回接続したWiFiに自動接続
  WiFi.begin();
  // WiFi.begin("SSID", "password");

  // タイムゾーンとNTPサーバーの設定
  configTzTime("JST-9", "ntp.jst.mfeed.ad.jp", "ntp.nict.jp", "pool.ntp.org");

  // 各タスクを生成
  xTaskCreate(ota_task, "ota", 4096, NULL, 1, NULL);
  xTaskCreate(smartConfig_task, "smartConfig", 4096, NULL, 1, NULL);
  xTaskCreate(dynamic_task, "dynamic", 4096, NULL, 1, NULL);
  xTaskCreate(colon_task, "colon", 4096, NULL, 1, NULL);
  xTaskCreate(time_task, "time", 4096, NULL, 1, NULL);
}

/**
 * @brief メインループ
 * @details すべての処理はFreeRTOSのタスクで行うので，ここでは何もしない
 */
void loop() {}
