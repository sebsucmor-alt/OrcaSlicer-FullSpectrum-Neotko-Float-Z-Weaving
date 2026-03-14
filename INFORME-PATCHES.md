# OrcaSlicer FullSpectrum — Neotko Fork - Informe Técnico Completo

**Fork:** Snapmaker FullSpectrum  
**Base:** OrcaSlicer 0.9.3 (BBS/Bambu fork)  
**Última actualización:** 2026-03-13 (sesión 9)  
**ZIP actual:** `OrcaSlicer-FullSpectrum-patches-18.zip`  
**Autor de los patches:** Neotko + Claude (Anthropic)

---

## Propósito de este documento

Este informe está diseñado para dos casos de uso:

1. **Reanudar el desarrollo desde cero** — contiene suficiente contexto técnico para que una nueva sesión pueda continuar sin haber participado en las anteriores.
2. **Referencia para otros builders** — explica el razonamiento detrás de cada decisión técnica, no solo qué se cambió sino por qué.

---

## Arquitectura del proyecto

OrcaSlicer FullSpectrum es un fork del fork de Bambu Lab de OrcaSlicer (versión 0.9.3). El repositorio Snapmaker ya incluye algunos patches propios. Este patch set se aplica encima de ese estado base.

### Estructura de ficheros modificados

```
src/
├── libslic3r/                         ← Motor de slicing (sin UI)
│   ├── PrintConfig.hpp / .cpp         ← Definición de todos los parámetros
│   ├── PrintObject.cpp                ← Lógica de preparación del objeto
│   ├── GCode.cpp                      ← Generación de GCode (el corazón)
│   ├── Preset.cpp                     ← Sistema de presets
│   ├── Print.cpp                      ← Patches preexistentes Snapmaker
│   ├── Model.hpp / .cpp               ← Modelo de objetos en escena
│   ├── Fill/FillBase.cpp              ← Lógica base de relleno
│   └── Format/3mf.cpp                 ← Lectura/escritura de ficheros 3MF
└── slic3r/GUI/                        ← Interfaz gráfica (wxWidgets)
    ├── Tab.cpp                        ← Pestañas de parámetros
    ├── Plater.hpp / .cpp              ← Ventana principal, AUI layout
    ├── GLCanvas3D.hpp / .cpp          ← Canvas 3D OpenGL
    ├── GUI_ObjectList.hpp / .cpp      ← Lista de objetos
    ├── Selection.hpp / .cpp           ← Sistema de selección
    └── ParamsPanel.hpp / .cpp         ← Panel de parámetros de proceso
```

### Sistema de configuración de OrcaSlicer

Entender cómo funciona el sistema de configuración es crítico para añadir nuevos parámetros:

- **`PrintConfigDef`** — base de datos global de todos los parámetros (nombre, tipo, tooltip, etc.). Se registra en `PrintConfig.cpp` dentro de `init_fff_params()`.
- **`PrintObjectConfig`**, **`PrintRegionConfig`**, **`PrintConfig`**, **`FullPrintConfig`** — structs estáticos generados por macros Boost.PP en `PrintConfig.hpp`. Cada key declarada en la macro **debe** tener una entrada en `PrintConfigDef` o el arranque crashea con "Unhandled unknown exception" justo después del mensaje "Initializing StaticPrintConfigs".
- **`StaticPrintConfig::initialize_cache()`** — valida en runtime que cada key declarada en la macro del struct existe en `PrintConfigDef`. Si no existe, lanza una excepción no tipada que OrcaSlicer captura como crash.
- **`Preset.cpp::s_Preset_print_options`** — lista de strings de todas las keys que se guardan en un preset de impresión. Añadir una key aquí no causa crash, pero si se omite la key no se persistirá en presets.

**Regla de oro al añadir un nuevo parámetro:**
1. Añadir la key al struct apropiado en `PrintConfig.hpp` (macro `PRINT_CONFIG_CLASS_DEFINE`)
2. Registrarla en `PrintConfig.cpp::init_fff_params()` con `this->add("nombre", tipo)`
3. Añadirla a `s_Preset_print_options` en `Preset.cpp`
4. Añadir la UI en `Tab.cpp`

