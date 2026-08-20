# VentPy

[English](https://github.com/Miqueas7/VentPy/blob/master/README.md) | [Español](https://github.com/Miqueas7/VentPy/blob/master/README.es.md)

[![PyPI version](https://img.shields.io/pypi/v/ventpy)](https://pypi.org/project/ventpy/)
[![Python versions](https://img.shields.io/pypi/pyversions/ventpy)](https://pypi.org/project/ventpy/)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/Miqueas7/VentPy/blob/master/LICENSE)
[![Tests](https://img.shields.io/github/actions/workflow/status/Miqueas7/VentPy/tests.yml?branch=master&label=tests)](https://github.com/Miqueas7/VentPy/actions/workflows/tests.yml)

> **Documentación:** https://miqueas.dev/ventpy

Calculos de ventilacion subterranea de alto rendimiento para Python, respaldados por un nucleo en C++20.

VentPy ayuda a ingenieros de minas a estimar la demanda de ventilacion en operaciones subterraneas mediante una API auditable que cubre personal, flotas diesel, voladura, polvo, carga termica, correcciones atmosfericas, diseno de ductos/red y seleccion de ventilador, actualmente bajo los marcos regulatorios peruano (`DS 024-2016-EM` / `DS 023-2017-EM`) y chileno (`DS 132` / `DS 594`). Los calculos corren en un nucleo C++ tipado y probado en vez de una hoja de calculo: cada resultado trae su criterio gobernante y su cita normativa, no solo un numero.

## Que Hace VentPy

- Demanda de caudal por personal (escala de `DS 024-2016-EM, Art. 247` por altitud / `DS 132, Art. 138`)
- Demanda de caudal por flota diesel, incluida la dilucion de CO/NOx segun el tier de emision del motor
- Dilucion de gases de voladura
- Caudal por polvo respirable y por carga termica, integrados a la demanda consolidada
- Ajustes por fugas en ventilacion con ductos
- Correcciones atmosfericas por altitud (presion, densidad, presion parcial de oxigeno)
- Demanda consolidada de ventilacion con seleccion del factor gobernante (`VentilationGovernor`)
- Analisis de cobertura/deficit: caudal medido vs. requerido, por zona y para toda la mina
- Resistencia de Atkinson en labores y dimensionamiento de ducto (criterio tecnico y economico)
- Balance de red de ventilacion por el metodo de Hardy Cross, con deteccion automatica de mallas
- Curva de ventilador, punto de operacion y margen de stall, independiente o acoplado a una red
- Limites maximos permisibles (LMP) de gases regulados, por norma
- Una interfaz de linea de comandos `ventpy` y ayudas opcionales de visualizacion y reportes HTML

## Soporte Multi-Norma

VentPy trae preajustes oficiales para dos marcos regulatorios, cada constante citando su articulo:

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
- `RegulatoryConfig.chile()` — `DS 132`, Reglamento de Seguridad Minera (los limites de gases tambien usan el `DS 594`).
- `RegulatoryConfig.for_standard(standard)` — construye cualquiera de los dos preajustes a partir de un miembro del enum `RegulatoryStandard`; es lo que usa internamente el flag `--norma peru|chile` de la CLI.

Agregar la norma de otro pais es la contribucion que mas necesita VentPy — ver [Contribuir](#contribuir).

## Estado Del Proyecto

VentPy se encuentra en **beta**. Los calculos de personal, diesel, voladura, polvo, termico, cobertura, red, ventilador y atmosfera estan implementados, probados (197 pruebas en C++ y 151 en Python, incluidas pruebas basadas en propiedades y de escala hasta 500 ramales) y expuestos a Python para ambos preajustes.

> **Los resultados cambian entre 0.1.0 y 0.2.0.** La version 0.2.0 corrige tres citas normativas del nucleo. Si calculaste algo con 0.1.0, conviene rehacerlo, especialmente por encima de 1.500 msnm.
>
> - **Escala de caudal por persona** (`DS 024-2016-EM, Art. 247`): ahora es 3/4/5/6 m³/min por persona con umbrales en 1.500 / 3.000 / 4.000 msnm, que es el texto vigente del articulo. La 0.1.0 usaba una escala 3/4/5 con umbrales solo en 3.000 y 4.000 msnm, y calculaba de menos para toda mina por encima de 1.500 msnm.
> - **Polvo respirable**: la cita correcta es el `Art. 111` (limite de 3 mg/m³ para jornada de 8 h, con paralizacion obligatoria de la labor si se supera), no "Art. 103-107" como se citaba antes.
> - **Temperatura efectiva maxima**: la cita "Art. 240: 30 °C" no existe en la norma vigente, proviene del derogado `DS 055-2010-EM`. El criterio aplicable es el `Art. 252 lit. d` (velocidad minima de 30 m/min con temperatura seca entre 24 y 29 °C) mas el `Art. 104` / Anexo 13 para estres termico.

## Instalacion

```bash
pip install ventpy
```

Hay ruedas precompiladas para Python 3.9 a 3.13 en Linux, macOS y Windows, asi que no se necesita compilador ni CMake.

Para renderizado de graficos y generacion de reportes HTML, instala el extra de visualizacion:

```bash
pip install ventpy[viz]
```

## Inicio Rapido

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

Cinco ejemplos resueltos y auto-verificables (cada `run.py` afirma sus propios numeros documentados) viven en [`examples/`](https://github.com/Miqueas7/VentPy/tree/master/examples/), cubriendo demanda limitada por diesel, el preajuste chileno, cobertura/deficit, un ventilador acoplado a una red, y dimensionamiento de ducto.

## Interfaz De Linea De Comandos

Instalar VentPy tambien instala el comando `ventpy`, una capa de presentacion delgada sobre la misma API publica (sin logica de calculo propia) con 5 subcomandos:

- `ventpy demanda <archivo.json> [--norma peru|chile] [--json]` — demanda total de ventilacion de una zona/frente
- `ventpy lmp [--norma peru|chile] [--gas GAS] [--json]` — limites maximos permisibles (LMP) de gases regulados
- `ventpy cobertura <archivo.json> [--norma peru|chile] [--json]` — analisis de deficit/cobertura de un levantamiento de zonas
- `ventpy red <archivo.json> [--json]` — balance de una red de ventilacion (Hardy Cross)
- `ventpy ventilador <archivo.json> [--json]` — punto de operacion de un ventilador (independiente o acoplado a una red)

Los codigos de salida son significativos: `0` exito, `1` entrada invalida, `2` un resultado calculado pero no confiable (una red que no convergio, una zona en deficit, un punto de operacion de ventilador fuera de su curva de catalogo). Ver la ayuda de cada subcomando (`ventpy <subcomando> -h`).

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

## API Principal

VentPy expone 87 nombres publicos (`ventpy.__all__`). Puntos de entrada principales:

- `RegulatoryConfig` (`.peru()`, `.chile()`, `.for_standard(...)`)
- `VentilationGovernor`, `VentilationInput`
- `DieselFleet`, `AtmosphericParams`, `PersonnelParams`, `BlastingParams`, `DustParams`, `ThermalParams`

Calculadores directos:

- `calculate_personnel_flow`, `calculate_diesel_flow`, `calculate_blasting_flow`, `calculate_dust_flow`, `calculate_thermal_flow`, `calculate_atmospheric_corrections`

Red, ducto y ventilador:

- `AtkinsonCalculator`, `DuctSizingCalculator`, `NetworkSolver`, `FanCalculator`

Cobertura y limites de gases:

- `CoverageCalculator`, `gas_limits(standard)`, `lmp_for(standard, gas)`

Utilidades:

- `calculate_pressure_kpa`, `calculate_density_kg_m3`, `calculate_volume_correction_factor`, `calculate_diesel_derate_factor`, `get_min_velocity`, `safety_ceil`, `safety_ceil_decimals`

## Visualizacion

VentPy incluye un modulo de visualizacion en Python para comunicacion tecnica y reporte rapido: `plot_flow_comparison`, `plot_flow_breakdown`, `create_dashboard`, `generate_html_report`.

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

El renderizado de graficos requiere `matplotlib`, incluido en el extra `viz` (`pip install ventpy[viz]`).

## Contribuir

Ver [`CONTRIBUTING.md`](https://github.com/Miqueas7/VentPy/blob/master/CONTRIBUTING.md). La contribucion mas buscada es agregar la norma de otro pais, siguiendo el patron del preajuste chileno (`RegulatoryConfig::chile()`): ubicar los valores en el reglamento, citar cada articulo, agregar un preajuste y agregar un test con un caso conocido. Ver [`CHANGELOG.md`](https://github.com/Miqueas7/VentPy/blob/master/CHANGELOG.md) para el historial completo de versiones.

## Descargo De Ingenieria

VentPy es una libreria de calculo, no un sustituto del criterio ingenieril, los aforos de ventilacion, las mediciones de campo ni la revision regulatoria.

Usalo como apoyo computacional. Las decisiones finales de diseno de ventilacion siguen siendo responsabilidad del ingeniero competente a cargo.

## Licencia

MIT — ver [`LICENSE`](https://github.com/Miqueas7/VentPy/blob/master/LICENSE).
