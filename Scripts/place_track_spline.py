"""
place_track_spline.py  -  Adds the race line to the test track map (Cosmic Hotshots, UE 5.7)

Places (or replaces) ONE TrackSplineActor in /Game/Maps/TestTrack, through the same centre
line generate_test_track.py builds the track from. Road, walls, checkpoints, grid and item
boxes are NOT touched. Then saves the map.

HOW TO RUN
  In the editor:   open the TestTrack map, File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generate_test_track as track  # noqa: E402

MAP_PATH = "/Game/Maps/TestTrack"


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    if world is None:
        unreal.log_error("place_track_spline: could not load %s" % MAP_PATH)
        return

    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    pts, length = track.build_centerline()
    with unreal.ScopedEditorTransaction("Place track spline"):
        track.place_track_spline(sub, pts)

    if unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        unreal.log("place_track_spline: saved %s (centre line %.0f m)" % (MAP_PATH, length))
    else:
        unreal.log_error("place_track_spline: saving %s failed" % MAP_PATH)


main()