---

## Feature 3 — Control de bridge infill (`disable_bridge_infill`)

### Qué hace
Permite desactivar el relleno de puentes (bridge infill), total o parcialmente. Útil cuando la geometría del modelo genera puentes innecesarios que empeoran la calidad de la superficie superior.

### Enum `DisableBridgeInfill`
- `None` — comportamiento por defecto, puentes normales
- `Partial` — desactiva solo parte del bridge infill
- `Full` — desactiva completamente el bridge infill

### Archivos afectados
`PrintConfig.hpp/cpp`, `PrintObject.cpp`, `GCode.cpp`, `Preset.cpp`, `Tab.cpp`

---

## Feature 4 — Densidad de superficie superior/inferior en la UI

### Qué hace
Expone en la UI los controles de densidad de superficie superior e inferior que existían en el motor pero no tenían un control accesible en la interfaz de OrcaSlicer 0.9.3.

### Archivos afectados
`PrintConfig.hpp/cpp`, `Tab.cpp`

---

## Features 5 y 6 — Neotko Neoweaving

Esta es la feature más compleja y técnicamente interesante del build.

### Contexto y nomenclatura

Neoweaving fue inventado por Neotko (el mismo autor de Ironing). Anteriormente se llamaba "Interlayer Sanding" en el código, un nombre que generaba confusión con la función de ironing. Se renombró a **Neoweaving** en todos los labels y keys visibles al usuario.

Las keys internas siguen siendo `interlayer_neoweave_*` e `infill_neoweave_*`.

### La idea fundamental

Cuando imprimes capas de superficie con un patrón monotónico, las líneas adyacentes de la misma capa se tocan por sus lados, pero hay un hueco vertical entre capas (el espesor de capa). Neoweaving hace que las líneas de la capa N queden físicamente encajadas en los huecos de la capa N-1, creando un entrelazado mecánico que:

- Aumenta la resistencia a la delaminación
- Mejora la cohesión de la superficie
- Puede mejorar el acabado visual en ciertos materiales

Hay dos variantes del mismo concepto:

- **Feature 5 — Surface Neoweaving:** actúa sobre `erTopSolidInfill` (la última capa de la superficie superior)
- **Feature 6 — Infill Neoweaving:** actúa sobre `erInternalInfill` (el relleno interno disperso)

### Parámetros de configuración

**Quality > Neotko Neoweaving** (superficie superior, Feature 5):

| Parámetro | Key interna | Default | Descripción |
|-----------|-------------|---------|-------------|
| Habilitar | `interlayer_neoweave_enabled` | false | Master switch |
| Modo | `interlayer_neoweave_mode` | Wave | Wave o Linear |
| Amplitud Z | `interlayer_neoweave_amplitude` | 0.10 mm | Cuánto sube/baja la boquilla |
| Período | `interlayer_neoweave_period` | 0 (auto) | Longitud de un ciclo; 0 = usar line width |
| Velocidad Z máx | `interlayer_neoweave_max_z_speed` | 20 mm/s | Límite de velocidad del eje Z |
| Longitud mínima | `interlayer_neoweave_min_length` | 3.0 mm | Líneas más cortas no se weave |

**Strength > Neotko Infill Neoweaving** (relleno disperso, Feature 6):

| Parámetro | Key interna | Default | Descripción |
|-----------|-------------|---------|-------------|
| Override por objeto | `infill_neoweave_enabled` | Inherit | Tristate (ver abajo) |
| Amplitud Z | `infill_neoweave_amplitude` | 0.10 mm | |
| Período | `infill_neoweave_period` | 0 (auto) | |
| Velocidad Z máx | `infill_neoweave_max_z_speed` | 20 mm/s | |

**Por qué `infill_neoweave_enabled` es un enum tristate y no un bool:**  
Si fuera `bool`, un valor `false` sería indistinguible de "el usuario no ha configurado nada" (sin override). Con el tristate `InfillNeoweaveOverride::Inherit/Enable/Disable`, el override por objeto puede forzar activación o desactivación independientemente del preset global.

