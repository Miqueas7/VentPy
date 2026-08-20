# VentPy

[English](https://github.com/Miqueas7/VentPy/blob/master/README.md) | [Español](https://github.com/Miqueas7/VentPy/blob/master/README.es.md)

[![PyPI version](https://img.shields.io/pypi/v/ventpy)](https://pypi.org/project/ventpy/)
[![Python versions](https://img.shields.io/pypi/pyversions/ventpy)](https://pypi.org/project/ventpy/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Miqueas7/VentPy/blob/master/LICENSE)
[![Tests](https://img.shields.io/github/actions/workflow/status/Miqueas7/VentPy/tests.yml?branch=master&label=tests)](https://github.com/Miqueas7/VentPy/actions/workflows/tests.yml)

> **Documentación:** **https://miqueas.dev/ventpy** — bilingüe, con casos resueltos, las tablas normativas generadas desde la propia librería y la matriz de trazabilidad que cruza cada artículo con su código y su prueba.
>
> Por dónde empezar: [Instalación](https://miqueas.dev/ventpy/guia/instalacion/) · [Primer cálculo](https://miqueas.dev/ventpy/guia/primer-calculo/) · [El resultado auditable](https://miqueas.dev/ventpy/guia/resultado-auditable/) · [Casos resueltos](https://miqueas.dev/ventpy/casos/demanda/) · [Criterios de ingeniería](https://miqueas.dev/ventpy/fundamento/criterios/)

Cálculos de ventilación subterránea de alto rendimiento para Python, respaldados por un núcleo en C++20.

VentPy ayuda a ingenieros de minas a estimar la demanda de ventilación en operaciones subterráneas mediante una API auditable que cubre personal, flotas diesel, voladura, polvo, carga térmica, correcciones atmosféricas, diseño de ductos/red y selección de ventilador, actualmente bajo los marcos regulatorios peruano (`DS 024-2016-EM` / `DS 023-2017-EM`) y chileno (`DS 132` / `DS 594`). Los cálculos corren en un núcleo C++ tipado y probado en vez de una hoja de cálculo: cada resultado trae su criterio gobernante y su cita normativa, no solo un número.

## Qué hace VentPy

- Demanda de caudal por personal (escala de `DS 024-2016-EM, Art. 247` por altitud / `DS 132, Art. 138`)
- Demanda de caudal por flota diesel, incluida la dilución de CO/NOx según el tier de emisión del motor
- Dilución de gases de voladura
- Caudal por polvo respirable y por carga térmica, integrados a la demanda consolidada
- Ajustes por fugas en ventilación con ductos
- Correcciones atmosféricas por altitud (presión, densidad, presión parcial de oxígeno)
- Demanda consolidada de ventilación con selección del factor gobernante (`VentilationGovernor`)
- Análisis de cobertura/déficit: caudal medido vs. requerido, por zona y para toda la mina
- Resistencia de Atkinson en labores y dimensionamiento de ducto (criterio técnico y económico)
- Balance de red de ventilación por el método de Hardy Cross, con detección automática de mallas
- Curva de ventilador, punto de operación y margen de stall, independiente o acoplado a una red
- Límites máximos permisibles (LMP) de gases regulados, por norma
- Una interfaz de línea de comandos `ventpy` y ayudas opcionales de visualización y reportes HTML

## Soporte multi-norma

VentPy trae preajustes oficiales para dos marcos regulatorios, cada constante citando su artículo:

```python
import ventpy

peru = ventpy.calculate_personnel_flow(15, 4200.0, ventpy.RegulatoryConfig.peru())
chile = ventpy.calculate_personnel_flow(15, 4200.0, ventpy.RegulatoryConfig.chile())

print(f"Peru:  {peru.q_personnel} m3/min ({peru.flow_per_person_base} m3/min/person)")
print(f"Chile: {chile.q_personnel} m3/min ({chile.flow_per_person_base} m3/min/person)")
```

Salida:

```
Peru:  90.0 m3/min (6.0 m3/min/person)
Chile: 45.0 m3/min (3.0 m3/min/person)
```

- `RegulatoryConfig.peru()` — `DS 024-2016-EM` / `DS 023-2017-EM`.
- `RegulatoryConfig.chile()` — `DS 132`, Reglamento de Seguridad Minera (los límites de gases también usan el `DS 594`).
- `RegulatoryConfig.for_standard(standard)` — construye cualquiera de los dos preajustes a partir de un miembro del enum `RegulatoryStandard`; es lo que usa internamente el flag `--norma peru|chile` de la CLI.

Agregar la norma de otro país es la contribución que más necesita VentPy — ver [Contribuir](#contribuir).

## Estado del proyecto

VentPy se encuentra en **beta**. Los cálculos de personal, diesel, voladura, polvo, térmico, cobertura, red, ventilador y atmósfera están implementados, probados (219 pruebas en C++ y 176 en Python, incluidas pruebas basadas en propiedades y de escala hasta 500 ramales) y expuestos a Python para ambos preajustes.

> **En la 0.2.1 cambian las citas, no los números.** Hasta la 0.2.0, todo resultado citaba el marco peruano aunque se construyera con `RegulatoryConfig.chile()`: el caudal era correcto, pero la referencia legal impresa junto a él no. Desde la 0.2.1 la cita sigue a la norma configurada, y donde el `DS 132` no regula un concepto, la cita lo declara explícitamente en vez de tomar prestado un artículo peruano. Si emitiste informes con el preajuste chileno, conviene reemitirlos.

> **Los resultados cambian entre 0.1.0 y 0.2.0.** La versión 0.2.0 corrige tres citas normativas del núcleo. Si calculaste algo con 0.1.0, conviene rehacerlo, especialmente por encima de 1.500 msnm.
>
> - **Escala de caudal por persona** (`DS 024-2016-EM, Art. 247`): ahora es 3/4/5/6 m³/min por persona con umbrales en 1.500 / 3.000 / 4.000 msnm, que es el texto vigente del artículo. La 0.1.0 usaba una escala 3/4/5 con umbrales solo en 3.000 y 4.000 msnm, y calculaba de menos para toda mina por encima de 1.500 msnm.
> - **Polvo respirable**: la cita correcta es el `Art. 111` (límite de 3 mg/m³ para jornada de 8 h, con paralización obligatoria de la labor si se supera), no "Art. 103-107" como se citaba antes.
> - **Temperatura efectiva máxima**: la cita "Art. 240: 30 °C" no existe en la norma vigente, proviene del derogado `DS 055-2010-EM`. El criterio aplicable es el `Art. 252 lit. d` (velocidad mínima de 30 m/min con temperatura seca entre 24 y 29 °C) más el `Art. 104` / Anexo 13 para estrés térmico.

## Instalación

```bash
pip install ventpy
```

Hay ruedas precompiladas para Python 3.9 a 3.13 en Linux, macOS y Windows, así que no se necesita compilador ni CMake.

Para renderizado de gráficos y generación de reportes HTML, instala el extra de visualización:

```bash
pip install ventpy[viz]
```

## Inicio rápido

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

Salida:

```
Q_total = 207.0 m3/min
Q_total = 7310.1 cfm
Governing factor = personnel (Q_Per)
```

Cinco ejemplos resueltos y auto-verificables (cada `run.py` afirma sus propios números documentados) viven en [`examples/`](https://github.com/Miqueas7/VentPy/tree/master/examples/), cubriendo demanda limitada por diesel, el preajuste chileno, cobertura/déficit, un ventilador acoplado a una red, y dimensionamiento de ducto.

## Interfaz de línea de comandos

Instalar VentPy también instala el comando `ventpy`, una capa de presentación delgada sobre la misma API pública (sin lógica de cálculo propia) con 5 subcomandos:

- `ventpy demanda <archivo.json> [--norma peru|chile] [--json]` — demanda total de ventilación de una zona/frente
- `ventpy lmp [--norma peru|chile] [--gas GAS] [--json]` — límites máximos permisibles (LMP) de gases regulados
- `ventpy cobertura <archivo.json> [--norma peru|chile] [--json]` — análisis de déficit/cobertura de un levantamiento de zonas
- `ventpy red <archivo.json> [--json]` — balance de una red de ventilación (Hardy Cross)
- `ventpy ventilador <archivo.json> [--json]` — punto de operación de un ventilador (independiente o acoplado a una red)

Los códigos de salida son significativos: `0` éxito, `1` entrada inválida, `2` un resultado calculado pero no confiable (una red que no convergió, una zona en déficit, un punto de operación de ventilador fuera de su curva de catálogo). Ver la ayuda de cada subcomando (`ventpy <subcomando> -h`).

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

## API principal

VentPy expone 87 nombres públicos (`ventpy.__all__`). Puntos de entrada principales:

- `RegulatoryConfig` (`.peru()`, `.chile()`, `.for_standard(...)`)
- `VentilationGovernor`, `VentilationInput`
- `DieselFleet`, `AtmosphericParams`, `PersonnelParams`, `BlastingParams`, `DustParams`, `ThermalParams`

Calculadores directos:

- `calculate_personnel_flow`, `calculate_diesel_flow`, `calculate_blasting_flow`, `calculate_dust_flow`, `calculate_thermal_flow`, `calculate_atmospheric_corrections`

Red, ducto y ventilador:

- `AtkinsonCalculator`, `DuctSizingCalculator`, `NetworkSolver`, `FanCalculator`

Cobertura y límites de gases:

- `CoverageCalculator`, `gas_limits(standard)`, `lmp_for(standard, gas)`

Utilidades:

- `calculate_pressure_kpa`, `calculate_density_kg_m3`, `calculate_volume_correction_factor`, `calculate_diesel_derate_factor`, `get_min_velocity`, `safety_ceil`, `safety_ceil_decimals`

## Visualización

VentPy incluye un módulo de visualización en Python para comunicación técnica y reporte rápido: `plot_flow_comparison`, `plot_flow_breakdown`, `create_dashboard`, `generate_html_report`.

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

El renderizado de gráficos requiere `matplotlib`, incluido en el extra `viz` (`pip install ventpy[viz]`).

## Contribuir

Ver [`CONTRIBUTING.md`](https://github.com/Miqueas7/VentPy/blob/master/CONTRIBUTING.md). La contribución más buscada es agregar la norma de otro país, siguiendo el patrón del preajuste chileno (`RegulatoryConfig::chile()`): ubicar los valores en el reglamento, citar cada artículo, agregar un preajuste y agregar un test con un caso conocido. Ver [`CHANGELOG.md`](https://github.com/Miqueas7/VentPy/blob/master/CHANGELOG.md) para el historial completo de versiones.

## Descargo de ingeniería

VentPy es una librería de cálculo, no un sustituto del criterio ingenieril, los aforos de ventilación, las mediciones de campo ni la revisión regulatoria.

Úsalo como apoyo computacional. Las decisiones finales de diseño de ventilación siguen siendo responsabilidad del ingeniero competente a cargo.

## Licencia

MIT — ver [`LICENSE`](https://github.com/Miqueas7/VentPy/blob/master/LICENSE).
