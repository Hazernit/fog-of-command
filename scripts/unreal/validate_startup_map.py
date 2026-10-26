"""Validate the saved FOC-002 map in an isolated Unreal Python commandlet.

No assets or configuration are saved. Failure raises an exception so the Python
commandlet returns a nonzero exit code. This is asset validation, not a PIE test.
"""

import math
from pathlib import Path

import unreal


MAP_PACKAGE = "/Game/Maps/Startup"
MAP_OBJECT = MAP_PACKAGE + ".Startup"
GAME_MODE = "/Script/FogOfCommand.FogOfCommandGameMode"
MESHES = (
    ("FOC_Floor", "Cube", (0.0, 0.0, -50.0), (80.0, 80.0, 1.0)),
    ("FOC_Cube", "Cube", (0.0, 0.0, 100.0), (2.0, 2.0, 2.0)),
    ("FOC_Cylinder", "Cylinder", (400.0, 200.0, 100.0), (2.0, 2.0, 2.0)),
    ("FOC_Sphere", "Sphere", (-350.0, 200.0, 100.0), (2.0, 2.0, 2.0)),
)
PLAYER_LOCATION = (-650.0, -900.0, 180.0)
PLAYER_YAW = 50.0


def require(condition, message):
    if not condition:
        raise RuntimeError("FOC002_STARTUP_MAP_VALIDATION_FAIL: " + message)


def vector_values(vector):
    return (vector.x, vector.y, vector.z)


def require_vector(actual, expected, description, tolerance=0.01):
    values = vector_values(actual)
    require(
        all(math.isfinite(v) and abs(v - e) <= tolerance for v, e in zip(values, expected)),
        description + ": expected " + repr(expected) + ", found " + repr(values),
    )


def require_project():
    require(unreal.SystemLibrary.get_game_name() == "FogOfCommand", "Run only with FogOfCommand.uproject")


def validate_project_settings():
    require_project()
    game_mode = unreal.load_class(None, GAME_MODE)
    require(game_mode is not None, "Build the FogOfCommand C++ module first: GameMode class missing")
    settings = unreal.get_default_object(unreal.GameMapsSettings)
    for property_name in ("editor_startup_map", "game_default_map"):
        path = settings.get_editor_property(property_name).export_text()
        require(path == MAP_OBJECT, property_name + " must resolve to " + MAP_OBJECT)
    global_mode = settings.get_editor_property("global_default_game_mode").export_text()
    require(global_mode == GAME_MODE, "GlobalDefaultGameMode must be the native FogOfCommand GameMode")
    mode_defaults = unreal.get_default_object(game_mode)
    pawn_class = mode_defaults.get_editor_property("default_pawn_class")
    controller_class = mode_defaults.get_editor_property("player_controller_class")
    require(pawn_class is not None and controller_class is not None, "GameMode pawn/controller classes must exist")
    require(pawn_class.get_path_name() == "/Script/Engine.DefaultPawn", "This minimal map expects the standard DefaultPawn")
    pawn_defaults = unreal.get_default_object(pawn_class)
    collision = pawn_defaults.get_editor_property("collision_component")
    require(collision is not None, "DefaultPawn collision sphere missing")
    radius = collision.get_unscaled_sphere_radius()
    require(radius > 0.0, "DefaultPawn collision radius must be positive")
    return game_mode, radius