### Modo Wave — Onda continua ✅ Validado en hardware

La boquilla sigue una **onda triangular continua** a lo largo del path completo. El camino se subdivide en micro-segmentos (mínimo 8 por período) para que la onda quede suave.

**Cálculo de la Z en cada micro-segmento:**

```
period = interlayer_neoweave_period  (o line_width si es 0)
phase  = fmod(dist_recorrida / period, 1.0) × 4   ← valor 0..4

si phase < 1:  z_off = amplitude × phase           ← subiendo
si phase < 3:  z_off = amplitude × (2 - phase)     ← bajando
si no:         z_off = amplitude × (phase - 4)     ← subiendo de nuevo
```

La boquilla dibuja picos y valles de forma continua mientras extruye.

**Límite de velocidad XY (Wave mode):**  
Para que el eje Z pueda seguir la onda sin perder pasos, la velocidad XY se limita globalmente:

```
XY_speed_max = max_z_speed × period / (4 × amplitude)
```

Esto garantiza que la velocidad vertical real nunca supere `max_z_speed`. Al final del path se restaura la velocidad original.

**Mejora de Wave mode en la sesión 9 (Smoothed Sine Wave):**  
En esta sesión se actualizó el G-code generado para que la trayectoria dibuje una onda sinusoidal matemáticamente precisa en lugar de un triángulo con puntas duras. El uso de `sin(...)` y `cos(...)` permite que el cabezal trace el movimiento suavemente sin saturar la cola de lookahead del firmware, eliminando los parones erráticos, los saltos (stutters) y suavizando significativamente la aceleración vertical.
Además, se optimizó el cálculo de derivadas para permitir una mezcla perfecta con el planificador de la impresora.

**Nota importante sobre el modo Wave:** La implementación actual aún tiene limitadores de velocidad XY que podrian recalcularse por iteración.

### Modo Linear — Zigzag plano por línea ⚠️ Implementado, pendiente de test en hardware

En el modo Linear, **cada línea se imprime completamente plana a una Z fija**. La boquilla no se mueve en Z durante la extrusión — el cambio de altura ocurre en el travel entre líneas (movimiento Z puro sin extrusión).

**Por qué esta arquitectura:**  
En muchas impresoras (especialmente Cartesianas), mover Z mientras se extruye crea artefactos porque el eje Z es lento o tiene demasiada inercia. Con el modo Linear, Z solo se mueve en los travels (sin filamento en movimiento), y la extrusión siempre es perfectamente plana.

**Cómo funciona el zigzag entre líneas:**

```
Capa IMPAR (m_layer_index % 2 == 0):
   Línea 0 → Z = nominal + 0      (a la Z nominal de la capa)
   Línea 1 → Z = nominal + A      (elevada A milímetros)
   Línea 2 → Z = nominal + 0
   Línea 3 → Z = nominal + A
   ...

Capa PAR (m_layer_index % 2 == 1):
   Línea 0 → Z = nominal + A      (empieza elevada)
   Línea 1 → Z = nominal + 0
   Línea 2 → Z = nominal + A
   ...
```

**Cambio de diseño en sesión 7 (de ±A a 0/+A):**  
La primera implementación usaba `nominal - A` y `nominal + A` (simétrico alrededor de la Z nominal). Se cambió a `nominal + 0` y `nominal + A` porque con ±A, cuando dos objetos en la misma cama comparten Z, los viajes en negativo de un objeto pueden cruzarse visualmente con el positivo de otro, creando un patrón de interferencia (efecto moiré). Con 0/+A, ninguna línea va por debajo de la Z nominal de la capa, eliminando la colisión.

**Secuencia exacta por línea:**
1. `travel_to_z(z_destino)` — movimiento Z puro, sin extrusión
2. `extrude_to_xy(punto_final, dE)` — extrusión completamente plana a esa Z

