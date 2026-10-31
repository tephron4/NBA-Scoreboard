from PIL import Image
import requests
from io import BytesIO

def convert_url_to_rgb565(image_url, variable_name):
    try:
        # Fetch the image directly from the URL
        response = requests.get(image_url)
        response.raise_for_status()
        
        # Open and resize to 8x8 pixels
        img = Image.open(BytesIO(response.content)).convert("RGB")
        img = img.resize((8, 8))
        width, height = img.size
        
        print(f"// Generated from: {image_url}")
        print(f"const uint16_t {variable_name}[{width * height}] = {{")
        
        for y in range(height):
            row_str = "    "
            for x in range(width):
                r, g, b = img.getpixel((x, y))
                # Convert 8-bit RGB to 16-bit RGB565
                rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                row_str += f"0x{rgb565:04X}, "
            print(row_str)
        print("};\n")
    except Exception as e:
        print(f"Error processing URL: {e}")

# Example usage (just drop your ESPN logo link here):
# convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/lal.png", "logo_LAL")

convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/bos.png", "logo_BOS")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/bos.png", "logo_BOS_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/chi.png", "logo_CHI")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/chi.png", "logo_CHI_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/bkn.png", "logo_BKN")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/bkn.png", "logo_BKN_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/det.png", "logo_DET")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/det.png", "logo_DET_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/phi.png", "logo_PHI")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/phi.png", "logo_PHI_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/ny.png", "logo_NY")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/ny.png", "logo_NY_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/tor.png", "logo_TOR")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/tor.png", "logo_TOR_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/orl.png", "logo_ORL")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/orl.png", "logo_ORL_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/mia.png", "logo_MIA")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/mia.png", "logo_MIA_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/ind.png", "logo_IND")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/ind.png", "logo_IND_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/dal.png", "logo_DAL")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/dal.png", "logo_DAL_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/hou.png", "logo_HOU")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/hou.png", "logo_HOU_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/min.png", "logo_MIN")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/min.png", "logo_MIN_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/lac.png", "logo_LAC")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/lac.png", "logo_LAC_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/gs.png", "logo_GS")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/gs.png", "logo_GS_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/lal.png", "logo_LAL")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/lal.png", "logo_LAL_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/phx.png", "logo_PHX")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/phx.png", "logo_PHX_dark")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500/scoreboard/den.png", "logo_DEN")
convert_url_to_rgb565("https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/den.png", "logo_DEN_dark")