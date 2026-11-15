"""Verified minigame starts in the 1.4.3 location and actor tables."""

CATEGORY = "小游戏"

# Native place labels. Reuse the game's location records and visit flags.
LOCATION_ACTIVITIES = {
    "GameBitalockGolf": "ball_haul",
    "GameShiledSurf": "shield_surfing",
    "ValleyVillage": "flight_range",
    "Circuit_Ichikara": "tarrey_rally",
    "Circuit_Uotori": "lurelin_rally",
    "SkyIsland0001": "courage_island",
    "SkyIsland0002": "bravery_island",
    "SkyIsland0003": "valor_island",
}

# Registration NPCs from the corresponding EventFlow files. Placement IDs
# distinguish copies of the same character.
ACTOR_ACTIVITIES = {
    14808125152899804756: "ball_toss",              # Npc_MiniGame_Basketball
    8096764838095634820: "desert_race",            # Npc_GerudoDesert001_Minigame
    14118764274026021738: "wing_flight",           # Npc_Minigame_BirdMan001
    18337801341533657739: "yiga_training",         # Npc_Assassin_Middle_Azito001
    12388881017280410959: "gong",                  # Npc_SmallOasis001
    12642686300924923755: "horse_obstacles",       # Npc_FaronWoods010
    4133078490861602972: "plush_field",            # Npc_HighMountain041 / Ground
    10381961885709758924: "plush_lakeside",         # Npc_HighMountain041 / LakeSide
    14635949842126189684: "plush_cliff",            # Npc_HighMountain041 / Cliff
    3389724788057670177: "mine_carts",             # Npc_Gaman01 / Mine-Cart Land
    5090719780544004038: "death_mountain_course",   # Npc_Gaman03 / Death Mountain
}
