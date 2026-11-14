"""
create_phase1_assets.py  -  Creates the Phase 1 data assets (Cosmic Hotshots, UE 5.7)

Creates, only if they do not exist yet (existing assets are never overwritten):
  /Game/Input/Actions/IA_Throttle, IA_Brake, IA_Steer, IA_Drift, IA_Boost, IA_UseItem
  /Game/Input/IMC_Driving              default gamepad + keyboard bindings
  /Game/Input/DA_DrivingInput          UDrivingInputConfig pointing at the assets above
  /Game/Data/Vehicles/DA_VehicleTuning_Default   UVehicleTuningData with the class defaults

The project settings (Config/DefaultGame.ini, section CosmicHotshotsSettings) point at
DA_DrivingInput and DA_VehicleTuning_Default.

HOW TO RUN
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"

To change a default binding, edit IMC_Driving in the editor (or delete the asset and
re-run this script after changing BINDINGS below).
"""
import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
VEHICLE_DATA_DIR = "/Game/Data/Vehicles"

# action name -> value type
ACTIONS = {
    "IA_Throttle": unreal.InputActionValueType.AXIS1D,
    "IA_Brake": unreal.InputActionValueType.AXIS1D,
    "IA_Steer": unreal.InputActionValueType.AXIS1D,
    "IA_Drift": unreal.InputActionValueType.BOOLEAN,
    "IA_Boost": unreal.InputActionValueType.BOOLEAN,
    "IA_UseItem": unreal.InputActionValueType.BOOLEAN,
}

# (action, key name, modifiers)   modifiers: "negate", "deadzone"
BINDINGS = [
    ("IA_Throttle", "Gamepad_RightTriggerAxis", []),
    ("IA_Throttle", "W", []),
    ("IA_Throttle", "Up", []),
    ("IA_Brake", "Gamepad_LeftTriggerAxis", []),
    ("IA_Brake", "S", []),
    ("IA_Brake", "Down", []),
    ("IA_Steer", "Gamepad_LeftX", ["deadzone"]),
    ("IA_Steer", "D", []),
    ("IA_Steer", "Right", []),
    ("IA_Steer", "A", ["negate"]),
    ("IA_Steer", "Left", ["negate"]),
    ("IA_Drift", "Gamepad_RightShoulder", []),
    ("IA_Drift", "SpaceBar", []),
    ("IA_Boost", "Gamepad_FaceButton_Bottom", []),
    ("IA_Boost", "LeftShift", []),
    ("IA_UseItem", "Gamepad_FaceButton_Left", []),
    ("IA_UseItem", "E", []),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create(name, folder, asset_class, factory):
    """Returns (asset, created). Never overwrites an existing asset."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        unreal.log("Phase1Assets: keeping existing %s" % path)
        return eal.load_asset(path), False
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create %s" % path)
    unreal.log("Phase1Assets: created %s" % path)
    return asset, True


def make_key(key_name):
    key = unreal.Key()
    key.import_text(key_name)
    return key


def make_modifiers(owner, names):
    modifiers = []
    for name in names:
        if name == "negate":
            modifiers.append(unreal.new_object(unreal.InputModifierNegate, owner))
        elif name == "deadzone":
            # Engine default dead zone (lower threshold 0.2). Tune it on the IMC asset in the editor.
            modifiers.append(unreal.new_object(unreal.InputModifierDeadZone, owner))
        else:
            raise ValueError("Unknown modifier %s" % name)
    return modifiers


def main():
    created_any = []

    # --- input actions
    actions = {}
    for name, value_type in ACTIONS.items():
        action, created = get_or_create(name, ACTIONS_DIR, unreal.InputAction, unreal.InputAction_Factory())
        if created:
            action.set_editor_property("value_type", value_type)
            created_any.append(action)
        actions[name] = action

    # --- mapping context
    imc, created = get_or_create("IMC_Driving", INPUT_DIR, unreal.InputMappingContext,
                                 unreal.InputMappingContext_Factory())
    if created:
        mappings = []
        for action_name, key_name, modifier_names in BINDINGS:
            mapping = unreal.EnhancedActionKeyMapping()
            mapping.set_editor_property("action", actions[action_name])
            mapping.set_editor_property("key", make_key(key_name))
            mapping.set_editor_property("modifiers", make_modifiers(imc, modifier_names))
            mappings.append(mapping)
        data = imc.get_editor_property("default_key_mappings")
        data.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", data)
        created_any.append(imc)

    # --- driving input config
    config, created = get_or_create("DA_DrivingInput", INPUT_DIR, unreal.DrivingInputConfig,
                                    unreal.DataAssetFactory())
    if created:
        config.set_editor_property("mapping_context", imc)
        config.set_editor_property("throttle", actions["IA_Throttle"])
        config.set_editor_property("brake", actions["IA_Brake"])
        config.set_editor_property("steer", actions["IA_Steer"])
        config.set_editor_property("drift", actions["IA_Drift"])
        config.set_editor_property("boost", actions["IA_Boost"])
        config.set_editor_property("use_item", actions["IA_UseItem"])
        created_any.append(config)

    # --- default vehicle tuning (class defaults; edit the asset to tune)
    tuning, created = get_or_create("DA_VehicleTuning_Default", VEHICLE_DATA_DIR,
                                    unreal.VehicleTuningData, unreal.DataAssetFactory())
    if created:
        created_any.append(tuning)

    for asset in created_any:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)

    unreal.log("Phase1Assets: done, %d asset(s) created." % len(created_any))


main()
