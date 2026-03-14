# OrcaSlicer FullSpectrum - Fork: Neotko ZPreset, Neoweave and Real Per Object Setting+Float objects and link system — Complete Technical Documentation
**Version:** patch series v1–v78  
**Base:** OrcaSlicer / Snapmaker fork 0.9.3  
**Target platform:** macOS ARM64 (Apple Silicon), builds via `./build_release_macos.sh -s -x`

---

## Table of Contents
1. [Project Overview](#project-overview)
2. [Build Instructions](#build-instructions)
3. [Feature Index](#feature-index)
4. [Z-Override Regions — Deep Dive](#z-override-regions--deep-dive)
5. [Neoweaving](#neoweaving)
6. [Other Features](#other-features)
7. [Architecture Decisions & Lessons Learned](#architecture-decisions--lessons-learned)
8. [File Map](#file-map)
9. [Known Issues & Future Work](#known-issues--future-work)

---

## Project Overview

FullSpectrum Fork: Neotko ZPreset, Neoweave and Real Per Object Setting+Float objects and link system is a set of patches on top of OrcaSlicer adding:

- **Z-Override Regions** — per-height-zone print parameter overrides on a single object
- **Neoweaving** — wave and linear infill weaving modes
- **Temporal Link** — synchronized scale/rotation between linked object instances
- **Simplify3D .factory importer** — v4/v5 format support
- **Right-side process panel** — AUI pane for quick parameter access
- **Various UI improvements** — opacity slider, save-as-preset button, Z-band canvas highlight

---

## Build Instructions

```bash
# Clean build (required after CMakeLists changes or first build)
rm -rf build/arm64
./build_release_macos.sh -s -x

# Incremental build (after .cpp/.hpp changes only)
./build_release_macos.sh -s -x
# or directly:
cd build/arm64 && ninja -j$(sysctl -n hw.ncpu) Snapmaker_Orca
```

**CMakeLists.txt** — new .cpp files must be added here. ZPresetRegions.cpp is already listed.

**Patch application:** extract ZIP into the source root, replacing files, then build.

---

## Feature Index

| Feature | Files | Status |
|---------|-------|--------|
| Z-Override Regions | ZPresetRegions.hpp/cpp, PrintObject.cpp, Print.cpp, Model.hpp | ✅ Complete |
| Prime tower + Z-override compat | Print.cpp, Model.hpp | ✅ Complete |
| Neoweaving Wave | PrintConfig.hpp/cpp, GCode.cpp, FillBase.cpp | ✅ Experimental |
| Neoweaving Linear | PrintConfig.hpp/cpp, GCode.cpp, FillBase.cpp | ✅ (hardware untested) |
| Temporal Link | Model.hpp/cpp, Plater.hpp/cpp, 3mf.cpp | ✅ Complete |
| S3D .factory importer | Format/S3DFactory.hpp/cpp | ✅ Tested |
| Right-side process panel | ParamsPanel.hpp/cpp, Plater.hpp/cpp | ✅ Complete |
| Model opacity slider | ParamsPanel.hpp/cpp | ✅ Complete |
| Save object config as preset | Tab.cpp, Preset.cpp | ✅ Complete |
| Z-band canvas highlight | GLCanvas3D.hpp/cpp | ✅ Complete |
| Z-Override tree nodes | GUI_ObjectList.hpp/cpp, ObjectDataViewModel.hpp/cpp | ✅ Complete |
| Hook 2 (GCode temp/fan injection) | GCode.cpp | ⏳ TODO |

---

## Z-Override Regions — Deep Dive

### Concept

Allows assigning per-zone print parameter overrides to height ranges on a single object.
Example: bottom 10mm at wall_loops=4, top 10mm at sparse_infill_density=10.

This is different from OrcaSlicer's native **Height Range Modifier** (which only modifies
individual volumes) — Z-Override works on the full object cross-section.

### Architecture (v2 — post v75)

**Storage:** `ModelObject::layer_config_ranges` (same map as native height painting).

Each Z-override zone is a `t_layer_height_range → ModelConfig` entry where:
- `ModelConfig` contains `KEY_Z_PRESET_NAME = "z_preset_name"` as an empty-string marker
- Plus any user-checked override keys with their serialized values
- Zones with no user overrides store only the marker key

**Key invariant:** Z ranges are **object-local** (0 = object bottom, h = object top).
Moving the object on the plate does NOT affect ranges.

**Marker key:** `z_preset_name` (value always `""`) — distinguishes our ranges from
native height-painting ranges. Never serialized to GCode; ignored by all slicer paths
except our own UI code.

### Data flow

```
User checks override in dialog
    ↓
save_overrides_from_panel(row)
    → m_rows[row].overrides[key] = serialized_value
    ↓
on_ok() → apply_to_model()
    → for each row: ModelConfig.set(KEY_Z_PRESET_NAME, "")
                    + DynamicPrintConfig tmp; tmp.set_deserialize(key, val, ctx)
                    + ModelConfig.apply(tmp, ignore_nonexistent=true)
    ↓
changed_object(obj_idx) → schedule_background_process()
    ↓
Print::validate() (UI thread, via wxTimer)
    → has_custom_layering check (PATCHED — ignores z_preset_name ranges)
    → layer_height_profile_from_ranges (PATCHED — skips z_preset_name ranges)
    ↓
Print::process() → slicer thread
    → apply_to_print_region_config()
      reads layer_config_ranges, applies per-region configs
      (z_preset_name key is silently skipped — not in PrintRegionConfig)
    ↓ GCode generated with overrides applied per layer
```

### Critical bug history

**Bug 1 — layer_height_profile_from_ranges crash (fixed v73)**
`Print::validate()` calls `update_layer_height_profile()` which calls
`layer_height_profile_from_ranges()`. This function iterates `layer_config_ranges`
and crashes when it encounters configs with unknown keys (our 130+ override keys).
**Fix:** In `PrintObject.cpp::update_layer_height_profile()`, filter the map before
passing to `layer_height_profile_from_ranges()` — only pass ranges that contain
`layer_height`. If none do (our normal case), pass an empty map → uniform profile.

```cpp
// PrintObject.cpp ~line 3835
bool any_lh = false;
for (const auto& [r, cfg] : *ranges_to_use)
    if (cfg.has("layer_height")) { any_lh = true; break; }
static const t_layer_config_ranges s_empty_ranges;
layer_height_profile = layer_height_profile_from_ranges(
    slicing_parameters, any_lh ? lh_ranges : s_empty_ranges);
```

**Bug 2 — Prime tower "same variable layer height" error (fixed v74, v78)**
`has_custom_layering()` in `Model.hpp` returns true if `layer_config_ranges` is
non-empty. Our zones always populate it → prime tower validation fails.

Fix 1 (`Model.hpp`): `has_custom_layering()` only returns true if a range has
`layer_height` key AND does NOT have `z_preset_name`.

Fix 2 (`Print.cpp`): `Print::validate()` has its own inline lambda replicating
`has_custom_layering()` — this also needed the same fix.

```cpp
// Model.hpp
bool has_custom_layering() const {
    if (!this->layer_height_profile.empty()) return true;
    for (const auto& [range, cfg] : this->layer_config_ranges) {
        if (cfg.has("z_preset_name")) continue; // our marker — skip
        if (cfg.has("layer_height"))  return true;
    }
    return false;
}

// Print.cpp — same logic inline in validate()
```

### UI Structure

```
ZPresetRegionsDialog (wxDialog, 1020×560, resizable)
├── Header label
├── Body (wxHORIZONTAL)
│   ├── LEFT: wxGrid (3 cols: Z min | Z max | Label)
│   │         + "Add Zone" / "Remove Zone" buttons
│   ├── CENTRE: wxScrolledWindow (override panel)
│   │   ├── Hint text (hidden when zone selected)
│   │   └── Per-group sections:
│   │       ├── Bold colored group header
│   │       └── wxFlexGridSizer (3 cols: [✓] | label | value ctrl)
│   │           value ctrl = wxChoice (enum/bool) or wxTextCtrl (numeric/string)
│   └── RIGHT: ZRegionPreview (160px wide, draggable boundaries)
└── Bottom: Cancel | Apply buttons
```

### Zone management rules (v78)

- **Open dialog:** zones always span [0, obj_height]. Front zone clamped to z_min=0,
  back zone clamped to z_max=obj_height.
- **Add zone:** splits the currently selected zone (or last zone) at its midpoint.
  New zone inherits parent's overrides as starting point.
- **Remove zone:** expands adjacent zone to fill gap. If last zone: confirms and
  clears all z-override ranges from ModelObject (disables feature entirely).
- **Draggable boundaries:** ZRegionPreview handles mouse drag, updates both m_rows
  and grid cells, fires highlight_band_in_canvas.
- **Row select → load:** saves current panel to m_rows[prev], loads m_rows[next].
- **Apply:** saves panel → validates → writes to ModelObject → changed_object().

### Override panel — supported types

| Config type | UI control | Value stored |
|------------|-----------|-------------|
| `coBool` | wxChoice ("false"/"true") | "true" or "false" |
| `coEnum` (listed below) | wxChoice (enum strings) | serialized enum string |
| `coFloat`, `coInt`, `coFloatOrPercent` | wxTextCtrl | plain number or "50%" |
| `coString` | wxTextCtrl | raw string |

**Keys with wxChoice (enum dropdowns):**

| Key | Choices |
|-----|---------|
| `seam_position` | nearest, aligned, aligned_back, back, random |
| `wall_sequence` | inner wall/outer wall, outer wall/inner wall, inner-outer-inner wall |
| `wall_direction` | auto, ccw, cw |
| `ironing_type` | no ironing, top, topmost, solid |
| `fuzzy_skin` | none, external, all, allwalls |
| `sparse_infill_pattern` | monotonic, monotonicline, rectilinear, … (27 options) |
| `top_surface_pattern` | monotonic, monotonicline, rectilinear, … (10 options) |
| `bottom_surface_pattern` | same as top_surface_pattern |

### Key groups in override panel

1. **Walls** — wall_loops, wall_sequence, wall_direction, line widths, speeds, flow, single-wall options
2. **Seam** — seam_position, seam_gap, wipe_on_loops, wipe_before_external_loop, wipe_speed
3. **Infill** — density, pattern, line width, speed, filament, rotate template, combination, anchors, overlap
4. **Top / Bottom** — shell layers, thickness, patterns, speeds, density, flow ratio, overlap
5. **Ironing** — type, speed, flow, spacing, inset
6. **Speed & Bridges** — bridge speed, gap fill, small perimeter, overhang speed
7. **Flow** — print_flow_ratio, bridge_flow
8. **Fuzzy Skin** — type, thickness, point distance
9. **Layer Height** — layer_height (use with caution — triggers custom layering checks)
10. **Advanced** — filter_out_gap_fill, gap_closing_radius, enable_arc_fitting

### ZRegionPreview widget

Custom `wxPanel` (160px wide). Coordinate system: Z=0 at bottom → high Y; Z=max → low Y.

- Colored bands per zone (palette of 7 colors, cycles)
- Selected band: lightened 140%
- Draggable boundaries: hit test ±6px, cursor changes to wxCURSOR_SIZENS
- Shows override count per zone (number inside band)
- Tick marks at all boundaries with Z value labels
- Fires `m_on_boundary_drag(boundary_idx, new_z)` callback on drag

### 3D canvas Z-band highlight (GLCanvas3D)

```cpp
// Highlight a Z range in world coordinates on the canvas:
canvas->set_z_band_highlight(z_min_world, z_max_world, x_min, y_min, x_max, y_max);
canvas->clear_z_band_highlight();
```

Rendered as two semi-transparent planes (40% alpha blue border, 8% alpha fill).
Cleared on: dialog close, Cancel, new project, `priv::reset()`.

---

## Neoweaving

Wave-pattern and linear infill modes. Adds keys:
- `neoweave_mode` (enum: none / wave / linear)
- `neoweave_amplitude`, `neoweave_frequency`, `neoweave_angle_lock`

Implemented in `FillBase.cpp` (infill geometry generation) and `GCode.cpp` (speed modulation).
Marked as Experimental in UI. Linear mode: hardware test pending.

---

## Other Features

### Temporal Link
Linked object instances (Ctrl+drag creates link) propagate rotation and scale changes.
Prevents "floating Z" when a linked object is rotated (Z is recalculated from world bbox).
Data stored in `ModelObject::linked_ids` (custom field in Model.hpp/cpp + 3MF persistence).

### Simplify3D .factory Importer
`Format/S3DFactory.hpp/cpp` — parses v4 and v5 .factory XML files.
Imports: models, process profiles, support settings, layer heights, colors.
Registered in `Format/` CMakeLists and accessible via File → Import.

### Right-Side Process Panel
AUI pane on right edge of main window. Toggle via: right-edge GLToolbar button OR
"Process" button in ParamsPanel top bar. Visibility persistence via app_config: TODO.

### Model Opacity Slider
In ParamsPanel. Adjusts `GLVolume::color.a()` for the selected object. Range 10–100%.

---

## Architecture Decisions & Lessons Learned

### 1. ModelConfig API
```cpp
cfg.option(key)          // get ConfigOption* (NOT optptr — that doesn't exist on ModelConfig)
cfg.set(key, value)      // set typed value
cfg.set_key_value(key, ConfigOption*)  // takes ownership of raw pointer
cfg.apply(DynamicPrintConfig, ignore_nonexistent)
cfg.has(key)             // check presence
cfg.opt_serialize(key)   // returns raw string WITHOUT surrounding quotes

// set_deserialize requires ConfigSubstitutionContext:
ConfigSubstitutionContext ctx(ForwardCompatibilitySubstitutionRule::Disable);
cfg.set_deserialize(key, val, ctx);
```

### 2. Enum value `comDevelop`
The correct enum value for developer mode is `comDevelop` (NOT `comNone`, `comDeveloper`).
Used when adding new keys to `PrintConfig` to avoid validation errors.

### 3. Lambda capture in TBB parallel_for
TBB lambdas with explicit capture lists `[this, x]` cannot implicitly capture outer
variables. Use file-scope structs with static methods instead of `auto lambda = [&]{}`.

### 4. `has_custom_layering()` is checked in TWO places
Both `Model.hpp::has_custom_layering()` AND an inline lambda in `Print.cpp::validate()`
replicate the same logic. Any patch must update BOTH.

### 5. `layer_height_profile_from_ranges` expects only `layer_height` ranges
This function (in the base Orca codebase, not in our patches) assumes every
`ModelConfig` entry in the map has a `layer_height` key. Passing configs with
arbitrary keys causes out-of-range crashes. Always filter before calling.

### 6. Z-band highlight must be cleared in all exit paths
The 3D canvas highlight persists across frames. Must call `clear_z_band_highlight()`
in: wxEVT_CLOSE_WINDOW, wxID_CANCEL button, and any path that closes the dialog.

### 7. changed_object() must NOT be called from inside apply_to_model()
Calling `changed_object()` triggers re-entrant UI updates while the dialog is alive.
Always call it AFTER `ShowModal()` returns, in the entry-point function.

### 8. Signal for crash diagnosis
The crash at `layer_height_profile_from_ranges` showed as:
```
[WARNING in_range_cast.h:38] value -634136515 out of range
zsh: segmentation fault
```
The `in_range_cast` warning is a Chromium base library warning about integer overflow —
it's a symptom, not the cause. Actual cause was memory corruption from unexpected
config keys. Use signal handlers + `backtrace_symbols()` for crash diagnosis.

---

## File Map

```
src/
├── libslic3r/
│   ├── PrintConfig.hpp      Fork: Neotko ZPreset, Neoweave and Real Per Object Setting+Float objects and link system keys: z_preset_name (comDevelop),
│   │                        neoweave_*, all new feature config keys
│   ├── PrintConfig.cpp      Enum string maps, key defaults, group assignments
│   ├── PrintObject.cpp      apply_to_print_region_config (debug removed v76),
│   │                        update_layer_height_profile (layer_height filter v73),
│   │                        apply_to_print_region_config (cleanup v76)
│   ├── Print.cpp            validate() patches:
│   │                          - has_custom_layering inline lambda (v78)
│   │                          - prime tower variable layer height check (v78)
│   ├── Model.hpp            has_custom_layering() (v74+v78),
│   │                        ModelObject::linked_ids (Temporal Link)
│   ├── Model.cpp            Temporal Link propagation
│   ├── GCode.cpp            Neoweaving speed modulation, layer-change hooks
│   ├── Fill/FillBase.cpp    Neoweaving geometry (wave + linear modes)
│   └── Format/
│       ├── 3mf.cpp          Temporal Link persistence
│       └── S3DFactory.hpp/cpp  Simplify3D .factory importer
└── slic3r/GUI/
    ├── ZPresetRegions.hpp   ZRegionPreview widget, ZPresetRegionsDialog class
    ├── ZPresetRegions.cpp   Full implementation (639 lines as of v78)
    ├── Plater.hpp/cpp       open_z_preset_regions_for_selection(), menu injection,
    │                        changed_object() (debug removed v76)
    ├── GLCanvas3D.hpp/cpp   ZBandHighlight struct, render(), set/clear methods
    ├── GUI_ObjectList.hpp/cpp  Z-override tree nodes (itZPresetRoot/itZPreset),
    │                           right-click menu injection
    ├── ObjectDataViewModel.hpp/cpp  itZPreset node types
    ├── Tab.cpp              Save-as-preset button, neoweave UI
    ├── Preset.cpp           Neoweave preset handling
    ├── ParamsPanel.hpp/cpp  Right-side panel, opacity slider, z_regions_btn
    └── Selection.hpp/cpp    Feature 12 (selection improvements)
```

---

## Known Issues & Future Work

### TODO: Hook 2 — GCode temperature/fan injection
At each layer change in GCode.cpp, detect which z-override zone covers the current Z,
and emit M-codes for:
- `nozzle_temperature` → M104/M109
- `bed_temperature` → M140/M190  
- `fan_speed` → M106

Track last-emitted values to avoid redundant codes. Implementation point:
`GCode.cpp::change_layer()` or `process_layer()`.

### TODO: Right panel visibility persistence
Save/restore the right-side process panel visibility in `app_config`.

### TODO: Neoweaving Linear hardware test
Linear mode is implemented but not validated on physical hardware.

### TODO: 3MF round-trip for Z-override zones
`layer_config_ranges` is already serialized to 3MF by the base Orca code.
However, the `z_preset_name` marker key needs to survive the round-trip.
Verify in `Format/3mf.cpp` that arbitrary string keys in ModelConfig are preserved.

### KNOWN: Zone label not persisted
Zone labels (user-editable names) are NOT stored in ModelConfig — only the override
keys are stored. On re-open, labels revert to "Zone N". Fix: store label as a
specially-prefixed string key in ModelConfig, e.g. `_zpreset_label`.

### KNOWN: Preview band label shows count, not name
The ZRegionPreview shows override count (e.g. "3") instead of zone label.
Once label persistence is fixed, display label text instead.

### KNOWN: Z-band canvas highlight disappears on scene reload
`reload_scene()` clears some GLCanvas state. The ZBandHighlight may need to be
re-applied after reload. Tracked from session 13.
