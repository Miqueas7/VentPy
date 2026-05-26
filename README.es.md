# VentPy

[English](README.md) | [Español](README.es.md)

Calculos de ventilacion subterranea de alto rendimiento para Python, respaldados por un nucleo en C++20.

VentPy ayuda a ingenieros de minas a estimar la demanda de ventilacion en operaciones subterraneas mediante una API auditable orientada al marco regulatorio peruano `DS 024-2016-EM / DS 023-2017-EM`.

## Que Hace VentPy

VentPy proporciona herramientas de calculo para:

- Demanda de caudal por personal
- Demanda de caudal por flota diesel
- Dilucion de gases de voladura
- Correcciones atmosfericas por altitud
- Ajustes por fugas en ventilacion con ductos
- Demanda consolidada de ventilacion con seleccion del factor gobernante
- Ayudas opcionales de visualizacion y reportes HTML en Python

La libreria esta orientada a flujos de trabajo de ingenieria donde importan el rendimiento, la reproducibilidad y la trazabilidad.

## Por Que Existe Esta Libreria

Los calculos de ventilacion de mina suelen implementarse en hojas de calculo que se vuelven dificiles de validar, reutilizar o integrar en flujos de trabajo mayores. VentPy traslada esos calculos a una libreria tipada y testeable con:

- Un nucleo en C++ para rendimiento predecible
- Bindings de Python para scripting, analisis e integracion
- Objetos de resultado explicitos en lugar de formulas opacas en hojas de calculo
- Entradas orientadas al dominio para personal, equipos diesel, voladura, atmosfera y fugas

## Alcance Regulatorio

Los valores por defecto actuales se basan en:

- `DS 024-2016-EM`
- `DS 023-2017-EM`

La `RegulatoryConfig` por defecto refleja este marco peruano, pero permite inyectar parametros corporativos mas exigentes cuando sea necesario.

## Estado Del Proyecto

VentPy se encuentra actualmente en estado `alpha`.

Implementado y expuesto hoy:

- Calculos de caudal por personal
- Calculos de caudal por equipos diesel
- Calculos de caudal por explosivos
- Utilidades de correccion atmosferica
- Governor integrado de demanda de ventilacion
- Ayudas de visualizacion en Python

Los tipos de resultado para polvo, termica y fugas ya forman parte del modelo publico, pero la madurez practica de cada flujo debe validarse frente a tu caso de uso de ingenieria antes de una adopcion operativa.

## Instalacion

VentPy se construye actualmente desde codigo fuente.

### Requisitos

- Python `3.9+`
- CMake `3.20+`
- Un compilador con soporte `C++20`

### Instalacion En Un Entorno Virtual

```bash
python -m venv .venv
.venv\Scripts\activate
python -m pip install --upgrade pip
pip install .
```

Para desarrollo:

```bash
pip install -e .[test,viz]
```

## Inicio Rapido

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
print(f"Factor gobernante = {result.governing_factor}")
```

## Ejemplo Con Flota Diesel Y Voladura

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

## API Principal

Puntos de entrada principales:

- `RegulatoryConfig`
- `VentilationGovernor`
- `VentilationInput`
- `DieselFleet`
- `AtmosphericParams`
- `PersonnelParams`
- `BlastingParams`

Calculadores directos:

- `calculate_personnel_flow(...)`
- `calculate_diesel_flow(...)`
- `calculate_blasting_flow(...)`
- `calculate_atmospheric_corrections(...)`

Utilidades relevantes:

- `calculate_pressure_kpa(...)`
- `calculate_density_kg_m3(...)`
- `calculate_volume_correction_factor(...)`
- `calculate_diesel_derate_factor(...)`
- `get_min_velocity(...)`
- `safety_ceil(...)`

## Visualizacion

VentPy incluye un modulo de visualizacion en Python orientado a comunicacion tecnica y reporte rapido.

Entre las ayudas disponibles se incluyen:

- `plot_flow_comparison(...)`
- `plot_flow_breakdown(...)`
- `create_dashboard(...)`
- `generate_html_report(...)`

Ejemplo:

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

Si necesitas renderizado de graficos, instala `matplotlib` en tu entorno.
El conjunto opcional de dependencias de visualizacion puede instalarse con `pip install -e .[viz]`.

## Desarrollo

Ejecuta las pruebas de Python con:

```bash
pytest
```

El proyecto tambien contiene pruebas C++ que pueden habilitarse mediante CMake cuando sea necesario.

## Estructura Del Repositorio

```text
include/          Cabeceras C++ del nucleo de calculo
bindings/         Bindings de Python con nanobind
python/ventpy/    Paquete Python y ayudas de visualizacion
tests/            Pruebas Python y C++
```

## Descargo De Ingenieria

VentPy es una libreria de calculo, no un sustituto del criterio ingenieril, los aforos de ventilacion, las mediciones de campo ni la revision regulatoria.

Usalo como apoyo computacional. Las decisiones finales de diseno de ventilacion siguen siendo responsabilidad del ingeniero competente a cargo.

## Licencia

MIT