**Coordinación inter-capa (el corazón del sistema):**  
La paridad del índice de capa (`m_layer_index % 2`) invierte la fase del zigzag. Donde la capa N pone una línea en `+A`, la capa N+1 pone su línea en `+0` — exactamente en el hueco. Las líneas de capas consecutivas se interdigitan en Z.

```cpp
// GCode.cpp — lógica de paridad
const bool layer_flip   = (m_layer_index % 2 != 0);
const bool line_is_even = (weave_line_idx % 2 == 0);

// XOR: si la capa y la línea tienen la misma paridad → z_off positivo
const double z_off = ((line_is_even != layer_flip)
    ? weave_amplitude   // línea elevada
    : 0.0);             // línea a Z nominal

gcode += m_writer.travel_to_z(m_nominal_z + z_off, "Neoweaving: line Z");
gcode += m_writer.extrude_to_xy(point_to_gcode(line.b), dE, "Neoweaving linear");
```

**Bug crítico resuelto — bypass de arc fitting (sesión 7):**  
Todo el código de neoweaving estaba dentro de un bloque `if (!enable_arc_fitting || ...)`. Con arc fitting activado (el caso normal en OrcaSlicer), ese bloque **nunca se ejecutaba**. El único movimiento Z que aparecía en el GCode era el lift anti-scratch de OrcaSlicer, no neoweave.

Fix: calcular `any_neoweave` **antes** del if-block y añadirlo como condición:

```cpp
const bool any_neoweave = weave_active_pre || infill_weave_active_pre;
if (!m_config.enable_arc_fitting
    || path.polyline.fitting_result.empty()
    || m_config.spiral_mode
    || sloped != nullptr
    || any_neoweave)      // ← fuerza rama G1 cuando neoweave está activo
{
    // ... código de neoweaving
}
```

**Bug crítico resuelto — bug de paridad con relleno monotónico (sesión 7):**  
El relleno monotónico es una polilínea conectada, no líneas individuales. Los segmentos cortos de conexión entre líneas paralelas también incrementaban `weave_line_idx`, invirtiendo la paridad de todas las líneas largas (todas acababan en la misma Z en lugar de alternar).

Fix: filtro automático con `max(min_length_usuario, 2 × line_width)`. Los conectores (siempre < 1 line_width) quedan filtrados incluso si el usuario pone `min_length = 0`.

**Restauración al final de cada path:**
```
travel_to_z(nominal_z)    // vuelve a la Z exacta de la capa
set_speed(F)              // restaura velocidad (solo Wave mode)
```

### Soporte de erSolidInfill en modo Linear

En sesión 7 se extendió `weave_active` para incluir `erSolidInfill` (la capa de solid infill inmediatamente debajo del top surface) cuando el modo es Linear. Esto permite que esa capa también participe en la interdigitación Z, mejorando la adherencia entre el solid infill y la superficie.

**Nota:** El enum correcto en OrcaSlicer para el solid infill interno es `erSolidInfill`, no `erInternalSolidInfill` (que no existe).

### Lock de ángulo de solid infill para Linear mode

Para que la interdigitación Z funcione correctamente, la capa de solid infill debajo del top surface debe tener el mismo ángulo de relleno que el top surface. OrcaSlicer por defecto rota el ángulo en cada capa.

**Solución:** En `FillBase.cpp`, cuando neoweave Linear está activo, se suprime la rotación de ángulo para superficies `stInternalSolid`:

```cpp
// Leer los flags de neoweave via option() dinámico (sin nueva key en el struct)
const ConfigOption* opt_en   = print_object_config->option("interlayer_neoweave_enabled");
const ConfigOption* opt_mode = print_object_config->option("interlayer_neoweave_mode");
if (opt_en && opt_en->getBool()) {
    const auto* mode_opt = dynamic_cast<const ConfigOptionEnumGeneric*>(opt_mode);
    if (mode_opt && mode_opt->value == 1 /* Linear */)
        neoweave_lock_angle = true;
}
if (!is_using_template_angle && !neoweave_lock_angle)
    out_angle += this->_layer_angle(...);
```

