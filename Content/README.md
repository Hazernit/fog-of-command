# Minimal project content

`Maps/Startup.umap` is the small FOC-002 startup scene, stored with Git LFS.
Its ground and three props reference the installed Unreal Engine Basic Shapes
(Cube, Cylinder, Sphere) and their engine material. Lighting, atmosphere and
PlayerStart are built-in Unreal actors. No Starter Content, marketplace content,
external textures, downloaded models or paid assets are imported.

The map is created with `scripts/unreal/create_startup_map.py` using the pinned
engine. The committed map is used directly after clone; regeneration is optional.
`scripts/unreal/validate_startup_map.py` validates the actual saved scene.
