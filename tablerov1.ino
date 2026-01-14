#include <TFT_eSPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

// ==================== TFT ====================
TFT_eSPI tft = TFT_eSPI();

// ==================== TEMP (DS18B20) ====================
#define ONE_WIRE_BUS 27
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

// ==================== VOLTAJE (ADC) ====================
#define BAT_ADC_PIN 36

const float R1 = 100000.0;
const float R2 = 27000.0;
const float DIV_FACTOR = (R1 + R2) / R2;

const float VREF = 3.30;
const int   ADC_MAX = 4095;
const int   ADC_SAMPLES = 20;

// ==================== UI ====================
unsigned long lastUpdate = 0;

// Cache para evitar parpadeo
float lastTC = NAN;
float lastVB = NAN;

// Colores base
const uint16_t COL_BG     = TFT_BLACK;
const uint16_t COL_LINE   = 0x39C7;   // gris oscuro
const uint16_t COL_SUBTXT = 0xBDF7;   // gris claro
const uint16_t COL_PANEL  = 0x1082;   // panel oscuro

// TEMP: escala hasta 100 (como pediste)
const float TEMP_MIN = 0.0;
const float TEMP_MAX = 100.0;

// Umbrales voltaje (ajústalos si quieres)
const float VOLT_OK_MIN = 12.0;
const float VOLT_OK_MAX = 15.0;
// Umbral voltaje para considerar "sin conexión"
const float VOLT_DISCONNECT = 0.10;

// Colores por rangos de temperatura (RGB565 explícito)
const uint16_t TEMP_COLD       = 0x6DFF; // 0-50 celeste
const uint16_t TEMP_WARMING    = 0xFFB2; // 50-75 amarillo pastel
const uint16_t TEMP_OPTIMAL    = 0x07E0; // 75-95 verde
const uint16_t TEMP_HOT_NORMAL = 0xFFE0; // 95-105 amarillo
const uint16_t TEMP_HIGH       = 0xFD20; // 105-115 naranja
const uint16_t TEMP_OVERHEAT   = 0xF800; // >115 rojo

// Tema de colores fijo por sección
const uint16_t VOLT_OK  = 0x07E0;   // VOLTAJE OK (verde)
const uint16_t VOLT_BAD = 0xF800;   // VOLTAJE CAE (rojo)

// ==================== Splash ====================
// Reemplaza el contenido del arreglo con la imagen convertida a RGB565.
// Tip: usa "TFT_eSPI > Tools > ImageConverter" y exporta como array RGB565.
const uint16_t SPLASH_WIDTH = 320;
const uint16_t SPLASH_HEIGHT = 240;
const uint16_t splashImage[SPLASH_WIDTH * SPLASH_HEIGHT] PROGMEM = {0};

const uint16_t SPLASH_HOLD_MS = 3000;
const uint8_t SPLASH_FADE_STEPS = 16;
const uint16_t SPLASH_FADE_STEP_DELAY_MS = 20;

// ==================== Lecturas ====================
float readBatteryVolts() {
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(BAT_ADC_PIN);
    delay(2);
  }
  float adc = (float)sum / ADC_SAMPLES;
  float vPin = (adc / ADC_MAX) * VREF;
  return vPin * DIV_FACTOR;
}

float readTempC() {
  tempSensors.requestTemperatures();
  return tempSensors.getTempCByIndex(0);
}

// ==================== Helpers UI ====================
void drawIconThermo(int x, int y, uint16_t circleColor, uint16_t iconColor) {
  tft.fillCircle(x, y, 16, circleColor);
  // termómetro simple (iconColor = negro)
  tft.drawCircle(x, y + 6, 4, iconColor);
  tft.drawLine(x - 1, y - 9, x - 1, y + 2, iconColor);
  tft.drawLine(x, y - 9, x, y + 2, iconColor);
  tft.drawLine(x + 1, y - 9, x + 1, y + 2, iconColor);
  tft.fillCircle(x, y + 6, 3, iconColor);
}

