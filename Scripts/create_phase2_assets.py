"""
create_phase2_assets.py  -  Creates the Phase 2 data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Data/Race/DA_RaceTuning_Default        URaceTuningData with the class defaults
  /Game/Data/Passives/DA_Passive_Spark         USparkPassiveDefinition (class defaults = placeholder values)
  /Game/Data/Passives/DA_Passive_Sulgwen       USulgwenPassiveDefinition
  /Game/Data/Characters/DA_Character_Spark     blue, passive = DA_Passive_Spark
  /Game/Data/Characters/DA_Character_Sulgwen   purple, passive = DA_Passive_Sulgwen

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

# (passive asset, passive class, display name)
PASSIVES = [
    ("DA_Passive_Spark", unreal.SparkPassiveDefinition, "Comeback"),
    ("DA_Passive_Sulgwen", unreal.SulgwenPassiveDefinition, "Chain overtaking"),
]

# (character asset, display name, colour RGBA, passive asset). Stats stay neutral until the real designs exist.
CHARACTERS = [
    ("DA_Character_Spark", "Spark", (0.05, 0.3, 1.0, 1.0), "DA_Passive_Spark"),
    ("DA_Character_Sulgwen", "Sulgwen", (0.55, 0.15, 0.9, 1.0), "DA_Passive_Sulgwen"),
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


main()
