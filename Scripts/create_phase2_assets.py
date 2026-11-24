"""
create_phase2_assets.py  -  Creates the Phase 2 data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Data/Race/DA_RaceTuning_Default        URaceTuningData with the class defaults
  /Game/Data/Passives/DA_Passive_Spark         USparkPassiveDefinition (class defaults = placeholder values)
  /Game/Data/Passives/DA_Passive_<Name>        one per character: Sulgwen, Freya, Killjoke, Glick, Wilqis, Slot7, Slot8
  /Game/Data/Characters/DA_Character_<Name>    Spark (blue), Sulgwen (purple), Freya (red), Killjoke (green), Glick (yellow),
                                              Wilqis (white), Slot7 (orange), Slot8 (pink); each points at its passive
  /Game/Data/AI/DA_AIDifficulty_<Level>       Easy, Normal, Hard: parameter sets of the computer drivers
  /Game/Data/Items/DA_Item_Empty              the placeholder item (no effect)
  /Game/Data/Items/DA_ItemLootTable_Default   loot table: Empty for every rank band

The project settings (Config/DefaultGame.ini, section CosmicHotshotsSettings) point at
DA_RaceTuning_Default (RaceTuning=) and list the characters in the roster (+Characters=).

HOW TO RUN
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

RACE_DATA_DIR = "/Game/Data/Race"
PASSIVE_DATA_DIR = "/Game/Data/Passives"
CHARACTER_DATA_DIR = "/Game/Data/Characters"
AI_DATA_DIR = "/Game/Data/AI"
ITEM_DATA_DIR = "/Game/Data/Items"

# (asset, display name, values that differ from the class defaults of UAIDifficultyData). Placeholders to tune.
AI_DIFFICULTIES = [
    ("DA_AIDifficulty_Easy", "Easy", {
        "top_speed_factor": 0.85, "corner_acceleration": 1300.0, "planned_braking": 1600.0,
        "apex_offset": 400.0, "steering_speed": 4.0, "use_drift": False, "use_boost": False,
        "rubber_band_catch_up": 0.05, "rubber_band_slow_down": 0.06}),
    ("DA_AIDifficulty_Normal", "Normal", {}),
    ("DA_AIDifficulty_Hard", "Hard", {
        "top_speed_factor": 1.0, "corner_acceleration": 2400.0, "planned_braking": 2600.0,
        "apex_offset": 900.0, "steering_speed": 8.0, "use_drift": True, "boost_min_charge": 0.2,
        "rubber_band_catch_up": 0.03, "rubber_band_slow_down": 0.02}),
]

# (passive asset, passive class, display name)
PASSIVES = [
    ("DA_Passive_Spark", unreal.SparkPassiveDefinition, "Comeback"),
    ("DA_Passive_Sulgwen", unreal.SulgwenPassiveDefinition, "Chain overtaking"),
    ("DA_Passive_Freya", unreal.FreyaPassiveDefinition, "Aggressive overtaking"),
    ("DA_Passive_Killjoke", unreal.KilljokePassiveDefinition, "Leader hunt"),
    ("DA_Passive_Glick", unreal.GlickPassiveDefinition, "Fighting for space"),
    ("DA_Passive_Wilqis", unreal.WilqisPassiveDefinition, "Chase"),
    ("DA_Passive_Slot7", unreal.Slot7PassiveDefinition, "Drift chain"),
    ("DA_Passive_Slot8", unreal.Slot8PassiveDefinition, "Defending the lead"),
]

# (character asset, display name, colour RGBA, passive asset). Stats stay neutral until the real designs exist.
CHARACTERS = [
    ("DA_Character_Spark", "Spark", (0.05, 0.3, 1.0, 1.0), "DA_Passive_Spark"),
    ("DA_Character_Sulgwen", "Sulgwen", (0.55, 0.15, 0.9, 1.0), "DA_Passive_Sulgwen"),
    ("DA_Character_Freya", "Freya", (1.0, 0.08, 0.05, 1.0), "DA_Passive_Freya"),
    ("DA_Character_Killjoke", "Killjoke", (0.1, 0.8, 0.15, 1.0), "DA_Passive_Killjoke"),
    ("DA_Character_Glick", "Glick", (1.0, 0.85, 0.05, 1.0), "DA_Passive_Glick"),
    ("DA_Character_Wilqis", "Wilqis", (0.95, 0.95, 0.95, 1.0), "DA_Passive_Wilqis"),
    # Slots 7 and 8: working names until the characters are named.
    ("DA_Character_Slot7", "Slot7", (1.0, 0.45, 0.05, 1.0), "DA_Passive_Slot7"),
    ("DA_Character_Slot8", "Slot8", (1.0, 0.35, 0.7, 1.0), "DA_Passive_Slot8"),
]


def create_data_asset(directory, name, asset_class):
    """Returns (asset, created)."""
    path = "%s/%s" % (directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("exists: %s" % path)
        return unreal.EditorAssetLibrary.load_asset(path), False

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(name, directory, asset_class, unreal.DataAssetFactory())
    if asset is None:
        unreal.log_error("could not create %s" % path)
        return None, False
    unreal.log("created: %s" % path)
    return asset, True


def main():
    asset, created = create_data_asset(RACE_DATA_DIR, "DA_RaceTuning_Default", unreal.RaceTuningData)
    if created:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    passives = {}
    for name, asset_class, display_name in PASSIVES:
        passive, created = create_data_asset(PASSIVE_DATA_DIR, name, asset_class)
        passives[name] = passive
        if created:
            passive.set_editor_property("display_name", display_name)
            unreal.EditorAssetLibrary.save_loaded_asset(passive)

    for name, display_name, colour, passive_name in CHARACTERS:
        character, created = create_data_asset(CHARACTER_DATA_DIR, name, unreal.CharacterDefinition)
        if created:
            character.set_editor_property("display_name", display_name)
            character.set_editor_property("colour", unreal.LinearColor(*colour))
            character.set_editor_property("passive_definition", passives[passive_name])
            unreal.EditorAssetLibrary.save_loaded_asset(character)

    for name, display_name, values in AI_DIFFICULTIES:
        difficulty, created = create_data_asset(AI_DATA_DIR, name, unreal.AIDifficultyData)
        if created:
            difficulty.set_editor_property("display_name", display_name)
            for property_name, value in values.items():
                difficulty.set_editor_property(property_name, value)
            unreal.EditorAssetLibrary.save_loaded_asset(difficulty)

    # --- items: one placeholder item and a loot table that gives it to every race place
    empty_item, created = create_data_asset(ITEM_DATA_DIR, "DA_Item_Empty", unreal.ItemDefinition)
    if created:
        empty_item.set_editor_property("item_id", "Empty")
        empty_item.set_editor_property("display_name", "Empty")
        unreal.EditorAssetLibrary.save_loaded_asset(empty_item)

    loot_table, created = create_data_asset(ITEM_DATA_DIR, "DA_ItemLootTable_Default", unreal.ItemLootTable)
    if created:
        bands = []
        for min_rank, max_rank in ((1, 1), (2, 3), (4, 6), (7, 8)):
            entry = unreal.ItemLootEntry()
            entry.set_editor_property("item", empty_item)
            entry.set_editor_property("weight", 1.0)
            band = unreal.ItemLootBand()
            band.set_editor_property("min_rank", min_rank)
            band.set_editor_property("max_rank", max_rank)
            band.set_editor_property("entries", [entry])
            bands.append(band)
        loot_table.set_editor_property("bands", bands)
        unreal.EditorAssetLibrary.save_loaded_asset(loot_table)


main()
