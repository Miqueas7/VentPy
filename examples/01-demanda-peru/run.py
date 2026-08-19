"""Ejemplo 1 — Demanda de ventilacion de un frente de desarrollo (norma Peru).

Reproduce, usando SOLO el API publico de `ventpy` (sin pasar por `ventpy.cli`),
el caso de 15 trabajadores / 4200 msnm / scoop diesel 150 HP / voladura ANFO
50 kg documentado en README.md de este ejemplo. Este script ES su propia
verificacion: si algun `assert` falla, el numero de referencia (981 m3/min)
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

blast = ventpy.BlastingParams()
blast.explosive_kg = 50
blast.gas_volume_per_kg = 0.04
blast.dilution_time_min = 30
blast.face_area_m2 = 12
blast.face_length_m = 200
inp.blasting_params = blast

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
assert result.q_total_m3min == 981.0
assert result.governing_factor == "diesel (Q_Eq)"

# --- Dentro de Q_Eq, el sub-criterio gobernante es dilucion de NOx (motor
# Tier3 por defecto: EngineEmissionTier.Tier3), no el factor HP normativo
# ni la dilucion de CO. Ver DOMAIN/README de este ejemplo para el detalle. ---
diesel = result.diesel

# Formula EXACTA de "1. Q por factor HP normativo (Art. 246)" tal como la
# calcula el nucleo (include/ventpy/caudal_equipo.hpp,
# DieselFlowCalculator::calculate_full):
#   q_hp_method = total_effective_hp * hp_factor_corrected * simultaneity_factor
# OJO: usa `total_effective_hp` (HP con disponibilidad/utilizacion, SIN
# de-rating por altitud) - NO `total_derated_hp` (que SI trae el de-rate
# aplicado, mas el factor de simultaneidad ya multiplicado adentro). Usar
# `total_derated_hp * hp_factor_corrected` aplicaria el de-rate DOS veces
# (una vez para obtener total_derated_hp, otra al multiplicar de nuevo por
# hp_factor_corrected, que ya trae su propia correccion volumetrica) y da
# un numero que el nucleo nunca calcula. `simultaneity_factor` no viaja en
# el resultado (es un parametro de entrada, no un campo de
# DieselFlowResult) asi que se toma del propio `inp` que armamos arriba.
q_hp = diesel.total_effective_hp * diesel.hp_factor_corrected * inp.simultaneity_factor
print(f"q_hp (factor HP, Art. 246) = {q_hp}")

assert diesel.q_for_nox_dilution > diesel.q_for_co_dilution
assert diesel.q_for_nox_dilution > q_hp
assert abs(q_hp - 384.08) < 0.05
assert "dilucion NOx" in diesel.regulation_ref

# --- El piso de velocidad del frente (Art. 236, 0.25 m/s) esta cubierto:
# no es el que gobierna Q_total (eso lo hace el diesel), pero forma parte
# de la auditoria obligatoria de todo *FlowResult. ---
assert ventpy.get_min_velocity(ventpy.ZoneType.DevelopmentFace) == 0.25
assert result.velocity_ok is True

print("OK: q_total_m3min == 981.0 (gobierna diesel/NOx, piso de velocidad cumplido)")
