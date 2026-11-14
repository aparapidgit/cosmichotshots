"""
create_phase1_assets.py  -  Creates the Phase 1 data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Input/Actions/IA_Throttle, IA_Brake, IA_Steer, IA_Drift, IA_Boost, IA_UseItem, IA_LookBack
  /Game/Input/IMC_Driving              default gamepad + keyboard bindings
  /Game/Input/DA_DrivingInput          UDrivingInputConfig pointing at the assets above
  /Game/Data/Vehicles/DA_VehicleTuning_Default   UVehicleTuningData with the class defaults
  /Game/Data/Camera/DA_CameraTuning_Default      UCameraTuningData with the class defaults
  /Game/VFX/M_SpeedLines               post-process material for speed lines (parameter "Intensity")

Running it again after new actions were added to this script only ADDS the missing
actions, the bindings of those new actions, and empty references in DA_DrivingInput.
Bindings you changed in the editor are left alone.

The project settings (Config/DefaultGame.ini, section CosmicHotshotsSettings) point at
DA_DrivingInput, DA_VehicleTuning_Default and DA_CameraTuning_Default.

HOW TO RUN
  In the editor:   File > Execute Python Script...  and pick this file.
  Without editor:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"
VEHICLE_DATA_DIR = "/Game/Data/Vehicles"
CAMERA_DATA_DIR = "/Game/Data/Camera"
VFX_DIR = "/Game/VFX"

# action name -> value type
ACTIONS = {
    "IA_Throttle": unreal.InputActionValueType.AXIS1D,
    "IA_Brake": unreal.InputActionValueType.AXIS1D,
    "IA_Steer": unreal.InputActionValueType.AXIS1D,
    "IA_Drift": unreal.InputActionValueType.BOOLEAN,
    "IA_Boost": unreal.InputActionValueType.BOOLEAN,
    "IA_UseItem": unreal.InputActionValueType.BOOLEAN,
    "IA_LookBack": unreal.InputActionValueType.BOOLEAN,
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
    ("IA_LookBack", "Gamepad_FaceButton_Top", []),
    ("IA_LookBack", "C", []),
]

# UDrivingInputConfig property -> action name
CONFIG_ACTIONS = {
    "throttle": "IA_Throttle",
    "brake": "IA_Brake",
    "steer": "IA_Steer",
    "drift": "IA_Drift",
    "boost": "IA_Boost",
    "use_item": "IA_UseItem",
    "look_back": "IA_LookBack",
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create(name, folder, asset_class, factory):
    """Returns (asset, created). Never overwrites an existing asset."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
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


def make_mapping(imc, action, key_name, modifier_names):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", make_key(key_name))
    mapping.set_editor_property("modifiers", make_modifiers(imc, modifier_names))
    return mapping


SPEED_LINES_HLSL = """
// Radial speed lines drawn over the finished image. One cheap full-screen pass.
float2 c = UV - 0.5;
float r = length(c) * 2.0;                     // 0 at the centre, 1 at the screen edge (vertically)
float angle = atan2(c.y, c.x) / 6.2831853 + 0.5; // 0..1 around the screen

float seg = angle * Density;
float id = floor(seg);
float f = frac(seg);

// Each line gets its own random phase; it re-rolls several times per second so lines flicker.
float phase = frac(sin(id * 127.1) * 43758.5453);
float tick = floor(Time * FlickerRate + phase * 10.0);
float rnd = frac(sin(id * 311.7 + tick * 17.3) * 43758.5453);
float visible = step(1.0 - Coverage, rnd);

// Thin streak in the middle of its segment, thicker towards the edge of the screen.
float thin = 1.0 - smoothstep(0.0, Width * saturate(r), abs(f - 0.5));

// Only towards the edges, so the car and the road ahead stay clear.
float edge = smoothstep(InnerRadius, 1.0, r);

float alpha = saturate(thin * visible * edge * Intensity * LineColor.a);
return lerp(Scene.rgb, LineColor.rgb, alpha);
"""