**Por qué `option()` dinámico en lugar de nueva key:**  
Las keys `interlayer_neoweave_*` son `PrintRegionConfig`, y en `FillBase.cpp` solo hay acceso a `print_object_config` (`PrintObjectConfig`). Añadir una nueva key a `PrintObjectConfig` requiere modificar la macro del struct y registrar la key en `PrintConfigDef` — si hay cualquier desajuste entre `.hpp` y `.cpp` durante compilaciones incrementales, `initialize_cache()` crashea al arranque. Usar `option()` dinámico evita por completo ese riesgo.

---

## Feature 7 — Preservación del origen del STL ✅ Validado

### El problema
Al importar un fichero STL o 3MF en OrcaSlicer, el objeto siempre aparecía centrado en la cama, ignorando las coordenadas XY del fichero original. Esto rompía flujos de trabajo donde múltiples piezas están posicionadas intencionalmente entre sí en el fichero fuente.

### La causa raíz
`ModelObject::rotate()` llama internamente a `center_around_origin()` **siempre**, incluso cuando el ángulo de rotación es 0 grados. El loop de pre-carga en Plater ejecutaba `obj->rotate(0, Z)` sobre cada objeto importado — efectivamente llamando `center_around_origin()` sin que ningún usuario lo hubiera pedido, destruyendo la posición XY antes de que `load_model_objects()` pudiera capturarla.

### La solución
```cpp
// Antes: siempre rotaba (y por tanto centraba)
obj->rotate(preferred_orientation, Z_axis);

// Después: solo rota si hay un ángulo real que aplicar
if (std::abs(preferred_orientation) > 1e-9)
    obj->rotate(preferred_orientation, Z_axis);
```

Y el autocentrado solo se aplica cuando el offset es exactamente `(0.0, 0.0)` — es decir, cuando el objeto realmente está en el origen y necesita ser colocado en la cama.

**Casos validados:** 1 STL solo, N STLs sin agrupar, N STLs agrupados, STL cuyo origen es (0,0) del modelo.

---

## Feature 8 — Temporal Link (grupos de movimiento conjunto) ✅ Validado

### Qué hace
Permite agrupar objetos para que se muevan juntos en XYZ en la UI **sin afectar al slicing**. Es útil para piezas que tienen una relación posicional definida (ej: parte superior e inferior de una carcasa, pieza y su soporte personalizado).

A diferencia de los grupos de OrcaSlicer (que afectan al slicing), el Temporal Link es puramente de viewport — los objetos vinculados mantienen su configuración de slicing independiente.

### Arquitectura

**Núcleo del modelo:**
- `int link_group_id { 0 }` en `ModelObject` — el ID de grupo (0 = sin grupo)
- Serializado en cereal para persistencia en memoria

**Estado en runtime (Plater::priv):**
- `m_link_groups` — mapa de group_id → lista de objetos
- `m_next_link_group_id` — contador para nuevos grupos
- `m_pre_move_offsets` — posiciones antes de un movimiento (para calcular deltas)
- `m_syncing_links` — flag para evitar recursión infinita al propagar movimientos
- `m_floating_z_guard` — defensa contra el bug de snap a Z=0 (ver Bug 10)

**Persistencia en 3MF:**  
Las funciones de guardar 3MF (`store_bbs_3mf`, `read_from_archive`) son propias del fork BBS y están fuera del patch set. La solución es codificar el grupo como prefijo en el nombre del objeto: `[L1]Nombre del objeto`. Al cargar, `rebuild_link_groups_from_model()` parsea los prefijos y reconstruye los grupos. También se guarda como metadata explícita en el formato Prusa 3MF via `3mf.cpp`.

**Protección contra Re-entrada (Sync Lock):**  
En la sesión 9 se identificó un problema de "UI Cascading Updates" donde actualizar un grupo desencadenaba llamadas recursivas de texto (wxEVT_TEXT) al rotar o transformar. Se mitigó inyectando un booleano de guardia `m_syncing_group` directamente en `Plater::changed_object` que corta de raíz la reacción en cadena de la GUI.

