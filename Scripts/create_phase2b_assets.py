"""
create_phase2b_assets.py  -  Creates the Phase 2b data assets (Cosmic Hotshots, UE 5.7)

Creates what is missing and never replaces or changes what exists:
  /Game/Input/Actions/IA_Pause    input action (bool, also triggers while the game is paused)
  /Game/Input/IMC_System          mapping context of the system actions: Pause = Start, Escape, P
  /Game/Input/DA_SystemInput      USystemInputConfig pointing at the two above
The driving assets (IMC_Driving, DA_DrivingInput) are not touched.

The project settings point at DA_SystemInput: Config/DefaultGame.ini (CosmicHotshotsSettings, SystemInput).

HOW TO RUN (the editor must be closed)
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
"""
import unreal

INPUT_DIR = "/Game/Input"
ACTIONS_DIR = "/Game/Input/Actions"

# (action, key name) of IMC_System
SYSTEM_BINDINGS = [
    ("IA_Pause", "Gamepad_Special_Right"),
    ("IA_Pause", "Escape"),
    ("IA_Pause", "P"),
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


def main():
    changed = []

    pause, created = get_or_create("IA_Pause", ACTIONS_DIR, unreal.InputAction, unreal.InputAction_Factory())
    if created:
        pause.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        pause.set_editor_property("trigger_when_paused", True)
        changed.append(pause)
    actions = {"IA_Pause": pause}

    imc, created = get_or_create("IMC_System", INPUT_DIR, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    if created:
        data = imc.get_editor_property("default_key_mappings")
        data.set_editor_property("mappings", [make_mapping(actions[name], key) for name, key in SYSTEM_BINDINGS])
        imc.set_editor_property("default_key_mappings", data)
        changed.append(imc)

    config, created = get_or_create("DA_SystemInput", INPUT_DIR, unreal.SystemInputConfig, unreal.DataAssetFactory())
    if created:
        config.set_editor_property("mapping_context", imc)
        config.set_editor_property("pause", pause)
        changed.append(config)

    for asset in changed:
        eal.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log("Phase2bAssets: done, %d asset(s) created." % len(changed))


main()
