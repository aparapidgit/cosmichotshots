"""
create_phase2b_assets.py  -  Creates the Phase 2b data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces what exists:
  /Game/Input/Actions/IA_Pause      input action (bool, also triggers while the game is paused)
  /Game/Input/Actions/IA_ToggleHud  input action (bool): show / hide the race HUD
  /Game/Input/IMC_System            mapping context of the system actions:
                                    Pause = Start, Escape, P;  ToggleHud = D-pad Up, H
  /Game/Input/DA_SystemInput        USystemInputConfig pointing at the above
An IMC_System / DA_SystemInput made by an older version of this script only gets what is missing: the
bindings of actions that are new, and references that are still empty. Nothing in them is changed or removed.
The driving assets (IMC_Driving, DA_DrivingInput) are not touched.

On foot in the Adventure hub:
  /Game/Input/Actions/IA_Move, IA_Look, IA_LookMouse (Axis2D), IA_Run, IA_Interact (bool)
  /Game/Input/IMC_OnFoot            Move = left stick / WASD, Look = right stick, LookMouse = mouse,
                                    Run = left stick click / Left Shift, Interact = A / E
  /Game/Input/DA_OnFootInput        UOnFootInputConfig pointing at the above
  /Game/Data/Adventure/DA_AdventureHubTuning   UAdventureHubTuningData with the class defaults

Adventure Mode (placeholder campaign: every level on the test track, difficulty rising):
  /Game/Data/Adventure/DA_AdventureLevel_S<section>_L<level>   3 sections x 4 levels (UAdventureLevelDefinition)
  /Game/Data/Adventure/DA_AdventureCampaign                    player character Spark, the 3 sections in order

The project settings point at these: Config/DefaultGame.ini (CosmicHotshotsSettings: SystemInput,
OnFootInput, AdventureCampaign, AdventureHubTuning).

HOW TO RUN (the editor must be closed)
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"

# action name -> triggers while the game is paused
SYSTEM_ACTIONS = {
    "IA_Pause": True,
    "IA_ToggleHud": False,
}

# (action, key name) of IMC_System
SYSTEM_BINDINGS = [
    ("IA_Pause", "Gamepad_Special_Right"),
    ("IA_Pause", "Escape"),
    ("IA_Pause", "P"),
    ("IA_ToggleHud", "Gamepad_DPad_Up"),
    ("IA_ToggleHud", "H"),
]

# USystemInputConfig property -> action name
CONFIG_ACTIONS = {
    "pause": "IA_Pause",
    "toggle_hud": "IA_ToggleHud",
}

# On-foot actions: name -> value type
ON_FOOT_ACTIONS = {
    "IA_Move": unreal.InputActionValueType.AXIS2D,
    "IA_Look": unreal.InputActionValueType.AXIS2D,
    "IA_LookMouse": unreal.InputActionValueType.AXIS2D,
    "IA_Run": unreal.InputActionValueType.BOOLEAN,
    "IA_Interact": unreal.InputActionValueType.BOOLEAN,
}

# (action, key name, modifiers) of IMC_OnFoot.  Move and Look are Axis2D: X = right, Y = forward / up.
# "swizzle" turns a key's value into the Y axis, "negate" flips it, "deadzone" ignores a resting stick.
ON_FOOT_BINDINGS = [
    ("IA_Move", "Gamepad_Left2D", ["deadzone"]),
    ("IA_Move", "W", ["swizzle"]),
    ("IA_Move", "S", ["swizzle", "negate"]),
    ("IA_Move", "D", []),
    ("IA_Move", "A", ["negate"]),
    ("IA_Look", "Gamepad_Right2D", ["deadzone"]),
    ("IA_LookMouse", "Mouse2D", []),
    ("IA_Run", "Gamepad_LeftThumbstick", []),
    ("IA_Run", "LeftShift", []),
    ("IA_Interact", "Gamepad_FaceButton_Bottom", []),
    ("IA_Interact", "E", []),
]

# UOnFootInputConfig property -> action name
ON_FOOT_CONFIG_ACTIONS = {
    "move": "IA_Move",
    "look": "IA_Look",
    "look_mouse": "IA_LookMouse",
    "run": "IA_Run",
    "interact": "IA_Interact",
}

ADVENTURE_DIR = "/Game/Data/Adventure"
ADVENTURE_PLAYER = "/Game/Data/Characters/DA_Character_Spark"
ADVENTURE_TRACK = "/Game/Data/Tracks/DA_Track_TestTrack"

# One row per level, in campaign order: (rivals, AI difficulty index, laps). Placeholders to tune.
# Difficulty indices: 0 Easy, 1 Normal, 2 Hard.
ADVENTURE_SECTIONS = [
    ("Section 1", [(3, 0, 2), (3, 0, 2), (4, 0, 2), (4, 1, 2)]),
    ("Section 2", [(5, 1, 3), (5, 1, 3), (6, 1, 3), (6, 2, 3)]),
    ("Section 3", [(7, 2, 3), (7, 2, 3), (7, 2, 3), (7, 2, 3)]),
]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def get_or_create(name, folder, asset_class, factory):
    """Returns (asset, created). Never overwrites an existing asset."""
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        unreal.log("exists: %s" % path)
        return eal.load_asset(path), False
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create %s" % path)
    unreal.log("created: %s" % path)
    return asset, True


def make_mapping(action, key_name):
    key = unreal.Key()
    key.import_text(key_name)
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", key)
    return mapping


def make_modified_mapping(owner, action, key_name, modifier_names):
    mapping = make_mapping(action, key_name)
    modifiers = []
    for name in modifier_names:
        if name == "negate":
            modifiers.append(unreal.new_object(unreal.InputModifierNegate, owner))
        elif name == "deadzone":
            modifiers.append(unreal.new_object(unreal.InputModifierDeadZone, owner))
        elif name == "swizzle":
            # Default order YXZ: the key's value goes to the Y axis.
            modifiers.append(unreal.new_object(unreal.InputModifierSwizzleAxis, owner))
        else:
            raise ValueError("Unknown modifier %s" % name)
    mapping.set_editor_property("modifiers", modifiers)
    return mapping


def create_on_foot_input(changed):
    """Actions, mapping context and config for walking in the hub. Existing assets are left as they are."""
    actions = {}
    for name, value_type in ON_FOOT_ACTIONS.items():
        action, created = get_or_create(name, ACTIONS_DIR, unreal.InputAction, unreal.InputAction_Factory())
        if created:
            action.set_editor_property("value_type", value_type)
            changed.append(action)
        actions[name] = action

    imc, created = get_or_create("IMC_OnFoot", INPUT_DIR, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    if created:
        data = imc.get_editor_property("default_key_mappings")
        data.set_editor_property("mappings", [make_modified_mapping(imc, actions[name], key, modifiers)
                                              for name, key, modifiers in ON_FOOT_BINDINGS])
        imc.set_editor_property("default_key_mappings", data)
        changed.append(imc)

    config, created = get_or_create("DA_OnFootInput", INPUT_DIR, unreal.OnFootInputConfig, unreal.DataAssetFactory())
    if created:
        config.set_editor_property("mapping_context", imc)
        for property_name, action_name in ON_FOOT_CONFIG_ACTIONS.items():
            config.set_editor_property(property_name, actions[action_name])
        changed.append(config)

    tuning, created = get_or_create("DA_AdventureHubTuning", ADVENTURE_DIR, unreal.AdventureHubTuningData, unreal.DataAssetFactory())
    if created:
        changed.append(tuning)


def create_adventure_campaign(changed):
    """The placeholder campaign. Existing level and campaign assets are left exactly as they are."""
    track = eal.load_asset(ADVENTURE_TRACK)
    sections = []
    for section_index, (section_name, levels) in enumerate(ADVENTURE_SECTIONS):
        level_assets = []
        for level_index, (rivals, difficulty, laps) in enumerate(levels):
            level_id = "S%d_L%d" % (section_index + 1, level_index + 1)
            level, created = get_or_create("DA_AdventureLevel_%s" % level_id, ADVENTURE_DIR,
                                           unreal.AdventureLevelDefinition, unreal.DataAssetFactory())
            if created:
                level.set_editor_property("portal_id", level_id)
                level.set_editor_property("display_name", "Level %d-%d" % (section_index + 1, level_index + 1))
                level.set_editor_property("track", track)
                level.set_editor_property("num_laps", laps)
                level.set_editor_property("num_rivals", rivals)
                level.set_editor_property("ai_difficulty", difficulty)
                changed.append(level)
            level_assets.append(level)
        section = unreal.AdventureSection()
        section.set_editor_property("display_name", section_name)
        section.set_editor_property("levels", level_assets)
        sections.append(section)

    campaign, created = get_or_create("DA_AdventureCampaign", ADVENTURE_DIR,
                                      unreal.AdventureCampaignDefinition, unreal.DataAssetFactory())
    if created:
        campaign.set_editor_property("player_character", eal.load_asset(ADVENTURE_PLAYER))
        campaign.set_editor_property("sections", sections)
        changed.append(campaign)


def main():
    changed = []

    actions = {}
    new_actions = set()
    for name, when_paused in SYSTEM_ACTIONS.items():
        action, created = get_or_create(name, ACTIONS_DIR, unreal.InputAction, unreal.InputAction_Factory())
        if created:
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            action.set_editor_property("trigger_when_paused", when_paused)
            new_actions.add(name)
            changed.append(action)
        actions[name] = action

    # Mapping context: every binding when new, otherwise only the bindings of actions created just now.
    imc, imc_created = get_or_create("IMC_System", INPUT_DIR, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    added = 0
    for action_name, key_name in SYSTEM_BINDINGS:
        if imc_created or action_name in new_actions:
            mappings.append(make_mapping(actions[action_name], key_name))
            added += 1
    if added:
        data.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", data)
        changed.append(imc)
        if not imc_created:
            unreal.log("IMC_System: added %d binding(s) of new actions" % added)

    # Config: fill only references that are still empty.
    config, config_created = get_or_create("DA_SystemInput", INPUT_DIR, unreal.SystemInputConfig, unreal.DataAssetFactory())
    config_changed = config_created
    if config.get_editor_property("mapping_context") is None:
        config.set_editor_property("mapping_context", imc)
        config_changed = True
    for property_name, action_name in CONFIG_ACTIONS.items():
        if config.get_editor_property(property_name) is None:
            config.set_editor_property(property_name, actions[action_name])
            config_changed = True
            if not config_created:
                unreal.log("set DA_SystemInput.%s" % property_name)
    if config_changed:
        changed.append(config)

    create_adventure_campaign(changed)
    create_on_foot_input(changed)

    for asset in changed:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log("Phase2bAssets: done, %d asset(s) created or updated." % len(changed))


main()
