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
            formatted_games.append(format_game(event))
            
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

def format_game(game):
    competition = game.get("competitions", [{}])[0]
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
        "game_id": game.get("id"),
        "status": competition.get("status", {}).get("type", {}).get("detail"),
        "period": competition.get("status", {}).get("period"),
        "game_clock": competition.get("status", {}).get("displayClock"),
        "home_team": home_team,
        "away_team": away_team
    }

    return game_info

@app.route("/game/<game_id>/boxscore", methods=["GET"])
def get_game_boxscore(game_id):
    try:
        url = f"https://site.api.espn.com/apis/site/v2/sports/basketball/nba/summary?event={game_id}"
        response = requests.get(url, headers=HEADERS, timeout=5)
        
        if response.status_code != 200:
            return jsonify({"error": f"Box score not found (status {response.status_code})"}), 404
            
        data = response.json()
        
        # Extract boxscore and header sections safely
        boxscore_data = data.get("boxscore", {})
        header_competitions = data.get("header", {}).get("competitions", [{}])[0]
        competitors = header_competitions.get("competitors", [])
        
        # Build quarter/line scores mapping
        quarter_totals = {}
        for comp in competitors:
            tricode = comp.get("team", {}).get("abbreviation")
            linescores = comp.get("linescores", [])
            quarter_totals[tricode] = [ls.get("displayValue") for ls in linescores]

        teams_payload = {}
        
        # ESPN groups stats by team in boxscore['teams'] and boxscore['players']
        espn_teams = boxscore_data.get("teams", [])
        espn_players_groups = boxscore_data.get("players", [])
        
        for i, t_obj in enumerate(espn_teams):
            team_info = t_obj.get("team", {})
            tricode = team_info.get("abbreviation")
            home_away = t_obj.get("homeAway") # "home" or "away"
            
            # Extract player stats if available for this team group
            active_players = []
            inactive_players = []
            labels = []
            
            if i < len(espn_players_groups):
                player_group = espn_players_groups[i]
                statistics_categories = player_group.get("statistics", [])
                
                if statistics_categories:
                    # Grab stat labels (e.g., MIN, PTS, REB, AST) from the first category
                    labels = statistics_categories[0].get("labels", [])
                    
                    for stat_cat in statistics_categories:
                        athletes = stat_cat.get("athletes", [])
                        for athlete_entry in athletes:
                            athlete_info = athlete_entry.get("athlete", {})
                            is_active = athlete_entry.get("active", True)
                            
                            player_data = {
                                "id": athlete_info.get("id"),
                                "name": athlete_info.get("displayName"),
                                "position": athlete_info.get("position", {}).get("abbreviation"),
                                "stats": athlete_entry.get("stats", [])
                            }
                            
                            if is_active:
                                active_players.append(player_data)
                            else:
                                inactive_players.append(player_data)

            teams_payload[home_away] = {
                "displayOrder": t_obj.get("displayOrder"),
                "abbreviation": tricode,
                "displayName": team_info.get("displayName"),
                "logo": team_info.get("logo"),
                "activePlayers": active_players,
                "inactivePlayers": inactive_players,
                "labels": labels,
                "teamStats": t_obj.get("statistics", [])
            }

        formatted_payload = {
            "game_id": game_id,
            "teams": teams_payload,
            "quarterTotals": quarter_totals
        }
        
        return jsonify(formatted_payload), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)