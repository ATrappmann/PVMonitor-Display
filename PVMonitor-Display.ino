// NAME: PVMonitor-Display
//
// DESC: Vizualize data from PVMonitor
//
// BOARD: ESP32 Dev Board
//  VSPI: MOSI=GPIO23(D23), MISO=GPIO19(D19), CLK=GPIO18(D18), CS=GPIO5(D5)
//
// CONNECTIONS:
//  ICST7789VW, 1,3" TFT display, 240x240
//  pin desc                        -> ESP32 pin
//  1   GND                         -> GND
//  2   VCC (3.3V)                  -> 3.3V
//  3   SCK (Serial clock)          -> D18 SCLK
//  4   SDA (Serial data input)     -> D23 MOSI
//  5   RES (Reset)                 -> D17 RST
//  6   DC  (Data/Command control)  -> D16 DC
//  7   BLK (Backlight control)     -> 3.3V (optional)
//

#define LED_BUILTIN 2

#define PVMON_STATUS_URL      "http://IP-Address>/status"
#define BATTERY_CAPACITY 5120 // Wh

#include <esp_task_wdt.h>
#include <esp_rom_sys.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // from Benoit Blanchon
#include <time.h>
/*
  Liste der Zeitzonen
  https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  Zeitzone CET = Central European Time -1 -> 1 Stunde zurück
  CEST = Central European Summer Time von
  M3 = März, 5.0 = Sonntag 5. Woche, 02 = 2 Uhr
  bis M10 = Oktober, 5.0 = Sonntag 5. Woche 03 = 3 Uhr
*/
// local time zone definition (Berlin)
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#include <Adafruit_ST7789.h>
#define TFT_CS -1   // Chip select line for TFT display
#define TFT_DC 16   // Data/command line for TFT
#define TFT_RST 17  // Reset line for TFT (or connect to +5V)

#define SCREEN_WIDTH 240   // 1,3" TFT display width, in pixels
#define SCREEN_HEIGHT 240  // 1,3" TFT display height, in pixels

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// global variables
uint32_t lastMillis = 0L;
HTTPClient pvMonClient;
float batVoltage = 0.0;
float invPower = 0.0;
float sumInvPower = 0.0;
float chargePower = 0.0;
float panelPower = 0.0;
float housePower = 0.0;
float batSOC = 0.0;
float sumHouseToday = 0.0;
float sumInverterToday = 0.0;

void drawPV(int x0, int y0, float power) {
  uint16_t w = SCREEN_WIDTH / 3 - 10;
  uint16_t h = SCREEN_HEIGHT / 2 - 10;
  display.drawRect(x0, y0, w, h, ST77XX_WHITE);

  // sun icon
  display.fillCircle(x0 + 35, y0 + 25, 12, ST77XX_WHITE);
  display.drawLine(x0 + 35, y0 + 4, x0 + 35, y0 + 46, ST77XX_WHITE);   // senkrecht +9
  display.drawLine(x0 + 14, y0 + 25, x0 + 56, y0 + 25, ST77XX_WHITE);  // wagerecht +9
  display.drawLine(x0 + 18, y0 + 8, x0 + 52, y0 + 42, ST77XX_WHITE);   // diagonal
  display.drawLine(x0 + 18, y0 + 42, x0 + 52, y0 + 8, ST77XX_WHITE);   // diagonal

  // sun text
  display.fillRect(x0+12, y0+60, w-15, 2*16+9, ST77XX_BLACK);
  display.setCursor(x0+12, y0+60);
  if (power <= 1000.0) {
    display.setCursor(x0+6+12, y0+60);
    display.print(power, 0);
    display.setCursor(x0+2*12, y0+85);
    display.print(" W");
  } else {
    display.setCursor(x0+12, y0+60);
    display.print(power/1000.0, 2);
    display.setCursor(x0+2*12, y0+85);
    display.print("KW");
  }
}

