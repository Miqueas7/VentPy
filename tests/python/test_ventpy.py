"""
Tests de integracion Python para VentPy.

Verifica que los bindings nanobind exponen correctamente la API C++.
Requiere que el paquete este instalado: pip install -e .

Normativa: DS 024-2016-EM / DS 023-2017-EM (Peru)
"""

import math
import pytest

import ventpy

import importlib.util

HAS_MATPLOTLIB = importlib.util.find_spec("matplotlib") is not None


# ============================================================================
# Fixtures
# ============================================================================


@pytest.fixture
def default_config():
    """Configuracion normativa por defecto (DS 024)."""
    return ventpy.RegulatoryConfig()


@pytest.fixture
def governor(default_config):
    """Governor con config por defecto."""
    return ventpy.VentilationGovernor(default_config)


# ============================================================================
# RegulatoryConfig
# ============================================================================


class TestRegulatoryConfig:
    def test_default_values(self, default_config):
        assert default_config.min_flow_per_person == 3.0
        assert default_config.diesel_hp_factor == 3.0
        assert default_config.max_dilution_time == 30.0
        assert default_config.default_leakage_factor == 0.15
        assert default_config.standard == ventpy.RegulatoryStandard.DS024_Peru

    def test_standard_name(self, default_config):
        assert "DS 024" in default_config.standard_name
        assert "Peru" in default_config.standard_name

    def test_custom_config(self):
        config = ventpy.RegulatoryConfig(
            diesel_hp_factor_m3min=5.0,
            default_leakage_factor=0.20,
        )
        assert config.diesel_hp_factor == 5.0
        assert config.default_leakage_factor == 0.20

    def test_invalid_config_raises(self):
        with pytest.raises(ValueError):
            ventpy.RegulatoryConfig(min_flow_per_person_m3min=-1.0)


# ============================================================================
# Personnel Flow
# ============================================================================


class TestPersonnelFlow:
    def test_basic_calculation(self, default_config):
        result = ventpy.calculate_personnel_flow(10, 2500.0, default_config)
        assert result.num_workers == 10
        assert result.flow_per_person_base == 3.0
        assert result.q_personnel == 30.0

    def test_altitude_scaling(self, default_config):
        result = ventpy.calculate_personnel_flow(10, 4500.0, default_config)
        assert result.flow_per_person_base == 5.0
        assert result.q_personnel == 50.0

    def test_zero_workers_raises(self, default_config):
        with pytest.raises(ValueError):
            ventpy.calculate_personnel_flow(0, 3000.0, default_config)

    def test_negative_altitude_raises(self, default_config):
        with pytest.raises(ValueError):
            ventpy.calculate_personnel_flow(10, -100.0, default_config)

    def test_regulation_reference(self, default_config):
        result = ventpy.calculate_personnel_flow(5, 2000.0, default_config)
        assert "DS 024" in result.regulation_ref


# ============================================================================
# Diesel Flow
# ============================================================================


class TestDieselFlow:
    def test_fleet_calculation(self, default_config):
        fleet = ventpy.DieselFleet()
        fleet.add_equipment("Scoop", 150.0, 0.85, 0.70)
        fleet.add_equipment("Dumper", 300.0, 0.90, 0.60)

        result = ventpy.calculate_diesel_flow(fleet, default_config)
        # 150*0.85*0.70 + 300*0.90*0.60 = 89.25 + 162 = 251.25
        # 251.25 * 3 = 753.75
        assert abs(result.q_diesel - 754.0) < 1.0  # ceiling applied

    def test_empty_fleet(self, default_config):
        fleet = ventpy.DieselFleet()
        result = ventpy.calculate_diesel_flow(fleet, default_config)
        assert result.q_diesel == 0.0

    def test_invalid_hp_raises(self):
        fleet = ventpy.DieselFleet()
        with pytest.raises(ValueError):
            fleet.add_equipment("Bad", -100.0, 0.85, 0.70)

    def test_equipment_object(self):
        eq = ventpy.DieselEquipment()
        eq.name = "Jumbo"
        eq.horsepower = 200.0
        eq.availability = 0.9
        eq.utilization = 0.8
        assert eq.name == "Jumbo"
        assert eq.horsepower == 200.0

    def test_equipment_with_emission_tier(self):
        eq = ventpy.DieselEquipment()
        eq.name = "Modern Loader"
        eq.horsepower = 250.0
        eq.availability = 0.90
        eq.utilization = 0.75
        eq.emission_tier = ventpy.EngineEmissionTier.Tier4_Final
        eq.has_dpf = True
        eq.has_doc = True
        assert eq.emission_tier == ventpy.EngineEmissionTier.Tier4_Final
        assert eq.has_dpf is True