void drawIconBolt(int x, int y, uint16_t circleColor, uint16_t iconColor) {
  tft.fillCircle(x, y, 16, circleColor);
  // rayo simple
  tft.fillTriangle(x - 4, y - 9, x + 5, y - 9, x - 1, y + 2, iconColor);
  tft.fillTriangle(x + 1, y - 3, x + 7, y - 3, x - 3, y + 10, iconColor);
}

void drawBar(int x, int y, int w, int h, float value, float vmin, float vmax,
             uint16_t cFill, uint16_t cBack, uint16_t cBorder) {
  if (value < vmin) value = vmin;
  if (value > vmax) value = vmax;

  float p = (value - vmin) / (vmax - vmin);
  if (p < 0) p = 0;
  if (p > 1) p = 1;

  tft.drawRoundRect(x, y, w, h, 6, cBorder);
  tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 5, cBack);

  int fillW = (int)((w - 4) * p);
  if (fillW < 1) fillW = 1;
  tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 5, cFill);
}

uint16_t scaleColor565(uint16_t color, uint8_t brightness) {
  if (brightness >= 255) return color;
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5) & 0x3F;
  uint8_t b = color & 0x1F;
  r = (r * brightness) / 255;
  g = (g * brightness) / 255;
  b = (b * brightness) / 255;
  return (r << 11) | (g << 5) | b;
}

void drawSplashImage(uint8_t brightness) {
  static uint16_t lineBuffer[SPLASH_WIDTH];
  tft.startWrite();
  tft.setAddrWindow(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT);

  for (int y = 0; y < SPLASH_HEIGHT; y++) {
    uint32_t rowOffset = (uint32_t)y * SPLASH_WIDTH;
    for (int x = 0; x < SPLASH_WIDTH; x++) {
      uint16_t c = pgm_read_word(&splashImage[rowOffset + x]);
      lineBuffer[x] = scaleColor565(c, brightness);
    }
    tft.pushColors(lineBuffer, SPLASH_WIDTH, true);
  }

  tft.endWrite();
}

void showSplashScreen() {
  for (uint8_t step = 0; step <= SPLASH_FADE_STEPS; step++) {
    uint8_t brightness = (uint8_t)((step * 255) / SPLASH_FADE_STEPS);
    drawSplashImage(brightness);
    delay(SPLASH_FADE_STEP_DELAY_MS);
  }

  delay(SPLASH_HOLD_MS);

  for (int step = SPLASH_FADE_STEPS; step >= 0; step--) {
    uint8_t brightness = (uint8_t)((step * 255) / SPLASH_FADE_STEPS);
    drawSplashImage(brightness);
    delay(SPLASH_FADE_STEP_DELAY_MS);
  }
}

