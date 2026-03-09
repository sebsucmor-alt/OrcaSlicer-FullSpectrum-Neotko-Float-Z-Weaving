
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

All info documentation and changes listed - Update v5
- Working Link system and save/load
- Working Select Uesr Process for any object, except Multimaterial tag to avoid mix options with Wipetower
- All other stuff comment downbelow all documented

# OrcaSlicer FullSpectrum — Patches Report
**Build:** Snapmaker FullSpectrum fork of OrcaSlicer 0.9.3
**Last updated:** 2026-03-08 (session 5)

---

## Modified files structure

```
src/
├── libslic3r/
│   ├── PrintConfig.hpp/cpp    Features 3, 4, 5, 6
│   ├── PrintObject.cpp        Feature 3
│   ├── GCode.cpp              Features 3, 5, 6
│   ├── Preset.cpp             Features 3, 5, 6
│   ├── Print.cpp              Pre-existing patches
│   ├── Model.hpp/cpp          Feature 8 (Temporal Link)
│   ├── Fill/FillBase.cpp      Reference only
│   └── Format/3mf.cpp         Feature 8 + 3MF persistence
└── slic3r/GUI/
    ├── Tab.cpp                Features 3, 4, 5, 6 + Bug 9
    ├── Plater.hpp/cpp         Features 7, 8, 11, 12, 13 + STL origin bug fix
    ├── GLCanvas3D.hpp/cpp     Bug 10
    ├── GUI_ObjectList.hpp/cpp Feature 11
    ├── Selection.hpp/cpp      Feature 12
    ├── ParamsPanel.hpp/cpp    Feature 13
    └── PresetComboBoxes.hpp/cpp  Reference only
```

---

## Implemented features

### Feature 3 — disable_bridge_infill
`DisableBridgeInfill` enum with options: None, Partial, Full.
Affects `PrintConfig.hpp/cpp`, `PrintObject.cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

### Feature 4 — Top/Bottom Surface Density UI
Top and bottom surface density controls in the parameters UI.
Affects `PrintConfig.hpp/cpp`, `Tab.cpp`.

### Feature 5 — Neotko Interlayer Sanding (surfaces)
Inter-layer sanding for external surfaces (`erTopSolidInfill`).
Affects `PrintConfig.hpp/cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

DATA for this Feautre:

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