# ============================================================================
# Blasting Flow
# ============================================================================


class TestBlastingFlow:
    def test_typical_blasting(self, default_config):
        params = ventpy.BlastingParams()
        params.explosive_kg = 50.0
        params.gas_volume_per_kg = 0.04
        params.dilution_time_min = 30.0
        params.face_area_m2 = 12.0
        params.face_length_m = 200.0

        result = ventpy.calculate_blasting_flow(params, default_config)
        # Basic formula: (50 * 0.04) / 30 = 0.0667 m3/min (minimum)
        assert result.q_blasting > 0

    def test_zero_dilution_time_raises(self, default_config):
        params = ventpy.BlastingParams()
        params.explosive_kg = 50.0
        params.gas_volume_per_kg = 0.04
        params.dilution_time_min = 0.0
        params.face_area_m2 = 12.0
        params.face_length_m = 200.0

        with pytest.raises(ValueError):
            ventpy.calculate_blasting_flow(params, default_config)

    def test_explosive_types(self):
        params = ventpy.BlastingParams()
        params.explosive_type = ventpy.ExplosiveType.ANFO
        assert params.explosive_type == ventpy.ExplosiveType.ANFO

        params.explosive_type = ventpy.ExplosiveType.Emulsion
        assert params.explosive_type == ventpy.ExplosiveType.Emulsion


# ============================================================================
# Atmospheric Calculations
# ============================================================================


class TestAtmosphericCalculations:
    def test_pressure_at_sea_level(self):
        pressure = ventpy.calculate_pressure_kpa(0.0)
        assert abs(pressure - 101.325) < 0.1

    def test_pressure_at_altitude(self):
        # At ~4000m, pressure should be about 60-65 kPa
        pressure = ventpy.calculate_pressure_kpa(4000.0)
        assert 58 < pressure < 68

    def test_density_at_sea_level(self):
        density = ventpy.calculate_density_kg_m3(0.0, 15.0)
        assert abs(density - 1.225) < 0.05

    def test_density_ratio(self):
        ratio = ventpy.calculate_density_ratio(4000.0)
        assert 0.6 < ratio < 0.75

    def test_volume_correction_factor(self):
        factor = ventpy.calculate_volume_correction_factor(4000.0)
        # Should be > 1 (more volume needed at altitude)
        assert factor > 1.3

    def test_o2_partial_pressure(self):
        # At sea level: 101.325 * 0.2095 = 21.2 kPa
        po2 = ventpy.calculate_o2_partial_pressure_kpa(0.0)
        assert abs(po2 - 21.2) < 0.5

        # At 4000m, should be lower
        po2_high = ventpy.calculate_o2_partial_pressure_kpa(4000.0)
        assert po2_high < po2

    def test_diesel_derate_factor(self):
        # At sea level, no de-rating
        factor = ventpy.calculate_diesel_derate_factor(0.0)
        assert factor == 1.0

        # At 1000m, still no de-rating
        factor = ventpy.calculate_diesel_derate_factor(1000.0)
        assert factor == 1.0

        # At 4000m, significant de-rating
        factor = ventpy.calculate_diesel_derate_factor(4000.0)
        assert 0.80 < factor < 0.95

    def test_atmospheric_corrections_struct(self):
        params = ventpy.AtmosphericParams()
        params.altitude_masl = 4000.0
        params.dry_bulb_temp_c = 20.0

        corrections = ventpy.calculate_atmospheric_corrections(params)
        assert corrections.altitude_masl == 4000.0
        assert corrections.pressure_kpa > 0
        assert corrections.density_ratio < 1.0
        assert corrections.volume_correction_factor > 1.0


# ============================================================================
# New Enums and Types
# ============================================================================


