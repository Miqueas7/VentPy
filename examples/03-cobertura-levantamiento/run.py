"""Ejemplo 3 — Levantamiento de cobertura de ventilacion, 2 zonas.

Usa SOLO el API publico de `ventpy` (`ZoneSurvey`, `ZoneMeasurement`,
`CoverageCalculator.analyze_survey`) para reproducir un levantamiento de
campo con 2 frentes: uno bien ventilado y otro en deficit. La mina, en
conjunto, cubre el caudal requerido (Art. 252.f), pero una zona individual
NO lo hace (Art. 252.g) -> el resultado global "compliant" es False aunque
"global_compliant" sea True.

A diferencia de los Ejemplos 1/2/4, este caso demuestra por que el CLI
retorna exit code 2 (resultado "no confiable" segun el contrato del CLI,
ver README.md de este ejemplo) aun cuando el script Python no "falla": el
resultado es perfectamente valido y auditable, solo que exige atencion del
ingeniero antes de darlo por bueno operacionalmente.

Equivalente por CLI (exit code 2 INTENCIONAL, ver README de este ejemplo):
    ventpy cobertura examples/03-cobertura-levantamiento/input.json --json
"""
import ventpy


def build_zone(name, num_workers, altitude_masl, q_measured_m3min):
    vent_input = ventpy.VentilationInput()
    vent_input.zone_type = ventpy.ZoneType.DevelopmentFace
    vent_input.num_workers = num_workers
    vent_input.altitude_masl = altitude_masl

    measurement = ventpy.ZoneMeasurement()
    measurement.q_measured_m3min = q_measured_m3min

    survey = ventpy.ZoneSurvey()
    survey.zone_name = name
    survey.input = vent_input
    survey.measurement = measurement
    return survey


zones = [
    build_zone("Rampa 4200", num_workers=10, altitude_masl=4200.0, q_measured_m3min=270.0),
    build_zone("Frente N-02", num_workers=10, altitude_masl=2500.0, q_measured_m3min=150.0),
]

config = ventpy.RegulatoryConfig()  # DS024_Peru
params = ventpy.CoverageParams()

result = ventpy.CoverageCalculator.analyze_survey(zones, config, params)

print(f"q_required_total_m3min = {result.q_required_total_m3min}")
print(f"q_measured_total_m3min = {result.q_measured_total_m3min}")
print(f"global_compliant        = {result.global_compliant}")
print(f"all_zones_compliant     = {result.all_zones_compliant}")
print(f"compliant                = {result.compliant}")
for zone in result.zones:
    print(f"  zona '{zone.zone_name}': requerido={zone.q_required_m3min} "
          f"medido={zone.q_measured_m3min} deficit={zone.deficit_m3min} "
          f"compliant={zone.compliant}")

# --- Global cubre la demanda total (Art. 252.f) ---
assert result.global_compliant is True

# --- Pero NO todas las zonas individuales cumplen (Art. 252.g): el
# "Frente N-02" (altitud mas baja, menos caudal requerido, pero tambien
# menos medido) esta en deficit ---
assert result.all_zones_compliant is False
assert result.compliant is False  # compliant estricto = global AND todas las zonas

frente_n02 = result.zones[1]
assert frente_n02.zone_name == "Frente N-02"
assert frente_n02.deficit_m3min == 57.0
assert frente_n02.compliant is False

print("OK: global cubre, pero 'Frente N-02' esta en deficit de 57 m3/min "
      "(Art. 252.f/g) -> el CLI documenta esto como exit code 2")
