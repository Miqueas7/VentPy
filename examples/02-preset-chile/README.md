# Ejemplo 2 — Mismo caso diesel-limitado, con el preset chileno (DS 132)

## Que calcula

La misma idea del Ejemplo 1 (demanda de un frente con equipo diesel a gran
altitud), pero con **10** trabajadores y usando el flag `--norma chile`, que
selecciona el preset `RegulatoryConfig.for_standard(RegulatoryStandard.DS132_Chile)`
en vez del `DS024_Peru` por defecto:

- 10 trabajadores, a 4200 msnm
- 1 scoop diesel de 150 HP (disponibilidad 0.85, utilizacion 0.70)
- sin voladura

## Comando CLI

```bash
ventpy demanda examples/02-preset-chile/input.json --norma chile --json
```

## Salida esperada (claves relevantes)

```json
{
  "standard": "DS132_Chile",
  "governing_factor": "diesel (Q_Eq)",
  "q_total_m3min": 981.0
}
```

`q_total_m3min == 981.0` — el mismo numero que el Ejemplo 1, pero por una
razon distinta a "coincidencia": en ambos casos gobierna el mismo
sub-criterio (dilucion de NOx del equipo diesel, ver Ejemplo 1), y ese
calculo no depende de la norma seleccionada — la fisica de emisiones del
motor es la misma con cualquier preset. Lo que SI cambia entre Peru y Chile
son los factores normativos de personal/diesel (ver abajo); en este caso en
particular, el diesel (Q_Eq) sigue siendo mayor que el personal (Q_Per) bajo
ambos presets, asi que el resultado final coincide.

## Que cambia con el preset chileno

`RegulatoryConfig.for_standard(DS132_Chile)` (`include/ventpy/normativa.hpp`,
`RegulatoryConfig::chile()`) fija:

- **Art. 132 (DS 132)**: factor diesel **2.83 m3/min/HP** (vs. 3.0 en Peru,
  Art. 246 de DS 024-2016-EM) — el minimo cuando el motor viene con
  convertidor catalitico.
- **Art. 138 (DS 132)**: **3.0 m3/min por persona**, y a diferencia del
  Art. 247 peruano (que escala a 4/5/6 m3/min sobre 1500/3000/4000 msnm),
  el DS 132 **NO escala este caudal por altitud** — los 3 umbrales de
  altitud quedan neutralizados al mismo valor (3.0 en los 4 tramos), de
  forma que `min_flow_per_person == flow_above_threshold_1 ==
  flow_above_threshold_2 == flow_above_threshold_3 == 3.0`.

## Ejecutar

```bash
python examples/02-preset-chile/run.py
```

El script construye el mismo `VentilationInput` que el CLI, pero
seleccionando el preset chileno via API (`RegulatoryConfig.for_standard`),
imprime los factores de `RegulatoryConfig` para dejar visible el "sin
escalon" del Art. 138, y termina en `assert`.