---

## Bug 9 — Crash al arrancar por icono faltante ✅ Resuelto

`page->new_optgroup(L("Neotko Neoweaving"))` fue llamado sin el parámetro de icono requerido por la versión Snapmaker de OrcaSlicer. Fix: añadir el parámetro vacío correcto en `Tab.cpp`.

---

## Bug 10 — Objetos flotantes snapeados a Z=0 ✅ Resuelto

### El problema
Objetos que legítimamente flotaban sobre la cama (z > 0) eran arrastrados a Z=0 cuando se movían en la vista 3D.

### La causa
El código de snap-to-bed en `GLCanvas3D.cpp` usaba:
```cpp
if (shift_z != 0.0)  // ← incorrecto: también "corregía" objetos flotantes
    obj->translate(0, 0, -shift_z);
```

### La solución
```cpp
if (shift_z < 0.0)   // ← correcto: solo sube objetos que están debajo de la cama
    obj->translate(0, 0, -shift_z);
```

Este cambio se aplica en 4 ubicaciones de `GLCanvas3D.cpp`. La defensa secundaria `m_floating_z_guard` en Plater usa `wxCallAfter` para detectar y revertir snaps incorrectos que ocurren fuera del path de movimiento directo.

---

## Feature 11 — Indicadores visuales de Temporal Link ✅ Validado

En la lista de objetos, los objetos vinculados muestran un prefijo `[L1]`, `[L2]`, etc. coloreado según el grupo. La paleta usa 5 colores cíclicos para distinguir grupos diferentes. `refresh_link_names()` se llama tras cada operación de link/break/rebuild para mantener los labels actualizados.

---

## Feature 12 — Copy/paste propaga el grupo completo ✅ Validado

Al copiar un objeto que pertenece a un Temporal Link, `Selection::copy_to_clipboard()` expande la selección para incluir todos los objetos del mismo grupo. Al pegar, `Plater::regroup_pasted_link_objects()` asigna nuevos IDs de grupo a las copias para que los objetos pegados formen su propio grupo independiente.

---

## Feature 13 — Aplicar preset de proceso a objetos seleccionados ✅ Validado

### Qué hace
Un botón en la barra superior del panel de proceso (ParamsPanel) abre un combo con todos los **User Presets** (no presets del sistema ni presets por defecto) y aplica sus valores al objeto seleccionado como overrides por objeto.

### Por qué solo User Presets
Los presets de sistema pueden contener valores para parámetros de máquina (multi-material, etc.) que no tienen sentido como overrides por objeto. Filtrado con `!p.is_system && !p.is_default`.

### Qué keys se aplican
Solo las keys válidas como override por objeto: `PrintObjectConfig().keys()` + `PrintRegionConfig().keys()`. Las keys de `PrintConfig` (máquina) se excluyen. También se excluyen las keys MM (multi-material) que podrían causar problemas.

**Nota técnica:** `Preset::print_options()` devuelve la lista de keys de un preset de impresión, que es más amplia que las keys válidas per-objeto. Usar `print_options()` directo causaría que se intentaran aplicar keys inválidas como overrides. Siempre usar `PrintObjectConfig().keys() + PrintRegionConfig().keys()`.

---

## Feature 14 — Monotonic Interlayer Nesting ✅ Validado

### Qué hace
Desplaza las líneas del relleno monotónico del top surface media línea perpendicular en capas alternas. Las líneas de la capa N caen en los huecos de la capa N-1, mejorando la cohesión de la superficie sin cambiar la Z (a diferencia de Neoweaving).

**UI:** Strength > Top/bottom shells > "Monotonic interlayer nesting"

### Condiciones de activación
- `monotonic_interlayer_fill == true`
- El fill es de tipo `FillMonotonic` (top surface pattern = Monotonic)
- El rol es `erTopSolidInfill`
- El `layer_id` es impar (alterna entre capas)

### Implementación
En `FillBase.cpp`, el punto de origen del fill se desplaza medio espaciado perpendicular a la dirección de relleno:

