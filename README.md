
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

- SORRY it's in **Spanish** (use a web translator, I can't keep all in both languages for me and online, it takes too much time sorry

# OrcaSlicer FullSpectrum — Informe de Patches
**Build:** Snapmaker FullSpectrum fork de OrcaSlicer 0.9.3
**Última actualización:** 2026-03-08 (sesión 5)

---

## Estructura de ficheros modificados

```
src/
├── libslic3r/
│   ├── PrintConfig.hpp/cpp    Features 3, 4, 5, 6
│   ├── PrintObject.cpp        Feature 3
│   ├── GCode.cpp              Features 3, 5, 6
│   ├── Preset.cpp             Features 3, 5, 6
│   ├── Print.cpp              Patches preexistentes
│   ├── Model.hpp/cpp          Feature 8 (Temporal Link)
│   ├── Fill/FillBase.cpp      Referencia
│   └── Format/3mf.cpp         Feature 8 + 3MF persistence
└── slic3r/GUI/
    ├── Tab.cpp                Features 3, 4, 5, 6 + Bug 9
    ├── Plater.hpp/cpp         Features 7, 8, 11, 12, 13
    ├── GLCanvas3D.hpp/cpp     Bug 10
    ├── GUI_ObjectList.hpp/cpp Feature 11
    ├── Selection.hpp/cpp      Feature 12
    ├── ParamsPanel.hpp/cpp    Feature 13
    └── PresetComboBoxes.hpp/cpp  Referencia
```

---

## Features implementadas

### Feature 3 — disable_bridge_infill
Enum `DisableBridgeInfill` con opciones: None, Partial, Full.
Afecta a `PrintConfig.hpp/cpp`, `PrintObject.cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

### Feature 4 — Top/Bottom Surface Density UI
Controles de densidad de superficie superior/inferior en la UI de parámetros.
Afecta a `PrintConfig.hpp/cpp`, `Tab.cpp`.

### Feature 5 — Neotko Interlayer Sanding (superficie)
Lijado intercapas para superficies externas (`erTopSolidInfill`).
Afecta a `PrintConfig.hpp/cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

### Feature 6 — Neotko Infill Interlayer Sanding
Lijado intercapas para relleno interno (`erInternalInfill`).
Afecta a `PrintConfig.hpp/cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`.

### Feature 7 — STL file origin preservation
Objetos cuyo centro XY supera 0.5 mm del origen mantienen su posición original al importar.
Afecta a `Plater.cpp`.

---

## Feature 8 — Temporal Link (sistema completo)

Agrupa objetos para movimiento XYZ conjunto sin afectar al slicing.

### Núcleo del sistema
- `int link_group_id { 0 }` en `ModelObject` (`Model.hpp/cpp`), serializado en cereal + assign_copy
- Runtime en `Plater::priv`: `m_link_groups`, `m_next_link_group_id`, `m_pre_move_offsets`, `m_syncing_links`, `m_floating_z_guard`
- Funciones: `update_pre_move_snapshot()`, `rebuild_link_groups_from_model()`, `on_instance_moved_with_link_sync()`, `link_selected_objects()`, `break_link_selected_objects()`
- Menú contextual: "Link Objects" / "Break Link"
- Hooks: `EVT_GLCANVAS_INSTANCE_MOVED`, `DRAGGING_STARTED`, `update_after_undo_redo`, `load_model_objects`, `remove()`

### Persistencia en 3MF (Plater.cpp)
Dado que `store_bbs_3mf` y `read_from_archive` son funciones BBS en ficheros fuera del patch set, la persistencia se implementa en Plater.cpp:

**Al guardar** (`export_3mf` → antes de `store_bbs_3mf`):
- Itera todos los `ModelObject`, stripea cualquier prefijo `[Ln]` existente del nombre
- Re-codifica `[L{link_group_id}]` al inicio del nombre antes de la llamada
- Restaura los nombres originales inmediatamente después

**Al cargar** (`rebuild_link_groups_from_model`):
- Parsea prefijos `[Ln]` de los nombres de objetos
- Stripea el prefijo y almacena el nombre limpio en el modelo
- Setea `link_group_id` desde el prefijo (o lo mantiene si ya venía por metadatos Prusa)
- `wxCallAfter` para diferir `refresh_link_names` hasta que todo el UI post-load haya terminado

**3mf.cpp** (formato Prusa, belt-and-suspenders):
- `_add_model_config_file_to_archive`: también codifica `[Ln]` en el nombre + guarda `link_group_id` como metadata explícita
- `_extract_model_config_from_archive`: parsea el nombre y `link_group_id` metadata

---

## Bug 9 — Crash al arrancar (icono faltante)
`page->new_optgroup(L("Neotko Interlayer Sanding"))` sin segundo parámetro de icono.
Fix en `Tab.cpp`.

---

