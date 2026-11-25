"""
create_phase2b_maps.py  -  Creates the Phase 2b maps (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Maps/MainMenu    empty map; World Settings > GameMode Override = MenuGameMode
                         (no vehicle, the player controller shows the main menu screen)

The project settings point at it: Config/DefaultEngine.ini (GameDefaultMap) and
Config/DefaultGame.ini (CosmicHotshotsSettings, MainMenuMap).

HOW TO RUN (the editor must be closed)
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

MAIN_MENU_MAP = "/Game/Maps/MainMenu"


def create_map(map_path, game_mode_class):
    """Creates an empty map with a game mode override. Returns True when the map was created."""
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.log("exists: %s" % map_path)
        return False

    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        unreal.log_error("could not create a blank map for %s" % map_path)
        return False

    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        unreal.log_error("saving %s failed" % map_path)
        return False
    unreal.log("created: %s" % map_path)
    return True


def main():
    create_map(MAIN_MENU_MAP, unreal.MenuGameMode)


main()
