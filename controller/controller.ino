#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/TomThumb.h>
#include <string>
#include <time.h>

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

struct PlayerData {
  String name;
  String shortName;
  String jerseyNumber;
  String position;
  bool starter;
  bool active;
  bool didNotPlay;
  String reason;
  bool ejected;
  String stats[14];
};

struct BoxScoreData {
  String teamTricode;
  PlayerData playerData[18];
  uint8_t playerCount = 0;
};

// Cache for home and away boxscores
BoxScoreData awayBoxScore;
BoxScoreData homeBoxScore;

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
unsigned long lastBoxScoreFetchTime = 0;
const unsigned long FETCH_INTERVAL = 3000; // Poll every 3s for snappy web dashboard sync
const unsigned long FETCH_BOX_SCORE_INTERVAL = 5000;
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

void setupNTP() {
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2)
    // -18000 = -5 hours (Eastern Standard Time), 3600 = 1 hour DST offset
    configTime(-18000, 3600, "pool.ntp.org", "time.nist.gov");
}

String getCurrentDate() {
    struct tm timeinfo;
    
    // Check if time is successfully synchronized
    if (!getLocalTime(&timeinfo)) {
        return "Syncing...";
    }
    
    char buffer[11]; // Space for "MM/DD/YYYY" plus null terminator
    // %m = month (01-12), %d = day (01-31)
    strftime(buffer, sizeof(buffer), "%m/%d", &timeinfo);
    
    return String(buffer);
}

void fetchBoxScoreDetails(String gameId) {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = String(PI_BASE_URL) + "/game/" + gameId + "/boxscore";
  // Serial.println("Fetching boxscore from: " + url);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  // Serial.println("HTTP Response Code: " + String(httpCode)); // Check if it's 200 OK

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Print a snippet of the payload to verify the JSON structure
    // Serial.println("Payload length: " + String(payload.length()));
    if (payload.length() > 200) {
      // Serial.println("Payload preview: " + payload.substring(0, 200));
    } else {
      // Serial.println("Payload: " + payload);
    }

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      if (doc["status"]["detail"].is<const char*>()) {
        std::string rawStatus = doc["status"]["detail"];
        boxScoreDetail = cleanGameStatus(rawStatus);
      }
      lastFetchedBoxScoreId = gameId;

      auto parseTeamPlayers = [](JsonObject teamObj, BoxScoreData &boxScore) {
        boxScore.teamTricode = teamObj["abbreviation"].as<String>();
        boxScore.playerCount = 0;

        JsonArray activeArr = teamObj["activePlayers"];
        for (JsonObject athlete : activeArr) {
          if (boxScore.playerCount >= 18) break;
          PlayerData &p = boxScore.playerData[boxScore.playerCount];
          
          p.name = athlete["name"].as<String>();
          p.shortName = athlete["shortName"].as<String>();
          p.jerseyNumber = athlete["jerseyNumber"].as<String>();
          p.position = athlete["position"].as<String>();
          p.starter = athlete["starter"] | false;
          p.active = true;
          p.didNotPlay = athlete["didNotPlay"] | false;
          p.reason = athlete["reason"].as<String>();
          p.ejected = athlete["ejected"] | false;

          JsonArray statsArr = athlete["stats"];
          int statIdx = 0;
          for (JsonVariant statVal : statsArr) {
            if (statIdx < 14) {
              p.stats[statIdx] = statVal.as<String>();
            }
            statIdx++;
          }
          boxScore.playerCount++;
        }
      };

      JsonObject teamsObj = doc["teams"];

      if (teamsObj["away"].is<JsonObject>()) {
        parseTeamPlayers(teamsObj["away"], awayBoxScore);
        // Serial.println("Away players parsed successfully: " + String(awayBoxScore.playerCount));
      } else {
        // Serial.println("Warning: 'teams.away' object missing or invalid in JSON");
      }

      if (teamsObj["home"].is<JsonObject>()) {
        parseTeamPlayers(teamsObj["home"], homeBoxScore);
        // Serial.println("Home players parsed successfully: " + String(homeBoxScore.playerCount));
      } else {
        // Serial.println("Warning: 'teams.home' object missing or invalid in JSON");
      }

    } else {
      Serial.println("Deserialization failed: " + String(error.c_str()));
    }
  } else {
    Serial.println("HTTP error: " + String(httpCode));
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
        // Serial.println(gameList[totalGames].status);
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

  // If in box score view, fetch deep stats periodically
  if (currentView == "boxscore" && totalGames > 0 && selectedIndex < totalGames) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastBoxScoreFetchTime >= FETCH_BOX_SCORE_INTERVAL || lastBoxScoreFetchTime == 0) {
      lastFetchTime = currentMillis;
      String activeGameId = gameList[selectedIndex].gameId;
      fetchBoxScoreDetails(activeGameId);
    }
  }
}

