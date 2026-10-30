from flask import Flask, jsonify, request, render_template_string
import requests
from datetime import datetime

app = Flask(__name__)

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
}

# Global control state managed by the web dashboard
scoreboard_state = {
    "view": "list",           # "list" or "boxscore"
    "selected_index": 0       # Currently highlighted game row
}

# --- HTML / CSS CONTROL DASHBOARD TEMPLATE ---
CONTROL_PAGE = """
<!DOCTYPE html>
<html>
<head>
    <title>NBA Scoreboard Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #121212; color: #fff; text-align: center; margin: 0; padding: 20px; }
        h1 { color: #ffa500; font-size: 1.5rem; }
        .card { background: #1e1e1e; border-radius: 12px; padding: 15px; max-width: 400px; margin: 0 auto 20px auto; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
        .btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 15px; }
        button { background: #333; color: white; border: none; padding: 15px; font-size: 1rem; border-radius: 8px; cursor: pointer; font-weight: bold; }
        button:active { background: #555; }
        button.primary { background: #007acc; grid-column: span 2; }
        button.accent { background: #d97706; grid-column: span 2; }
        .status { font-size: 0.9rem; color: #aaa; margin-top: 10px; }
    </style>
</head>
<body>
    <h1>NBA Scoreboard Control</h1>
    
    <div class="card">
        <div class="status">Current View: <span id="lbl-view" style="color:#00ffff;">Loading...</span></div>
        <div class="status">Selected Index: <span id="lbl-index" style="color:#ffa500;">0</span></div>
        
        <div class="btn-grid">
            <button onclick="sendAction('up')">▲ Up</button>
            <button onclick="sendAction('down')">▼ Down</button>
            <button class="primary" onclick="sendAction('toggle_view')">Toggle Box Score / List</button>
            <button class="accent" onclick="sendAction('refresh')">Force Refresh Data</button>
        </div>
    </div>

    <script>
        function sendAction(action) {
            fetch('/api/control', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: action })
            })
            .then(res => res.json())
            .then(data => {
                document.getElementById('lbl-view').innerText = data.view.toUpperCase();
                document.getElementById('lbl-index').innerText = data.selected_index;
            });
        }

        // Poll state to keep UI in sync
        setInterval(() => {
            fetch('/api/state')
            .then(res => res.json())
            .then(data => {
                document.getElementById('lbl-view').innerText = data.view.toUpperCase();
                document.getElementById('lbl-index').innerText = data.selected_index;
            });
        }, 2000);
    </script>
</body>
</html>
"""

@app.route('/')
@app.route('/control')
def control_panel():
    return render_template_string(CONTROL_PAGE)

@app.route('/api/state', methods=['GET'])
def get_state():
    return jsonify(scoreboard_state)

@app.route('/api/control', methods=['POST'])
def control_action():
    data = request.json
    action = data.get('action')
    
    if action == 'up':
        scoreboard_state['selected_index'] = max(0, scoreboard_state['selected_index'] - 1)
    elif action == 'down':
        scoreboard_state['selected_index'] += 1  # Will clamp against total games in live-scores route if needed
    elif action == 'toggle_view':
        if scoreboard_state['view'] == 'list':
            scoreboard_state['view'] = 'boxscore'
        else:
            scoreboard_state['view'] = 'list'
            
    return jsonify(scoreboard_state)

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
        competitors = header_competitions.get("competitions", [])
        
        # Extract game status, quarter, and clock details
        status_info = header_competitions.get("status", {})
        game_status = {
            "detail": status_info.get("type", {}).get("detail"),       # e.g., "3rd Qtr - 4:12" or "Final"
            "state": status_info.get("type", {}).get("name"),         # e.g., "STATUS_IN_PROGRESS" or "STATUS_FINAL"
            "period": status_info.get("period"),                       # e.g., 3
            "clock": status_info.get("displayClock"),                 # e.g., "4:12"
            "completed": status_info.get("type", {}).get("completed", False)
        }
        
        # Build quarter/line scores mapping
        quarter_totals = {}
        for comp in competitors:
            tricode = comp.get("team", {}).get("abbreviation")
            linescores = comp.get("linescores", [])
            quarter_totals[tricode] = [ls.get("displayValue") for ls in linescores]

        teams_payload = {}
        espn_teams = boxscore_data.get("teams", [])
        espn_players_groups = boxscore_data.get("players", [])
        
        for i, t_obj in enumerate(espn_teams):
            team_info = t_obj.get("team", {})
            tricode = team_info.get("abbreviation")
            home_away = t_obj.get("homeAway") # "home" or "away"
            
            active_players = []
            inactive_players = []
            labels = []
            
            if i < len(espn_players_groups):
                player_group = espn_players_groups[i]
                statistics_categories = player_group.get("statistics", [])
                
                if statistics_categories:
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
            "status": game_status,
            "teams": teams_payload,
            "quarterTotals": quarter_totals
        }
        
        return jsonify(formatted_payload), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)