# Ejemplo 3 — Levantamiento de cobertura: la mina cubre, una zona no

## Que calcula

El analisis de deficit/cobertura de un levantamiento de campo
(`CoverageCalculator.analyze_survey`) con 2 zonas de tipo `DevelopmentFace`
(10 trabajadores cada una):

| Zona          | Altitud   | Requerido | Medido | Cobertura |
|---------------|-----------|-----------|--------|-----------|
| Rampa 4200    | 4200 msnm | 207 m3/min| 270    | 130.4 %   |
| Frente N-02   | 2500 msnm | 207 m3/min| 150    | 72.5 %    |
| **Total mina**|           | **414**   | **420**| **101.4 %**|

## Comando CLI

```bash
ventpy cobertura examples/03-cobertura-levantamiento/input.json --json
```

## Salida esperada (claves relevantes) — y exit code 2 INTENCIONAL

```json
{
  "global_compliant": true,
  "all_zones_compliant": false,
  "compliant": false,
  "zones": [
    {"zone_name": "Rampa 4200",  "deficit_m3min": 0.0,  "compliant": true},
    {"zone_name": "Frente N-02", "deficit_m3min": 57.0, "compliant": false}
  ]
}
```

El comando anterior **termina con `exit code 2`**, no 0. Esto es
intencional: el CLI usa exit codes para distinguir "error de entrada"
(1) de "resultado calculado correctamente pero no confiable" (2, ver
`python/ventpy/cli.py::_flags_exit`). Aqui `compliant = false` porque, aunque
la mina en conjunto cubre el caudal total requerido (420 >= 414), la zona
"Frente N-02" individualmente esta en deficit — un resultado que un
ingeniero de guardia NO deberia poder pasar por alto solo mirando el
agregado. `pytest`/CI que invoquen este comando deben esperar `exit == 2`
para este `input.json`, no `0`.

## Fundamento normativo

- **DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252, lit. f**: la mina en su
  conjunto debe recibir un caudal de aire igual o mayor al requerido —
  criterio de `global_compliant` (Σ medido ≥ Σ requerido). Aqui se cumple:
  420 ≥ 414.
- **DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252, lit. g**: cada zona
  individual del levantamiento debe estar cubierta — criterio de
  `all_zones_compliant`. Aqui NO se cumple: "Frente N-02" tiene un deficit
  de **57 m3/min** (207 requerido − 150 medido).

El campo `compliant` del resultado es la conjuncion estricta de ambos
literales (f Y g) — exactamente el criterio que exige el exit code 2.

## Ejecutar

```bash
python examples/03-cobertura-levantamiento/run.py
```

El script construye las 2 `ZoneSurvey` via API publico
(`ZoneMeasurement`/`VentilationInput`) y llama
`CoverageCalculator.analyze_survey` directamente — sin pasar por
`ventpy.cli`. Termina en `assert` con exit 0 (es un script Python normal,
no el CLI): el `exit 2` documentado arriba es un comportamiento del
subcomando `cobertura` del CLI, no de este script.
