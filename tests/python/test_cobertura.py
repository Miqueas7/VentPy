"""
Tests de integracion Python del analisis de cobertura/deficit.

Verifica que los bindings nanobind exponen correctamente CoverageCalculator
(compare_zone / analyze_survey) y las structs de entrada/resultado.

Normativa: DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252 lit. f/g.
Numeros tomados del probe SP-2 / tests/cpp/test_cobertura.cpp (ya validados).
"""

import math

import pytest

import ventpy


def make_zone(name, altitude, measured_direct):
    """Espejo de make_zone() en tests/cpp/test_cobertura.cpp."""
    z = ventpy.ZoneSurvey()
    z.zone_name = name
    z.input = ventpy.VentilationInput()
    z.input.zone_type = ventpy.ZoneType.DevelopmentFace
    z.input.num_workers = 10
    z.input.altitude_masl = altitude
    z.measurement = ventpy.ZoneMeasurement()
    z.measurement.zone_name = name
    z.measurement.q_measured_m3min = measured_direct
    return z


# ============================================================================
# compare_zone - medicion directa
# ============================================================================


class TestCompareZoneDirect:
    def test_compliant_reports_ratio(self):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Rampa 4200"
        m.q_measured_m3min = 250.0

        r = ventpy.CoverageCalculator.compare_zone(207.0, m)

        assert r.zone_name == "Rampa 4200"
        assert r.q_required_m3min == 207.0
        assert r.q_measured_m3min == 250.0
        assert r.coverage_ratio == pytest.approx(250.0 / 207.0)
        assert r.compliant is True
        assert r.deficit_m3min == 0.0
        assert "252" in r.regulation_ref

    def test_deficit_uses_safety_ceil(self):
        # deficit = ceil(100 - 90.2) = ceil(9.8) = 10 -- NUNCA 9
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Frente N-02"
        m.q_measured_m3min = 90.2

        r = ventpy.CoverageCalculator.compare_zone(100.0, m)

        assert r.compliant is False
        assert r.deficit_m3min == 10.0
        assert r.coverage_ratio == pytest.approx(0.902)

    def test_measured_infinite_raises(self):
        # FIX 1 (pre-bindings): inf pasaba require_non_negative (inf >= 0)
        # y reportaba compliant=True de forma espuria. Debe lanzar.
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Z"
        m.q_measured_m3min = math.inf

        with pytest.raises(ValueError):
            ventpy.CoverageCalculator.compare_zone(100.0, m)


# ============================================================================
# compare_zone - estaciones de aforo
# ============================================================================


class TestCompareZoneStations:
    def test_stations_sum_in_parallel(self):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Nivel 380"
        e1 = ventpy.AirflowStation()
        e1.station_id = "E-1"
        e1.area_m2 = 10.0
        e1.velocity_mps = 1.0  # Q = 10 x 1.0 x 60 = 600
        e2 = ventpy.AirflowStation()
        e2.station_id = "E-2"
        e2.area_m2 = 5.0
        e2.velocity_mps = 0.5  # Q = 5 x 0.5 x 60 = 150
        m.stations = [e1, e2]

        r = ventpy.CoverageCalculator.compare_zone(700.0, m)

        assert len(r.stations) == 2
        assert r.stations[0].q_station_m3min == 600.0
        assert r.stations[1].q_station_m3min == 150.0
        assert r.q_measured_m3min == 750.0
        assert r.compliant is True

    def test_reasignar_medicion_a_estaciones(self):
        # Reasignar la fuente de medicion (de directa a estaciones) debe
        # dejar la anterior sin efecto: compare_zone usa exactamente UNA
        # fuente (has_direct == has_stations lanza), la vigente al momento
        # de la llamada.
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Z-reasignada"
        m.q_measured_m3min = 100.0

        m.q_measured_m3min = None
        e1 = ventpy.AirflowStation()
        e1.station_id = "E-1"
        e1.area_m2 = 10.0
        e1.velocity_mps = 1.0  # Q = 10 x 1.0 x 60 = 600
        m.stations = [e1]

        r = ventpy.CoverageCalculator.compare_zone(500.0, m)

        assert len(r.stations) == 1
        assert r.q_measured_m3min == 600.0
        assert r.compliant is True

    def test_velocity_out_of_range_warns_248(self):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Z"
        e1 = ventpy.AirflowStation()
        e1.station_id = "E-1"
        e1.area_m2 = 12.0
        e1.velocity_mps = 0.30  # 18 m/min < 20 (Art. 248)
        m.stations = [e1]

        r = ventpy.CoverageCalculator.compare_zone(100.0, m)

        assert len(r.stations) == 1
        assert r.stations[0].velocity_ok is False
        assert "248" in r.stations[0].warning
        # El caudal SI se contabiliza aunque la velocidad este fuera de rango
        assert r.q_measured_m3min == pytest.approx(216.0)


# ============================================================================
# analyze_survey - balance de mina via Governor (E2E)
# ============================================================================


class TestAnalyzeSurvey:
    def test_global_covers_with_zone_in_deficit(self):
        zones = [
            make_zone("Rampa 4200", 4200.0, 270.0),
            make_zone("Frente N-02", 2500.0, 150.0),
        ]

        r = ventpy.CoverageCalculator.analyze_survey(
            zones, ventpy.RegulatoryConfig.peru())

        assert len(r.zones) == 2
        assert r.zones[0].q_required_m3min == 207.0
        assert r.zones[1].q_required_m3min == 207.0
        assert r.zones[0].compliant is True     # 270 >= 207
        assert r.zones[1].compliant is False    # 150 < 207
        assert r.zones[1].deficit_m3min == 57.0

        assert r.q_required_total_m3min == 414.0
        assert r.q_measured_total_m3min == 420.0
        assert r.global_compliant is True       # Art. 252.f: Sum(med) >= Sum(req)
        assert r.all_zones_compliant is False   # Art. 252.g: zona en deficit
        assert r.compliant is False             # criterio estricto: ambos
        assert r.deficit_total_m3min == 0.0

        # Cada zona lleva su desglose completo del Governor
        assert r.zones[0].demand is not None
        assert r.zones[0].demand.standard == ventpy.RegulatoryStandard.DS024_Peru

        deficit_warned = any("Frente N-02" in w for w in r.warnings)
        assert deficit_warned

    def test_general_mine_zone_raises(self):
        z = make_zone("Mina total", 2500.0, 1000.0)
        z.input.zone_type = ventpy.ZoneType.GeneralMine  # doble conteo -> prohibido

        with pytest.raises(ValueError):
            ventpy.CoverageCalculator.analyze_survey(
                [z], ventpy.RegulatoryConfig.peru())
