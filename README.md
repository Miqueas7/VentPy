# VentPy

[English](README.md) | [Español](README.es.md)

High-performance underground mine ventilation calculations for Python, backed by a C++20 core.

VentPy helps mining engineers estimate ventilation demand for underground operations using an auditable API oriented around the Peruvian regulatory framework `DS 024-2016-EM / DS 023-2017-EM`.

## What VentPy Does

VentPy provides calculation tools for:

- Personnel-based airflow demand
- Diesel fleet airflow demand
- Blasting gas dilution airflow
- Atmospheric corrections at altitude
- Leakage adjustments for ducted ventilation
- Consolidated ventilation demand with governing-factor selection
- Optional visualization and HTML reporting helpers in Python

The library is designed for engineering workflows where performance, reproducibility, and traceability matter.

## Why This Library Exists

Mine ventilation calculations are often implemented in spreadsheets that become difficult to validate, reuse, or integrate into larger workflows. VentPy moves those calculations into a typed, testable library with:

- A C++ core for predictable performance
- Python bindings for scripting, analysis, and integration
- Explicit result objects instead of opaque spreadsheet formulas
- Domain-oriented inputs for personnel, diesel equipment, blasting, atmosphere, and leakage

## Regulatory Scope

Current defaults are based on:

- `DS 024-2016-EM`
- `DS 023-2017-EM`

The default `RegulatoryConfig` reflects this Peruvian framework, while still allowing stricter corporate parameters to be injected when needed.

## Project Status

VentPy is currently in `alpha`.

Implemented and exposed today:

- Personnel flow calculations
- Diesel flow calculations
- Blasting flow calculations
- Atmospheric correction utilities
- Integrated ventilation governor
- Python visualization helpers

Additional result types for dust, thermal, and leakage are already part of the public model, but the practical maturity of each workflow should be validated against your engineering use case before operational adoption.

## Installation

VentPy currently builds from source.

### Requirements

- Python `3.9+`
- CMake `3.20+`
- A compiler with `C++20` support

### Install in a Virtual Environment

```bash
python -m venv .venv
.venv\Scripts\activate
python -m pip install --upgrade pip
pip install .
```

For development:

```bash
pip install -e .[test,viz]
```

## CLI and Examples

Installing VentPy also installs the `ventpy` command, a thin presentation
layer over the same public API (no calculation logic of its own) with 5
subcommands:

- `ventpy demanda <archivo.json> [--norma peru|chile] [--json]` — total ventilation demand for a zone/face
- `ventpy lmp [--norma peru|chile] [--gas GAS] [--json]` — permissible exposure limits (LMP) for regulated gases
- `ventpy cobertura <archivo.json> [--norma peru|chile] [--json]` — coverage/deficit analysis of a zone survey
- `ventpy red <archivo.json> [--json]` — ventilation network balance (Hardy Cross)
- `ventpy ventilador <archivo.json> [--json]` — fan operating point (standalone or coupled to a network)

Exit codes are meaningful: `0` success, `1` invalid input, `2` a
calculated-but-unreliable result (e.g. a network that did not converge, or
a zone in deficit) — see each subcommand's help (`ventpy <subcommand> -h`).

Five worked, self-verifying examples (each `run.py` asserts its own
documented numbers) live under [`examples/`](examples/), one per JSON input
schema plus a duct-sizing case that only uses the API (no CLI subcommand
for it yet):

- [`examples/01-demanda-peru`](examples/01-demanda-peru/) — diesel-limited demand, Peru preset
- [`examples/02-preset-chile`](examples/02-preset-chile/) — same case with `--norma chile`
- [`examples/03-cobertura-levantamiento`](examples/03-cobertura-levantamiento/) — 2-zone survey, one zone in deficit (`exit 2`)
- [`examples/04-red-ventilador`](examples/04-red-ventilador/) — fan coupled to a parallel-mesh network
- [`examples/05-ducto-seleccion`](examples/05-ducto-seleccion/) — duct sizing, technical vs. economic criterion (API only)

## Quick Start