### Feature 6 — Neotko Infill Interlayer Sanding
Inter-layer sanding for internal infill (`erInternalInfill`).
Affects `PrintConfig.hpp/cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

### Feature 7 — STL file origin preservation
Objects whose XY position in the file is not exactly (0,0) retain their original coordinates on import without auto-centering on the bed. Only STLs exported at the absolute origin are auto-placed at bed center.

**Implementation in `Plater.cpp` (`load_model_objects`):**
- On instance creation, `orig_bbox.center()` is captured before `center_around_origin()` moves the mesh
- Instance offset is restored to the file's world-space coordinates
- In the auto-placement loop: `has_any_file_position = (x != 0.0 || y != 0.0)` — any non-zero offset is preserved; auto-centering only happens when offset is exactly `(0, 0)`

**Bug fixed this session:** The previous 0.5 mm threshold caused assembly parts whose individual bounding-box center is near the origin to be wrongly repositioned when loaded one by one, even though loading them all at once (via "Load as single object with multiple parts") worked correctly. Both load paths are now consistent.

---

## Feature 8 — Temporal Link (full system)

Groups objects for joint XYZ movement without affecting slicing.

### Core system
- `int link_group_id { 0 }` in `ModelObject` (`Model.hpp/cpp`), serialized in cereal + assign_copy
- Runtime fields in `Plater::priv`: `m_link_groups`, `m_next_link_group_id`, `m_pre_move_offsets`, `m_syncing_links`, `m_floating_z_guard`
- Functions: `update_pre_move_snapshot()`, `rebuild_link_groups_from_model()`, `on_instance_moved_with_link_sync()`, `link_selected_objects()`, `break_link_selected_objects()`
- Context menu: "Link Objects" / "Break Link"
- Hooks: `EVT_GLCANVAS_INSTANCE_MOVED`, `DRAGGING_STARTED`, `update_after_undo_redo`, `load_model_objects`, `remove()`

### 3MF persistence (`Plater.cpp`)
`store_bbs_3mf` and `read_from_archive` are BBS functions in source files outside the patch set, so persistence is implemented directly in `Plater.cpp`:

**On save** (before `store_bbs_3mf`):
- Strips any existing `[Ln]` prefix from each object's name
- Re-encodes `[L{link_group_id}]` at the start of the name
- Restores the original names immediately after the save call

**On load** (`rebuild_link_groups_from_model`):
- Parses `[Ln]` prefixes from object names
- Strips the prefix and stores the clean name in the model
- Sets `link_group_id` from the prefix (preserves existing value if already set by metadata)
- `wxGetApp().CallAfter(...)` defers `refresh_link_names` until all post-load UI operations have settled

**3mf.cpp** (Prusa format, belt-and-suspenders):
- `_add_model_config_file_to_archive`: also encodes `[Ln]` in the name + saves `link_group_id` as explicit metadata
- `_extract_model_config_from_archive`: parses both the name prefix and `link_group_id` metadata

---

## Bug 9 — Startup crash (missing icon parameter)
`page->new_optgroup(L("Neotko Interlayer Sanding"))` called without the second icon parameter.
Fixed in `Tab.cpp`.

---

## Bug 10 — Floating objects snapped to Z=0
Root cause in `GLCanvas3D.cpp`: `do_move()`, `do_rotate()`, `do_scale()` — "fix flying instances" block was firing for ALL objects.
Fix: added `&& shift_z < 0.0` at 4 locations — only corrects sinking objects, never floating ones.
Secondary defense: `m_floating_z_guard` in `Plater::priv` with `wxCallAfter` Z restore.

---

## Feature 11 — Temporal Link visual indicators in object list

- `[L1]`, `[L2]`... prefix in the object list, color-coded per group
- 5-color palette (amber/sky-blue/purple/emerald/crimson), cycling
- Helpers: `link_group_color(int)` and `make_link_display_name(const ModelObject*)` in `GUI_ObjectList.cpp`
- `wxRenderer::DrawItemText()` extended: draws the prefix in group color, rest in normal color
- `ObjectList::refresh_link_names()` — iterates objects, applies prefixed names, calls `Refresh()`
- Hooks in `Plater.cpp`: `refresh_link_names()` called after link/break/rebuild

---

## Feature 12 — Copy/paste propagates full link group

**Behavior:** Copy A (L1={A,B}) → paste A_copy+B_copy into new group L3. Copy A+C (L1,L2) → two new groups.

**`Selection::copy_to_clipboard()`** — expands selection to include all link-group partners before copying. Preserves `link_group_id` in clipboard objects.

**`Selection::paste_objects_from_clipboard()`** — after `paste_objects_into_list()`, calls `plater()->regroup_pasted_link_objects(object_idxs)`.

**`Plater::regroup_pasted_link_objects()`** — groups pasted indices by original `link_group_id`. 2+ members → new `m_next_link_group_id++`. 1 member → cleared to 0. Calls `update_pre_move_snapshot()` + `refresh_link_names()`.

---

## Feature 13 — Apply Process Preset to selected objects

### UI (ParamsPanel.cpp/hpp)
- `m_object_preset_btn` (ScalableButton, "save" icon) in `m_mode_sizer`, next to `m_tips_arrow`
- Tooltip: "Apply a process preset to selected object(s)"
- Click: shows `wxSingleChoiceDialog` with **User Presets only** (`!p.is_system && !p.is_default`)
- Call deferred via `wxGetApp().CallAfter(...)` to avoid crash during dialog destruction

### Plater::apply_print_preset_to_selected_objects() (Plater.cpp/hpp)
- Finds preset by name, collects `obj_idxs` from current selection
- Applies with `obj->config.apply_only(preset->config, safe_keys, true)`
- `safe_keys` = `PrintObjectConfig().keys() + PrintRegionConfig().keys()` excluding multi-material keys:
  `wall_filament`, `sparse_infill_filament`, `solid_infill_filament`, `support_filament`, `support_interface_filament`, `flush_into_*`, `mmu_*`, `wipe_*`, `role_based_wipe_speed`
- Finishes with `this->changed_objects(changed_idxs)`

---

## Build commands

```bash
./build_release_macos.sh -x        # full rebuild
./build_release_macos.sh -s -x     # incremental (skip deps)
```

---

## Feature status

| # | Feature | Status |
|---|---------|--------|
| 3 | disable_bridge_infill | ✅ Complete |
| 4 | Top/Bottom density UI | ✅ Complete |
| 5 | Neotko Interlayer Sanding (surfaces) | ✅ Complete |
| 6 | Neotko Infill Interlayer Sanding | ✅ Complete |
| 7 | STL origin preservation (single + multi file) | ✅ Complete |
| 8 | Temporal Link — core | ✅ Complete |
| 9 | Bug startup crash missing icon | ✅ Fixed |
| 10 | Bug floating objects snapped to Z=0 | ✅ Fixed |
| 11 | Temporal Link — visual [Ln] indicators | ✅ Complete |
| 12 | Temporal Link — copy/paste propagates group | ✅ Complete |
| 13 | Apply Process Preset to objects (User Presets only, MM keys excluded) | ✅ Complete |
| — | Temporal Link — 3MF persistence | ✅ Complete |

---

## Key technical notes

- `Preset::print_options()` ≠ valid per-object keys. Use `PrintObjectConfig().keys() + PrintRegionConfig().keys()`
- `store_bbs_3mf` / `read_from_archive` are in BBS source files outside the patch set → link persistence via object name encoding in `Plater.cpp`
- Use `StaticBox*` not `wxPanel*` for panels using `SetBackgroundColor()`
- Use `wxGetApp().CallAfter(lambda)` not free `wxCallAfter()` (which requires `<wx/app.h>` not always included)
- `!p.is_system && !p.is_default` to filter User Presets only
- The 0.5 mm STL origin threshold was replaced with exact `!= 0.0` check to avoid false negatives on assembly parts with bbox near origin
