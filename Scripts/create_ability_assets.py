"""
Cosmic Hotshots: the characters' active abilities (UE 5.7 editor Python).

  /Game/Data/Abilities/DA_Ability_<Character>   one per character that has a designed ability:
                                                Spark, Freya, Killjoke, Glick, Wilqis, Sulgwen
  /Game/Data/Characters/DA_Character_<Character>   CHANGED: "active_ability" is set to that asset

Creates the ability assets that are missing, SETS the values listed below on all six (run it again
after changing a value here), and points each character at its ability. Nothing else in the
character assets is touched. Slot7 and Slot8 have no designed ability and get none.
All values are placeholders to tune by playtesting.

HOW TO RUN (the editor must be closed for the second form)
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""

import unreal

ABILITY_DIR = "/Game/Data/Abilities"
CHARACTER_DIR = "/Game/Data/Characters"

# character: (definition class, values). Times in s, distances in cm, accelerations in cm/s^2.
# Every ability: active_time = how long it counts as running; cooldown = wait after it ended.
ABILITIES = {
    "Freya": (unreal.FreyaSurgeDefinition, {
        "display_name": "Shared Surge",
        "description": "A strong, short burst of acceleration. Every other driver gets a weaker one at the same time.",
        "active_time": 2.0, "cooldown": 2.0, "fade_out": 0.5,
        "self_acceleration": 2.0, "self_max_speed": 1.15,
        "others_acceleration": 1.3, "others_max_speed": 1.05, "others_duration": 1.5,
    }),
    "Sulgwen": (unreal.SulgwenSurgeDefinition, {
        "display_name": "Surge",
        "description": "A short burst of acceleration, and the leader briefly loses part of their speed. Cannot be used while leading.",
        "active_time": 2.0, "cooldown": 2.0, "fade_out": 0.5,
        "self_acceleration": 1.8, "self_max_speed": 1.12,
        "leader_max_speed": 0.85, "leader_duration": 1.5,
    }),
    "Killjoke": (unreal.KilljokeDisruptionDefinition, {
        "display_name": "Drive Disruption",
        "description": "Every rival ahead briefly loses part of their speed.",
        "active_time": 1.5, "cooldown": 2.0, "fade_out": 0.5,
        "rival_max_speed": 0.88, "rival_duration": 1.5,
    }),
    "Glick": (unreal.GlickMagnetDefinition, {
        "display_name": "Magnetic Field",
        "description": "Briefly pulls nearby vehicles towards Glick.",
        "active_time": 1.5, "cooldown": 2.0,
        "range": 2500.0, "pull_acceleration": 1500.0,
    }),
    "Spark": (unreal.SparkTeleportDefinition, {
        "display_name": "Teleport",
        "description": "Appears a safe distance behind the nearest rival ahead, if that rival is in range.",
        "active_time": 0.5, "cooldown": 2.0,
        "range": 6000.0, "behind_distance": 900.0, "free_length": 600.0, "free_width": 300.0,
    }),
    "Wilqis": (unreal.WilqisSwapDefinition, {
        "display_name": "Position Swap",
        "description": "Swaps places with the nearest rival ahead within a short range. Both keep their own speed.",
        "active_time": 0.5, "cooldown": 2.0,
        "range": 3000.0,
    }),
}


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for character, (definition_class, values) in ABILITIES.items():
        asset_name = "DA_Ability_" + character
        path = "{}/{}".format(ABILITY_DIR, asset_name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            ability = unreal.EditorAssetLibrary.load_asset(path)
        else:
            ability = tools.create_asset(asset_name, ABILITY_DIR, definition_class, unreal.DataAssetFactory())
        if ability is None:
            unreal.log_error("could not create or load {}".format(path))
            continue
        for property_name, value in values.items():
            ability.set_editor_property(property_name, value)
        unreal.EditorAssetLibrary.save_loaded_asset(ability)

        character_path = "{}/DA_Character_{}".format(CHARACTER_DIR, character)
        character_asset = unreal.EditorAssetLibrary.load_asset(character_path)
        if character_asset is None:
            unreal.log_error("character not found: {}".format(character_path))
            continue
        if character_asset.get_editor_property("active_ability") != ability:
            character_asset.set_editor_property("active_ability", ability)
            unreal.EditorAssetLibrary.save_loaded_asset(character_asset)
        # log_warning: plain log lines are not shown when the script runs without the editor.
        unreal.log_warning("ability written: {} -> {}".format(path, character_path))


main()