// ==================== Layout fijo ====================
void drawTempHeader(uint16_t tempThemeColor) {
  drawIconThermo(28, 28, tempThemeColor, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(tempThemeColor, COL_PANEL);
  tft.setCursor(55, 16);
  tft.print("TEMP");
}

void drawVoltHeader(uint16_t voltThemeColor) {
  int W = tft.width();
  int mid = W / 2;

  drawIconBolt(mid + 28, 28, voltThemeColor, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(voltThemeColor, COL_PANEL);
  tft.setCursor(mid + 55, 16);
  tft.print("VOLTAJE");
}

void drawLayout(uint16_t tempThemeColor, uint16_t voltThemeColor) {
  tft.fillScreen(COL_BG);

  int W = tft.width();   // 320
  int H = tft.height();  // 240
  int mid = W / 2;

  // Marco
  tft.drawRoundRect(4, 4, W - 8, H - 8, 12, COL_LINE);

  // Paneles
  tft.fillRoundRect(8, 8, mid - 12, H - 16, 12, COL_PANEL);
  tft.fillRoundRect(mid + 4, 8, mid - 12, H - 16, 12, COL_PANEL);

  // Separador
  tft.drawLine(mid, 10, mid, H - 10, COL_LINE);

  // ===== TEMP (izq) =====
  drawTempHeader(tempThemeColor);

  // ===== VOLTAJE (der) =====
  drawVoltHeader(voltThemeColor);

  // Líneas bajo header
  tft.drawLine(12, 55, mid - 12, 55, COL_LINE);
  tft.drawLine(mid + 12, 55, W - 12, 55, COL_LINE);

  // Subtexto fijo
  tft.setTextSize(1);
  tft.setTextColor(COL_SUBTXT, COL_PANEL);
  tft.setCursor(14, 60);
  tft.print("DS18B20");

  tft.setCursor(mid + 14, 60);
  tft.print("ADC BAT");
}

// ==================== Valores (sin parpadeo) ====================
void drawValues(float tC, float vBat) {
  int W = tft.width();
  int mid = W / 2;

  int leftX  = 12;
  int rightX = mid + 12;
  int boxW   = mid - 24;

  // Cambios mínimos para evitar flicker
  bool tempChanged = isnan(lastTC) || fabs(tC - lastTC) >= 0.1;    // 0.1°C
  static int lastVDisconnectedState = -1;
  bool vDisconnected = (vBat <= VOLT_DISCONNECT);
  bool vDisconnectedChanged = (lastVDisconnectedState == -1) || ((vDisconnected ? 1 : 0) != lastVDisconnectedState);
  bool voltChanged = vDisconnectedChanged || isnan(lastVB) || fabs(vBat - lastVB) >= 0.02; // 0.02V

  // Tema voltaje: rojo <=12V o >=15V, verde entre (12,15)
  int vState = 0; // 0=LOW/DISCONNECT, 1=OK, 2=HIGH
  if (vDisconnected || vBat <= VOLT_OK_MIN) {
    vState = 0;
  } else if (vBat < VOLT_OK_MAX) {
    vState = 1;
  } else {
    vState = 2;
  }
  uint16_t vTheme = (vState == 1) ? VOLT_OK : VOLT_BAD;

  // Colores por temperatura
  uint16_t tColor;
  const char* tSts;
  if (tC < 50.0) { tColor = TEMP_COLD; tSts = "STS:FRIA"; }
  else if (tC < 75.0) { tColor = TEMP_WARMING; tSts = "STS:CAL"; }
  else if (tC < 95.0) { tColor = TEMP_OPTIMAL; tSts = "STS:OPT"; }
  else if (tC < 105.0) { tColor = TEMP_HOT_NORMAL; tSts = "STS:HOT"; }
  else if (tC < 115.0) { tColor = TEMP_HIGH; tSts = "STS:ALTA"; }
  else { tColor = TEMP_OVERHEAT; tSts = "STS:OVR"; }

  // Si cambia el "estado" de voltaje (verde<->rojo), redibuja header/icono (sin parpadeo grande)
  static uint16_t lastVTheme = 0;
  if (lastVTheme == 0) {
    lastVTheme = vTheme;
    drawLayout(tColor, vTheme);
    tempChanged = true;
    voltChanged = true;
  } else if (vTheme != lastVTheme) {
    // Redibuja layout solo cuando cambia de verde a rojo o viceversa
    drawLayout(tColor, vTheme);

    // Forzar redibujar todo dinámico
    tempChanged = true;
    voltChanged = true;
    lastVTheme = vTheme;
  }

  static uint16_t lastTempTheme = 0;
  if (lastTempTheme == 0) lastTempTheme = tColor;
  if (tColor != lastTempTheme) {
    drawTempHeader(tColor);
    tempChanged = true;
    lastTempTheme = tColor;
  }

  // Limpia SOLO zonas dinámicas pequeñas
  if (tempChanged) {
    tft.fillRect(leftX, 80,  boxW, 75, COL_PANEL);  // número + unidad
    tft.fillRect(leftX, 160, boxW, 60, COL_PANEL);  // barra + STS
  }

  if (voltChanged) {
    tft.fillRect(rightX, 80,  boxW, 75, COL_PANEL);
    tft.fillRect(rightX, 160, boxW, 60, COL_PANEL);
  }

  // ===== TEMP =====
  if (tempChanged) {
    tft.setTextSize(6);

    if (tC == DEVICE_DISCONNECTED_C || tC < -100) {
      tft.setTextColor(TFT_RED, COL_PANEL);
      int w = tft.textWidth("ERR");
      tft.setCursor(leftX + (boxW - w) / 2, 92);
      tft.print("ERR");

      tft.setTextSize(2);
      tft.setTextColor(COL_SUBTXT, COL_PANEL);
      tft.setCursor(leftX, 195);
      tft.print("STS:ERR");
    } else {
      tft.setTextColor(tColor, COL_PANEL);

      char tStr[10];
      snprintf(tStr, sizeof(tStr), "%.1f", tC);
      int w = tft.textWidth(tStr);
      tft.setCursor(leftX + (boxW - w) / 2, 92);
      tft.print(tStr);

      // unidad pequeña
      tft.setTextSize(2);
      tft.setTextColor(tColor, COL_PANEL);
      tft.setCursor(leftX + boxW - 46, 148);
      tft.print("C");

      // barra temp (escala 0–100)
      drawBar(leftX, 170, boxW, 20, tC, TEMP_MIN, TEMP_MAX, tColor, COL_BG, COL_LINE);

      // STS abreviado
      tft.setTextSize(2);
      tft.setTextColor(tColor, COL_PANEL);
      tft.setCursor(leftX, 195);
      tft.print(tSts);
    }

    lastTC = tC;
  }

  // ===== VOLTAJE =====
  if (voltChanged) {
    if (vDisconnected) {
      tft.setTextSize(3);
      tft.setTextColor(VOLT_BAD, COL_PANEL);
      const char* vMsg = "SIN CON";
      int w2 = tft.textWidth(vMsg);
      tft.setCursor(rightX + (boxW - w2) / 2, 110);
      tft.print(vMsg);

      drawBar(rightX, 170, boxW, 20, 0.0, 0.0, 16.0, VOLT_BAD, COL_BG, COL_LINE);

      tft.setTextSize(2);
      tft.setTextColor(VOLT_BAD, COL_PANEL);
      tft.setCursor(rightX, 195);
      tft.print("STS:NC");
    } else {
      // Voltaje uniforme según rango
      tft.setTextSize(6);
      tft.setTextColor(vTheme, COL_PANEL);

      char vStr[10];
      snprintf(vStr, sizeof(vStr), "%.2f", vBat);
      int w2 = tft.textWidth(vStr);
      tft.setCursor(rightX + (boxW - w2) / 2, 92);
      tft.print(vStr);

      // unidad pequeña
      tft.setTextSize(2);
      tft.setTextColor(vTheme, COL_PANEL);
      tft.setCursor(rightX + boxW - 22, 148);
      tft.print("V");

      // barra voltaje (mismo color uniforme)
      // Escala típica 0–16V solo para visual (no afecta tu medición)
      drawBar(rightX, 170, boxW, 20, vBat, 0.0, 16.0, vTheme, COL_BG, COL_LINE);

      // STS abreviado y del mismo color del voltaje
      tft.setTextSize(2);
      tft.setTextColor(vTheme, COL_PANEL);
      tft.setCursor(rightX, 195);
      if (vState == 1) {
        tft.print("STS:OK");
      } else if (vState == 2) {
        tft.print("STS:HIGH");
      } else {
        tft.print("STS:LOW");
      }
    }

    lastVB = vBat;
    lastVDisconnectedState = vDisconnected ? 1 : 0;
  }
}

// ==================== Setup / Loop ====================
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  tempSensors.begin();

  tft.init();
  tft.setRotation(1); // Landscape 320x240

  showSplashScreen();

  // Dibuja layout con VOLTAJE inicialmente verde
  drawLayout(TEMP_COLD, VOLT_OK);
}

void loop() {
  if (millis() - lastUpdate >= 500) {
    lastUpdate = millis();

    float tC = readTempC();
    float vB = readBatteryVolts();

    Serial.printf("Temp: %.2f C | Bat: %.2f V\n", tC, vB);
    drawValues(tC, vB);
  }
}
