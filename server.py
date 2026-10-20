from flask import Flask, jsonify
import requests
from datetime import datetime

app = Flask(__name__)

# Modern browser headers to prevent NBA.com WAF / Cloudflare blocks (HTTP 403)
HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Referer": "https://www.nba.com/",
    "Origin": "https://www.nba.com",
    "Accept": "application/json, text/plain, */*"
}

@app.route("/live-scores", methods=["GET"])
def get_live_scores():
    try:
        url = "https://cdn.nba.com/static/json/liveData/scoreboard/todaysScoreboard_00.json"
        response = requests.get(url, headers=HEADERS, timeout=5)
        
        if response.status_code != 200:
            return jsonify({
                "timestamp": datetime.now().isoformat(),
                "games": [],
                "error": f"NBA CDN returned status code {response.status_code}"
            }), 200

        data = response.json()
        scoreboard_data = data.get("scoreboard", {})
        games_data = scoreboard_data.get("games", [])
        
        formatted_games = []
        for game in games_data:
            game_info = {
                "game_id": game.get("gameId"),
                "status": game.get("gameStatusText"),
                "period": game.get("period"),
                "game_clock": game.get("gameClock"),
                "home_team": {
                    "tricode": game["homeTeam"]["teamTricode"],
                    "score": game["homeTeam"]["score"]
                },
                "away_team": {
                    "tricode": game["awayTeam"]["teamTricode"],
                    "score": game["awayTeam"]["score"]
                }
            }
            formatted_games.append(game_info)
            
        return jsonify({
            "timestamp": datetime.now().isoformat(),
            "games": formatted_games
        }), 200

    except Exception as e:
        return jsonify({
            "timestamp": datetime.now().isoformat(),
            "games": [],
            "error": str(e)
        }), 200


@app.route("/game/<game_id>/boxscore", methods=["GET"])
def get_game_boxscore(game_id):
    try:
        url = f"https://cdn.nba.com/static/json/liveData/boxscore/boxscore_{game_id}.json"
        response = requests.get(url, headers=HEADERS, timeout=5)
        
        if response.status_code != 200:
            return jsonify({"error": f"Box score not found (status {response.status_code})"}), 404
            
        data = response.json()
        game_data = data.get("game", {})
        
        boxscore_payload = {
            "game_id": game_data.get("gameId"),
            "status": game_data.get("gameStatusText"),
            "home_team": game_data.get("homeTeam"),
            "away_team": game_data.get("awayTeam")
        }
        
        return jsonify(boxscore_payload), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)