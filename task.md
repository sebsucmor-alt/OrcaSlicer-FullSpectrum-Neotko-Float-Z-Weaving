# OrcaSlicer FullSpectrum Implementation Plan

## 1. Z-Preset Regions Persistence & GUI
- [x] **Zone Label Persistence:** Update [ZPresetRegions.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp) to store the label string inside the `z_preset_name` ConfigOption value, and read it back during [populate_from_model](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp#171-208).
- [/] **Right Panel Visibility:** Update `MainFrame.cpp` (or [Plater.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp)) to save the `wxAuiManager` pane visibility state for the Right Process Panel in `app_config`, and restore it on startup.

## 2. Z-Override Temperature & Fan Hooks (Hook 2)
- [ ] **GCode Injection:** Implement logic in [GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp) (`process_layer` or `set_extruders`) to detect when the current Z height crosses a Z-Preset threshold.
- [ ] Emit `M104`/`M109` for temperature changes and `M106`/`M107` for fan speed changes based on the overriding config values active at that Z block.

## 3. Neoweaving Enhancements
- [ ] **Wave Smoothing / Arc Fitting:** Modify the `NeoweaveMode::Wave` implementation in [GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp) to either generate smoothed sinusoidal segments or output arc commands (`G2`/`G3`) to prevent firmware lookahead stuttering.
- [ ] **Pressure Advance:** Consider adding tuning parameters (or disabling PA briefly) during Neoweave oscillation if possible, or optimizing the extrusion rate.

## 4. Temporal Link Sync Lock
- [ ] **Prevent Re-entrancy:** Add a block flag `m_syncing_group` in [Plater](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#13112-13120) / [Selection](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#404-405) to avoid cascading UI updates when updating multiple linked objects in a single dragged transformation.
