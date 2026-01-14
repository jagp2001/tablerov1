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

// Umbral voltaje para "cae" (ajústalo si quieres)
const float VOLT_LOW = 12.2;

// Tema de colores fijo por sección
const uint16_t TEMP_THEME = TFT_YELLOW;  // TEMP siempre “amarillo” en título/círculo
const uint16_t VOLT_OK    = TFT_GREEN;   // VOLTAJE OK
const uint16_t VOLT_BAD   = TFT_RED;     // VOLTAJE CAE

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
  tft.drawCircle(x - 4, y + 6, 5, iconColor);
  tft.drawLine(x - 4, y - 8, x - 4, y + 2, iconColor);
  tft.drawLine(x - 2, y - 8, x - 2, y + 2, iconColor);
  tft.fillCircle(x - 4, y + 6, 3, iconColor);
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

// ==================== Layout fijo ====================
void drawLayout(uint16_t voltThemeColor) {
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
  drawIconThermo(28, 28, TEMP_THEME, TFT_BLACK); // círculo amarillo, icono negro
  tft.setTextSize(2);
  tft.setTextColor(TEMP_THEME, COL_PANEL);
  tft.setCursor(55, 16);
  tft.print("TEMP");

  // ===== VOLTAJE (der) =====
  drawIconBolt(mid + 28, 28, voltThemeColor, TFT_BLACK); // círculo verde/rojo, icono negro
  tft.setTextColor(voltThemeColor, COL_PANEL);
  tft.setCursor(mid + 55, 16);
  tft.print("VOLTAJE");

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
  bool voltChanged = isnan(lastVB) || fabs(vBat - lastVB) >= 0.02; // 0.02V

  // Tema voltaje: verde o rojo (TODO uniforme)
  bool vLow = (vBat < VOLT_LOW);
  uint16_t vTheme = vLow ? VOLT_BAD : VOLT_OK;

  // Si cambia el "estado" de voltaje (verde<->rojo), redibuja header/icono (sin parpadeo grande)
  static int lastVLowState = -1;
  if (lastVLowState == -1) lastVLowState = vLow ? 1 : 0;
  if ((vLow ? 1 : 0) != lastVLowState) {
    // Redibuja layout solo cuando cambia de verde a rojo o viceversa
    drawLayout(vTheme);

    // Forzar redibujar todo dinámico
    tempChanged = true;
    voltChanged = true;
    lastVLowState = vLow ? 1 : 0;
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
      // Colores por temperatura como pediste:
      // <80 verde, 80-90 amarillo, >90 naranja
      uint16_t tColor;
      const char* tSts;
      if (tC < 80.0) { tColor = TFT_GREEN;  tSts = "STS:OK";  }
      else if (tC <= 90.0) { tColor = TFT_YELLOW; tSts = "STS:YEL"; }
      else { tColor = TFT_ORANGE; tSts = "STS:ORG"; }

      tft.setTextColor(tColor, COL_PANEL);

      char tStr[10];
      snprintf(tStr, sizeof(tStr), "%.1f", tC);
      int w = tft.textWidth(tStr);
      tft.setCursor(leftX + (boxW - w) / 2, 92);
      tft.print(tStr);

      // unidad pequeña
      tft.setTextSize(2);
      tft.setTextColor(COL_SUBTXT, COL_PANEL);
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
    // TODO el voltaje uniforme (verde o rojo)
    tft.setTextSize(6);
    tft.setTextColor(vTheme, COL_PANEL);

    char vStr[10];
    snprintf(vStr, sizeof(vStr), "%.2f", vBat);
    int w2 = tft.textWidth(vStr);
    tft.setCursor(rightX + (boxW - w2) / 2, 92);
    tft.print(vStr);

    // unidad pequeña
    tft.setTextSize(2);
    tft.setTextColor(COL_SUBTXT, COL_PANEL);
    tft.setCursor(rightX + boxW - 22, 148);
    tft.print("V");

    // barra voltaje (mismo color uniforme)
    // Escala típica 0–16V solo para visual (no afecta tu medición)
    drawBar(rightX, 170, boxW, 20, vBat, 0.0, 16.0, vTheme, COL_BG, COL_LINE);

    // STS abreviado y del mismo color del voltaje
    tft.setTextSize(2);
    tft.setTextColor(vTheme, COL_PANEL);
    tft.setCursor(rightX, 195);
    tft.print(vLow ? "STS:LOW" : "STS:OK");

    lastVB = vBat;
  }
}

// ==================== Setup / Loop ====================
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  tempSensors.begin();

  tft.init();
  tft.setRotation(1); // Landscape 320x240

  // Dibuja layout con VOLTAJE inicialmente verde
  drawLayout(VOLT_OK);
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