void drawListView() {
  // Get current date
  String todayDate = getCurrentDate();
  // Serial.println(todayDate);

  // Top Header (Standard Font)
  dma_display->setFont();
  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 165, 0));
  dma_display->setCursor(35, 0);
  dma_display->print("NBA SCORES");
  dma_display->setFont(&TomThumb);
  dma_display->setTextColor(dma_display->color565(60, 60, 60));
  dma_display->setCursor(100, 7);
  dma_display->print(todayDate);
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
    // Serial.print("Away color: ");
    // Serial.println(g.awayColor);
    if (g.awayColor == "000000" || g.awayColor == "#000000") {
      dma_display->setTextColor(hexToRGB565(g.awayAltColor));
    } else {
      dma_display->setTextColor(hexToRGB565(g.awayColor));
    }
    dma_display->setCursor(2, baselineY - 7);
    dma_display->print(g.awayTricode);

    // Switch to TomThumb Font for scores and status (baseline aligned)
    dma_display->setFont(&TomThumb);

    // 2. Away Score (TomThumb)
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(26, baselineY - 1);
    dma_display->print(g.awayScore);

    // 3. Hyphen (TomThumb)
    dma_display->setTextColor(dma_display->color565(120, 120, 120));
    dma_display->setCursor(36, baselineY - 1);
    dma_display->print("-");

    // 4. Home Score (TomThumb)
    dma_display->setTextColor(isSelected ? dma_display->color565(255, 255, 0) : dma_display->color565(200, 200, 0));
    dma_display->setCursor(41, baselineY - 1);
    dma_display->print(g.homeScore);

    // 5. Home Tricode (Regular Font - top-left aligned)
    dma_display->setFont();
    dma_display->setTextSize(1);
    // Serial.print("Home color: ");
    // Serial.println(g.homeColor);
    if (g.homeColor == "000000" || g.homeColor == "#000000") {
      dma_display->setTextColor(hexToRGB565(g.homeAltColor));
    } else {
      dma_display->setTextColor(hexToRGB565(g.homeColor));
    }
    dma_display->setCursor(53, baselineY - 7);
    dma_display->print(g.homeTricode);

    // 6. Status (TomThumb)
    dma_display->setFont(&TomThumb);
    dma_display->setTextColor(isSelected ? dma_display->color565(0, 0, 255) : dma_display->color565(0, 0, 200));
    dma_display->setCursor(78, baselineY - 1);
    dma_display->print(g.status);
  }
}