def create_speed_lines_material(folder, name):
    """Post-process material with a scalar parameter "Intensity" (0 = invisible)."""
    material, created = get_or_create(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not created:
        return material, False

    mel = unreal.MaterialEditingLibrary
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
    custom.set_editor_property("description", "SpeedLines")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("code", SPEED_LINES_HLSL)

    input_names = ["UV", "Scene", "Time", "Intensity", "Density", "Width", "Coverage",
                   "InnerRadius", "FlickerRate", "LineColor"]
    inputs = []
    for input_name in input_names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", input_name)
        inputs.append(custom_input)
    custom.set_editor_property("inputs", inputs)

    screen = mel.create_material_expression(material, unreal.MaterialExpressionScreenPosition, -700, -300)
    mel.connect_material_expressions(screen, "ViewportUV", custom, "UV")

    scene = mel.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -700, -150)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(scene, "Color", custom, "Scene")

    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -700, 0)
    mel.connect_material_expressions(time, "", custom, "Time")

    # name -> default value. Intensity is driven by the race camera at runtime.
    scalars = [("Intensity", 0.0), ("Density", 90.0), ("Width", 0.22), ("Coverage", 0.35),
               ("InnerRadius", 0.45), ("FlickerRate", 14.0)]
    y = 100
    for scalar_name, default in scalars:
        parameter = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, y)
        parameter.set_editor_property("parameter_name", scalar_name)
        parameter.set_editor_property("default_value", default)
        mel.connect_material_expressions(parameter, "", custom, scalar_name)
        y += 90

    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, y)
    color.set_editor_property("parameter_name", "LineColor")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 0.6))
    mel.connect_material_expressions(color, "RGBA", custom, "LineColor")

    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    return material, True


def main():
    changed = []

    # --- input actions
    actions = {}
    new_actions = set()
    for name, value_type in ACTIONS.items():
        action, created = get_or_create(name, ACTIONS_DIR, unreal.InputAction, unreal.InputAction_Factory())
        if created:
            action.set_editor_property("value_type", value_type)
            new_actions.add(name)
            changed.append(action)
        actions[name] = action

    # --- mapping context: all bindings when new, otherwise only the bindings of new actions
    imc, imc_created = get_or_create("IMC_Driving", INPUT_DIR, unreal.InputMappingContext,
                                     unreal.InputMappingContext_Factory())
    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    added = 0
    for action_name, key_name, modifier_names in BINDINGS:
        if imc_created or action_name in new_actions:
            mappings.append(make_mapping(imc, actions[action_name], key_name, modifier_names))
            added += 1
    if added:
        data.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", data)
        changed.append(imc)
        if not imc_created:
            unreal.log("Phase1Assets: added %d binding(s) to existing IMC_Driving" % added)

    # --- driving input config: fill only references that are still empty
    config, config_created = get_or_create("DA_DrivingInput", INPUT_DIR, unreal.DrivingInputConfig,
                                           unreal.DataAssetFactory())
    config_changed = config_created
    if config.get_editor_property("mapping_context") is None:
        config.set_editor_property("mapping_context", imc)
        config_changed = True
    for property_name, action_name in CONFIG_ACTIONS.items():
        if config.get_editor_property(property_name) is None:
            config.set_editor_property(property_name, actions[action_name])
            config_changed = True
            if not config_created:
                unreal.log("Phase1Assets: set DA_DrivingInput.%s" % property_name)
    if config_changed:
        changed.append(config)

    # --- default tuning assets (class defaults; edit the assets to tune)
    tuning, created = get_or_create("DA_VehicleTuning_Default", VEHICLE_DATA_DIR,
                                    unreal.VehicleTuningData, unreal.DataAssetFactory())
    if created:
        changed.append(tuning)

    camera_tuning, created = get_or_create("DA_CameraTuning_Default", CAMERA_DATA_DIR,
                                           unreal.CameraTuningData, unreal.DataAssetFactory())
    if created:
        changed.append(camera_tuning)

    # --- speed lines post-process material, referenced by the camera tuning
    speed_lines, created = create_speed_lines_material(VFX_DIR, "M_SpeedLines")
    if created:
        changed.append(speed_lines)
    if camera_tuning.get_editor_property("speed_lines_material") is None:
        camera_tuning.set_editor_property("speed_lines_material", speed_lines)
        unreal.log("Phase1Assets: set DA_CameraTuning_Default.speed_lines_material")
        if camera_tuning not in changed:
            changed.append(camera_tuning)

    for asset in changed:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)

    unreal.log("Phase1Assets: done, %d asset(s) created or updated." % len(changed))


main()
