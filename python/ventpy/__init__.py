"""
VentPy - High-performance mine ventilation calculation library.

Normativa base: DS 024-2016-EM / DS 023-2017-EM (Peru)

Uso rapido:
    >>> import ventpy
    >>> config = ventpy.RegulatoryConfig()
    >>> governor = ventpy.VentilationGovernor(config)
    >>> inp = ventpy.VentilationInput()
    >>> inp.num_workers = 15
    >>> inp.altitude_masl = 4200.0
    >>> result = governor.calculate_total_demand(inp)
    >>> print(f"Q_total = {result.q_total_m3min} m3/min")

Uso avanzado (version robusta):
    >>> import ventpy
    >>> from ventpy import visualization as viz
    >>>
    >>> config = ventpy.RegulatoryConfig()
    >>> governor = ventpy.VentilationGovernor(config)
    >>>
    >>> inp = ventpy.VentilationInput()
    >>> inp.atmospheric = ventpy.AtmosphericParams()
    >>> inp.atmospheric.altitude_masl = 4200.0
    >>> inp.personnel = ventpy.PersonnelParams()
    >>> inp.personnel.num_workers = 15
    >>> inp.personnel.activity = ventpy.ActivityLevel.Moderate
    >>>
    >>> fleet = ventpy.DieselFleet()
    >>> fleet.add_equipment("Scooptram", 180, 0.85, 0.70)
    >>> inp.diesel_fleet = fleet
    >>>
    >>> result = governor.calculate_total_demand(inp)
    >>> print(f"Q_total = {result.q_total_m3min} m3/min")
    >>> print(f"Factor gobernante: {result.governing_factor}")
    >>>
    >>> # Generar graficos
    >>> viz.quick_dashboard(result)
    >>> viz.show()

Copyright 2026 VentPy Project
"""

from ventpy._ventpy_core import (
    # Enums
    ZoneType,
    RegulatoryStandard,
    ActivityLevel,
    ExplosiveType,
    DuctType,
    InstallationQuality,
    EngineEmissionTier,
    GasType,
    ConcentrationUnit,

    # Input structs
    AtmosphericParams,
    PersonnelParams,
    DieselEquipment,
    BlastingParams,
    DuctParams,
    DustParams,
    ThermalParams,

    # Result structs
    AtmosphericCorrections,
    PersonnelFlowResult,
    DieselFlowResult,
    BlastingFlowResult,
    LeakageFlowResult,
    DustFlowResult,
    ThermalFlowResult,
    VentilationDemandResult,
    GasLimit,

    # Cobertura / deficit structs
    AirflowStation,
    ZoneMeasurement,
    CoverageParams,
    ZoneSurvey,
    StationResult,
    ZoneCoverageResult,
    MineCoverageResult,

    # Red de ventilacion (SP-3): enums
    AirwayLining,
    SingularityType,

    # Red de ventilacion (SP-3): structs de entrada
    AirwaySingularity,
    AirwayParams,
    DuctSizingParams,
    EconomicParams,
    NetworkBranch,
    NetworkDefinition,
    SolverParams,
    FanCurvePoint,
    FanCurve,
    FanOperatingParams,

    # Red de ventilacion (SP-3): structs de resultado
    FrictionFactorEntry,
    ShockFactorEntry,
    AirwayResistanceResult,
    DuctOptionResult,
    DuctSizingResult,
    BranchFlowResult,
    NetworkSolveResult,
    FanOperatingResult,

    # Main classes
    DieselFleet,
    RegulatoryConfig,
    VentilationInput,
    VentilationGovernor,
    CoverageCalculator,
    AtkinsonCalculator,
    DuctSizingCalculator,
    NetworkSolver,
    FanCalculator,

    # Individual calculators (simple API)
    calculate_personnel_flow,
    calculate_diesel_flow,
    calculate_blasting_flow,
    calculate_dust_flow,
    calculate_thermal_flow,

    # Atkinson (resistencia de ramal: tablas y funciones sueltas)
    atkinson_friction_factors,
    shock_factors,
    friction_factor_for,
    resolve_shock_factor,

    # Atmospheric calculations
    calculate_pressure_kpa,
    calculate_density_kg_m3,
    calculate_density_ratio,
    calculate_volume_correction_factor,
    calculate_o2_partial_pressure_kpa,
    calculate_diesel_derate_factor,
    calculate_atmospheric_corrections,

    # LMP (limites maximos permisibles) por norma
    gas_limits,
    lmp_for,

    # Utility functions
    safety_ceil,
    safety_ceil_decimals,
    get_o2_consumption,
    get_min_velocity,

    # Constants (main module)
    M3MIN_TO_CFM,

    # Constants submodule
    constants,
)

