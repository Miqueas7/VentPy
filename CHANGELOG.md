# Registro de cambios

Todas las versiones publicadas de VentPy. El formato sigue
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y el versionado es
[semántico](https://semver.org/lang/es/).

## [0.2.0] — 2026-08-20

### ⚠️ Los resultados cambian respecto de 0.1.0

Esta versión corrige tres citas normativas del núcleo. **Un cálculo hecho con
0.1.0 puede quedar por debajo del mínimo legal**, así que conviene rehacer los
que se hayan emitido con esa versión.

| Corrección | 0.1.0 | 0.2.0 |
|---|---|---|
| Escala de caudal por persona (DS 024-2016-EM, **Art. 247**) | 3/4/5 m³/min con umbrales de 3.000 y 4.000 msnm | **3/4/5/6** m³/min con umbrales de **1.500, 3.000 y 4.000** msnm, que es el texto vigente del artículo |
| Polvo respirable | citado como "Art. 103-107" | **Art. 111**: límite de 3 mg/m³ para jornada de 8 h, con paralización de la labor si se supera |
| Temperatura efectiva máxima | citada como "Art. 240: 30 °C" | No existe en la norma vigente (proviene del derogado DS 055-2010-EM). El criterio normativo real es el **Art. 252 lit. d** (velocidad mínima de 30 m/min con temperatura seca entre 24 y 29 °C) y la remisión del **Art. 104 / Anexo 13** para estrés térmico |

El efecto práctico de la primera corrección: toda mina por encima de 1.500 msnm
exige más aire por persona del que 0.1.0 calculaba.

### Cambio incompatible

- El constructor de `RegulatoryConfig` pasa de 10 a 12 parámetros por el tercer
  umbral de altitud del Art. 247. Quien use los preajustes
  (`RegulatoryConfig.peru()`, `.chile()`, `.for_standard()`) no necesita
  cambiar nada; solo afecta a quien construya la configuración parámetro a
  parámetro.

### Nuevas capacidades

- **Multi-norma.** Preajustes por país mediante `peru()`, `chile()` y
  `for_standard()`. El preajuste chileno implementa el DS 132 (Art. 138:
  3 m³/min por persona sin escalón por altitud; Art. 132: 2,83 m³/min por HP
  efectivo al freno).
- **Límites máximos permisibles de gases** consultables por norma
  (`gas_limits`, `lmp_for`): Perú (DS 024, Anexo 15) y Chile (DS 594 / DS 132),
  con la cita de cada valor. Un gas no regulado por la norma lanza una
  excepción en vez de devolver un valor por omisión.
- **Cobertura y déficit de ventilación** (Art. 252, literales f y g): compara
  el caudal medido en campo, directo o por estaciones de aforo, contra el
  requerido, por labor y para la mina completa, con verificación de la
  velocidad del aire del Art. 248.
- **Red de ventilación**: resistencias de Atkinson con corrección por densidad
  y pérdidas por choque, dimensionamiento de ducto con criterio técnico y
  económico, balance de red por el método de Hardy Cross con detección
  automática de mallas, y curva de ventilador con punto de operación y margen
  de entrada en pérdida.
- **Polvo respirable y carga térmica** integrados al cálculo consolidado.
- **Interfaz de línea de comandos** `ventpy` con cinco subcomandos sobre
  archivos JSON y códigos de salida pensados para automatización: 0 correcto,
  1 error de entrada y 2 resultado no confiable.
- **Ejemplos resueltos** en `examples/`, cada uno con su comando, la salida
  esperada y el artículo que lo sustenta.

### Correcciones

- El solver de red rechaza resistencias no finitas. Un valor infinito se
  aceptaba y el balance devolvía caudales inválidos marcados como convergidos.
- Las funciones de validación rechazan NaN e infinito en todos sus parámetros.
- El cálculo simplificado de gases de voladura recupera la advertencia por
  exceder el tiempo máximo de dilución del Art. 243.

### Interno

- La suite pasó de 55 a 197 pruebas en C++ y 151 en Python, incluidas pruebas
  basadas en propiedades y de escala hasta 500 ramales.
- Integración continua en Linux, macOS y Windows, con analizadores dinámicos
  de memoria y comportamiento indefinido, y medición de cobertura.

## [0.1.0] — 2026-08-16

- Primera versión publicada: cálculo de demanda de ventilación según
  DS 024-2016-EM (personal, equipos diésel, explosivos y fugas), correcciones
  atmosféricas por altitud y visualización opcional.
