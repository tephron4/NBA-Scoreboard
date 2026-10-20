from flask import Flask, jsonify
from nba_api.live.nba.endpoints import scoreboard, boxscore
from datetime import datetime

app = Flask(__name__)

@app.route("/live-scores", methods=["GET"])
def get_live_scores():
    try:
        board = scoreboard.ScoreBoard()
        data = board.get_dict()
        
        if not data or "scoreboard" not in data:
            return jsonify({
                "timestamp": datetime.now().isoformat(),
                "games": [],
                "message": "No live scoreboard data available right now."
            }), 200

        games_data = data.get("scoreboard", {}).get("games", [])
        
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
        # Query the live box score endpoint for the specific game ID
        game_box = boxscore.BoxScore(game_id=game_id)
        data = game_box.get_dict()
        
        if not data or "game" not in data:
            return jsonify({"error": "Box score not found for this game ID"}), 404
            
        game_data = data.get("game", {})
        
        # Structure the payload with team breakdowns, player stats, and period info
        boxscore_payload = {
            "game_id": game_data.get("gameId"),
            "status": game_data.get("gameStatusText"),
            "home_team": game_data.get("homeTeam"), # Includes players, stats, and periods
            "away_team": game_data.get("awayTeam")  # Includes players, stats, and periods
        }
        
        return jsonify(boxscore_payload), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)