# Plan de Implementación de Mejoras (OrcaSlicer FullSpectrum)

Este plan detalla los pasos para resolver todas las deficiencias detectadas y añadir las mejoras solicitadas.

## Proposed Changes

### 1. Z-Override Regions: Persistencia de Etiquetas
Aprovecharemos que `z_preset_name` es de tipo [string](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/Model.cpp#2906-2920) para guardar el nombre de la zona en su valor.
#### [MODIFY] [src/slic3r/GUI/ZPresetRegions.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp)
- En [populate_from_model](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp#171-208): leer el valor de `cfg.option(KEY_Z_PRESET_NAME)`. Si no está vacío, asignarlo a `row.label`.
- En [apply_to_model](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/ZPresetRegions.cpp#713-754): en lugar de guardar un string vacío `""` en `KEY_Z_PRESET_NAME`, guardar `row.label` para que persista al guardar y cargar de 3MF.

### 2. Z-Override Regions: Persistencia del Panel Lateral Derecho
#### [MODIFY] [src/slic3r/GUI/Plater.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp) (o MainFrame)
- Localizar la función donde se oculta o muestra el nuevo panel de procesos derecho ([toggle_process_panel](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#6904-6926)).
- Configurar una directiva en `app_config` (ej: `show_right_process_panel`) que se grabe al cambiar el estado.
- Leer esta propiedad al arrancar la UI y establecer la visibilidad del panel de `wxAuiManager` en consonancia.

### 3. Z-Override Regions: Hook de GCode (Fan/Temperatura)
Los Z-Presets actualmente aplican overrides de perímetro/infill que lee Slic3r durante el pathing, pero los cambios de estado directos de máquina (Ventilador y Temperatura) necesitan ser secuenciados.
#### [MODIFY] [src/libslic3r/GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp)
- En `process_layer()` o la evaluación del Z actual, detectar qué rango de `layer_config_ranges` (con z_preset_name) está activo.
- Si cambió respecto a la capa anterior, comprobar si se está modificando temperatura (ej. `first_layer_temperature`, `temperature`) o ventilador (`fan_always_on`, `min_fan_speed`).
- Inyectar las cadenas correspondientes en el buffer del GCode (`writer.set_temperature()`, `writer.set_fan()`).

### 4. Neoweaving: Smooth Wave / Arc Fitting
La oscilación en zigzag daña el rendimiento cinemático de ciertas impresoras.
#### [MODIFY] [src/libslic3r/GCode.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/libslic3r/GCode.cpp)
- Localizar la lógica de `NeoweaveMode::Wave`.
- Generar micro-segmentos que aproximen un [sin(t)](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp#17454-17458) localizando crestas y valles suaves (splines discretizadas) en lugar de triángulos, reduciendo las esquinas de alto jerk y previniendo la bajada de velocidad global de XY.

### 5. Temporal Link: Prevenir colapsos en GUI (Re-entrancy)
#### [MODIFY] [src/slic3r/GUI/Plater.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Plater.cpp) o [Selection.cpp](file:///Users/sebastiansuchowolskimorelli/Downloads/OrcaSlicer-FullSpectrum-patches-79/src/slic3r/GUI/Selection.cpp)
- Cuando se altera (escala, mueve, rota) un objeto dentro de un `link_group_id`, el evento se propaga a los hermanos. Crear un `bool m_is_syncing_group` que actúe como semáforo para evitar que las notificaciones de los hermanos secundarios disparen más ciclos.

## Verification Plan
1. **Compilar y arrancar la UI.**
2. **Validar Z-Presets GUI**: Crear 3 zonas Z, renombrarlas. Guardar en formato 3MF (Project). Al reabrir el 3MF, las etiquetas deben coincidir.
3. **Validar Panel**: Activar panel derecho, cerrar Orca, reabrir y comprobar que se mantiene abierto.
4. **Validar Temporal Link**: Crear 3 instancias clonadas, vincularlas, escalarlas y rotarlas arrastrando el ratón en la UI. No debe producirse lag ni loops infinitos.
5. **Validar Neoweave y GCode**: Slicear cubos de prueba con Z-preset de distinta temperatura y/o Neoweaving activo y buscar visualmente en el GCODE o Gcode Viewer si los vectores ahora son más curvos y si aparecen las inyecciones de calor `M104`/`M106`.
