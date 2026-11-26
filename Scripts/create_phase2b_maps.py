"""
create_phase2b_maps.py  -  Creates the Phase 2b maps (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Maps/MainMenu      empty map; World Settings > GameMode Override = MenuGameMode
                           (no vehicle, the player controller shows the main menu screen)
  /Game/Maps/AdventureHub  sun, sky light, sky and a PlayerStart; GameMode Override = AdventureHubGameMode.
                           The hub itself (floors, walls, gates, portals) is NOT in the map: the game mode
                           builds it from the Adventure campaign and the hub tuning when the map starts.

The project settings point at them: Config/DefaultEngine.ini (GameDefaultMap = MainMenu) and
Config/DefaultGame.ini (CosmicHotshotsSettings: MainMenuMap, AdventureHubMap).

HOW TO RUN (the editor must be closed)
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

MAIN_MENU_MAP = "/Game/Maps/MainMenu"
ADVENTURE_HUB_MAP = "/Game/Maps/AdventureHub"

# Placeholder lighting of the hub (degrees, cm).
HUB_SUN_PITCH = -50.0
HUB_SUN_YAW = 35.0
HUB_LIGHTS_HEIGHT = 2000.0
HUB_PLAYER_START = (500.0, 0.0, 150.0)


def populate_hub(world):
    """Light and sky for the grey hub, and a PlayerStart the game mode needs to spawn the player."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    above = unreal.Vector(0.0, 0.0, HUB_LIGHTS_HEIGHT)

    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, above, unreal.Rotator(roll=0.0, pitch=HUB_SUN_PITCH, yaw=HUB_SUN_YAW))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)

    sky_light = actors.spawn_actor_from_class(unreal.SkyLight, above)
    sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)

    actors.spawn_actor_from_class(unreal.SkyAtmosphere, above)
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(*HUB_PLAYER_START))


def create_map(map_path, game_mode_class, populate=None):
    """Creates a map with a game mode override. Returns True when the map was created."""
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.log("exists: %s" % map_path)
        return False

    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        unreal.log_error("could not create a blank map for %s" % map_path)
        return False

    world.get_world_settings().set_editor_property("default_game_mode", game_mode_class)
    if populate:
        populate(world)

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, map_path):
        unreal.log_error("saving %s failed" % map_path)
        return False
    unreal.log("created: %s" % map_path)
    return True


def main():
    create_map(MAIN_MENU_MAP, unreal.MenuGameMode)
    create_map(ADVENTURE_HUB_MAP, unreal.AdventureHubGameMode, populate_hub)


main()