def validate_current_map():
    game_mode, pawn_radius = validate_project_settings()
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    require(world is not None and world.get_path_name() == MAP_OBJECT, "The current world must be the saved Startup map")
    world_settings = world.get_world_settings()
    require(world_settings.get_editor_property("world_partition") is None, "World Partition must be disabled")
    require(not levels.get_current_level().get_editor_property("use_external_actors"), "External actor packages are out of scope")
    require(world_settings.get_editor_property("default_game_mode") == game_mode, "Map GameMode override is incorrect")
    require(world_settings.get_editor_property("force_no_precomputed_lighting"), "The map must use fully dynamic lighting")

    actors = list(actors_api.get_all_level_actors())
    labels = {}
    for actor in actors:
        labels.setdefault(actor.get_actor_label(), []).append(actor)

    def actor_with_label(label, actor_class):
        matches = labels.get(label, [])
        require(len(matches) == 1, "Expected exactly one actor labelled " + label)
        require(isinstance(matches[0], actor_class), label + " has the wrong actor class")
        return matches[0]

    mesh_actors = [actor for actor in actors if isinstance(actor, unreal.StaticMeshActor)]
    require(len(mesh_actors) == len(MESHES), "Expected floor and exactly three display primitives")
    for label, shape, location, scale in MESHES:
        actor = actor_with_label(label, unreal.StaticMeshActor)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        require(component is not None, label + " StaticMeshComponent missing")
        mesh = component.get_editor_property("static_mesh")
        require(mesh is not None and mesh.get_path_name() == "/Engine/BasicShapes/" + shape + "." + shape, label + " must reference the Engine basic shape")
        require_vector(actor.get_actor_location(), location, label + " location")
        require_vector(actor.get_actor_scale3d(), scale, label + " scale")
        require(component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_AND_PHYSICS, label + " collision must support queries and physics")
        require(str(component.get_collision_profile_name()) == "BlockAll", label + " collision profile must be BlockAll")

    floor = actor_with_label("FOC_Floor", unreal.StaticMeshActor)
    floor_component = floor.get_component_by_class(unreal.StaticMeshComponent)
    require(floor_component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_BLOCK, "Floor must block the Pawn collision channel")
    require(floor_component.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY) == unreal.CollisionResponseType.ECR_BLOCK, "Floor must block Visibility queries")
    floor_origin, floor_extent = floor.get_actor_bounds(True)
    require_vector(floor_extent, (4000.0, 4000.0, 50.0), "Floor collision bounds", tolerance=1.0)
    floor_top = floor_origin.z + floor_extent.z
    require(abs(floor_top) <= 1.0, "Floor surface must be Z=0")

    sun = actor_with_label("FOC_Sun", unreal.DirectionalLight)
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sky = actor_with_label("FOC_SkyLight", unreal.SkyLight)
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    for component in (sun_component, sky_component):
        require(component is not None, "Light component missing")
        require(component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE, "Lights must be Movable")
        require(component.get_editor_property("intensity") > 0.0, "Light intensity must be positive")
    require(sun_component.get_editor_property("atmosphere_sun_light"), "Sun must illuminate SkyAtmosphere")
    require(sky_component.get_editor_property("real_time_capture"), "SkyLight must capture the dynamic sky")
    actor_with_label("FOC_SkyAtmosphere", unreal.SkyAtmosphere)
    actor_with_label("FOC_Fog", unreal.ExponentialHeightFog)

    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    require(len(starts) == 1, "Expected exactly one PlayerStart")
    player = actor_with_label("FOC_PlayerStart", unreal.PlayerStart)
    location = player.get_actor_location()
    require_vector(location, PLAYER_LOCATION, "PlayerStart location")
    rotation = player.get_actor_rotation()
    require(abs(rotation.yaw - PLAYER_YAW) <= 0.01 and abs(rotation.roll) <= 0.01 and abs(rotation.pitch) <= 0.01, "PlayerStart must face yaw=50, pitch=roll=0")
    require(location.z - pawn_radius > floor_top, "Pawn would begin intersecting the floor")
    require(abs(location.x - floor_origin.x) + pawn_radius < floor_extent.x and abs(location.y - floor_origin.y) + pawn_radius < floor_extent.y, "Pawn spawn must be inside floor edges")
    for label, _, _, _ in MESHES[1:]:
        primitive = labels[label][0]
        origin, extent = primitive.get_actor_bounds(True)
        require(any(abs(value - center) > half + pawn_radius for value, center, half in zip(vector_values(location), vector_values(origin), vector_values(extent))), "Pawn spawn overlaps " + label)
    forward = player.get_actor_forward_vector()
    toward_center = (-location.x, -location.y)
    facing = (forward.x * toward_center[0] + forward.y * toward_center[1]) / math.hypot(*toward_center)
    require(facing > 0.9, "PlayerStart must face the central primitives")
    return {"map": MAP_OBJECT, "mesh_actors": len(mesh_actors), "player_starts": len(starts), "default_pawn_radius": pawn_radius}


def main():
    require_project()
    map_file = Path(unreal.Paths.project_content_dir()) / "Maps" / "Startup.umap"
    require(map_file.is_file(), "Saved Content/Maps/Startup.umap is missing; fetch Git LFS assets first")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    require(levels.load_level(MAP_PACKAGE), "Failed to load the saved Startup map")
    result = validate_current_map()
    unreal.log("FOC002_STARTUP_MAP_VALIDATION_PASS " + repr(result))


if __name__ == "__main__":
    main()