void drawBoxScoreView() {
  if (totalGames == 0 || selectedIndex >= totalGames) return;
  GameData g = gameList[selectedIndex];

  // Setup away abbr. offset
  int awayXOffset = 2;

  // Logos
  const uint16_t* awayLogo = getTeamLogo(g.awayTricode);
  const uint16_t* homeLogo = getTeamLogo(g.homeTricode);

  if (awayLogo != nullptr) {
    dma_display->drawRGBBitmap(2, 2, awayLogo, 16, 16);
    awayXOffset = 20;
  }

  // Header (Standard Font)
  dma_display->setFont();
  dma_display->setTextSize(1);
  if (g.awayColor == "000000" || g.awayColor == "#000000") {
    dma_display->setTextColor(hexToRGB565(g.awayAltColor));
  } else {
    dma_display->setTextColor(hexToRGB565(g.awayColor));
  }
  dma_display->setCursor(awayXOffset, 1);
  dma_display->print(g.awayTricode);

  int16_t x1, y1;

  // Get width of g.awayTricode
  uint16_t awayWidth, awayHeight;
  dma_display->getTextBounds(g.awayTricode, 0, 0, &x1, &y1, &awayWidth, &awayHeight);
  dma_display->drawFastHLine(awayXOffset - 1, 9, awayWidth + 1, dma_display->color565(60, 60, 60));

  // Get width of g.homeTricode
  uint16_t homeWidth, homeHeight;
  dma_display->getTextBounds(g.homeTricode, 0, 0, &x1, &y1, &homeWidth, &homeHeight);

  int homeXOffset = 127 - homeWidth;
  if (homeLogo != nullptr) {
    dma_display->drawRGBBitmap(111, 1, homeLogo, 16, 16);
    homeXOffset = homeXOffset - 17;
  }

  dma_display->setFont();
  dma_display->setTextSize(1);
  if (g.homeColor == "000000" || g.homeColor == "#000000") {
    dma_display->setTextColor(hexToRGB565(g.homeAltColor));
  } else {
    dma_display->setTextColor(hexToRGB565(g.homeColor));
  }
  dma_display->setCursor(homeXOffset, 1);
  dma_display->print(g.homeTricode);
  dma_display->drawFastHLine(homeXOffset - 1, 9, homeWidth + 1, dma_display->color565(60, 60, 60));

  // Draw status & score
  if (pregame(g.status)) { 
    dma_display->setTextColor(dma_display->color565(0, 75, 0));
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(1);

    uint16_t w, h;
    dma_display->getTextBounds(g.status, 0, 0, &x1, &y1, &w, &h);
    dma_display->setCursor(64 - (w / 2), 7);
    dma_display->print(g.status);
  } else {
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(2);
    dma_display->setTextColor(dma_display->color565(255, 255, 0));
    dma_display->setCursor(awayXOffset + awayWidth + 2, 12);
    dma_display->print(g.awayScore);

    uint16_t w, h;
    dma_display->setFont(&TomThumb);
    dma_display->setTextSize(2);
    dma_display->getTextBounds(g.homeScore, 0, 0, &x1, &y1, &w, &h);
    dma_display->setTextColor(dma_display->color565(255, 255, 0));
    dma_display->setCursor(homeXOffset - w - 3, 12);
    dma_display->print(g.homeScore);

    // Show the game clock below the score
    dma_display->setTextSize(1);
    dma_display->getTextBounds(g.status, 0, 0, &x1, &y1, &w, &h);
    dma_display->setTextColor(dma_display->color565(0, 75, 0));
    dma_display->setCursor(64 - (w / 2), 19);
    dma_display->print(g.status);
  }

  // --- BOXSCORE HEADERS & ACTIVE PLAYERS ROSTER VIEW ---
  dma_display->setFont(&TomThumb);
  dma_display->setTextSize(1);

  // 1. Draw Column Labels above the player rows (Y = 22)
  // Assuming your server passes labels in homeBoxScore / awayBoxScore or you can grab them.
  // For standard NBA boxscores, the common stat headers are: "MIN", "PTS", "REB", "AST" (or similar).
  // We'll draw static or dynamic headers for the inside stats columns:
  dma_display->setTextColor(dma_display->color565(120, 120, 120));
  
  // Away Side Headers (Left)
  dma_display->setCursor(2, 26);   dma_display->print("#");
  dma_display->setCursor(25, 26);  dma_display->print("PTS");
  dma_display->setCursor(38, 26);  dma_display->print("REB");
  dma_display->setCursor(51, 26);  dma_display->print("AST");

  // Home Side Headers (Right)
  dma_display->setCursor(67, 26);  dma_display->print("AST");
  dma_display->setCursor(80, 26);  dma_display->print("REB");
  dma_display->setCursor(93, 26);  dma_display->print("PTS");
  dma_display->setCursor(120, 26); dma_display->print("#");

  // Divider line under headers
  dma_display->drawFastHLine(0, 27, 128, dma_display->color565(40, 40, 40));

  int startY = 34;
  int rowHeight = 7;

  // 2. Draw Away Active Players (Left Side)
  int renderedAwayCount = 0;
  for (int i = 0; i < awayBoxScore.playerCount && renderedAwayCount < 5; i++) {
    PlayerData &p = awayBoxScore.playerData[i];
    
    if (p.shortName.length() > 0) {
      int yPos = startY + (renderedAwayCount * rowHeight);
      
      // Outside -> Inside sequence:
      // A. Jersey Number (X = 2)
      dma_display->setTextColor(dma_display->color565(150, 150, 150));
      dma_display->setCursor(2, yPos);
      dma_display->print(p.jerseyNumber);

      // B. First 4 characters of last name (shortName format is "J. Doe" -> grab last name part)
      String lastNameOnly = p.shortName;
      int spaceIdx = p.shortName.indexOf(' ');
      if (spaceIdx != -1) {
        lastNameOnly = p.shortName.substring(spaceIdx + 1);
      }
      String nameSnippet = lastNameOnly.substring(0, min((unsigned int)3, (unsigned int)lastNameOnly.length()));
      
      dma_display->setTextColor(dma_display->color565(175, 175, 175));
      dma_display->setCursor(12, yPos);
      dma_display->print(nameSnippet);

      // C. Stats: PTS, REB, AST (moving inward toward the center split)
      String pts = (p.stats[0].length() > 0) ? p.stats[1] : "0";
      String reb = (p.stats[1].length() > 0) ? p.stats[5] : "0";
      String ast = (p.stats[2].length() > 0) ? p.stats[6] : "0";
      // String pts = "00"; // for testing
      // String reb = "00"; // for testing
      // String ast = "00"; // for testing

      dma_display->setTextColor(dma_display->color565(255, 255, 255));
      dma_display->setCursor(27, yPos); dma_display->print(pts);
      dma_display->setCursor(40, yPos); dma_display->print(reb);
      dma_display->setCursor(53, yPos); dma_display->print(ast);

      renderedAwayCount++;
    }
  }

  // 3. Draw Home Active Players (Right Side)
  int renderedHomeCount = 0;
  for (int i = 0; i < homeBoxScore.playerCount && renderedHomeCount < 5; i++) {
    PlayerData &p = homeBoxScore.playerData[i];
    
    if (p.shortName.length() > 0) {
      int yPos = startY + (renderedHomeCount * rowHeight);
      
      // Inside -> Outside sequence for Home (from center split outward to the right edge):
      String pts = (p.stats[0].length() > 0) ? p.stats[1] : "0";
      String reb = (p.stats[1].length() > 0) ? p.stats[5] : "0";
      String ast = (p.stats[2].length() > 0) ? p.stats[6] : "0";
      // String pts = "00"; // for testing
      // String reb = "00"; // for testing
      // String ast = "00"; // for testing

      dma_display->setTextColor(dma_display->color565(175, 175, 175));
      dma_display->setCursor(69, yPos); dma_display->print(ast);
      dma_display->setCursor(82, yPos); dma_display->print(reb);
      dma_display->setCursor(95, yPos); dma_display->print(pts);

      // Last name snippet (first 4 chars)
      String lastNameOnly = p.shortName;
      int spaceIdx = p.shortName.indexOf(' ');
      if (spaceIdx != -1) {
        lastNameOnly = p.shortName.substring(spaceIdx + 1);
      }
      String nameSnippet = lastNameOnly.substring(0, min((unsigned int)3, (unsigned int)lastNameOnly.length()));

      dma_display->setTextColor(dma_display->color565(200, 200, 200));
      dma_display->setCursor(106, yPos);
      dma_display->print(nameSnippet);

      // Jersey Number (Right edge near X = 125)
      dma_display->setTextColor(dma_display->color565(150, 150, 150));
      dma_display->setCursor(120, yPos);
      dma_display->print(p.jerseyNumber);

      renderedHomeCount++;
    }
  }
}

