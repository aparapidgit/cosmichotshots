"""
create_phase2_assets.py  -  Creates the Phase 2 data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Data/Race/DA_RaceTuning_Default   URaceTuningData with the class defaults

The project settings (Config/DefaultGame.ini, section CosmicHotshotsSettings, RaceTuning=)
point at DA_RaceTuning_Default.

HOW TO RUN
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

RACE_DATA_DIR = "/Game/Data/Race"


def create_data_asset(directory, name, asset_class):
    path = "%s/%s" % (directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("exists: %s" % path)
        return unreal.EditorAssetLibrary.load_asset(path)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(name, directory, asset_class, unreal.DataAssetFactory())
    if asset is None:
        unreal.log_error("could not create %s" % path)
        return None
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log("created: %s" % path)
    return asset


create_data_asset(RACE_DATA_DIR, "DA_RaceTuning_Default", unreal.RaceTuningData)
