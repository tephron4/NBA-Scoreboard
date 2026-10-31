#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>
#include <string>

#include "config.h"
#include "logos.h"

// --- MATRIX DIMENSIONS ---
#define PANEL_WIDTH      64
#define PANEL_HEIGHT     64
#define PANEL_CHAIN      2  // Changed from 1 to 2!

// --- GPIO PIN DEFINITIONS (Seengreat S3 Controller) ---
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

MatrixPanel_I2S_DMA *dma_display = nullptr;  // --- ADD THESE TIMING FIXES ---

struct GameData {
  String gameId;
  String awayTricode;
  String awayColor;
  String awayAltColor;
  String awayScore;
  String awayLineScores;
  String awayRecord;
  String homeTricode;
  String homeColor;
  String homeAltColor;
  String homeScore;
  String homeLineScores;
  String homeRecord;
  String status;
};

const int MAX_GAMES = 20;
GameData gameList[MAX_GAMES];
int totalGames = 0;

// Received from Pi dashboard state
String currentView = "list";
int selectedIndex = 0;
const int GAMES_PER_PAGE = 5;

// Box score cache details
String boxScoreDetail = "";
String lastFetchedBoxScoreId = "";

unsigned long lastFetchTime = 0;
const unsigned long FETCH_INTERVAL = 3000; // Poll every 3s for snappy web dashboard sync
String statusMessage = "Connecting...";

void setupWifi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

void fetchBoxScoreDetails(String gameId) {
  if (WiFi.status() != WL_CONNECTED) return;

  // Uses PI_BASE_URL defined in config.h
  String url = String(PI_BASE_URL) + "/game/" + gameId + "/boxscore";

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      std::string rawStatus = doc["status"]["detail"];
      boxScoreDetail = cleanGameStatus(rawStatus);
      lastFetchedBoxScoreId = gameId;
    }
  }
  http.end();
}

void fetchState() {
  if (WiFi.status() != WL_CONNECTED) {
    statusMessage = "WiFi Offline";
    return;
  }

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
        gameList[totalGames].gameId         = game["game_id"].as<String>();
        gameList[totalGames].awayTricode    = game["away_team"]["tricode"].as<String>();
        gameList[totalGames].awayColor      = game["away_team"]["color"].as<String>();
        gameList[totalGames].awayAltColor   = game["away_team"]["alternateColor"].as<String>();
        gameList[totalGames].awayScore      = game["away_team"]["score"].as<String>();
        gameList[totalGames].awayLineScores = game["away_team"]["lineScores"].as<String>();
        gameList[totalGames].homeTricode    = game["home_team"]["tricode"].as<String>();
        gameList[totalGames].homeColor      = game["home_team"]["color"].as<String>();
        gameList[totalGames].homeAltColor   = game["home_team"]["alternateColor"].as<String>();
        gameList[totalGames].homeScore      = game["home_team"]["score"].as<String>();
        gameList[totalGames].homeLineScores = game["home_team"]["lineScores"].as<String>();
        std::string rawStatus = game["status"];
        gameList[totalGames].status       = cleanGameStatus(rawStatus);
        totalGames++;
      }
      statusMessage = "";
    } else {
      statusMessage = "JSON Parse Err";
    }
  } else {
    statusMessage = "HTTP Err: " + String(httpCode);
  }
  http.end();

  // If in box score view, fetch deep stats if game ID changes
  if (currentView == "boxscore" && totalGames > 0 && selectedIndex < totalGames) {
    String activeGameId = gameList[selectedIndex].gameId;
    if (activeGameId != lastFetchedBoxScoreId) {
      fetchBoxScoreDetails(activeGameId);
    }
  }
}

void drawListView() {
  // Top Header (Standard Font)
  dma_display->setFont();
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 165, 0));
  dma_display->setCursor(35, 0);
  dma_display->print("NBA SCORES");
  dma_display->drawFastHLine(33, 8, 63, dma_display->color565(60, 60, 60));

  int currentPage = selectedIndex / GAMES_PER_PAGE;
  int startIndex = currentPage * GAMES_PER_PAGE;
  int endIndex = min(startIndex + GAMES_PER_PAGE, totalGames);

  for (int i = startIndex; i < endIndex; i++) {
    int rowOffset = i - startIndex;
    int baselineY = 18 + (rowOffset * 10);
    bool isSelected = (i == selectedIndex);

    // Highlight bar behind selected row
    if (isSelected) {
      dma_display->drawFastHLine(1, baselineY - 9, 127, dma_display->color565(40, 40, 90));
      dma_display->drawFastHLine(1, baselineY + 1, 127, dma_display->color565(40, 40, 90));
    }

    GameData g = gameList[i];

    // 1. Away Tricode (Regular Font - top-left aligned)
    dma_display->setFont();
    dma_display->setTextSize(1);
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 255) : dma_display->color565(200, 200, 200));
    dma_display->setCursor(2, baselineY - 7);
    dma_display->print(g.awayTricode);

    // Switch to TomThumb Font for scores and status (baseline aligned)
    dma_display->setFont(&TomThumb);

    // 2. Away Score (TomThumb)
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(20, baselineY - 1);
    dma_display->print(g.awayScore);

    // 3. Hyphen (TomThumb)
    dma_display->setTextColor(dma_display->color565(120, 120, 120));
    dma_display->setCursor(30, baselineY - 1);
    dma_display->print("-");

    // 4. Home Score (TomThumb)
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(35, baselineY - 1);
    dma_display->print(g.homeScore);

    // 5. Home Tricode (Regular Font - top-left aligned)
    dma_display->setFont();
    dma_display->setTextSize(1);
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 255) : dma_display->color565(200, 200, 200));
    dma_display->setCursor(46, baselineY - 7);
    dma_display->print(g.homeTricode);

    // 6. Status (TomThumb)
    dma_display->setFont(&TomThumb);
    dma_display->setTextColor(isSelected ? dma_display->color565(0, 0, 255) : dma_display->color565(0, 0, 200));
    dma_display->setCursor(68, baselineY - 1);
    dma_display->print(g.status);
  }
}