bool pregame(String status) {
  int hour, minute;
  char ampm[3];
  char timezone[10];

  // sscanf parses the 4 components
  int matched = sscanf(status.c_str(), "%d:%2d %2s %s", &hour, &minute, ampm, timezone);

  // We expect exactly 4 successful conversions
  if (matched != 4) {
    return false;
  }

  // Validate logical ranges for time
  if (hour < 1 || hour > 12) return false;
  if (minute < 0 || minute > 59) return false;

  // STRICT CHECK: Ensure ampm is actually AM or PM (blocks "-", "1st", etc.)
  if (!(strcmp(ampm, "AM") == 0 || strcmp(ampm, "PM") == 0 || 
        strcmp(ampm, "am") == 0 || strcmp(ampm, "pm") == 0)) {
    return false;
  }

  return true;
}

// Helper function to trim trailing "Quarter"
String cleanGameStatus(std::string status) {
    // For before a game: Find the occurance of "at"
    size_t atPos = status.rfind("at");

    // For during game: Find the last occurrence of "Quarter"
    size_t qtrPos = status.rfind("Quarter");

    if (atPos != std::string::npos) { // Cut off start of datetime status
      // Erase text up to and including "at"
      // plus any trailing/leading spaces left behind
      status.erase(0, atPos + 2);
    }
    else if (qtrPos != std::string::npos) { // Cut off "Quarter"
      // Erase "Quarter" and any trailing/leading spaces left behind
      status.erase(qtrPos);
        
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

  setupNTP();
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