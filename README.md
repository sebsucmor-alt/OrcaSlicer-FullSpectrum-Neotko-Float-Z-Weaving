
<h1> <p "font-size:200px;"> Full Spectrum</p> </h1>

### A Snapmaker Orca Fork with Mixed-Color Filament Support

[![Build all](https://github.com/Snapmaker/OrcaSlicer/actions/workflows/build_all.yml/badge.svg?branch=main)](https://github.com/Snapmaker/OrcaSlicer/actions/workflows/build_all.yml)

---

## ☕ Support Development

If you find this fun or interesting! Give RatDoux a Coffee for his work

<a href="https://www.buymeacoffee.com/ratdoux" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

---

## ⚠️ **IMPORTANT DISCLAIMER** ⚠️

THIS IS A 0.93 Quite and Dirty fork, with my Beta Neotko Weaving Idea and Specially a Link system to allow free Z objects without needing to make 'parts'. Also allows to use different layer height for the prime tower.

All info documentation and changes listed:

# OrcaSlicer FullSpectrum — Complete Patch Reference

> **Recovery reference.** This document exhaustively describes every modification made to the OrcaSlicer source code. It is intended to allow resuming work from scratch if conversation context is lost.

---

## Index

1. [Modified files map](#1-modified-files-map)
2. [Pre-existing patches in the codebase](#2-pre-existing-patches-in-the-codebase)
3. [Feature: disable_bridge_infill](#3-feature-disable_bridge_infill)
4. [Feature: top_surface_density and bottom_surface_density UI](#4-feature-top_surface_density-and-bottom_surface_density-ui)
5. [Feature: Neotko Interlayer Sanding (surface)](#5-feature-neotko-interlayer-sanding-surface)
6. [Feature: Neotko Infill Interlayer Sanding (infill)](#6-feature-neotko-infill-interlayer-sanding-infill)
7. [Feature: STL file origin preservation](#7-feature-stl-file-origin-preservation)
8. [Feature: Temporal Link](#8-feature-temporal-link)
9. [Bug fixed: startup crash due to missing icon](#9-bug-fixed-startup-crash-due-to-missing-icon)
10. [Bug fixed: floating objects snapped to Z=0 on move](#10-bug-fixed-floating-objects-snapped-to-z0-on-move)

---

## 1. Modified files map

| File | Path | Features |
|---|---|---|
| PrintConfig.hpp | src/libslic3r/ | 3, 4, 5, 6 |
| PrintConfig.cpp | src/libslic3r/ | 3, 4, 5, 6 |
| PrintObject.cpp | src/libslic3r/ | 3 |
| GCode.cpp | src/libslic3r/ | 3, 5, 6 |
| FillBase.cpp | src/libslic3r/Fill/ | 4 (UI only, no fill logic changes) |
| Preset.cpp | src/libslic3r/ | 3, 5, 6 |
| Tab.cpp | src/slic3r/GUI/ | 3, 4, 5, 6, 9 |
| Print.cpp | src/libslic3r/ | pre-existing |
| Plater.cpp | src/slic3r/GUI/ | 7, 8 |
| 3mf.cpp | src/libslic3r/Format/ | 8 |
| Model.hpp | src/libslic3r/ | 8 |
| Model.cpp | src/libslic3r/ | 8 |
| GLCanvas3D.cpp | src/slic3r/GUI/ | 10 |

---

## 2. Pre-existing patches in the codebase

These changes already existed in the fork **before** the work sessions began. Documented here to avoid confusion with new changes.

### Model.cpp — ensure_on_bed() disabled

```cpp
void ModelObject::ensure_on_bed(bool allow_negative_z)
{
    return; // Disabled: do not auto-move objects to Z0
    // ... rest of code untouched
```

Critical: prevents OrcaSlicer from repositioning objects to Z=0 automatically when loading or moving them.

### PrintObject.cpp — Internal bridge reclassified as sparse infill

Internal bridges (stInternalBridge) are treated as normal infill (stInternal). The second-pass internal bridge layer logic was removed.

### Print.cpp — Prime tower height validation commented out + shortest_object_idx

The validation that prevented mixed layer height configurations with prime tower was commented out. Tracking of shortest_object_idx was added.

### GCode.cpp — Initial layer error commented out

```cpp
// COMMENT if (layers_to_print.size() == 1u) {
// COMMENT     if (!hasextrusions)
// COMMENT         throw Slic3r::SlicingError((L("One object has empty initial layer...")), object.id().id);
// COMMENT }
```

Result: floating objects (not touching the bed) slice correctly, emitting only a Warning without blocking. Works together with ensure_on_bed() disabled and Bug 10 fix.

---

## 3. Feature: disable_bridge_infill

### What it does
Adds a per-region option that allows external bridges, internal bridges, or both, to be printed using top surface infill settings instead of dedicated bridge parameters. The intervention is structural: it happens at the first step of the slicing pipeline.

### UI location
Quality > Bridging > **Disable bridge infill**

| Value | Behavior |
|---|---|
| Disabled | Normal bridges (default) |
| External bridges only | External bridges use top surface settings |
| Internal bridges only | Internal bridges use top surface settings |
| All bridges | All bridges use top surface settings |

### How it works

In detect_surfaces_type(), stBottomBridge surfaces are reclassified as stTop when the corresponding bridge type is disabled. The entire downstream pipeline (fill pattern, speed, flow, fan) natively sees stTop and behaves accordingly.

```
detect_surfaces_type()
    stBottomBridge -> stTop        (when external bridge disabled)
         |
process_external_surfaces()       -- sees stTop, treats as top surface
         |
FillBase                          -- uses top_surface_pattern automatically
         |
GCode.cpp                         -- emits erTopSolidInfill natively
```

### Modified files

**PrintConfig.hpp**
- Enum `DisableBridgeInfill { dbiDisabled, dbiExternalOnly, dbiInternalOnly, dbiAll }`
- Key `disable_bridge_infill` (`ConfigOptionEnum<DisableBridgeInfill>`) in `PrintRegionConfig`

**PrintConfig.cpp**
- String map `s_keys_map_DisableBridgeInfill`
- Full definition with label, tooltip and values

**Tab.cpp**
- UI line in Bridging group

**Preset.cpp**
- `"disable_bridge_infill"` in the preset key list

**PrintObject.cpp — detect_surfaces_type() (~line 1336)**
```cpp
const bool ext_bridge_disabled =
    this->printing_region(region_id).config().disable_bridge_infill.value == dbiExternalOnly ||
    this->printing_region(region_id).config().disable_bridge_infill.value == dbiAll;
SurfaceType surface_type_bottom_other = bottom_is_fully_supported ? stBottom :
                                        ext_bridge_disabled        ? stTop    :
                                                                     stBottomBridge;
```

The second external bridge layer pass is also skipped when bridges are disabled.

**GCode.cpp**
Only overrides for erInternalBridgeInfill (flow and speed) remain when dbiInternalOnly or dbiAll. External bridge overrides were removed — handled structurally.

---

## 4. Feature: top_surface_density and bottom_surface_density UI

### What it does
Exposes two options in the UI that **already existed** fully implemented in the fill engine and OrcaSlicer preset system, but never had a UI line.

- **top_surface_density** — top layer fill percentage (0-100%)
- **bottom_surface_density** — bottom layer fill percentage (0-100%)

No fill logic was modified. Only UI lines were added.

### UI location
Strength > Top/bottom shells

### Modified files
**Tab.cpp** only — two `add_option` lines in the Top/bottom shells group.

---

## 5. Feature: Neotko Interlayer Sanding (surface)

### What it does
Oscillates the nozzle in Z during erTopSolidInfill extrusion. Adjacent passes move up and down in opposition, physically interleaving and creating a mechanical bond between lines. Improves top layer cohesion and reduces delamination.

### UI location
Quality > Neotko Interlayer Sanding

| Parameter | Key | Default |
|---|---|---|
| Enable | interlayer_sanding_enabled | false |
| Z amplitude | interlayer_sanding_amplitude | 0.10 mm |
| Oscillation period | interlayer_sanding_period | 0 (auto = line width) |
| Max Z speed | interlayer_sanding_max_z_speed | 20 mm/s |

### How it works (GCode.cpp — _extrude())

Feature activates only for erTopSolidInfill when sloped == nullptr.

**Speed cap before the path:**
```
XY_speed_max = max_z_speed x period / (4 x amplitude)
```

**Line subdivision:** each segment is divided into at least 8 micro-segments per period. For each point, the Z offset of a triangular wave is computed:
```
phase = fmod(dist / period, 1.0) x 4
z_offset = amplitude x phase          if phase < 1
          = amplitude x (2 - phase)    if phase < 3
          = amplitude x (phase - 4)    otherwise
```

**Restoration at path end:**
1. travel_to_z(nominal_z) — returns nozzle to exact layer Z
2. Restores original speed if it was capped

### Modified files
PrintConfig.hpp, PrintConfig.cpp, Tab.cpp, Preset.cpp, GCode.cpp

---

## 6. Feature: Neotko Infill Interlayer Sanding (infill)

### What it does
Applies the same Z oscillation to **sparse infill** (erInternalInfill), improving mechanical bonding between infill layers throughout the full object height — not just the surface.

Only affects erInternalInfill. Perimeters, solid infill, bridges and top surface are never affected.

### UI location
Strength > Neotko Infill Interlayer Sanding

| Parameter | Key | Default |
|---|---|---|
| Enable | infill_interlayer_sanding_enabled | false |
| Z amplitude | infill_interlayer_sanding_amplitude | 0.10 mm |
| Oscillation period | infill_interlayer_sanding_period | 0 (auto = line width) |
| Max Z speed | infill_interlayer_sanding_max_z_speed | 20 mm/s |

### Modified files
PrintConfig.hpp, PrintConfig.cpp, Tab.cpp, Preset.cpp, GCode.cpp

---

## 7. Feature: STL file origin preservation

### What it does
When loading an STL/3MF object that has a significant XY position (>0.5 mm from origin), OrcaSlicer preserves that position instead of relocating the object to the bed center.

### How it works

**Plater.cpp — load_model_objects()**

The AUTOPLACEMENT_ON_LOAD block is active. Before centering the mesh, the original bounding box is captured:
```cpp
const BoundingBoxf3 orig_bbox = object->raw_mesh_bounding_box();
const Vec3d orig_center = orig_bbox.center();
object->center_around_origin();
ModelInstance* inst = object->add_instance();
inst->set_offset(Vec3d(orig_center.x(), orig_center.y(), -object->origin_translation(2)));
```

For new instances without a file position:
```cpp
const bool has_file_position = (std::abs(offset(0)) > 0.5 || std::abs(offset(1)) > 0.5);
if (has_file_position)
    continue; // keep XY from file, Z was already set correctly above
```

SUPPORT_AUTOCENTER is never defined — the center_instances_around_point block in update() is dead code and does not interfere.

---

## 8. Feature: Temporal Link

### What it does
Groups two or more objects so they move together without modifying the print structure (no "parts" created, no effect on slicing). Unlike Assemble, it operates entirely in the normal 3D view and has no effect on generated GCode.

Supports an **unlimited number of objects** in the same group. Multiple independent groups can coexist simultaneously.

| | Assemble | Temporal Link |
|---|---|---|
| Affects slicing | YES | NO |
| Own view | YES (AssembleView) | NO (normal 3D view) |
| Undone with | Split | Break Link |
| Persists in .3mf | YES | YES |

### Usage
1. Select 2 or more objects (Shift+click or area selection)
2. Right-click > Link Objects
3. Move any one > all move with the same XYZ delta
4. To break: select any group member > right-click > Break Link

### Delta architecture

```
DRAGGING_STARTED -> update_pre_move_snapshot()
     |  (user drags object A — or edits XYZ in sidebar)
INSTANCE_MOVED -> on_instance_moved_with_link_sync()
     |  delta = A.current_offset - A.pre_move_offset
     |  for each B in the same group (not selected):
     |      B.set_offset(B.offset + delta)
     +-> update_pre_move_snapshot()   <- new baseline
     +-> update()
```

### Bugs fixed during development

**Bug 1: Double-delta when moving multiple selected linked objects**

When A and B are linked and both selected, the gizmo already moves both correctly. The original handler propagated A's delta to B (double-move) and B's corrupted delta back to A.

Fix: build moved_obj_ids with all IDs in the selection. The propagation loop skips (continue) any object already in the selection — the gizmo moved it correctly. processed_groups ensures each group is only processed once per event.

**Bug 2: Prime Tower fires INSTANCE_MOVED and corrupts positions**

The Prime Tower is not a real ModelObject. Its GLVolume can return object_idx() values that alias real objects, producing an incoherent delta.

Fix:
```cpp
if (sel.is_wipe_tower()) {
    update_pre_move_snapshot();
    update();
    return;
}
```

### New fields in priv (Plater.cpp)

```cpp
std::map<size_t, int>   m_link_groups;           // object id().id -> group_id
int                     m_next_link_group_id{1};
std::map<size_t, Vec3d> m_pre_move_offsets;      // pre-move position snapshot
bool                    m_syncing_links{false};   // anti-recursion guard
std::map<size_t, Vec3d> m_floating_z_guard;      // see Bug 10
```

### New functions in Plater.cpp

- `update_pre_move_snapshot()` — XYZ snapshot of all objects; also populates m_floating_z_guard
- `rebuild_link_groups_from_model()` — rebuilds runtime map from link_group_id fields
- `on_instance_moved_with_link_sync()` — replaces the plain `{ update(); }` lambda for EVT_GLCANVAS_INSTANCE_MOVED
- `link_selected_objects()` — assigns shared group_id, takes snapshot
- `break_link_selected_objects()` — removes from group, takes snapshot

### Event hooks

- EVT_GLCANVAS_INSTANCE_MOVED -> on_instance_moved_with_link_sync()
- on_3dcanvas_mouse_dragging_started -> update_pre_move_snapshot()
- update_after_undo_redo -> rebuild_link_groups_from_model()
- load_model_objects (end) -> rebuild_link_groups_from_model()
- remove() -> erase from m_link_groups, m_pre_move_offsets, m_floating_z_guard

### Model.hpp — field declaration + serialization

```cpp
int link_group_id { 0 };   // TEMPORAL LINK: 0 = no link; >0 = group id
```

Also added to cereal save() and load() methods (undo/redo):
```cpp
ar(..., cut_connectors, cut_id, link_group_id); // TEMPORAL LINK
```

### Model.cpp — propagation in copy/move

```cpp
this->link_group_id = rhs.link_group_id;  // in assign_copy (const& and &&)
```

### 3mf.cpp — persistence

Export, import and valid_keys whitelist of link_group_id in model config metadata XML.

### Known limitations
- Scale and rotation are not propagated — only XYZ offset
- Objects on different plates can be linked; delta propagates independently of plate

---

## 9. Bug fixed: startup crash due to missing icon

### Symptom
Compiled without errors but crashed on startup:
```
[trace] Initializing StaticPrintConfigs
Unhandled unknown exception; terminating the application.
zsh: segmentation fault  ./Snapmaker_Orca
```

### Cause
The new new_optgroup calls in Tab.cpp used a second parameter with a non-existent icon name:
```cpp
// INCORRECT — unregistered icon -> runtime crash
page->new_optgroup(L("Neotko Interlayer Sanding"),       L"param_interlayer_sanding");
page->new_optgroup(L("Neotko Infill Interlayer Sanding"), L"param_infill_sanding");
```

### Fix
```cpp
page->new_optgroup(L("Neotko Interlayer Sanding"));
page->new_optgroup(L("Neotko Infill Interlayer Sanding"));
```

### Build note
After deleting the full build directory, dependencies must be rebuilt:
```bash
./build_release_macos.sh -x        # no -s -> rebuilds deps
```
Subsequent incremental changes can use:
```bash
./build_release_macos.sh -s -x     # -s -> skip deps, project only
```

---

## 10. Bug fixed: floating objects snapped to Z=0 on move

### Symptom
Moving any object (including the Prime Tower or any unrelated object) caused objects intentionally placed above the bed (floating) to be pulled down to Z=0. The behavior occurred both with and without Temporal Link active.

### Root cause — GLCanvas3D.cpp

The functions do_move(), do_rotate() and do_scale() all contain a "Fixes flying/sinking instances" block that iterates over **all objects in the scene** (not just selected ones) and applies:

```cpp
const double shift_z = m->get_instance_min_z(i.second);
if ((current_printer_technology() == ptSLA || shift_z > SINKING_Z_THRESHOLD) && (shift_z != 0.0f)) {
    const Vec3d shift(0.0, 0.0, -shift_z);
    m->translate_instance(i.second, shift);  // snaps object to Z=0
}
```

With SINKING_Z_THRESHOLD = -0.001f, the condition shift_z > SINKING_Z_THRESHOLD is true for **any object not sunken below the bed**, including intentionally floating ones. This code runs **before** EVT_GLCANVAS_INSTANCE_MOVED is fired, so any protection in Plater.cpp always arrived too late.

### Fix — GLCanvas3D.cpp (4 locations)

Add `&& shift_z < 0.0` to the FFF condition so it only corrects objects that are sinking below the bed, never intentionally floating ones:

```cpp
// do_move (~line 4756) — simple condition:
if ((current_printer_technology() == ptSLA ||
     (shift_z > SINKING_Z_THRESHOLD && shift_z < 0.0)) && (shift_z != 0.0f))

// do_rotate (~line 4870) and do_scale (~lines 4956, 5060) — min_zs condition:
if ((min_zs... >= SINKING_Z_THRESHOLD ||
     (shift_z > SINKING_Z_THRESHOLD && shift_z < 0.0)) && (shift_z != 0.0f))
```

Resulting logic:
- shift_z < -0.001 (sinking below bed): **corrected** — lifted to Z=0
- shift_z = 0 (exactly on bed): **untouched** (condition != 0.0f)
- shift_z > 0 (intentionally floating): **untouched** (shift_z < 0.0 fails)

### Note on m_floating_z_guard in Plater.cpp

During investigation of this bug, m_floating_z_guard was added to Plater::priv as a secondary protection system (persist canonical Z offsets + restore via wxCallAfter). Although the real fix is in GLCanvas3D.cpp, the guard remains as defense in depth against other potential reset paths that may exist in the fork.

---

## Appendix — Output files status

| File | Contains |
|---|---|
| PrintConfig.hpp | Features 3, 4, 5, 6 |
| PrintConfig.cpp | Features 3, 4, 5, 6 |
| PrintObject.cpp | Feature 3 |
| GCode.cpp | Features 3, 5, 6 + pre-existing patch |
| FillBase.cpp | No logic changes (reference only) |
| Preset.cpp | Features 3, 5, 6 |
| Tab.cpp | Features 3, 4, 5, 6 (bug 9 fixed) |
| Plater.cpp | Features 7, 8 + m_floating_z_guard |
| 3mf.cpp | Feature 8 |
| Model.hpp | Feature 8 |
| Model.cpp | Feature 8 |
| GLCanvas3D.cpp | Bug 10 |
| GLCanvas3D.hpp | No changes (included as reference) |
