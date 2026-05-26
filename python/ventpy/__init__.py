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
    
    # Main classes
    DieselFleet,
    RegulatoryConfig,
    VentilationInput,
    VentilationGovernor,
    
    # Individual calculators (simple API)
    calculate_personnel_flow,
    calculate_diesel_flow,
    calculate_blasting_flow,
    
    # Atmospheric calculations
    calculate_pressure_kpa,
    calculate_density_kg_m3,
    calculate_density_ratio,
    calculate_volume_correction_factor,
    calculate_o2_partial_pressure_kpa,
    calculate_diesel_derate_factor,
    calculate_atmospheric_corrections,
    
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
        from ventpy import visualization
        return visualization
    raise AttributeError(f"module 'ventpy' has no attribute '{name}'")


__version__ = "0.1.0"

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
    
    # Main classes
    "DieselFleet",
    "RegulatoryConfig",
    "VentilationInput",
    "VentilationGovernor",
    
    # Individual calculators
    "calculate_personnel_flow",
    "calculate_diesel_flow",
    "calculate_blasting_flow",
    
    # Atmospheric calculations
    "calculate_pressure_kpa",
    "calculate_density_kg_m3",
    "calculate_density_ratio",
    "calculate_volume_correction_factor",
    "calculate_o2_partial_pressure_kpa",
    "calculate_diesel_derate_factor",
    "calculate_atmospheric_corrections",
    
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