# Visualization module (lazy import to avoid matplotlib dependency issues)
def __getattr__(name):
    if name == "visualization":
        import importlib
        return importlib.import_module(".visualization", __name__)
    raise AttributeError(f"module 'ventpy' has no attribute '{name}'")


__version__ = "0.2.0"

__all__ = [
    # Version
    "__version__",

    # Enums
    "ZoneType",
    "RegulatoryStandard",
    "ActivityLevel",
    "ExplosiveType",
    "DuctType",
    "InstallationQuality",
    "EngineEmissionTier",
    "GasType",
    "ConcentrationUnit",

    # Input structs
    "AtmosphericParams",
    "PersonnelParams",
    "DieselEquipment",
    "BlastingParams",
    "DuctParams",
    "DustParams",
    "ThermalParams",

    # Result structs
    "AtmosphericCorrections",
    "PersonnelFlowResult",
    "DieselFlowResult",
    "BlastingFlowResult",
    "LeakageFlowResult",
    "DustFlowResult",
    "ThermalFlowResult",
    "VentilationDemandResult",
    "GasLimit",

    # Cobertura / deficit structs
    "AirflowStation",
    "ZoneMeasurement",
    "CoverageParams",
    "ZoneSurvey",
    "StationResult",
    "ZoneCoverageResult",
    "MineCoverageResult",

    # Red de ventilacion (SP-3): enums
    "AirwayLining",
    "SingularityType",

    # Red de ventilacion (SP-3): structs de entrada
    "AirwaySingularity",
    "AirwayParams",
    "DuctSizingParams",
    "EconomicParams",
    "NetworkBranch",
    "NetworkDefinition",
    "SolverParams",
    "FanCurvePoint",
    "FanCurve",
    "FanOperatingParams",

    # Red de ventilacion (SP-3): structs de resultado
    "FrictionFactorEntry",
    "ShockFactorEntry",
    "AirwayResistanceResult",
    "DuctOptionResult",
    "DuctSizingResult",
    "BranchFlowResult",
    "NetworkSolveResult",
    "FanOperatingResult",

    # Main classes
    "DieselFleet",
    "RegulatoryConfig",
    "VentilationInput",
    "VentilationGovernor",
    "CoverageCalculator",
    "AtkinsonCalculator",
    "DuctSizingCalculator",
    "NetworkSolver",
    "FanCalculator",

    # Individual calculators
    "calculate_personnel_flow",
    "calculate_diesel_flow",
    "calculate_blasting_flow",
    "calculate_dust_flow",
    "calculate_thermal_flow",

    # Atkinson (resistencia de ramal: tablas y funciones sueltas)
    "atkinson_friction_factors",
    "shock_factors",
    "friction_factor_for",
    "resolve_shock_factor",

    # Atmospheric calculations
    "calculate_pressure_kpa",
    "calculate_density_kg_m3",
    "calculate_density_ratio",
    "calculate_volume_correction_factor",
    "calculate_o2_partial_pressure_kpa",
    "calculate_diesel_derate_factor",
    "calculate_atmospheric_corrections",

    # LMP (limites maximos permisibles) por norma
    "gas_limits",
    "lmp_for",

    # Utility functions
    "safety_ceil",
    "safety_ceil_decimals",
    "get_o2_consumption",
    "get_min_velocity",

    # Constants
    "M3MIN_TO_CFM",
    "constants",

    # Submodules
    "visualization",
]