void drawHouse(int x0, int y0, float power) {
  uint16_t w = SCREEN_WIDTH / 3 - 10;
  uint16_t h = SCREEN_HEIGHT / 2 - 10;
  display.drawRect(x0, y0, w, h, ST77XX_WHITE);

  // house icon
  display.fillRect(x0+20, y0+30, 30, 20, ST77XX_WHITE);
  display.fillTriangle(x0+15, y0+30, x0+35, y0+4, x0+55, y0+30, ST77XX_WHITE);
  display.fillRect(x0+30, y0+40, 10, 10, ST77XX_BLACK);

  // house text
  display.fillRect(x0+12, y0+60, w-15, 2*16+9, ST77XX_BLACK);
  if (power <= 1000.0) {
    display.setCursor(x0+6+12, y0+60);
    display.print(power, 0);
    display.setCursor(x0+2*12, y0+85);
    display.print(" W");
  } else {
    display.setCursor(x0+12, y0+60);
    display.print(power/1000.0, 2);
    display.setCursor(x0+2*12, y0+85);
    display.print("KW");
  }
}

void drawGrid(int x0, int y0, float power) {
  uint16_t w = SCREEN_WIDTH / 3 - 10;
  uint16_t h = SCREEN_HEIGHT / 2 - 10;
  display.drawRect(x0, y0, w, h, ST77XX_WHITE);

  // grid icon
  display.drawLine(x0 + 20, y0 + 50, x0 + 35, y0 + 4, ST77XX_WHITE);
  display.drawLine(x0 + 35, y0 + 4, x0 + 50, y0 + 50, ST77XX_WHITE);

  display.drawLine(x0 + 25, y0 + 15, x0 + 45, y0 + 15, ST77XX_WHITE);
  display.drawLine(x0 + 15, y0 + 25, x0 + 55, y0 + 25, ST77XX_WHITE);

  display.drawLine(x0 + 22, y0 + 44, x0 + 46, y0 + 38, ST77XX_WHITE);
  display.drawLine(x0 + 24, y0 + 38, x0 + 48, y0 + 44, ST77XX_WHITE);

  // grid text
  display.fillRect(x0+12, y0+60, w-15, 2*16+9, ST77XX_BLACK);
  if (power <= 1000.0) {
    display.setCursor(x0+6+12, y0+60);
    display.print(power, 0);
    display.setCursor(x0+2*12, y0+85);
    display.print(" W");
  } else {
    display.setCursor(x0+12, y0+60);
    display.print(power/1000.0, 2);
    display.setCursor(x0+2*12, y0+85);
    display.print("KW");
  }
}

void drawBattery(int x0, int y0, float chargePower, float batVoltage, float batSOC) {
  if (isnan(batSOC)) batSOC = 0.0;

  uint16_t w = SCREEN_WIDTH / 3 * 2 - 5;
  uint16_t h = SCREEN_HEIGHT / 2 - 35; // 85
  display.drawRect(x0, y0, w, h, ST77XX_WHITE);

  // charge text
  display.fillRect(x0+10, y0+10, w-20, 16, ST77XX_BLACK);
  display.setCursor(x0+10, y0+10);
  if (abs(chargePower) <= 1000.0) {
    display.print(chargePower, 0);
    display.print('W');
  } else {
    display.print(chargePower/1000.0, 2);
    display.print("KW");
  }

  // SOC text
  display.setCursor(x0+75, y0+10);
  display.print(batSOC/1000.0, 1);
  display.print("KWh");

  // battery icon
  display.drawRect(x0+52, y0+30, 50, 25, ST77XX_WHITE); // y+4
  display.fillRect(x0+102, y0+38, 5, 8, ST77XX_WHITE); // y+8

  // battery level
  int level_width = int(44.0 * batSOC/BATTERY_CAPACITY);
  display.fillRect(x0+55, y0+33, 44, 19, ST77XX_BLACK); // y+3
  display.fillRect(x0+55, y0+33, level_width, 19, ST77XX_GREEN);

  // battery voltage text
  display.fillRect(x0+12, y0+61, 60, 16, ST77XX_BLACK); // y+6
  display.setCursor(x0+12, y0+61);
  display.print(batVoltage, 1);
  display.print('V');

  // battery level text
  display.fillRect(x0+w-49, y0+61, 48, 16, ST77XX_BLACK);
  display.setCursor(x0+w-49, y0+61);
  display.print(batSOC/BATTERY_CAPACITY*100.0, 0);
  display.print('%');
}

