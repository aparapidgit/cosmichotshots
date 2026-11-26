"""
Cosmic Hotshots: the track zone types (UE 5.7 editor Python).

  /Game/Data/Zones/DA_Zone_<Type>   one UTrackZoneDefinition per zone type (9 types)

Creates the assets that are missing and SETS the values listed below on all nine (run it again
after changing a value here). A zone on a map is an ATrackZone actor that points at one of these
assets; no zone is placed on any map by this script. All values are placeholders to tune.

HOW TO RUN (the editor must be closed for the second form)
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""

import unreal

ZONE_DIR = "/Game/Data/Zones"
T = unreal.TrackZoneType

# Lengths in cm, accelerations in cm/s^2, times in s. Half width 0 = from wall to wall.
ZONES = {
    "TurboHub": {
        "type": T.TURBO_HUB, "display_name": "Turbo Hub", "colour": (0.1, 1.0, 0.2),
        "half_length": 600.0, "half_width": 400.0,
        "acceleration_multiplier": 2.5, "speed_multiplier": 1.2, "duration": 2.5, "fade_out": 1.0,
    },
    "EnergyDock": {
        "type": T.ENERGY_DOCK, "display_name": "Energy Dock", "colour": (0.1, 0.6, 1.0),
        "half_length": 400.0, "half_width": 400.0,
        "segments": 1, "cooldown": 20.0,
    },
    "GravityAbyss": {
        "type": T.GRAVITY_ABYSS, "display_name": "Gravity Abyss", "colour": (0.05, 0.0, 0.1),
        "half_length": 500.0, "half_width": 450.0,
        "speed_multiplier": 0.75, "duration": 1.5, "fade_out": 0.5, "control_loss_time": 0.6, "cooldown": 5.0,
    },
    "Magnetonium": {
        "type": T.MAGNETONIUM, "display_name": "Magnetonium", "colour": (0.7, 0.2, 0.9),
        "radius": 1500.0, "pull_acceleration": 1800.0, "along_track_share": 0.3,
    },
    "DustVortex": {
        "type": T.DUST_VORTEX, "display_name": "Dust Vortex", "colour": (0.8, 0.65, 0.4),
        "radius": 1100.0, "core_radius": 250.0, "pull_acceleration": 1500.0, "along_track_share": 1.0,
        "speed_multiplier": 0.7, "duration": 0.3, "fade_out": 0.7, "control_loss_time": 0.8, "cooldown": 3.0,
    },
    "ArrowsPad": {
        "type": T.ARROWS_PAD, "display_name": "Arrows Pad", "colour": (1.0, 0.85, 0.1),
        "half_length": 600.0, "half_width": 350.0, "pull_acceleration": 3000.0,
    },
    "SpectralTower": {
        "type": T.SPECTRAL_TOWER, "display_name": "Spectral Tower", "colour": (0.6, 0.9, 1.0),
        "half_length": 400.0, "half_width": 400.0,
        "acceleration_multiplier": 1.6, "speed_multiplier": 1.05, "duration": 1.5, "fade_out": 0.5, "cooldown": 6.0,
    },
    "MeteorCluster": {
        "type": T.METEOR_CLUSTER, "display_name": "Meteor Cluster", "colour": (1.0, 0.35, 0.1),
        "half_length": 500.0, "half_width": 0.0,
        "meteor_period": 1.5, "meteor_warning": 1.0, "meteor_radius": 250.0,
        "segments": 1, "control_loss_time": 0.7, "speed_loss_share": 0.25,
    },
    "WarpGate": {
        "type": T.WARP_GATE, "display_name": "Warp Gate", "colour": (0.2, 1.0, 0.9),
        "half_length": 150.0, "half_width": 300.0, "warp_distance": 1000.0, "cooldown": 2.0,
    },
}


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name, values in ZONES.items():
        asset_name = "DA_Zone_" + name
        path = "{}/{}".format(ZONE_DIR, asset_name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            asset = unreal.EditorAssetLibrary.load_asset(path)
        else:
            asset = tools.create_asset(asset_name, ZONE_DIR, unreal.TrackZoneDefinition, unreal.DataAssetFactory())
        if asset is None:
            unreal.log_error("could not create or load {}".format(path))
            continue
        for property_name, value in values.items():
            if property_name == "colour":
                value = unreal.LinearColor(value[0], value[1], value[2], 1.0)
            asset.set_editor_property(property_name, value)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        # log_warning: plain log lines are not shown when the script runs without the editor.
        unreal.log_warning("zone type written: {}".format(path))


main()