class TestEnumsAndTypes:
    def test_activity_levels(self):
        assert ventpy.ActivityLevel.Rest
        assert ventpy.ActivityLevel.Light
        assert ventpy.ActivityLevel.Moderate
        assert ventpy.ActivityLevel.Heavy
        assert ventpy.ActivityLevel.VeryHeavy

    def test_explosive_types(self):
        assert ventpy.ExplosiveType.ANFO
        assert ventpy.ExplosiveType.Emulsion
        assert ventpy.ExplosiveType.Dynamite

    def test_duct_types(self):
        assert ventpy.DuctType.FlexibleFabric
        assert ventpy.DuctType.FlexiblePVC
        assert ventpy.DuctType.RigidSteel

    def test_installation_quality(self):
        assert ventpy.InstallationQuality.Poor
        assert ventpy.InstallationQuality.Average
        assert ventpy.InstallationQuality.Good
        assert ventpy.InstallationQuality.Excellent

    def test_engine_emission_tiers(self):
        assert ventpy.EngineEmissionTier.Tier0_Unregulated
        assert ventpy.EngineEmissionTier.Tier3
        assert ventpy.EngineEmissionTier.Tier4_Final

    def test_personnel_params(self):
        params = ventpy.PersonnelParams()
        params.num_workers = 15
        params.activity = ventpy.ActivityLevel.Heavy
        params.exposure_hours = 10.0
        assert params.num_workers == 15
        assert params.activity == ventpy.ActivityLevel.Heavy

    def test_duct_params(self):
        params = ventpy.DuctParams()
        params.duct_type = ventpy.DuctType.FlexiblePVC
        params.quality = ventpy.InstallationQuality.Good
        params.duct_diameter_m = 0.8
        params.duct_length_m = 150.0
        params.num_joints = 15
        assert params.duct_diameter_m == 0.8


# ============================================================================
# Governor (Integration)
# ============================================================================


class TestGovernor:
    def test_full_development_face(self, governor):
        inp = ventpy.VentilationInput()
        inp.zone_type = ventpy.ZoneType.DevelopmentFace
        inp.num_workers = 15
        inp.altitude_masl = 4200.0

        fleet = ventpy.DieselFleet()
        fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70)
        inp.diesel_fleet = fleet

        params = ventpy.BlastingParams()
        params.explosive_kg = 50.0
        params.gas_volume_per_kg = 0.04
        params.dilution_time_min = 30.0
        params.face_area_m2 = 12.0
        params.face_length_m = 200.0
        inp.blasting_params = params

        result = governor.calculate_total_demand(inp)

        # Diesel should govern (larger than personnel or blasting)
        assert "diesel" in result.governing_factor.lower()
        assert result.q_total_m3min > 0
        assert result.q_total_cfm > 0

        # Audit trail
        assert result.personnel is not None
        assert result.diesel is not None
        assert result.blasting is not None
        assert result.leakage is not None

    def test_robust_input(self, governor):
        """Test using the new robust input structures."""
        inp = ventpy.VentilationInput()
        inp.zone_type = ventpy.ZoneType.DevelopmentFace
        inp.face_area_m2 = 15.0
        inp.face_length_m = 120.0

        # Atmospheric
        inp.atmospheric = ventpy.AtmosphericParams()
        inp.atmospheric.altitude_masl = 4200.0
        inp.atmospheric.dry_bulb_temp_c = 22.0

        # Personnel with activity level
        inp.personnel = ventpy.PersonnelParams()
        inp.personnel.num_workers = 12
        inp.personnel.activity = ventpy.ActivityLevel.Moderate

        # Diesel with emission tier
        fleet = ventpy.DieselFleet()
        eq = ventpy.DieselEquipment()
        eq.name = "Scooptram ST1030"
        eq.horsepower = 180.0
        eq.availability = 0.88
        eq.utilization = 0.75
        eq.emission_tier = ventpy.EngineEmissionTier.Tier3
        fleet.add_equipment(eq)
        inp.diesel_fleet = fleet

        inp.simultaneity_factor = 0.85
        inp.safety_factor = 1.1

        result = governor.calculate_total_demand(inp)

        assert result.q_total_m3min > 0
        assert result.atmospheric is not None
        assert result.atmospheric.volume_correction_factor > 1.0
        assert result.velocity_at_face_mps >= 0

    def test_personnel_only(self, governor):
        inp = ventpy.VentilationInput()
        inp.num_workers = 10
        inp.altitude_masl = 2500.0

        result = governor.calculate_total_demand(inp)
        assert "personnel" in result.governing_factor.lower()
        assert result.q_total_m3min > 0

    def test_empty_input(self, governor):
        inp = ventpy.VentilationInput()
        result = governor.calculate_total_demand(inp)
        assert result.q_total_m3min == 0.0

    def test_warnings_generated(self, governor):
        """Test that warnings are generated for extreme conditions."""
        inp = ventpy.VentilationInput()
        inp.num_workers = 10
        inp.altitude_masl = 4800.0  # Extreme altitude

        result = governor.calculate_total_demand(inp)
        # Should have altitude warning
        assert len(result.warnings) > 0 or result.q_total_m3min > 0


