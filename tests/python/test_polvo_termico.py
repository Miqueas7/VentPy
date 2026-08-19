"""
Tests de integracion Python para polvo (Q_polvo) y carga termica (Q_termico).

Verifica que los bindings nanobind exponen correctamente calculate_dust_flow,
calculate_thermal_flow, DustParams/ThermalParams en VentilationInput y los
campos completos (incl. warnings) de DustFlowResult/ThermalFlowResult.

Normativa: DS 024-2016-EM, Art. 111 (polvo) y Art. 252.d / Art. 104+Anexo 13
(termico, criterio ingenieril heredado del derogado DS 055-2010-EM).
"""

import pytest

import ventpy


@pytest.fixture
def default_config():
    return ventpy.RegulatoryConfig()


@pytest.fixture
def governor(default_config):
    return ventpy.VentilationGovernor(default_config)


# ============================================================================
# calculate_dust_flow
# ============================================================================


class TestDustFlow:
    def test_basic_dilution(self, default_config):
        params = ventpy.DustParams()
        params.dust_generation_rate_mg_s = 50.0
        params.water_suppression = True
        params.suppression_efficiency = 0.7
        params.target_concentration_mg_m3 = 3.0

        result = ventpy.calculate_dust_flow(params, default_config)

        assert result.q_dust == 300.0
        assert "111" in result.regulation_ref

    def test_silica_warns_anexo_15(self, default_config):
        params = ventpy.DustParams()
        params.dust_generation_rate_mg_s = 50.0
        params.water_suppression = True
        params.suppression_efficiency = 0.7
        params.target_concentration_mg_m3 = 3.0
        params.silica_content_percent = 12.0

        result = ventpy.calculate_dust_flow(params, default_config)

        assert any("Anexo 15" in w for w in result.warnings)


# ============================================================================
# calculate_thermal_flow
# ============================================================================


class TestThermalFlow:
    def test_infeasible_floors_to_252d_velocity(self, default_config):
        atm = ventpy.AtmosphericParams()
        atm.dry_bulb_temp_c = 18.0
        atm.altitude_masl = 2500.0

        params = ventpy.ThermalParams()
        params.depth_below_surface_m = 1000.0
        params.auto_compression_c_per_100m = 0.98
        params.target_effective_temp_c = 28.0
        params.face_area_m2 = 12.0
        params.heat_from_equipment_kw = 400.0
        params.heat_from_oxidation_kw = 50.0

        result = ventpy.calculate_thermal_flow(params, atm, default_config)

        assert result.q_thermal == 360.0
        assert result.heat_from_oxidation_kw == 50.0
        assert "252" in result.regulation_ref
        assert any("refrigeracion" in w for w in result.warnings)


# ============================================================================
# Governor E2E (dust gobernante)
# ============================================================================


class TestGovernorDust:
    def test_dust_governs_and_warnings_prefixed(self, governor):
        inp = ventpy.VentilationInput()
        inp.zone_type = ventpy.ZoneType.DevelopmentFace
        inp.face_area_m2 = 0.1
        inp.num_workers = 1
        inp.altitude_masl = 0.0

        dust = ventpy.DustParams()
        dust.dust_generation_rate_mg_s = 50.0
        dust.water_suppression = False
        dust.target_concentration_mg_m3 = 2.9
        dust.silica_content_percent = 5.0
        inp.dust_params = dust

        result = governor.calculate_total_demand(inp)

        assert result.governing_factor == "dust (Q_Dust)"
        assert result.q_total_m3min == 1191.0
        assert any(w.startswith("Q_polvo: ") for w in result.warnings)
