# OrcaSlicer FullSpectrum - Análisis de Código y Mejoras

He analizado a fondo los ficheros clave de tu fork de OrcaSlicer centrados en las funcionalidades descritas. A continuación tienes el informe detallado de cómo están implementadas, qué fallos potenciales o "gotchas" existen, y qué deberíamos mejorar a continuación.

## 1. Z-Override Regions (Z-Preset Regions)
La característica usa un enfoque muy inteligente pero a la vez frágil: reutilizar `ModelObject::layer_config_ranges` pero inyectando una clave centinela "fantasma" (`z_preset_name`).

### Puntos débiles detectados:
1. **Pérdida al pintar (Height Modifier/Color Painting)**: Si el usuario usa los modificadores de altura nativos que operan sobre la misma variable `layer_config_ranges`, tus rangos Z-Preset pueden ser sobrescritos involuntariamente en [ModelObject](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Model.hpp#593-609), especialmente al calcular perfiles de altura donde el slicer "funde" mapas. 
2. **Crash al borrar todas las zonas**: En [ZPresetRegions.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp) a la hora de borrar el último elemento (On_Del), limpias todo del `layer_config_ranges` que tenga `KEY_Z_PRESET_NAME`. Esto es correcto, pero hay que asegurarse de que [invalidate_step()](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Print.cpp#385-393) se llama correctamente en [Plater](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#13112-13120) para volver a rebanar la pieza. He visto que llamas a [changed_object(obj_idx)](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#18191-18213) lo cual está bien, pero el canvas necesita imperativamente redibujar la pérdida del highlight 3D (se llama a `clear_z_band_highlight()`, lo que es correcto).
3. **Persistencia (Bugs detectados por ti)**: El mayor problema. La persistencia de la etiqueta de la zona no ha sido implementada al guardarla en 3MF. Las *label* están en el objeto del GUI (las *Rows*) pero al reconstruirlo en [populate_from_model](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp#171-208) se les hardcodea *"Zone N"*.
4. **Validación de Capas**: Modificaste `ModelObject::has_custom_layering()` para ignorar los Z-Presets, lo cual permite usar la Prime Tower y Timelapses sin que se desactiven. Esto es una muy buena aproximación.

### Mejoras a realizar:
- **Guardado de Labels en [ModelConfig](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Model.hpp#72-103):** Al estar usando variables dinámicas en el mapa, habría que guardar algo como `z_preset_label=Zone Name` en el propio Override.
- **Arreglar el panel lateral**: Como mencionaste, al cerrar y abrir sesión no se guarda la visibilidad del panel derecho. Esto requiere engancharlo al `wxGetApp().app_config` en el evento `wxEVT_CLOSE_WINDOW` del Frame principal (`MainFrame.cpp`), capturando su estado (`IsShown()`).
- **Hook de inyección GCode**: Nos falta crear el Hook en [GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp) que detecte el tránsito por un `z_max` e inyecte un cambio de fan `M106` o temperatura `M104`, leyendo del rango activo.

---

## 2. Neoweaving (Interlayer y Lineal)
Esta funcionalidad interrumpe y modifica asunciones básicas de generación de paths tanto de infill y perímetros como la inyección final del GCode.

### Puntos débiles detectados:
1. **Tristate vs Bool**: `infill_neoweave_enabled` está programado como TristateEnum mientras que el interlayer general es un booleano. En `GCode.cpp:6883`, se revisa la activación de infill solo si el enum es explícitamente `Enable`. Si un objeto no lo define explícitamente, heredará de `PrintConfig` pero puede que el *casting* inicial no ocurra adecuadamente antes del loop principal por cada Extrusion Entity.
2. **Fricción cinemática (Velocidad global / Gcode redundante)**: El modo *Wave* en `GCode.cpp:7042` subdivide la línea en múltiples micro-segmentos triangulares (onda). Esto satura la cola de *lookahead* del firmware debido a que hay cambios en la derivada de `Z` constantemente para cada G1. Esto provocará que los limites de aceleración global de `Z` o `Jerk / Square Corner Velocity` bloqueen la velocidad en `XY`. No puedes ir a más velocidad `XY` de lo que la aceleración `Z` permite por cada micro-triángulo.

### Mejoras a realizar:
- **Algoritmo Sinusoidal o Cuadrático**: Cambiar el zigzag triangular de `Wave` a una ecuación *spline* o curva sinusoidal aproximada permite crear comandos de arco con `G2/G3` u optimizar los vectores para que el planificador cinemático de klipper/marlin fluya mejor.
- **Filtrado G-code verbo**: Necesitamos habilitar/ignorar `m_config.enable_arc_fitting` que vimos en [GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp) forzándolos a unirse (ArcFitting *Weld* en firmware).
- **Validación del modo Lineal**: Está implementado por debajo aislando `NeoweaveMode::Linear` y el límite de `interlayer_neoweave_min_length`, pero no has hecho pruebas físicas. Podemos crear un script para aislar un cubo de tests y comprobar este path.

---

## 3. Temporal Link (Sincronización de Grupo)
Para burlar el pipeline rígido has inyectado un `link_group_id` en [ModelObject](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Model.hpp#593-609).

### Puntos débiles detectados:
1. **Serialización 3MF**: En `3mf.cpp:3150` modificaste la rutina de parcheado inyectando en texto plano la metadata `[Lx]` en el nombre del objeto como fallback. Esto es muy *hacky*, ya que un nombre de objeto válido ahora colisionará si el usuario lo llama `[L1]Cubo`. Sin embargo, es astuto porque permite al BambuStudio u otra gente sin el fork abrir el 3MF conservando visualmente el grupo. En `format/3mf.cpp:927` recuperas perfectamente la metadata nativa XML, lo cual está bien hecho.
2. **Ciclos de evento duplicados**: En la GUI, cuando el `link_group_id` iguala varios objetos y rotas uno, el [Selection](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#404-405) manager o el GUI [Plater](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#13112-13120) pueden entrar en bucles de propagación. He comprobado que al pegar (`Selection::paste`), reasignas bien con `m_next_link_group_id`.

### Mejoras a realizar:
- Blindar el [changed_object(obj_idx)](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#18191-18213) en el Plater: cuando se escala o traslada un elemento en un *LinkGroup*, evitar el temido *re-entrant GUI update*. Podemos añadir un booleano de bloqueo temporal `m_syncing_group = true` durante el [for](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Model.cpp#2155-2165) de transformación grupal, para evitar notificar a los objetos intermedios hasta que se acaba de actualizar el último.

---

## Próximos pasos recomendados
¿Por dónde quieres que empecemos a escribir código?

1. **Persistencia y GUI**:
   - Guardar el nombre de las zonas (Zone Labels) para Z-Overrides de forma segura usando propiedades dentro de *DynamicPrintConfig*.
   - Guardar la visibilidad de tu panel especial Derecho.
2. **Neoweaving - Modos Avanzados y Limpieza**:
   - Modificar la onda triangular para evitar la sobrecarga del MCU.
   - Aislar parámetros de Presión Adicional (Pressure Advance variable para no romper las esquinas durante la inyección en Z).
3. **Inyección Z-Override Temperatura / Gcode (Hook 2)**:
   - Acabar el "Hook 2" documentado donde modificamos variables de ventilador / hotend en medio del Gcode basado en nuestros rangos fantasmas.
