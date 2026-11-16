"""Ejemplo 2 — Mismo escenario diesel-limitado, con el preset chileno DS 132.

Usa SOLO el API publico de `ventpy`: `RegulatoryConfig.for_standard(...)` con
`RegulatoryStandard.DS132_Chile` en vez del `RegulatoryConfig()` (Peru) por
defecto del Ejemplo 1. 10 trabajadores / 4200 msnm / scoop diesel 150 HP,
sin voladura. Gobierna el mismo criterio que en el Ejemplo 1 (dilucion de NOx
del equipo diesel, Q_Eq), pero al limite chileno: LPP de NO2 de 2,6 ppm
(DS 594, Art. 66), mas exigente que los 3 ppm peruanos. Q_total = 2219.

Equivalente por CLI:
    ventpy demanda examples/02-preset-chile/input.json --norma chile --json
"""
import ventpy

config = ventpy.RegulatoryConfig.for_standard(ventpy.RegulatoryStandard.DS132_Chile)
governor = ventpy.VentilationGovernor(config)

inp = ventpy.VentilationInput()
inp.num_workers = 10
inp.altitude_masl = 4200.0

fleet = ventpy.DieselFleet()
scoop = ventpy.DieselEquipment()
scoop.name = "Scoop ST7"
scoop.horsepower = 150
scoop.availability = 0.85
scoop.utilization = 0.70
fleet.add_equipment(scoop)
inp.diesel_fleet = fleet

result = governor.calculate_total_demand(inp)

print(f"standard          = {result.standard.name}")
print(f"q_total_m3min       = {result.q_total_m3min}")
print(f"governing_factor   = {result.governing_factor}")
print(f"diesel_hp_factor   = {config.diesel_hp_factor}")
print(f"min_flow_per_person = {config.min_flow_per_person}")
print(f"flow_above_t1/t2/t3 = "
      f"{config.flow_above_threshold_1}/{config.flow_above_threshold_2}/"
      f"{config.flow_above_threshold_3}")

# --- Gobierna el diesel por dilucion de NOx al LPP chileno de NO2:
# 990.28 m3/min a 3 ppm (Ejemplo 1) x 3/2.6 x 1.6876 = 1928.30 -> 1929;
# total = ceil(1929 x 1.15) = 2219 ---
assert result.standard == ventpy.RegulatoryStandard.DS132_Chile
assert result.diesel.target_nox_ppm == 2.6
assert result.q_diesel_m3min == 1929.0
assert result.q_total_m3min == 2219.0
assert result.governing_factor == "diesel (Q_Eq)"

# --- DS 132, Art. 138 inc. 3: velocidad promedio entre 15 y 150 m/min. Con
# 12 m2 el caudal da 161 m/min y el Governor lo advierte (sin recortarlo) ---
assert any("> maximo 150 m/min" in w and "Art. 138 inc. 3" in w
           for w in result.warnings)

# --- DS 132, Art. 132: factor diesel 2.83 m3/min/HP (vs. 3.0 en Peru) ---
assert config.diesel_hp_factor == 2.83

# --- DS 132, Art. 138: 3 m3/min por persona SIN escalon por altitud (a
# diferencia del Art. 247 peruano, que sube a 4/5/6 m3/min sobre
# 1500/3000/4000 msnm). Los 3 umbrales quedan neutralizados al mismo valor. ---
assert config.min_flow_per_person == 3.0
assert config.flow_above_threshold_1 == 3.0
assert config.flow_above_threshold_2 == 3.0
assert config.flow_above_threshold_3 == 3.0

print("OK: q_total_m3min == 2219.0 con preset chile (Art. 132/138, sin escalon)")