## Bug 10 — Objetos flotantes snapeados a Z=0
Root cause en `GLCanvas3D.cpp`: `do_move()`, `do_rotate()`, `do_scale()` — bloque "fix flying instances" disparaba para TODOS los objetos.
Fix: añadido `&& shift_z < 0.0` en 4 ubicaciones — solo corrige objetos hundidos, nunca flotantes.
Defensa secundaria: `m_floating_z_guard` en `Plater::priv` con `wxCallAfter` para restaurar Z.

---

## Feature 11 — Indicadores visuales de Temporal Link en el listado

- Prefijo `[L1]`, `[L2]`... en el listado de objetos, coloreado por grupo
- Paleta de 5 colores (amber/sky-blue/purple/emerald/crimson), cíclica
- Helpers: `link_group_color(int)` y `make_link_display_name(const ModelObject*)` en `GUI_ObjectList.cpp`
- `wxRenderer::DrawItemText()` extendido: dibuja prefijo en color del grupo, resto en color normal
- `ObjectList::refresh_link_names()` — itera objetos, aplica nombres prefijados, llama `Refresh()`
- Hooks en `Plater.cpp`: `refresh_link_names()` tras link/break/rebuild

---

## Feature 12 — Copy/paste propaga el grupo completo

**Comportamiento:** Copiar A (L1={A,B}) → pegar A_copy+B_copy en nuevo grupo L3. Copiar A+C (L1,L2) → dos nuevos grupos.

**`Selection::copy_to_clipboard()`** — expande la selección para incluir todos los partners del grupo antes de copiar. Preserva `link_group_id` en objetos del portapapeles.

**`Selection::paste_objects_from_clipboard()`** — tras `paste_objects_into_list()`, llama `plater()->regroup_pasted_link_objects(object_idxs)`.

**`Plater::regroup_pasted_link_objects()`** — agrupa índices pegados por `link_group_id` original. 2+ miembros → nuevo `m_next_link_group_id++`. 1 miembro → limpia a 0. Llama `update_pre_move_snapshot()` + `refresh_link_names()`.

---

## Feature 13 — Apply Process Preset a objetos seleccionados

### UI (ParamsPanel.cpp/hpp)
- `m_object_preset_btn` (ScalableButton, icono "save") en `m_mode_sizer`, junto a `m_tips_arrow`
- Tooltip: "Apply a process preset to selected object(s)"
- Click: muestra `wxSingleChoiceDialog` con **solo User Presets** (excluye system y default)
- Llamada diferida con `wxGetApp().CallAfter(...)` para evitar crash al destruir el dialog

### Plater::apply_print_preset_to_selected_objects() (Plater.cpp/hpp)
- Localiza el preset por nombre
- Colecta `obj_idxs` de la selección actual
- Aplica con `obj->config.apply_only(preset->config, safe_keys, true)`
- `safe_keys` = `PrintObjectConfig().keys() + PrintRegionConfig().keys()` excluyendo keys multimaterial
- Keys MM excluidas: `wall_filament`, `sparse_infill_filament`, `solid_infill_filament`, `support_filament`, `support_interface_filament`, `flush_into_*`, `mmu_*`, `wipe_*`, `role_based_wipe_speed`
- Finaliza con `this->changed_objects(changed_idxs)`

---

## Comandos de build

```bash
./build_release_macos.sh -x        # rebuild completo
./build_release_macos.sh -s -x     # incremental (skip deps)
```

---

## Estado por feature

| # | Feature | Estado |
|---|---------|--------|
| 3 | disable_bridge_infill | ✅ Completo |
| 4 | Top/Bottom density UI | ✅ Completo |
| 5 | Neotko Interlayer Sanding (superficie) | ✅ Completo |
| 6 | Neotko Infill Interlayer Sanding | ✅ Completo |
| 7 | STL origin preservation | ✅ Completo |
| 8 | Temporal Link — núcleo | ✅ Completo |
| 9 | Bug startup crash icono | ✅ Fixed |
| 10 | Bug floating Z=0 snap | ✅ Fixed |
| 11 | Temporal Link — indicadores visuales [Ln] | ✅ Completo |
| 12 | Temporal Link — copy/paste propaga grupo | ✅ Completo |
| 13 | Apply Process Preset a objetos | ✅ Completo (pendiente confirmar build actual) |
| — | 3MF persistence de Temporal Link | ✅ Implementado (pendiente confirmar) |
| — | MM keys excluidas del Apply Preset | ✅ Completo |

---

## Notas técnicas clave

- `Preset::print_options()` ≠ keys válidas per-objeto. Usar `PrintObjectConfig().keys() + PrintRegionConfig().keys()`
- `store_bbs_3mf` / `read_from_archive` están en el source tree de BBS, no en el patch set → persistencia de links vía nombre del objeto en `Plater.cpp`
- `StaticBox*` no `wxPanel*` para paneles con `SetBackgroundColor()`
- `wxGetApp().CallAfter(lambda)` en lugar de `wxCallAfter()` libre (requiere `<wx/app.h>` que no siempre está incluido)
- `!p.is_system && !p.is_default` para filtrar solo User Presets