void drawBoxScoreView() {
  if (totalGames == 0 || selectedIndex >= totalGames) return;
  GameData g = gameList[selectedIndex];

  // Logos
  // 1. Try to get the logo pointers
  const uint16_t* awayLogo = getTeamLogo(g.awayTricode);
  const uint16_t* homeLogo = getTeamLogo(g.homeTricode);

  if (awayLogo != nullptr) {
    // Draw the 8x8 bitmap if the logo exists in your header
    dma_display->drawRGBBitmap(2, 1, awayLogo, 16, 16);
  }

  // Header (Standard Font)
  dma_display->setFont();
  dma_display->setTextSize(1);
  dma_display->setTextColor(hexToRGB565(g.awayColor));
  dma_display->setCursor(20, 0);
  dma_display->print(g.awayTricode);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(38, 0);
  dma_display->print(" vs ");
  dma_display->setTextColor(hexToRGB565(g.homeColor));
  dma_display->setCursor(63, 0);
  dma_display->print(g.homeTricode);
  dma_display->drawFastHLine(19, 8, 62, dma_display->color565(60, 60, 60));

  if (homeLogo != nullptr) {
    // Draw the 8x8 bitmap if the logo exists in your header
    dma_display->drawRGBBitmap(82, 1, homeLogo, 16, 16);
  }

  // Details (TomThumb Font)
  dma_display->setFont(&TomThumb);

  dma_display->setTextColor(dma_display->color565(255, 255, 0));
  dma_display->setCursor(2, 24);
  dma_display->print("Score: " + g.awayScore + " - " + g.homeScore);

  dma_display->setTextColor(dma_display->color565(0, 150, 0));
  dma_display->setCursor(2, 36);
  dma_display->print("Status: " + (boxScoreDetail.length() > 0 ? boxScoreDetail : g.status));
}

// Helper function to trim trailing "Quarter"
String cleanGameStatus(std::string status) {
    // Find the last occurrence of "Quarter"
    size_t pos = status.rfind("Quarter");
    
    // If found, ensure it's near the end of the string to avoid cutting off mid-word
    if (pos != std::string::npos) {
        // Erase "Quarter" and any trailing/leading spaces left behind
        status.erase(pos);
        
        // Optional: Trim any trailing whitespace left after removal
        while (!status.empty() && isspace(status.back())) {
            status.pop_back();
        }
    }
    
    return status.c_str();
}

uint16_t hexToRGB565(String hexStr) {
    // Strip the '#' character if the API includes it
    if (hexStr.length() > 0 && hexStr.charAt(0) == '#') {
        hexStr = hexStr.substring(1);
    }
    
    // Convert hex string (e.g. "00539B") to a long integer
    long number = strtol(hexStr.c_str(), NULL, 16);
    
    // Extract 8-bit R, G, B components
    uint8_t r = (number >> 16) & 0xFF;
    uint8_t g = (number >> 8) & 0xFF;
    uint8_t b = number & 0xFF;
    
    // Convert to 16-bit RGB565 using the display library's built-in helper
    return dma_display->color565(r, g, b);
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

  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M; // Lower the bus speed slightly to clean up signal noise
  mxconfig.latch_blanking = 4;               // Increase latch blanking (try 2, 3, or 4)
  mxconfig.clkphase = true;                  // Toggle clock phase (try true or false)
  mxconfig.double_buff = true;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("Matrix initialization failed!");
    while (true) delay(1000);
  }

  dma_display->setBrightness8(128);
  dma_display->clearScreen();

  dma_display->setFont();
  dma_display->setTextColor(dma_display->color565(255, 255, 0));
  dma_display->setTextSize(1);
  dma_display->setCursor(2, 28);
  dma_display->print("Connecting...");

  setupWifi();
}

void loop() {
  unsigned long currentMillis = millis();

  // Poll Pi server for state and game data periodically
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

  dma_display->flipDMABuffer();

  delay(100);
}