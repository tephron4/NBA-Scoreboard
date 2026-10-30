#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>

#include "config.h"

// --- MATRIX DIMENSIONS & GPIO (Seengreat S3) ---
#define PANEL_WIDTH      64
#define PANEL_HEIGHT     64
#define PANEL_CHAIN      1

#define R1_PIN   5
#define G1_PIN   4
#define B1_PIN   6
#define R2_PIN   15
#define G2_PIN   7
#define B2_PIN   17
#define A_PIN    8
#define B_PIN    18
#define C_PIN    10
#define D_PIN    9
#define E_PIN    16
#define LAT_PIN  11
#define OE_PIN   13
#define CLK_PIN  12

MatrixPanel_I2S_DMA *dma_display = nullptr;

struct GameData {
  String gameId;
  String awayTricode;
  String awayScore;
  String homeTricode;
  String homeScore;
  String status;
};

const int MAX_GAMES = 20;
GameData gameList[MAX_GAMES];
int totalGames = 0;

// Received from Pi dashboard state
String currentView = "list";
int selectedIndex = 0;
const int GAMES_PER_PAGE = 7;

unsigned long lastFetchTime = 0;
const unsigned long FETCH_INTERVAL = 3000; // Poll every 3 seconds for snappy dashboard response
String statusMessage = "Connecting...";

void setupWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void fetchState() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(PI_SERVER_URL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      currentView = doc["view"].as<String>();
      selectedIndex = doc["selected_index"].as<int>();

      JsonArray games = doc["games"];
      totalGames = 0;

      for (JsonObject game : games) {
        if (totalGames >= MAX_GAMES) break;
        gameList[totalGames].gameId      = game["game_id"].as<String>();
        gameList[totalGames].awayTricode = game["away_team"]["tricode"].as<String>();
        gameList[totalGames].awayScore   = game["away_team"]["score"].as<String>();
        gameList[totalGames].homeTricode = game["home_team"]["tricode"].as<String>();
        gameList[totalGames].homeScore   = game["home_team"]["score"].as<String>();
        gameList[totalGames].status      = game["status"].as<String>();
        totalGames++;
      }
      statusMessage = "";
    } else {
      statusMessage = "JSON Parse Err";
    }
  } else {
    statusMessage = "HTTP Err";
  }
  http.end();
}

void drawListView() {
  dma_display->setFont();
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 165, 0));
  dma_display->setCursor(2, 0);
  dma_display->print("NBA SCORES");
  dma_display->drawFastHLine(0, 8, 64, dma_display->color565(60, 60, 60));

  dma_display->setFont(&TomThumb);

  // Automatically calculate page based on web-selected index
  int currentPage = selectedIndex / GAMES_PER_PAGE;
  int startIndex = currentPage * GAMES_PER_PAGE;
  int endIndex = min(startIndex + GAMES_PER_PAGE, totalGames);

  for (int i = startIndex; i < endIndex; i++) {
    int rowOffset = i - startIndex;
    int baselineY = 16 + (rowOffset * 8);
    bool isSelected = (i == selectedIndex);

    if (isSelected) {
      dma_display->fillRect(0, baselineY - 6, 64, 7, dma_display->color565(40, 40, 90));
    }

    GameData g = gameList[i];

    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 255) : dma_display->color565(200, 200, 200));
    dma_display->setCursor(1, baselineY);
    dma_display->print(g.awayTricode);

    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(18, baselineY);
    dma_display->print(g.awayScore);

    dma_display->setTextColor(dma_display->color565(120, 120, 120));
    dma_display->setCursor(30, baselineY);
    dma_display->print("-");

    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(35, baselineY);
    dma_display->print(g.homeScore);

    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 255) : dma_display->color565(200, 200, 200));
    dma_display->setCursor(49, baselineY);
    dma_display->print(g.homeTricode);
  }
}

void drawBoxScoreView() {
  if (totalGames == 0 || selectedIndex >= totalGames) return;
  GameData g = gameList[selectedIndex];

  dma_display->setFont();
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(0, 255, 255));
  dma_display->setCursor(2, 0);
  dma_display->print("BOX SCORE");
  dma_display->drawFastHLine(0, 8, 64, dma_display->color565(60, 60, 60));

  dma_display->setFont(&TomThumb);
  
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(2, 18);
  dma_display->print(g.awayTricode + " vs " + g.homeTricode);

  dma_display->setTextColor(dma_display->color565(255, 255, 0));
  dma_display->setCursor(2, 28);
  dma_display->print("Score: " + g.awayScore + " - " + g.homeScore);

  dma_display->setTextColor(dma_display->color565(150, 150, 150));
  dma_display->setCursor(2, 40);
  dma_display->print("Status: " + g.status);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);
  mxconfig.gpio.r1 = R1_PIN; mxconfig.gpio.g1 = G1_PIN; mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN; mxconfig.gpio.g2 = G2_PIN; mxconfig.gpio.b2 = B2_PIN;
  mxconfig.gpio.a  = A_PIN;  mxconfig.gpio.b  = B_PIN;  mxconfig.gpio.c  = C_PIN;
  mxconfig.gpio.d  = D_PIN;  mxconfig.gpio.e  = E_PIN;
  mxconfig.gpio.lat = LAT_PIN; mxconfig.gpio.oe = OE_PIN; mxconfig.gpio.clk = CLK_PIN;
  mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);
  dma_display->clearScreen();

  setupWifi();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastFetchTime >= FETCH_INTERVAL || lastFetchTime == 0) {
    lastFetchTime = currentMillis;
    fetchState();
  }

  dma_display->clearScreen();
  if (totalGames == 0) {
    dma_display->setFont();
    dma_display->setTextSize(1);
    dma_display->setTextColor(dma_display->color565(255, 200, 0));
    dma_display->setCursor(2, 28);
    dma_display->print(statusMessage);
  } else if (currentView == "boxscore") {
    drawBoxScoreView();
  } else {
    drawListView();
  }

  delay(100);
}