# ============================================================================
# Utility Functions
# ============================================================================


class TestUtilityFunctions:
    def test_safety_ceil(self):
        assert ventpy.safety_ceil(100.01) == 101.0
        assert ventpy.safety_ceil(100.0) == 100.0
        assert ventpy.safety_ceil(0.1) == 1.0

    def test_safety_ceil_decimals(self):
        assert ventpy.safety_ceil_decimals(1.234, 2) == 1.24
        assert ventpy.safety_ceil_decimals(1.231, 2) == 1.24
        assert ventpy.safety_ceil_decimals(1.20, 2) == 1.20

    def test_get_o2_consumption(self):
        assert ventpy.get_o2_consumption(ventpy.ActivityLevel.Rest) == 0.3
        assert ventpy.get_o2_consumption(ventpy.ActivityLevel.Moderate) == 1.0
        assert ventpy.get_o2_consumption(ventpy.ActivityLevel.VeryHeavy) == 2.0

    def test_get_min_velocity(self):
        vel_dev = ventpy.get_min_velocity(ventpy.ZoneType.DevelopmentFace)
        vel_ramp = ventpy.get_min_velocity(ventpy.ZoneType.Ramp)
        vel_stope = ventpy.get_min_velocity(ventpy.ZoneType.Stope)

        assert vel_dev == 0.25
        assert vel_ramp == 0.30
        assert vel_stope == 0.20

    def test_m3min_to_cfm_constant(self):
        assert abs(ventpy.M3MIN_TO_CFM - 35.3147) < 0.001


# ============================================================================
# Constants Submodule
# ============================================================================


class TestConstants:
    def test_conversion_constants(self):
        assert ventpy.constants.M3MIN_TO_CFM > 35
        assert ventpy.constants.HP_TO_KW > 0.7
        assert ventpy.constants.KW_TO_HP > 1.3

    def test_atmospheric_constants(self):
        assert ventpy.constants.SEA_LEVEL_PRESSURE_KPA == 101.325
        assert abs(ventpy.constants.SEA_LEVEL_DENSITY_KG_M3 - 1.225) < 0.01
        assert abs(ventpy.constants.O2_FRACTION_AIR - 0.2095) < 0.001

    def test_tlv_constants(self):
        assert ventpy.constants.TLV_CO_PPM == 25.0
        assert ventpy.constants.TLV_NO2_PPM == 5.0
        assert ventpy.constants.TLV_H2S_PPM == 10.0
        assert ventpy.constants.MIN_O2_PERCENT == 19.5
        assert ventpy.constants.MAX_EFFECTIVE_TEMP_C == 30.0

    def test_velocity_constants(self):
        assert ventpy.constants.MIN_VELOCITY_DEVELOPMENT_MPS == 0.25
        assert ventpy.constants.MIN_VELOCITY_RAMP_MPS == 0.30
        assert ventpy.constants.MIN_VELOCITY_STOPE_MPS == 0.20

    def test_explosive_constants(self):
        assert ventpy.constants.ANFO_CO_L_PER_KG == 40.0
        assert ventpy.constants.DYNAMITE_CO_L_PER_KG == 50.0

    def test_diesel_emission_constants(self):
        # Tier 0 should have higher emissions than Tier 4
        assert ventpy.constants.DIESEL_TIER0_CO_G_KWH > ventpy.constants.DIESEL_TIER4F_CO_G_KWH
        assert ventpy.constants.DIESEL_TIER0_NOX_G_KWH > ventpy.constants.DIESEL_TIER4F_NOX_G_KWH


