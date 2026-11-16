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
  "q_diesel_m3min": 1929.0,
  "governing_factor": "diesel (Q_Eq)",
  "q_total_m3min": 2219.0
}
```

`q_total_m3min == 2219.0`. Gobierna el mismo sub-criterio que en el
Ejemplo 1 (dilucion de NOx del equipo diesel), pero cada norma fija su
propio limite de exposicion y el NOx se diluye al de la norma activa:

| | Peru (Ejemplo 1) | Chile (este ejemplo) |
|---|---|---|
| Limite de 8 h de NO2 | 3 ppm (DS 024, Anexo 15) | 2,6 ppm (DS 594, Art. 66, LPP) |
| Dilucion de NOx | 990.28 × 1.6876 = 1671.19 | 990.28 × 3 / 2.6 × 1.6876 = 1928.30 |
| `Q_Eq` | 1672 | 1929 |
| `Q_total` = ceil(`Q_Eq` × 1.15) | 1923 | 2219 |

(990.28 m3/min es el aire que diluye el NOx del scoop a 3 ppm antes de la
correccion volumetrica a 4200 msnm; la derivacion esta en el Ejemplo 1.)

Con 12 m2 de seccion, 2219 m3/min dan 161 m/min en el frente: mas que la
velocidad promedio maxima de 150 m/min del **DS 132, Art. 138 inc. 3**. El
Governor lo advierte ("ampliar la seccion o repartir el caudal") sin
recortar el caudal requerido.

Hasta la 0.2.1 los dos ejemplos daban 981 m3/min: el NOx se diluia a 5 ppm
en cualquier norma y se aplicaba una simultaneidad de 0.85. Ver
`CHANGELOG.md`.

## Que cambia con el preset chileno

`RegulatoryConfig.for_standard(DS132_Chile)` (`include/ventpy/normativa.hpp`,
`RegulatoryConfig::chile()`) fija:

- **Art. 132 (DS 132)**: factor diesel **2.83 m3/min/HP** por caballo de
  fuerza efectivo al freno (vs. 3.0 en Peru, Art. 252 lit. e y Anexo 38 del
  DS 024-2016-EM) — el minimo cuando el fabricante no especifica el caudal.
- **Art. 138 inc. 3 (DS 132)**: velocidad promedio entre 15 y 150 m/min.
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
