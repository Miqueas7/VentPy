# Registro de cambios

Todas las versiones publicadas de VentPy. El formato sigue
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y el versionado es
[semántico](https://semver.org/lang/es/).

## [0.3.0] — 2026-11-12

### El caudal de voladura cambia

Hasta la 0.2.1, declarar `gas_volume_per_kg` en `BlastingParams` desviaba el
cálculo a una fórmula que dividía el volumen total de gases entre el tiempo de
dilución. Ese cociente es el ritmo al que se generan los gases, no el caudal
que los diluye hasta un límite de exposición. Con 50 kg de explosivo,
0,04 m³/kg y 30 min daba 0,067 m³/min, que el redondeo de seguridad subía a 1.
El defecto alcanzaba a `calculate_blasting_flow` y también a
`VentilationGovernor`, y estaba presente desde la 0.1.0.

Desde la 0.3.0 hay un solo modelo: dilución de CO y de NOx según el tipo de
explosivo, recambio volumétrico y velocidad mínima de barrido.
`gas_volume_per_kg` ya no cambia el caudal; solo fija el volumen total de
gases que se reporta en `total_gas_volume_m3`.

**A quién afecta:** a quien haya calculado una demanda con voladura declarando
`gas_volume_per_kg`. Conviene recalcular: el caudal de voladura sube varios
órdenes de magnitud y puede pasar a ser el criterio gobernante.

| Caso (50 kg de ANFO, 30 min, frente de 12 m²) | 0.2.1 | 0.3.0 |
|---|---|---|
| `calculate_blasting_flow`, nivel del mar | 1 m³/min | 4.000 m³/min (dilución de NOx) |
| `VentilationGovernor`, 4.200 msnm, con un scoop de 150 HP | `Q_Exp` = 1; gobierna el diésel; total 981 | `Q_Exp` = 6.751; gobierna la voladura; total 7.764 |

Quien no declaraba `gas_volume_per_kg` ya obtenía en el Governor el modelo por
contaminante: sus resultados no cambian.

- `VentilationGovernor` exige ahora sección y longitud del frente para toda
  voladura (las toma de `VentilationInput` si la voladura no las trae). Antes,
  con `gas_volume_per_kg`, no las necesitaba.
- `calculate_blasting_flow` usa el mismo modelo sin corrección por altitud,
  porque no recibe atmósfera, y omite los dos criterios de sección si no se
  dan `face_area_m2` y `face_length_m`. Sin `gas_volume_per_kg` también
  devolvía 1 m³/min.
- El ejemplo `01-demanda-peru` ya no incluye voladura, para seguir mostrando
  el caso gobernado por el diésel. Su README explica el cambio.
- El modelo por contaminante es conservador: diluye todo el CO a 25 ppm y
  todo el NOx a 5 ppm dentro del tiempo declarado, con 20 % de margen.

### Metadatos del paquete

- `pyproject.toml` declara los clasificadores de Python 3.9, 3.10, 3.11, 3.12 y 3.13, las
  mismas versiones para las que `cibuildwheel` ya compila ruedas. Así PyPI las muestra en la
  ficha del paquete. No cambia el código.

### Normas de Colombia y México

Dos preajustes nuevos, con el mismo patrón que el chileno: cada valor cita el
artículo o numeral leído en el texto vigente, y donde la norma no regula un
concepto la cita lo declara en vez de tomar prestado el artículo de otro país.

| Concepto | Colombia (Decreto 1886 de 2015) | México (NOM-023-STPS-2012) |
|---|---|---|
| Caudal por persona | 3 m³/min hasta 1.500 msnm y 6 de 1.500 en adelante (Art. 54, nums. 1 y 2) | 1,50 m³/min, sin escala por altitud (numeral 8.4.4 a) 1) |
| Caudal por HP diésel | 6 m³/min con CO en el escape ≤ 0,12 % (Art. 54, par. 2, num. 1) | 2,13 m³/min (numeral 8.4.4 a) 2) |
| Voladura | Sin tiempo de dilución; reingreso no antes de 15 min (Art. 157) | Sin tiempo de dilución; reconocimiento a los 30 min y sin humo (numeral 8.6.9 g) |
| Gases | Tabla del Art. 39; oxígeno en el Art. 38 y metano en el Art. 53 | NOM-010-STPS-2014, Tabla I.1, por remisión de la NOM-023 |
| Polvo | Remite a los VLP de la ACGIH (Art. 74) | NOM-010-STPS-2014, Tabla I.1 |
| Carga térmica | Temperatura efectiva (Arts. 217 y 218): el piso de 30 m/min queda como criterio de ingeniería | Índice TGBH de la NOM-015-STPS-2001: el piso de 30 m/min queda como criterio de ingeniería |

