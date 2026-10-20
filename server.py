from flask import Flask, jsonify
import requests
from datetime import datetime

app = Flask(__name__)

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
}

@app.route("/live-scores", methods=["GET"])
def get_live_scores():
    try:
        # ESPN's public unblocked scoreboard API endpoint
        url = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/scoreboard"
        response = requests.get(url, headers=HEADERS, timeout=5)
        
        if response.status_code != 200:
            return jsonify({
                "timestamp": datetime.now().isoformat(),
                "games": [],
                "error": f"Scoreboard API returned status code {response.status_code}"
            }), 200

        data = response.json()
        events = data.get("events", [])
        
        formatted_games = []
        for event in events:
            competition = event.get("competitions", [{}])[0]
            competitors = competition.get("competitors", [])
            
            home_team, away_team = {}, {}
            for comp in competitors:
                team_info = {
                    "tricode": comp.get("team", {}).get("abbreviation"),
                    "score": comp.get("score", "0")
                }
                if comp.get("homeAway") == "home":
                    home_team = team_info
                else:
                    away_team = team_info
            
            game_info = {
                "game_id": event.get("id"),
                "status": competition.get("status", {}).get("type", {}).get("detail"),
                "period": competition.get("status", {}).get("period"),
                "game_clock": competition.get("status", {}).get("displayClock"),
                "home_team": home_team,
                "away_team": away_team
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
        url = f"https://site.api.espn.com/apis/site/v2/sports/basketball/nba/summary?event={game_id}"
        response = requests.get(url, headers=HEADERS, timeout=5)
        
        if response.status_code != 200:
            return jsonify({"error": f"Box score not found (status {response.status_code})"}), 404
            
        data = response.json()
        return jsonify(data), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)