```cpp
const coord_t half_sp = coord_t(scale_(this->spacing) * 0.5);
fill_dir.second += Point(
    coord_t(-std::sin(fill_dir.first) * double(half_sp)),
    coord_t( std::cos(fill_dir.first) * double(half_sp)));
```

### Sinergia con Neoweaving Linear
Activar ambas features produce una interdigitación tridimensional completa:
- Feature 14 desplaza las líneas en XY (medio espaciado perpendicular)
- Neoweaving Linear las alterna en Z (entre nominal y +amplitud)

El resultado es que cada línea de la capa N queda físicamente rodeada por las líneas de la capa N-1 en los tres ejes.

---

## Feature 15 — Save Object Config as Preset (sesión 7)

### Qué hace
Un botón de upload en la barra superior del ParamsPanel abre un diálogo de texto para introducir el nombre del nuevo preset. El sistema:
1. Clona el preset de impresión activo como base
2. Aplica encima los overrides por objeto del objeto seleccionado (mismas safe_keys que el Apply)
3. Guarda el resultado como nuevo User Preset desvinculado del padre
4. Recarga el combo de presets para reflejarlo inmediatamente

### Implementación
`Plater::save_selected_object_config_as_preset(const std::string& preset_name)`:
- `prints.get_edited_preset()` obtiene el preset activo (es una referencia, no puntero)
- Copia del preset como `Preset tmp`
- Aplica overrides del `ModelObject::config` del objeto seleccionado
- `prints.save_current_preset(name, detach=true, save_to_project=false, &tmp, nullptr)`

---

## Feature 16 — Panel de proceso en la derecha de la UI (sesión 8)

### Qué hace
Mueve el panel de parámetros de proceso (la zona marcada en fucsia en la imagen de referencia) del sidebar izquierdo a un pane independiente en el lado derecho de la ventana. El panel se puede mostrar/ocultar independientemente del sidebar.

### Arquitectura (wxAuiManager)
OrcaSlicer usa `wxAuiManager` para gestionar su layout. Antes, el `ParamsPanel` se reparentaba dentro del `wxScrolledWindow` del Sidebar. Ahora:

1. Se crea un `right_container` (wxPanel) que envuelve `params_panel->get_top_panel()` + el `ParamsPanel` completo
2. Se añade como pane `Right` al AUI manager (fijo, no flotable, no movible)
3. La función `toggle_process_panel()` en `Plater::priv` invierte `m_process_panel_shown` y llama `pane.Show()/Hide()` + `m_aui_mgr.Update()`

### Formas de toggle
- **Botón en el canvas** — segundo ítem de la `collapse_toolbar` existente (la que contiene el botón de colapsar sidebar), con icono `expand.svg`
- **Botón en el propio panel** — `ScalableButton` con icono `sidebar` en el extremo izquierdo del top bar del ParamsPanel. Permite ocultar el panel incluso cuando el sidebar está colapsado.

**Persistencia de estado (Sesión 9):**
El estado visible/oculto del lado derecho ahora se persiste correctamente entre sesiones usando `wxGetApp().app_config->set("process_panel_visible")` desde la clase `Plater`.

---

## Feature 17 & 18 — Z-Preset Regions & Z-Overrides (Sesión 9)

### Feature 17: Z-Preset Regions Persistence & GUI
**Qué hace:** 
Añade una interfaz para regiones Z preestablecidas que recuerda sus etiquetas personalizadas ("Zone Label"). Añade el grupo "Cooling & Temp" con control interactivo directamente asimilado del sistema de filamentos.

### Feature 18: Inyección Z-Override en GCode (Hook de Temperatura y Ventilador)
**Qué hace:**  
En `GCode.cpp` (`process_layer`), ahora OrcaSlicer inyecta inteligentemente comandos GCode de control ambiental cuando cruzamos un límite de preset configurado:
- Monitoreo en tiempo real de los umbrales de altura Z del bloque.
- Emisión inmediata de comandos de temperatura: `M104` o `M109`.
- Emisión de configuraciones de ventilador: controlando el cooling con `M106` y `M107` acorde al "Z Override".

