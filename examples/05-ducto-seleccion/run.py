"""Ejemplo 5 — Seleccion de diametro de ducto: criterio tecnico vs economico.

Usa SOLO el API publico de `ventpy` (`DuctSizingParams`, `EconomicParams`,
`DuctSizingCalculator.calculate` / `.calculate_full`). Sin CLI: el
subcomando `ventpy` de este repo (`demanda`/`lmp`/`cobertura`/`red`/
`ventilador`, ver `python/ventpy/cli.py`) todavia no expone
`DuctSizingCalculator` — queda fuera de alcance de v1 del CLI (ver
README.md de este ejemplo, seccion "Por que no hay CLI para este caso").

Caso: 1200 m3/min a traves de 400 m de ducto flexible en espiral
(`AirwayLining.DuctFlexibleSpiral`), velocidad maxima 25 m/s.

- Criterio TECNICO (`calculate`): elige el MENOR diametro comercial que
  cumple la velocidad maxima -> 1.07 m.
- Criterio ECONOMICO (`calculate_full`): entre los diametros viables, elige
  el de MENOR costo total (energia + capital) en el horizonte de operacion
  -> 1.22 m, un diametro MAYOR que el tecnico pero con ~40% menos costo
  total (menor caida de presion => menor consumo energetico del
  ventilador, que domina sobre el mayor costo de capital del ducto).

Bibliografia (NO normativa): fisica de resistencia via McPherson (2009)
Cap. 5 (Atkinson); lista de diametros comerciales y velocidad maxima
default son parametros ingenieriles del proyecto (NO normativos, ver
`include/ventpy/ducto.hpp`).
"""
import ventpy

params = ventpy.DuctSizingParams()
params.q_m3min = 1200.0
params.length_m = 400.0
params.duct_lining = ventpy.AirwayLining.DuctFlexibleSpiral
params.max_velocity_mps = 25.0

atm = ventpy.AtmosphericParams()

# --- Criterio tecnico: menor diametro comercial que cumple v <= vmax ---
technical = ventpy.DuctSizingCalculator.calculate(params, atm)

print(f"[tecnico]  selected_diameter_m = {technical.selected_diameter_m}")
print(f"[tecnico]  feasible             = {technical.feasible}")
print(f"[tecnico]  selection_criterion  = {technical.selection_criterion}")

assert technical.feasible is True
assert technical.selected_diameter_m == 1.07

# --- Criterio economico: costo total (energia + capital) minimo entre los
# diametros viables ---
economics = ventpy.EconomicParams()
economics.energy_cost_per_kwh = 0.12
economics.duct_cost_per_m_per_m_diam = 40.0
economics.operating_hours = 4000.0
economics.fan_efficiency = 0.65

economic = ventpy.DuctSizingCalculator.calculate_full(params, atm, economics)

print(f"[economico] selected_diameter_m = {economic.selected_diameter_m}")
print(f"[economico] feasible             = {economic.feasible}")
print(f"[economico] selection_criterion  = {economic.selection_criterion}")

assert economic.feasible is True
assert economic.selected_diameter_m == 1.22
assert "economico" in economic.selection_criterion

# --- El diametro economico (1.22) es mas grande que el tecnico (1.07),
# pero su costo total es menor: ~40% de ahorro sobre el costo total del
# diametro tecnico ---
cost_by_diameter = {round(o.diameter_m, 2): o.total_cost for o in economic.options}
technical_total_cost = cost_by_diameter[1.07]
economic_total_cost = cost_by_diameter[1.22]
savings_ratio = 1.0 - (economic_total_cost / technical_total_cost)

print(f"costo total tecnico (1.07 m)   = {technical_total_cost:.2f} USD")
print(f"costo total economico (1.22 m) = {economic_total_cost:.2f} USD")
print(f"ahorro                          = {savings_ratio * 100.0:.1f} %")

assert economic_total_cost < technical_total_cost
assert 0.35 <= savings_ratio <= 0.45  # ~40% documentado en el README

print("OK: tecnico 1.07 m vs economico 1.22 m (~40% menos costo total)")