```python
import ventpy

config = ventpy.RegulatoryConfig()
governor = ventpy.VentilationGovernor(config)

inp = ventpy.VentilationInput()
inp.num_workers = 15
inp.altitude_masl = 4200.0

result = governor.calculate_total_demand(inp)

print(f"Q_total = {result.q_total_m3min} m3/min")
print(f"Q_total = {result.q_total_cfm:.1f} cfm")
print(f"Governing factor = {result.governing_factor}")
```

## Example With Diesel Fleet and Blasting

```python
import ventpy

config = ventpy.RegulatoryConfig()
governor = ventpy.VentilationGovernor(config)

inp = ventpy.VentilationInput()
inp.zone_type = ventpy.ZoneType.DevelopmentFace
inp.face_area_m2 = 15.0
inp.face_length_m = 120.0
inp.safety_factor = 1.10
inp.simultaneity_factor = 0.85

inp.atmospheric = ventpy.AtmosphericParams()
inp.atmospheric.altitude_masl = 4200.0
inp.atmospheric.dry_bulb_temp_c = 22.0

inp.personnel = ventpy.PersonnelParams()
inp.personnel.num_workers = 12
inp.personnel.activity = ventpy.ActivityLevel.Moderate

fleet = ventpy.DieselFleet()
fleet.add_equipment("Scooptram ST1030", 180.0, 0.88, 0.75)
inp.diesel_fleet = fleet

blast = ventpy.BlastingParams()
blast.explosive_kg = 50.0
blast.explosive_type = ventpy.ExplosiveType.ANFO
blast.dilution_time_min = 30.0
blast.face_area_m2 = 15.0
blast.face_length_m = 120.0
inp.blasting_params = blast

result = governor.calculate_total_demand(inp)

print(result.q_personnel_m3min)
print(result.q_diesel_m3min)
print(result.q_blasting_m3min)
print(result.q_total_m3min)
print(result.velocity_at_face_mps)
print(result.warnings)
```

## Core API

Main entry points:

- `RegulatoryConfig`
- `VentilationGovernor`
- `VentilationInput`
- `DieselFleet`
- `AtmosphericParams`
- `PersonnelParams`
- `BlastingParams`

Direct calculators:

- `calculate_personnel_flow(...)`
- `calculate_diesel_flow(...)`
- `calculate_blasting_flow(...)`
- `calculate_atmospheric_corrections(...)`

Useful utilities:

- `calculate_pressure_kpa(...)`
- `calculate_density_kg_m3(...)`
- `calculate_volume_correction_factor(...)`
- `calculate_diesel_derate_factor(...)`
- `get_min_velocity(...)`
- `safety_ceil(...)`

## Visualization

VentPy includes a Python visualization module intended for quick engineering communication and reporting.

Available helpers include:

- `plot_flow_comparison(...)`
- `plot_flow_breakdown(...)`
- `create_dashboard(...)`
- `generate_html_report(...)`

Example:

```python
from ventpy import visualization as viz

html = viz.generate_html_report(
    {
        "q_personnel_m3min": 45.0,
        "q_diesel_m3min": 267.0,
        "q_blasting_m3min": 50.0,
        "q_dust_m3min": 0.0,
        "q_thermal_m3min": 0.0,
        "q_leakage_m3min": 50.0,
        "q_governing_m3min": 267.0,
        "q_total_m3min": 317.0,
    },
    include_charts=False,
)
```

If you want chart rendering, install `matplotlib` in your environment.
The optional visualization dependency set can be installed with `pip install -e .[viz]`.

## Development

Run Python tests with:

```bash
pytest
```

The project also contains C++ tests that can be enabled through CMake when needed.

## Repository Layout

```text
include/          C++ headers for the calculation core
bindings/         nanobind Python bindings
python/ventpy/    Python package and visualization helpers
tests/            Python and C++ tests
```

## Engineering Disclaimer

VentPy is a calculation library, not a substitute for engineering judgment, ventilation surveys, field measurements, or regulatory review.

Use it as a computational aid. Final ventilation design decisions remain the responsibility of the qualified engineer of record.

## License

MIT