void drawAutarkie(int x0, int y0, float r, float autarkie, bool hasLegend = true) {
  if (isnan(autarkie)) autarkie = 0.0;
  if (autarkie > 1.0) autarkie = 1.0;

  static float lastAutarkie = -1.0;
  if (autarkie == lastAutarkie) return;
  lastAutarkie = autarkie;

  display.fillCircle(x0, y0, r, ST77XX_RED);

  if (autarkie < 0.25) {
    for (float alpha=0.0; alpha <= 360.0*autarkie; alpha+=0.5) {
      float x2 = x0 + r*sin(radians(alpha));
      float y2 = y0 - r*cos(radians(alpha));
      display.drawLine(x0,y0, x2,y2, ST77XX_GREEN);
    }
  }
  else if (autarkie < 0.5) {
    myFillCircleHelper(x0,y0, r, 0b0010, ST77XX_GREEN); // oben rechts
    for (float alpha=90.0; alpha <= 360.0*autarkie; alpha+=0.5) {
      float x2 = x0 + r*sin(radians(alpha));
      float y2 = y0 - r*cos(radians(alpha));
      display.drawLine(x0,y0, x2,y2, ST77XX_GREEN);
    }
  }
  else if (autarkie < 0.75) {
    myFillCircleHelper(x0,y0, r, 0b0110, ST77XX_GREEN); // oben und unten rechts
    for (float alpha=180.0; alpha <= 360.0*autarkie; alpha+=0.5) {
      float x2 = x0 + r*sin(radians(alpha));
      float y2 = y0 - r*cos(radians(alpha));
      display.drawLine(x0,y0, x2,y2, ST77XX_GREEN);
    }
  }
  else {
    myFillCircleHelper(x0,y0, r, 0b1110, ST77XX_GREEN); // oben und unten rechts und unten links
    for (float alpha=270.0; alpha <= 360.0*autarkie; alpha+=0.5) {
      float x2 = x0 + r*sin(radians(alpha));
      float y2 = y0 - r*cos(radians(alpha));
      display.drawLine(x0,y0, x2,y2, ST77XX_GREEN);
    }
  }
  display.drawCircle(x0, y0, r, ST77XX_WHITE);

  if (hasLegend) {
    display.fillRect(x0-r+12, y0+r+15, 2*r,16, ST77XX_BLACK);
    display.setCursor(x0-r+12, y0+r+15);
    display.print(autarkie*100.0, 0);
    display.print('%');
  }
}

void myFillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
  // corners:
  // 0b0001 - oben links
  // 0b0010 - oben rechts
  // 0b0100 - unten rechts
  // 0b1000 - unten links

  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (cornername & 0x4) { // unten rechts
      display.writeFastVLine(x0+x, y0, y+1, color);
      display.writeFastVLine(x0+y, y0, x+1, color);
    }
    if (cornername & 0x2) { // oben rechts
      display.writeFastVLine(x0+x, y0-y, y, color);
      display.writeFastVLine(x0+y, y0-x, x, color);
    }
    if (cornername & 0x8) { // unten links
      display.writeFastVLine(x0-y, y0, x+1, color);
      display.writeFastVLine(x0+1-x, y0, y+1, color);
    }
    if (cornername & 0x1) { // oben links
      display.writeFastVLine(x0-y, y0-x, x, color);
      display.writeFastVLine(x0-x, y0-y, y, color);
    }
  }
}

String httpClientError2String(int httpCode) {
  switch (httpCode) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "Connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "Send header failed";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "Send payload failed";
    case HTTPC_ERROR_NOT_CONNECTED: return "Not connected";
    case HTTPC_ERROR_CONNECTION_LOST: return "Connection lost";
    case HTTPC_ERROR_NO_STREAM: return "No stream";
    case HTTPC_ERROR_NO_HTTP_SERVER: return "No HTTP server";
    case HTTPC_ERROR_TOO_LESS_RAM: return "Not enough RAM";
    case HTTPC_ERROR_ENCODING: return "Encoding";
    case HTTPC_ERROR_STREAM_WRITE: return "Stream write";
    case HTTPC_ERROR_READ_TIMEOUT: return "Read timeout";
    default: return "Unknown";
  }
}

