"""
create_track_corners.py  -  Second grey-box track: "Corners" (Cosmic Hotshots, UE 5.7)

A track with REAL corners, built by the same code as the test track (generate_test_track.py):
the test track is so gentle that everything is driven flat out, so drifting, braking and
racing lines cannot be tuned on it.

Creates what is missing and never replaces what exists:
  /Game/Maps/Track_Corners              sun, sky light, sky; road, walls, checkpoints, 8 grid slots,
                                        item box clusters and the race line (TrackSplineActor)
  /Game/Data/Tracks/DA_Track_Corners    track definition (name, map, 3 laps)
Config/DefaultGame.ini lists the track (Tracks), so the track select screen offers it.

It also puts the Adventure levels of section 2 on this track (developer decision): the "track" of the
existing assets DA_AdventureLevel_S2_L1 .. S2_L4 is CHANGED to DA_Track_Corners. Nothing else in them is touched.

Layout (24 m wide, about 2.5 km, driven clockwise; radii of the centre line):
  start straight 350 m -> fast right 150 m -> medium left 80 m (90 degrees) -> hairpin right 35 m (180 degrees)
  -> S-bend left / right 60 m -> back straight 630 m with a hill -> long right 100 m (120 degrees)
  -> chicane left / right 50 m -> right 70 m back onto the start straight.
What the car can do at its top speed of 170 km/h: without a drift it turns no tighter than about 45 m,
so the hairpin needs braking; a drift turns it on about 40 to 100 m.

HOW TO RUN (the editor must be closed)
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
  To rebuild the track actors of the existing map after changing the layout below, set the environment
  variable CH_TRACK_REBUILD=1 first (only the folder "Track_Corners" of that map is rebuilt).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_test_track as track  # noqa: E402

MAP_PATH = "/Game/Maps/Track_Corners"
TRACK_DATA_DIR = "/Game/Data/Tracks"
TRACK_ASSET = "DA_Track_Corners"
TRACK_DISPLAY_NAME = "Corners"
DEFAULT_LAPS = 3
# Existing Adventure levels that race on this track (their "track" property is set to it).
ADVENTURE_LEVELS = ["/Game/Data/Adventure/DA_AdventureLevel_S2_L%d" % index for index in range(1, 5)]

# ---------------------------------------------------------------- layout
# Straight lines and arcs from the start line; a positive angle turns right. The two long straights
# (628.76 m and 153.57 m) are solved so that the loop closes exactly.
PATH = [
    ("line", 350.0),
    ("arc", 150.0, 90.0),                       # fast right
    ("line", 60.0),
    ("arc", 80.0, -90.0),                       # medium left
    ("line", 100.0),
    ("arc", 35.0, 180.0),                       # hairpin right
    ("line", 120.0),
    ("arc", 60.0, -60.0), ("arc", 60.0, 60.0),  # S-bend
    ("line", 628.7586419431889),                # back straight (hill)
    ("arc", 100.0, 120.0),                      # long right
    ("line", 153.56843517611858),
    ("arc", 50.0, -40.0), ("arc", 50.0, 40.0),  # chicane
    ("line", 40.0),
    ("arc", 70.0, 60.0),                        # onto the start straight
    ("line", 90.0),
]
TRACK_WIDTH_M = 24.0
# Short segments keep the walls smooth in the tight corners; the overlap closes the wedge-shaped
# gaps between the straight road pieces on the outside of the hairpin (outer radius / centre radius).
SAMPLE_STEP_M = 4.0
SEGMENT_OVERLAP = 1.5
HILLS = [(0.60, 140.0, 8.0)]                    # on the back straight
ITEM_BOX_SPOTS = [0.08, 0.25, 0.43, 0.58, 0.68, 0.87]   # all on straights
ITEM_BOX_LATERAL_M = [-7.0, 0.0, 7.0]
FOLDER = "Track_Corners"

# Placeholder lighting (degrees, cm).
SUN_PITCH = -50.0
SUN_YAW = 35.0
LIGHTS_HEIGHT = 2000.0


def apply_layout():
    """The generator reads its layout from module settings: put this track's values there."""
    track.PATH = PATH
    track.TRACK_WIDTH_M = TRACK_WIDTH_M
    track.SAMPLE_STEP_M = SAMPLE_STEP_M
    track.SEGMENT_OVERLAP = SEGMENT_OVERLAP
    track.HILLS = HILLS
    track.ITEM_BOX_SPOTS = ITEM_BOX_SPOTS
    track.ITEM_BOX_LATERAL_M = ITEM_BOX_LATERAL_M
    track.FOLDER = FOLDER


def add_lights():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    above = unreal.Vector(0.0, 0.0, LIGHTS_HEIGHT)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, above, unreal.Rotator(roll=0.0, pitch=SUN_PITCH, yaw=SUN_YAW))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_light = actors.spawn_actor_from_class(unreal.SkyLight, above)
    sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, above)


def create_map():
    exists = unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH)
    rebuild = os.environ.get("CH_TRACK_REBUILD", "") == "1"
    if exists and not rebuild:
        unreal.log_warning("exists, left as it is: %s" % MAP_PATH)
        return

    if exists:
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    else:
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world is None:
        unreal.log_error("could not open a map for %s" % MAP_PATH)
        return
    if not exists:
        world.get_world_settings().set_editor_property("default_game_mode", unreal.RaceGameMode)
        add_lights()

    apply_layout()
    track.build_in_unreal()

    if unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        unreal.log_warning("%s: %s" % ("rebuilt" if exists else "created", MAP_PATH))
    else:
        unreal.log_error("saving %s failed" % MAP_PATH)


def create_track_definition():
    path = "%s/%s" % (TRACK_DATA_DIR, TRACK_ASSET)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning("exists, left as it is: %s" % path)
        return
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(TRACK_ASSET, TRACK_DATA_DIR, unreal.TrackDefinition, unreal.DataAssetFactory())
    if asset is None:
        unreal.log_error("could not create %s" % path)
        return
    asset.set_editor_property("display_name", TRACK_DISPLAY_NAME)
    asset.set_editor_property("map", unreal.SoftObjectPath("%s.%s" % (MAP_PATH, MAP_PATH.rsplit("/", 1)[1])))
    asset.set_editor_property("default_laps", DEFAULT_LAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log_warning("created: %s" % path)


def set_adventure_levels():
    track_asset = unreal.EditorAssetLibrary.load_asset("%s/%s" % (TRACK_DATA_DIR, TRACK_ASSET))
    if track_asset is None:
        unreal.log_error("no track definition: Adventure levels left as they are")
        return
    for path in ADVENTURE_LEVELS:
        level = unreal.EditorAssetLibrary.load_asset(path)
        if level is None:
            unreal.log_error("Adventure level not found: %s" % path)
        elif level.get_editor_property("track") != track_asset:
            level.set_editor_property("track", track_asset)
            unreal.EditorAssetLibrary.save_loaded_asset(level)
            unreal.log_warning("track set to %s: %s" % (TRACK_ASSET, path))


def main():
    create_map()
    create_track_definition()
    set_adventure_levels()


main()