- `RegulatoryConfig.colombia()` y `RegulatoryConfig.mexico()`, los miembros
  `RegulatoryStandard.D1886_Colombia` y `RegulatoryStandard.NOM023_Mexico`, y
  `--norma colombia|mexico` en `ventpy demanda`, `ventpy lmp` y
  `ventpy cobertura`.
- **Colombia, borde de 1.500 msnm.** Las dos bandas del Art. 54 incluyen el
  1.500 en su texto. Rige la regla común de la librería: a 1.500 msnm exactos
  aplica la banda inferior (3 m³/min).
- **Colombia, factor diésel.** El preajuste usa 6 m³/min por HP. Quien acredite
  CO en el escape ≤ 0,08 % puede construir su configuración con 4 (Art. 54,
  par. 2, num. 2). Con CO sobre 0,12 % el decreto no da valor.
- **México, oxígeno.** La NOM-023 no fija el mínimo de oxígeno en labores. La
  tabla trae 19,5 % como criterio de ingeniería y la cita lo dice.
- **México, carbón.** La NOM-023 exceptúa las minas de carbón, que se rigen por
  la NOM-032-STPS-2008. Este preajuste no las cubre.
- En las dos tablas de gases el SO2 solo tiene límite de corto tiempo: su
  `twa_8h` es `None`, no cero.
- Colombia se verificó contra el texto consolidado con el Decreto 944 de 2022
  y México contra el DOF del 11-oct-2012 y del 28-abr-2014.

### Correcciones

- `shock_factors()` listaba `x = 0.0` en expansión, contracción, codos y unión,
  donde el factor sale por fórmula o es manual. Un cero se lee como «sin
  pérdida»: ahora `x` es `None` en esas filas.

### Documentación de la API

- Los campos de `GasLimit` que la norma no fija valen `None`, y la ayuda lo
  dice.
- `calculate_diesel_flow` aclara que solo aplica el factor por HP de la norma;
  la dilución de CO y NOx por contaminante la calcula `VentilationGovernor`.

## [0.2.1] — 2026-08-20

### La cita normativa sigue a la norma configurada

Hasta 0.2.0 la referencia legal de cada resultado estaba escrita como literal
peruano dentro de cada calculador y no consultaba la norma de la
configuración. Con `RegulatoryConfig.chile()` los caudales eran correctos,
pero la cita que los acompañaba era el DS 024-2016-EM peruano.

**A quién afecta:** a quien haya emitido informes con el preajuste chileno. Los
números no cambian —ni uno solo—; lo que cambia es el respaldo legal impreso
junto a ellos. Conviene reemitir esos informes: citaban el reglamento peruano
en una faena chilena.

| Resultado | Perú (DS 024) | Chile (DS 132) |
|---|---|---|
| Caudal por persona | Art. 236, con la escala por altitud del Art. 247 | **Art. 138**, sin escala por altitud |
| Caudal por HP diésel | Art. 246 | **Art. 132** |
| Dilución tras voladura | Art. 243-244 | **Arts. 156, 571 y 585** (reingreso). El DS 132 no fija tiempo de dilución ni volumen de gases por kg: son criterio de ingeniería, y la cita lo dice |
| Polvo respirable | Art. 111 | Sin límite verificado: la cita declara el vacío y atribuye la concentración objetivo a criterio de ingeniería |
| Carga térmica | Art. 252 lit. d | Sin criterio verificado: el piso de 30 m/min se mantiene —el caudal es el mismo— pero la cita lo declara criterio de ingeniería |
| Cobertura y velocidad del aire | Art. 252 lit. f/g y Art. 248 | Sin régimen ni límites verificados: la cita declara el vacío y remite a los umbrales configurados |

Donde el DS 132 no regula un concepto, la cita lo dice explícitamente en vez de
trasladar el artículo peruano. Ningún artículo chileno se cita sin verificar
contra el texto vigente (LeyChile idNorma=221064, versión 09-abr-2024).

### Cambios

- Nueva función `regulation_reference(topic, config)` en `normativa.hpp`: único
  lugar del núcleo donde vive el texto de una cita. Cada calculador la consulta
  en lugar de llevar su propio literal.
- `CoverageCalculator.compare_zone` acepta un cuarto argumento `config` que fija
  la norma de la cita. Su valor por defecto es el preajuste peruano, igual que
  el constructor por defecto de `RegulatoryConfig`; el código existente no
  cambia de comportamiento. `analyze_survey` propaga la configuración que ya
  recibía.
- Las advertencias de sílice, de concentración de polvo sobre el umbral, de
  estrés térmico y de velocidad fuera de rango también siguen la norma activa.

### Versiones

- `CMakeLists.txt` declaraba 0.1.0 mientras `pyproject.toml` y el paquete
  Python declaraban 0.2.0. Las tres quedan en 0.2.1.

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
