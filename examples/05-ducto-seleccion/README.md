# Ejemplo 5 — Seleccion de diametro de ducto: tecnico vs economico

## Que calcula

El dimensionamiento de un ducto de ventilacion auxiliar
(`DuctSizingCalculator`) para transportar 1200 m3/min a traves de 400 m de
ducto flexible en espiral (`AirwayLining.DuctFlexibleSpiral`), con
velocidad maxima 25 m/s, bajo dos criterios de seleccion distintos:

- **Tecnico** (`DuctSizingCalculator.calculate`): entre los 8 diametros
  comerciales de catalogo (0.30 a 1.22 m), elige el **menor** que cumple la
  velocidad maxima.
- **Economico** (`DuctSizingCalculator.calculate_full`): entre los
  diametros viables (misma restriccion de velocidad), elige el de **menor
  costo total** (energia del ventilador + capital del ducto) sobre un
  horizonte de operacion, con tarifa 0.12 USD/kWh, costo de ducto 40
  USD/(m·m de diametro), 4000 h de operacion y eficiencia de ventilador
  0.65.

## No hay comando CLI para este caso (fuera de alcance de v1)

El CLI de este repo (`python/ventpy/cli.py`) expone 5 subcomandos:
`demanda`, `lmp`, `cobertura`, `red`, `ventilador`. Ninguno envuelve
`DuctSizingCalculator`: agregar un subcomando `ducto` requeriria decidir su
esquema de entrada/salida (¿un solo ducto o una comparacion tecnico/
economico como este ejemplo? ¿como se expone `EconomicParams`, que es
opcional?) — una decision de diseño de CLI que el brief de SP-5 dejo
explicitamente fuera de v1. Este ejemplo documenta el caso via API
directa hasta que ese subcomando exista.

## Salida esperada

| Criterio  | Diametro | Costo total    |
|-----------|----------|----------------|
| Tecnico   | 1.07 m   | ~137,712 USD   |
| Economico | 1.22 m   | ~82,101 USD    |

El diametro economico (1.22 m) es **mas grande** que el tecnico (1.07 m) —
NO es "el minimo que cumple", sino el de menor costo total — y sin
embargo cuesta **~40% menos** en el horizonte de operacion: un ducto mas
ancho reduce la velocidad del aire y, con ella, la caida de presion y el
consumo energetico del ventilador; ese ahorro de energia domina sobre el
mayor costo de capital del ducto mas ancho.

## Fundamento (bibliografia de ingenieria, NO normativa)

La fisica de resistencia del ducto usa las tablas de Atkinson de
McPherson (2009), Cap. 5 (via `AtkinsonCalculator`, reusado internamente
por `DuctSizingCalculator`). La lista de diametros comerciales
(`{0.30, 0.40, 0.50, 0.60, 0.76, 0.91, 1.07, 1.22}` m) y la velocidad
maxima por defecto son **parametros ingenieriles del proyecto, NO
normativos** — documentados como tales en
`include/ventpy/ducto.hpp` (gate 2026-08-17).

## Ejecutar

```bash
python examples/05-ducto-seleccion/run.py
```

El script usa solo el API publico de `ventpy` (`DuctSizingParams`,
`EconomicParams`, `DuctSizingCalculator`) y termina en `assert`.
