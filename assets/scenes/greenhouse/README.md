# Greenhouse Scene

The final showcase scene: a glass conservatory assembled from
procedural architecture and imported botanical assets.

Render it with:

```powershell
.\scripts\render.ps1 -Config Release -Scene Greenhouse
```

Add `-Draft` or `-Preview` for fast look-development renders.
Default output:

```text
results/renders/greenhouse_scene.ppm
results/renders/greenhouse_scene.jpg
```

## Scene Contents

### Architecture (procedural)

- Stone plinth walls and a paved interior floor with Perlin noise
  shading, plus a stone slab walkway down the centre.
- Iron structural frame: side columns, transverse ties, ridge beam,
  roof rafters, wall mullions and glazing rails.
- Thin-glass glazing: individual side-wall panes, roof slopes divided
  by slim pressure caps and transverse glazing bars, a rear glazed
  wall, and a front entrance with a central doorway.
- Copper ridge cap and an exterior approach path with gravel.

### Planting

- Imported botanical assets (see `assets/ASSET_CREDITS.md`):
  anthurium on the side exhibition plinth, six ferns in tapered pots
  forming staggered bands along the walkway, a potted monstera in
  the foreground, a pachira as the rear focal tree, and calathea
  orbifolias as broad-leaf accents.
- Naturalistic rear planting bed with a wandering stone curb,
  layered irregularly with shrubs, low ferns and calatheas;
  weathered stones line the edge.
- Four hanging baskets suspended from the transverse ties with
  trailing procedural vines.
- Wooden specimen display bench along the front-left wall holding
  potted fern and calathea specimens with tilted species-label
  plaques facing the walkway.

### Lighting

- Kloofendal 48d partly cloudy environment map.
- A soft downward fill panel under the ridge, sized to stay inside
  the roof envelope.

## Layout Validation

Interior objects are checked against `greenhouse_plant_bounds()`
(x in [-3.55, 3.55], z in [-7.65, 3.65]) and the roof clearance
line `5.98 - 0.485 * |x|`, so plants cannot intersect the glazing
or poke through the roof.

## Asset Credits

Every external asset is recorded with source, author and license
in `assets/ASSET_CREDITS.md`.
