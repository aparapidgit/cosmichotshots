"""
Cosmic Hotshots: sets the values of the AI difficulty assets (UE 5.7 editor Python).

  /Game/Data/AI/DA_AIDifficulty_Easy, _Normal, _Hard

Unlike create_phase2_assets.py (which only fills assets that do not exist yet), this script
CHANGES the three existing assets: it sets every property listed below and saves the asset.
It touches nothing else. Run it again whenever the values below change. The values are
placeholders tuned in headless races on the test track; playtesting has the last word.

HOW TO RUN (the editor must be closed for the second form)
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""

import unreal

AI_DATA_DIR = "/Game/Data/AI"

# Property names are the snake_case names of UAIDifficultyData's properties.
AI_DIFFICULTIES = {
    # Beatable by a beginner: clearly below the car's top speed, wide of the walls, no drift, no boost, frequent slips.
    "DA_AIDifficulty_Easy": {
        "top_speed_factor": 0.9, "corner_acceleration": 1300.0, "corner_steering_share": 0.55, "planned_braking": 1600.0,
        "apex_offset": 400.0, "apex_full_curvature": 0.00012, "wall_margin": 250.0, "wall_hug_clearance": 0.0,
        "item_box_max_sidestep": 1100.0, "steering_speed": 4.0,
        "use_drift": False, "use_boost": False,
        "top_speed_spread": 0.04, "corner_courage_spread": 0.15, "personal_line_spread": 500.0,
        "defend_max_shift": 0.0,
        "mistake_chance": 0.1, "mistake_lift_time": 0.9, "mistake_wide_offset": 500.0,
        "rubber_band_catch_up": 0.05, "rubber_band_slow_down": 0.06,
    },
    # About the pace of a car driven flat out along the middle: nearly full speed, a proper inside line,
    # half of the drivers run along the wall (at a safe distance) for boost, boost on straights and gentle bends.
    "DA_AIDifficulty_Normal": {
        "top_speed_factor": 0.99, "corner_acceleration": 2400.0, "corner_steering_share": 0.9, "planned_braking": 2000.0,
        "apex_offset": 800.0, "apex_full_curvature": 0.00005, "wall_margin": 250.0, "wall_hug_clearance": 90.0,
        "item_box_max_sidestep": 600.0, "steering_speed": 6.0,
        "use_drift": False, "use_boost": True, "boost_min_charge": 0.4, "boost_max_curvature": 0.00006,
        "top_speed_spread": 0.02, "corner_courage_spread": 0.15, "personal_line_spread": 400.0,
        "wall_hug_share": 0.7, "wall_hug_clearance_spread": 40.0, "boost_charge_spread": 0.15,
        "defend_max_shift": 200.0, "overtake_boost_charge": 0.3,
        "mistake_chance": 0.05, "mistake_lift_time": 0.6, "mistake_wide_offset": 400.0,
        "rubber_band_catch_up": 0.04, "rubber_band_slow_down": 0.04,
    },
    # Faster than a flat-out car: full speed, everybody runs right along the inside wall and boosts anywhere,
    # drifts corners of about 105 m radius or tighter that last (none on the test track), rare small slips.
    "DA_AIDifficulty_Hard": {
        "top_speed_factor": 1.0, "corner_acceleration": 4500.0, "corner_steering_share": 1.0, "planned_braking": 2600.0,
        "apex_offset": 900.0, "apex_full_curvature": 0.00003, "wall_margin": 250.0, "wall_hug_clearance": 60.0,
        "item_box_max_sidestep": 250.0, "steering_speed": 8.0,
        "use_drift": True, "drift_min_curvature": 0.000095, "drift_min_corner_time": 1.5, "drift_max_slip_angle": 18.0,
        "use_boost": True, "boost_min_charge": 0.45, "boost_max_curvature": 0.00012,
        "top_speed_spread": 0.0, "corner_courage_spread": 0.1, "personal_line_spread": 300.0,
        "wall_hug_share": 0.8, "wall_hug_clearance_spread": 40.0, "boost_charge_spread": 0.2,
        "defend_max_shift": 400.0, "overtake_boost_charge": 0.3,
        "mistake_chance": 0.03, "mistake_lift_time": 0.4, "mistake_wide_offset": 250.0,
        "rubber_band_catch_up": 0.03, "rubber_band_slow_down": 0.02,
    },
}


def main():
    for name, values in AI_DIFFICULTIES.items():
        path = "{}/{}".format(AI_DATA_DIR, name)
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            unreal.log_error("AI difficulty asset not found: {} (run create_phase2_assets.py first)".format(path))
            continue
        for property_name, value in values.items():
            asset.set_editor_property(property_name, value)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        # log_warning: plain log lines are not shown when the script runs without the editor.
        unreal.log_warning("AI difficulty updated: {} ({} values)".format(path, len(values)))


main()