esp_task_wdt_config_t wdtConfig { 
  .timeout_ms = 30*1000,  // 30s
  .trigger_panic = true
};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  Serial.println();

  // initialize display
  display.init(SCREEN_WIDTH, SCREEN_HEIGHT, SPI_MODE3); // important: set mode 3 if there is no CS pin on display
  display.setTextColor(ST77XX_WHITE);  // Draw white text
  display.setTextSize(2);              // 12x16
  //display.setRotation(2);              // upside down
  display.fillScreen(ST77XX_WHITE);

  // say hello
  Serial.println("PVMonitor-Display " __DATE__ " " __TIME__);
  Serial.printf("Using pins: MOSI=%d, (MISO=%d), SCK=%d, (SS=%d), DC=%d, RST=%d\n", MOSI, MISO, SCK, SS, TFT_DC, TFT_RST);
  Serial.printf("Reset reason core %d: %d\n", xPortGetCoreID(), esp_rom_get_reset_reason(xPortGetCoreID()));
  
  // init watchdog
  Serial.println(F("Configuring WDT..."));
  esp_task_wdt_deinit();
  esp_task_wdt_init(&wdtConfig); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  // initialize ESP WiFi connection
  Serial.print(F("Connecting to WiFi..."));
  WiFi.setHostname("PVMonitor-Display");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);  // station mode
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP().toString());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());

  pvMonClient.setReuse(true);

  // get local time
 /*
  Serial.print(F("Getting time..."));
  configTime(3600, 3600, "pool.ntp.org");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print('.');
  }  
  Serial.println();
  Serial.printf("%d-%02d-%02d %02d:%02d:%02d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
 */

  // display initial values
  display.fillScreen(ST77XX_BLACK);
  drawPV(0,0, 0.0);
  drawHouse(85,0, 0.0);
  drawGrid(169,0, 0.0);
  drawBattery(0,129, 0.0, 0.0, 0.0);
  drawAutarkie(200,170, 30, 0.0);

  Serial.println(F("Running..."));
}

void loop() {
  esp_task_wdt_reset();

  uint32_t currentMillis = millis();
  if (currentMillis - lastMillis < 2000L) return; // update display every 2s

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("ERROR: No WiFi connection..."));
    ESP.restart();
  }

  pvMonClient.begin(PVMON_STATUS_URL);
  int httpCode = pvMonClient.GET();
  if (HTTP_CODE_OK == httpCode) {
    String line = pvMonClient.getString();
    Serial.println(line);

    JsonDocument jsonDoc;
    if (deserializeJson(jsonDoc, line)) {
      Serial.println(F("ERROR: JSON parsing failed!")); 
    }

    batVoltage = jsonDoc["meanBatVoltage"];
    panelPower = jsonDoc["panelPower"];
    invPower = jsonDoc["inverterPower"];
    chargePower = jsonDoc["chargePower"];
    batSOC = jsonDoc["bat_soc"];
	  housePower = jsonDoc["housePower"];
    sumHouseToday = jsonDoc["sumHouseToday"];
    sumInverterToday = jsonDoc["sumInverterToday"];

    // display collected data
    if (panelPower > 0) {
      drawPV(0,0, panelPower);
      display.fillTriangle(70,50, 85,55, 70,60, ST77XX_BLUE); // PV to House
    }
    else {
      drawPV(0,0, 0);
      display.fillTriangle(70,50, 85,55, 70,60, ST77XX_BLACK); // PV to House
    }

    drawBattery(0,129, chargePower, batVoltage, batSOC);
    if (chargePower > 0) {
      display.fillTriangle(30,110, 40,110, 35,128, ST77XX_BLUE); // PV to Battery
      display.fillTriangle(119,110, 114,128, 124,128, ST77XX_BLACK); // Inverter off
    } else {
      display.fillTriangle(30,110, 40,110, 35,128, ST77XX_BLACK); // PV to Battery
      display.fillTriangle(119,110, 114,128, 124,128, ST77XX_BLUE); // Inverter on
    }

    drawGrid(169,0, housePower);
    drawHouse(85,0, housePower+invPower);

    drawAutarkie(120,26, 12, invPower/(housePower+invPower), false);
    drawAutarkie(200,170, 30, sumInverterToday/(sumHouseToday+sumInverterToday));

    if (housePower > 0) {
      display.fillTriangle(157,55, 168,50, 168,60, ST77XX_BLUE); // Grid to House
    } else {
      display.fillTriangle(157,55, 168,50, 168,60, ST77XX_BLACK); // Grid to House
    }
  }
  else {
    Serial.printf("ERROR: Cannot get status from PVMon at %s, httpCode=%d (%s)\n", PVMON_STATUS_URL, httpCode, httpClientError2String(httpCode).c_str());
  }
  pvMonClient.end(); // clear further data
  
  display.fillRect(0,223, 155,16, ST77XX_BLACK);
  display.setCursor(0, 223);
  display.print(httpCode);

  lastMillis = millis();
}
