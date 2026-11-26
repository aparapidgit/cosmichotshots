"""
Cosmic Hotshots: tuning values of the default vehicle (UE 5.7 editor Python).

  /Game/Data/Vehicles/DA_VehicleTuning_Default

CHANGES the existing asset: sets the properties listed below and saves it. Nothing else in the
asset is touched. Run it again whenever a value below changes.

Why these values (measured with one Hard computer driver on the Corners track, 2026-11-27):
  drift_grip 2 -> 3      With grip 2 a drifting car settles at a slide of (speed / corner radius) / 2;
                         in the 35 m hairpin at 140 km/h that is more than the critical 30 degrees.
  drift_min_turn 0.35 -> 0.2
                         With 0.35 a drifting car at top speed turns no wider than about 98 m, so in
                         corners of 80 to 150 m it spirals into the inside wall. With 0.2 the stick can
                         open the line that far.
  Together: the lap is 1.2 s faster (52.5 s instead of 53.7 s), with 11.5 s of drift per lap paid
  back as 3.2 s of boost, and no wall contact. Grip 4 is a touch faster but slides and charges less.

HOW TO RUN (the editor must be closed for the second form)
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""

import unreal

TUNING_PATH = "/Game/Data/Vehicles/DA_VehicleTuning_Default"

# Property names are the snake_case names of UVehicleTuningData's properties.
VALUES = {
    "drift_grip": 3.0,
    "drift_min_turn": 0.2,
}


def main():
    asset = unreal.EditorAssetLibrary.load_asset(TUNING_PATH)
    if not asset:
        unreal.log_error("vehicle tuning asset not found: {}".format(TUNING_PATH))
        return
    for property_name, value in VALUES.items():
        before = asset.get_editor_property(property_name)
        asset.set_editor_property(property_name, value)
        # log_warning: plain log lines are not shown when the script runs without the editor.
        unreal.log_warning("vehicle tuning {}: {} -> {}".format(property_name, before, value))
    unreal.EditorAssetLibrary.save_loaded_asset(asset)


main()