---

## Estado de todas las features

| Feature | Nombre | Estado |
|---------|--------|--------|
| 3 | disable_bridge_infill | ✅ Validado |
| 4 | Top/Bottom density UI | ✅ Validado |
| 5 | Neoweaving superficie — modo Wave | ✅ Validado en hardware |
| 5 | Neoweaving superficie — modo Linear | ⚠️ Implementado, pendiente hardware |
| 6 | Neoweaving infill (tristate override) | ✅ Validado |
| 7 | Preservación origen STL | ✅ Validado |
| 8 | Temporal Link — núcleo | ✅ Validado |
| Bug 9 | Crash icono faltante al arrancar | ✅ Resuelto |
| Bug 10 | Objetos flotantes snapeados a Z=0 | ✅ Resuelto |
| 11 | Temporal Link — indicadores visuales [Ln] | ✅ Validado |
| 12 | Temporal Link — copy/paste propaga grupo | ✅ Validado |
| 13 | Apply Process Preset a objeto | ✅ Validado |
| 14 | Monotonic Interlayer Nesting | ✅ Validado |
| 15 | Save Object Config as Preset | ✅ Implementado |
| 16 | Panel de proceso a la derecha (toggle) | ✅ Implementado |
| — | Temporal Link — persistencia 3MF | ✅ Validado |

---

## Trabajo pendiente / Mejoras conocidas

### Neoweaving Wave — Limitaciones Restantes
La implementación sinusoide mitigó la mayoría del stuttering, pero quedan limitaciones secundarias:
1. **Velocidad XY global:** El límite de velocidad se aplica a todo el path. Debería calcularse por micro-segmento.
2. **Compensación de presión:** Los cambios de trayectoria en Z durante la extrusión deberían coordinarse con pressure advance (PA).
3. **Impresoras sin CoreXZ:** En impresoras de cama Z (Cartesianas), esto puede seguir siendo impreciso.

---

## Notas técnicas críticas para futuros desarrollos

### Sobre el sistema de configuración
- Las keys `interlayer_neoweave_*` e `infill_neoweave_*` son `PrintRegionConfig` → accesibles en `GCode.cpp` via `m_config` (que es `FullPrintConfig`, hereda todo)
- `neoweave_lock_solid_infill_angle` fue eliminada como key registrada para evitar el crash de `initialize_cache()`. Ahora se implementa via `option()` dinámico en `FillBase.cpp`
- El crash "Unhandled unknown exception" justo después de "Initializing StaticPrintConfigs" **siempre** es una key en la macro del struct que no está registrada en `PrintConfigDef`, o viceversa

### Sobre el Sidebar y el AUI layout
- El Sidebar usa `wxAuiManager` (pane `Left`)
- El nuevo panel de proceso usa `wxAuiManager` (pane `Right`)
- Los splitters/separadores del sidebar original ya no son necesarios para el ParamsPanel
- `StaticBox*` para paneles con `SetBackgroundColor()` — no `wxPanel*`

### Sobre presets y overrides
- `prints.get_edited_preset()` devuelve referencia, no puntero (no existe `get_edited_preset_ptr()`)
- `save_current_preset(name, detach=true, ...)` corta el vínculo padre del preset clonado
- Para filtrar User Presets: `!p.is_system && !p.is_default`

### Sobre el Temporal Link
- `ModelObject::rotate()` **siempre** llama `center_around_origin()` — proteger con `if (abs(angle) > 1e-9)`
- La sincronización de movimientos usa el flag `m_syncing_links` para evitar recursión infinita

---

## Comandos de build

```bash
# Build completo (desde cero)
./build_release_macos.sh -x

# Build incremental (reutiliza deps compiladas)
./build_release_macos.sh -s -x
```

Los ficheros del ZIP se copian sobre el source tree del repositorio y se compila normalmente. No hay scripts de parcheo — los ficheros del ZIP reemplazan directamente a los originales en `src/`.
