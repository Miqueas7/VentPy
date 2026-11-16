"""Ejemplo 1 — Demanda de ventilacion de un frente de desarrollo (norma Peru).

Reproduce, usando SOLO el API publico de `ventpy` (sin pasar por `ventpy.cli`),
el caso de 15 trabajadores / 4200 msnm / scoop diesel 150 HP documentado en
README.md de este ejemplo (sin voladura: ver la nota del README). Este script ES su propia
verificacion: si algun `assert` falla, el numero de referencia (1923 m3/min)
dejo de sostenerse.

Equivalente por CLI:
    ventpy demanda examples/01-demanda-peru/input.json --json
"""
import ventpy

config = ventpy.RegulatoryConfig()  # default = DS024_Peru
governor = ventpy.VentilationGovernor(config)

inp = ventpy.VentilationInput()
inp.zone_type = ventpy.ZoneType.DevelopmentFace
inp.num_workers = 15
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

print(f"standard              = {result.standard.name}")
print(f"q_personnel_m3min      = {result.q_personnel_m3min}")
print(f"q_diesel_m3min         = {result.q_diesel_m3min}")
print(f"q_blasting_m3min       = {result.q_blasting_m3min}")
print(f"governing_factor       = {result.governing_factor}")
print(f"q_total_m3min           = {result.q_total_m3min}")
print(f"velocity_at_face_mps   = {result.velocity_at_face_mps}")
print(f"velocity_ok             = {result.velocity_ok}")
print(f"diesel.regulation_ref  = {result.diesel.regulation_ref}")

# --- El caudal total esta gobernado por el equipo diesel (Q_Eq) ---
assert result.q_total_m3min == 1923.0
assert result.governing_factor == "diesel (Q_Eq)"

# --- Dentro de Q_Eq, el sub-criterio gobernante es dilucion de NOx (motor
# Tier3 por defecto: EngineEmissionTier.Tier3) al TWA de NO2 del Anexo 15
# (3 ppm), no el factor HP normativo ni la dilucion de CO. Ver el README de
# este ejemplo para el detalle. ---
diesel = result.diesel
assert diesel.target_nox_ppm == 3.0
assert diesel.target_co_ppm == 25.0

# Formula EXACTA de "1. Q por factor HP normativo" (DS 024-2016-EM, Anexo 38:
# Q_Eq = 3 x HP x Dm x Fu) tal como la calcula el nucleo
# (include/ventpy/caudal_equipo.hpp, DieselFlowCalculator::calculate_full):
#   q_hp_method = total_effective_hp * hp_factor_corrected * simultaneity_factor
# OJO: usa `total_effective_hp` (HP con disponibilidad/utilizacion, SIN
# de-rating por altitud) - NO `total_derated_hp`. `hp_factor_corrected` ya
# trae su propia correccion volumetrica por altitud. `simultaneity_factor`
# no viaja en el resultado (es un parametro de entrada) asi que se toma del
# propio `inp`: por defecto 1.0, porque la norma no preve simultaneidad.
assert inp.simultaneity_factor == 1.0
q_hp = diesel.total_effective_hp * diesel.hp_factor_corrected * inp.simultaneity_factor
print(f"q_hp (factor HP, Anexo 38) = {q_hp}")

assert diesel.q_for_nox_dilution > diesel.q_for_co_dilution
assert diesel.q_for_nox_dilution > q_hp
assert abs(q_hp - 451.85) < 0.05
assert abs(diesel.q_for_nox_dilution - 1671.19) < 0.05
assert "dilucion NOx" in diesel.regulation_ref

# --- El piso de velocidad del frente (DS 024-2016-EM, Art. 248: 20 m/min) da
# Q_Per = 12 m2 x 20 m/min = 240: no es el que gobierna Q_total (eso lo hace
# el diesel), pero forma parte de la auditoria obligatoria de todo
# *FlowResult. ---
assert abs(ventpy.get_min_velocity(ventpy.ZoneType.DevelopmentFace) * 60 - 20.0) < 1e-9
assert result.q_personnel_m3min == 240.0
assert result.velocity_ok is True

print("OK: q_total_m3min == 1923.0 (gobierna diesel/NOx, piso de velocidad cumplido)")