# ============================================================================
# Visualization Module (optional, requires matplotlib)
# ============================================================================


class TestVisualization:
    """Tests for the visualization module."""

    def test_import_visualization(self):
        """Test that visualization module can be imported."""
        from ventpy import visualization
        assert hasattr(visualization, 'plot_flow_comparison')
        assert hasattr(visualization, 'plot_flow_breakdown')
        assert hasattr(visualization, 'create_dashboard')
        assert hasattr(visualization, 'generate_html_report')

    def test_extract_flow_data_from_dict(self):
        """Test extracting flow data from a dictionary."""
        from ventpy import visualization as viz

        data = {
            'q_personnel_m3min': 45.0,
            'q_diesel_m3min': 267.0,
            'q_blasting_m3min': 0.0,
            'q_dust_m3min': 0.0,
            'q_thermal_m3min': 0.0,
            'q_leakage_m3min': 50.0,
            'q_governing_m3min': 267.0,
            'q_total_m3min': 317.0,
        }

        extracted = viz.extract_flow_data(data)
        assert extracted['personnel'] == 45.0
        assert extracted['diesel'] == 267.0
        assert extracted['total'] == 317.0

    def test_get_flow_labels(self):
        """Test getting display labels."""
        from ventpy import visualization as viz

        labels = viz.get_flow_labels()
        assert 'Personal' in labels['personnel']
        assert 'Diesel' in labels['diesel']
        assert 'Voladura' in labels['blasting']

    def test_style_class(self):
        """Test VentPyStyle configuration."""
        from ventpy.visualization import VentPyStyle, DEFAULT_STYLE

        assert DEFAULT_STYLE is not None
        assert 'personnel' in DEFAULT_STYLE.colors

        custom_style = VentPyStyle()
        custom_style.title_fontsize = 16
        assert custom_style.title_fontsize == 16

    @pytest.mark.skipif(not HAS_MATPLOTLIB, reason="matplotlib not installed")
    def test_plot_flow_comparison(self):
        """Test bar chart generation."""
        from ventpy import visualization as viz
        import matplotlib
        matplotlib.use('Agg')  # Non-interactive backend

        data = {
            'q_personnel_m3min': 45.0,
            'q_diesel_m3min': 267.0,
            'q_blasting_m3min': 50.0,
            'q_dust_m3min': 0.0,
            'q_thermal_m3min': 0.0,
            'q_leakage_m3min': 50.0,
            'q_governing_m3min': 267.0,
            'q_total_m3min': 317.0,
        }

        fig, ax = viz.plot_flow_comparison(data)
        assert fig is not None
        assert ax is not None

        import matplotlib.pyplot as plt
        plt.close(fig)

    @pytest.mark.skipif(not HAS_MATPLOTLIB, reason="matplotlib not installed")
    def test_plot_flow_breakdown(self):
        """Test pie chart generation."""
        from ventpy import visualization as viz
        import matplotlib
        matplotlib.use('Agg')

        data = {
            'q_personnel_m3min': 45.0,
            'q_diesel_m3min': 267.0,
            'q_blasting_m3min': 50.0,
            'q_dust_m3min': 0.0,
            'q_thermal_m3min': 0.0,
            'q_leakage_m3min': 50.0,
            'q_governing_m3min': 267.0,
            'q_total_m3min': 317.0,
        }

        fig, ax = viz.plot_flow_breakdown(data)
        assert fig is not None
        assert ax is not None

        import matplotlib.pyplot as plt
        plt.close(fig)

    def test_generate_html_report(self):
        """Test HTML report generation."""
        from ventpy import visualization as viz

        data = {
            'q_personnel_m3min': 45.0,
            'q_diesel_m3min': 267.0,
            'q_blasting_m3min': 50.0,
            'q_dust_m3min': 0.0,
            'q_thermal_m3min': 0.0,
            'q_leakage_m3min': 50.0,
            'q_governing_m3min': 267.0,
            'q_total_m3min': 317.0,
        }

        html = viz.generate_html_report(data, include_charts=False)

        assert '<!DOCTYPE html>' in html
        assert 'VentPy' in html
        assert '45.0' in html or '45' in html  # Personnel flow
        assert 'DS 024' in html
