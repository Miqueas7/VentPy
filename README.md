# VentPy

[English](https://github.com/Miqueas7/VentPy/blob/master/README.md) | [Español](https://github.com/Miqueas7/VentPy/blob/master/README.es.md)

[![PyPI version](https://img.shields.io/pypi/v/ventpy)](https://pypi.org/project/ventpy/)
[![Python versions](https://img.shields.io/pypi/pyversions/ventpy)](https://pypi.org/project/ventpy/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Miqueas7/VentPy/blob/master/LICENSE)
[![Tests](https://img.shields.io/github/actions/workflow/status/Miqueas7/VentPy/tests.yml?branch=master&label=tests)](https://github.com/Miqueas7/VentPy/actions/workflows/tests.yml)

> **Documentation:** https://miqueas.dev/ventpy

High-performance underground mine ventilation calculations for Python, backed by a C++20 core.

VentPy helps mining engineers estimate ventilation demand for underground operations through an auditable API covering personnel, diesel fleets, blasting, dust, thermal load, atmospheric corrections, duct/network design and fan selection, currently under the Peruvian (`DS 024-2016-EM` / `DS 023-2017-EM`) and Chilean (`DS 132` / `DS 594`) regulatory frameworks. Calculations run in a typed, tested C++ core instead of a spreadsheet: every result carries its governing criterion and its regulatory citation, not just a number.

## What VentPy Does

- Personnel-based airflow demand (`DS 024-2016-EM, Art. 247` altitude scale / `DS 132, Art. 138`)
- Diesel fleet airflow demand, including CO/NOx dilution by engine emission tier
- Blasting gas dilution airflow
- Respirable dust and thermal-load airflow, integrated into the consolidated demand
- Leakage adjustments for ducted ventilation
- Atmospheric corrections at altitude (pressure, density, oxygen partial pressure)
- Consolidated ventilation demand with governing-factor selection (`VentilationGovernor`)
- Coverage/deficit analysis: measured airflow vs. required, by zone and for the whole mine
- Atkinson airway resistance and duct sizing (technical and economic criteria)
- Ventilation network balance by the Hardy Cross method, with automatic mesh detection
- Fan curve, operating point and stall margin, standalone or coupled to a network
- Permissible exposure limits (LMP) for regulated gases, by standard
- A `ventpy` command-line interface and optional visualization/HTML reporting helpers

## Multi-Standard Support

VentPy ships official presets for two regulatory frameworks, each constant citing its article:

```python
import ventpy

peru = ventpy.calculate_personnel_flow(15, 4200.0, ventpy.RegulatoryConfig.peru())
chile = ventpy.calculate_personnel_flow(15, 4200.0, ventpy.RegulatoryConfig.chile())

print(f"Peru:  {peru.q_personnel} m3/min ({peru.flow_per_person_base} m3/min/person)")
print(f"Chile: {chile.q_personnel} m3/min ({chile.flow_per_person_base} m3/min/person)")
```

Output:

```
Peru:  90.0 m3/min (6.0 m3/min/person)
Chile: 45.0 m3/min (3.0 m3/min/person)
```

- `RegulatoryConfig.peru()` — `DS 024-2016-EM` / `DS 023-2017-EM`.
- `RegulatoryConfig.chile()` — `DS 132`, Reglamento de Seguridad Minera (gas limits also draw on `DS 594`).
- `RegulatoryConfig.for_standard(standard)` — builds either preset from a `RegulatoryStandard` enum member; used internally by the CLI's `--norma peru|chile` flag.

Adding another country's standard is the contribution VentPy needs most — see [Contributing](#contributing).

## Project Status

VentPy is in **beta**. Personnel, diesel, blasting, dust, thermal, coverage, network, fan and atmospheric calculations are implemented, tested (197 C++ tests and 151 Python tests, including property-based and 500-branch scale tests) and exposed to Python for both presets.

> **Results changed between 0.1.0 and 0.2.0.** Version 0.2.0 corrects three normative citations in the core. If you calculated anything with 0.1.0, recompute it, especially above 1,500 masl.
>
> - **Personnel airflow scale** (`DS 024-2016-EM, Art. 247`): now 3/4/5/6 m³/min per person with thresholds at 1,500 / 3,000 / 4,000 masl, matching the article's actual text. 0.1.0 used a 3/4/5 scale with thresholds at 3,000 and 4,000 masl only, and under-counted every mine above 1,500 masl.
> - **Respirable dust**: the correct citation is `Art. 111` (3 mg/m³ limit for an 8-hour shift, with mandatory work stoppage above it), not "Art. 103-107" as previously cited.
> - **Maximum effective temperature**: the "Art. 240: 30°C" citation does not exist in the current regulation, it came from the repealed `DS 055-2010-EM`. The applicable rule is `Art. 252.d` (30 m/min minimum velocity between 24-29°C dry bulb) plus `Art. 104` / Annex 13 for heat stress.

## Installation

```bash
pip install ventpy
```

Prebuilt wheels are published for Python 3.9 to 3.13 on Linux, macOS and Windows, so no compiler or CMake is required.

For chart rendering and HTML report generation, install the visualization extra:

```bash
pip install ventpy[viz]
```

## Quick Start

```python
import ventpy

config = ventpy.RegulatoryConfig.peru()
governor = ventpy.VentilationGovernor(config)

inp = ventpy.VentilationInput()
inp.zone_type = ventpy.ZoneType.DevelopmentFace
inp.num_workers = 15
inp.altitude_masl = 4200.0

result = governor.calculate_total_demand(inp)

print(f"Q_total = {result.q_total_m3min} m3/min")
print(f"Q_total = {result.q_total_cfm:.1f} cfm")
print(f"Governing factor = {result.governing_factor}")
```

Output:

```
Q_total = 207.0 m3/min
Q_total = 7310.1 cfm
Governing factor = personnel (Q_Per)
```

Five worked, self-verifying examples (each `run.py` asserts its own documented numbers) live under [`examples/`](https://github.com/Miqueas7/VentPy/tree/master/examples/), covering diesel-limited demand, the Chilean preset, coverage/deficit, a fan coupled to a network, and duct sizing.

## Command Line Interface

Installing VentPy also installs the `ventpy` command, a thin presentation layer over the same public API (no calculation logic of its own) with 5 subcommands:

- `ventpy demanda <file.json> [--norma peru|chile] [--json]` — total ventilation demand for a zone/face
- `ventpy lmp [--norma peru|chile] [--gas GAS] [--json]` — permissible exposure limits (LMP) for regulated gases
- `ventpy cobertura <file.json> [--norma peru|chile] [--json]` — coverage/deficit analysis of a zone survey
- `ventpy red <file.json> [--json]` — ventilation network balance (Hardy Cross)
- `ventpy ventilador <file.json> [--json]` — fan operating point (standalone or coupled to a network)

Exit codes are meaningful: `0` success, `1` invalid input, `2` a calculated-but-unreliable result (a network that did not converge, a zone in deficit, a fan operating point outside its catalog curve). See each subcommand's help (`ventpy <subcommand> -h`).

```
$ ventpy lmp --norma peru --gas CO
standard: DS024_Peru
gas: CO
unit: PPM
twa_8h: 25.0
stel: None
ceiling: None
floor_min: None
regulation_ref: DS 024-2016-EM, Anexo 15, fila 33 (via Art. 246)
```

## Core API

VentPy exposes 87 public names (`ventpy.__all__`). Main entry points:

- `RegulatoryConfig` (`.peru()`, `.chile()`, `.for_standard(...)`)
- `VentilationGovernor`, `VentilationInput`
- `DieselFleet`, `AtmosphericParams`, `PersonnelParams`, `BlastingParams`, `DustParams`, `ThermalParams`

Direct calculators:

- `calculate_personnel_flow`, `calculate_diesel_flow`, `calculate_blasting_flow`, `calculate_dust_flow`, `calculate_thermal_flow`, `calculate_atmospheric_corrections`

Network, duct and fan:

- `AtkinsonCalculator`, `DuctSizingCalculator`, `NetworkSolver`, `FanCalculator`

Coverage and gas limits:

- `CoverageCalculator`, `gas_limits(standard)`, `lmp_for(standard, gas)`

Utilities:

- `calculate_pressure_kpa`, `calculate_density_kg_m3`, `calculate_volume_correction_factor`, `calculate_diesel_derate_factor`, `get_min_velocity`, `safety_ceil`, `safety_ceil_decimals`

## Visualization

VentPy includes a Python visualization module for quick engineering communication and reporting: `plot_flow_comparison`, `plot_flow_breakdown`, `create_dashboard`, `generate_html_report`.

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

Chart rendering requires `matplotlib`, included in the `viz` extra (`pip install ventpy[viz]`).

## Contributing

See [`CONTRIBUTING.md`](https://github.com/Miqueas7/VentPy/blob/master/CONTRIBUTING.md). The most-requested contribution is adding another country's regulatory standard, following the Chilean preset pattern (`RegulatoryConfig::chile()`): locate the values in the regulation, cite each article, add a preset, and add a test with a known case. See [`CHANGELOG.md`](https://github.com/Miqueas7/VentPy/blob/master/CHANGELOG.md) for the full version history.

## Engineering Disclaimer

VentPy is a calculation library, not a substitute for engineering judgment, ventilation surveys, field measurements, or regulatory review.

Use it as a computational aid. Final ventilation design decisions remain the responsibility of the qualified engineer of record.

## License

MIT — see [`LICENSE`](https://github.com/Miqueas7/VentPy/blob/master/LICENSE).
