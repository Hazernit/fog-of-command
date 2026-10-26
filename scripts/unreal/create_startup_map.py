"""Create only Content/Maps/Startup.umap using built-in Engine assets.

Run in a separate Unreal Python commandlet, never an interactive Editor with
unsaved work. Existing Startup maps are refused unless --overwrite is explicit.
No project/plugin/engine settings or additional assets are saved.
"""

import argparse
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_startup_map import (  # noqa: E402
    GAME_MODE, MAP_PACKAGE, MESHES, PLAYER_LOCATION, PLAYER_YAW,
    require, require_project, validate_current_map, validate_project_settings,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--overwrite", action="store_true", help="Explicitly rebuild the existing nonpartitioned Startup level in place")
    arguments = parser.parse_args()
    require_project()
    game_mode, _ = validate_project_settings()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    map_file = Path(unreal.Paths.project_content_dir()) / "Maps" / "Startup.umap"
    exists = map_file.exists() or assets.does_asset_exist(MAP_PACKAGE)
    require(not exists or arguments.overwrite, "Startup already exists; use --overwrite only when intentionally rebuilding this map")
    if exists:
        require(levels.load_level(MAP_PACKAGE), "Cannot load the existing Startup level")
    else:
        require(levels.new_level(MAP_PACKAGE, False), "Cannot create a new nonpartitioned Startup level")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world is not None, "Editor world missing")
    world_settings = world.get_world_settings()
    require(world_settings.get_editor_property("world_partition") is None, "Refusing to rebuild a partitioned level")
    require(not levels.get_current_level().get_editor_property("use_external_actors"), "Refusing external actor packages")
    # This isolated commandlet edits only the explicitly selected Startup level.
    old_actors = [actor for actor in actors_api.get_all_level_actors() if not isinstance(actor, unreal.WorldSettings)]
    if old_actors:
        require(actors_api.destroy_actors(old_actors), "Cannot clear existing Startup actors")
    world_settings.set_editor_property("default_game_mode", game_mode)
    world_settings.set_editor_property("force_no_precomputed_lighting", True)

    def spawn(actor_class, label, location, rotation=None):
        if rotation is None:
            rotation = unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0)
        actor = actors_api.spawn_actor_from_class(actor_class, unreal.Vector(*location), rotation)
        require(actor is not None, "Failed to spawn " + label)
        actor.set_actor_label(label)
        return actor

    for label, shape, location, scale in MESHES:
        mesh = unreal.load_asset("/Engine/BasicShapes/" + shape + "." + shape)
        require(mesh is not None, "Installed Engine basic shape is missing: " + shape)
        actor = spawn(unreal.StaticMeshActor, label, location)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        require(component.set_static_mesh(mesh), "Failed to assign " + shape)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("BlockAll")
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        actor.set_actor_scale3d(unreal.Vector(*scale))

    spawn(unreal.SkyAtmosphere, "FOC_SkyAtmosphere", (0.0, 0.0, 0.0))
    sun = spawn(unreal.DirectionalLight, "FOC_Sun", (0.0, 0.0, 500.0), unreal.Rotator(pitch=-35.0, yaw=-35.0, roll=0.0))
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_component.set_intensity(10000.0)
    sun_component.set_atmosphere_sun_light(True)
    sky = spawn(unreal.SkyLight, "FOC_SkyLight", (0.0, 0.0, 300.0))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_intensity(1.0)
    sky_component.set_real_time_capture(True)
    fog = spawn(unreal.ExponentialHeightFog, "FOC_Fog", (0.0, 0.0, 0.0))
    fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.01)
    spawn(unreal.PlayerStart, "FOC_PlayerStart", PLAYER_LOCATION, unreal.Rotator(pitch=0.0, yaw=PLAYER_YAW, roll=0.0))

    result = validate_current_map()
    require(levels.save_current_level(), "Failed to save Startup.umap")
    require(map_file.is_file(), "Startup.umap was not written")
    unreal.log("FOC002_STARTUP_MAP_CREATED " + repr(result) + " game_mode=" + GAME_MODE)


if __name__ == "__main__":
    main()
