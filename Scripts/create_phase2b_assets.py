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

The project settings point at DA_SystemInput: Config/DefaultGame.ini (CosmicHotshotsSettings, SystemInput).

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

    for asset in changed:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log("Phase2bAssets: done, %d asset(s) created or updated." % len(changed))


main()
