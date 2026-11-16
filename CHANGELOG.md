# Registro de cambios

Todas las versiones publicadas de VentPy. El formato sigue
[Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y el versionado es
[semántico](https://semver.org/lang/es/).

## [0.3.0] — 2026-11-15

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
| `calculate_blasting_flow`, nivel del mar | 1 m³/min | 6.667 m³/min (dilución de NOx a 3 ppm) |
| `VentilationGovernor`, 4.200 msnm, con un scoop de 150 HP | `Q_Exp` = 1; gobierna el diésel; total 981 | `Q_Exp` = 11.251; gobierna la voladura; total 12.939 |

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
- El modelo por contaminante es conservador: diluye todo el CO y todo el NOx
  (como NO2) a su límite de 8 h en la norma configurada dentro del tiempo
  declarado, con 20 % de margen (ver «Objetivo de dilución» más abajo). Con el
  DS 024 es criterio de ingeniería para el frente, y la cita lo declara: el
  reglamento fija el caudal de voladura solo en el total de mina.

### Total de mina con el DS 024: voladura según el Anexo 38

Con el preajuste peruano, `ZoneType.GeneralMine` sigue el Anexo 38 del
DS 024-2016-EM (texto del DS 023-2017-EM):

- Con equipos diésel (num. 1), la voladura no entra en el total. El resultado
  lo advierte y `q_blasting_m3min` queda en 0.
- Sin equipos diésel (num. 2 lit. e), rige el mayor entre el total con fugas y
  `Q_Ex = A × V × N`: área promedio de labores, velocidad mínima del Art. 248
  (25 m/min con ANFO u otros agentes de voladura, 20 m/min con dinamita) y
  niveles en voladura, el campo nuevo `BlastingParams.blasting_levels`
  (por defecto 1). `Q_Ex` no lleva fugas.

**A quién afecta:** a quien calcule el total de mina con voladura bajo el
DS 024. Hasta ahora se sumaba el caudal por dilución de contaminantes; con
50 kg de ANFO a 2.500 msnm, sin diésel, eso agregaba 5.428 m³/min y ahora
agrega 300 (12 m² × 25 m/min × 1 nivel). Chile, Colombia y México no tienen
un equivalente al Anexo 38 y siguen sumando la dilución.

### Advertencia de velocidad máxima

`VentilationGovernor` advierte cuando la velocidad en el frente, la rampa o el
tajeo supera la máxima de la norma: 250 m/min en Perú (DS 024, Art. 248),
150 m/min en Chile (DS 132, Art. 138 inc. 3) y 6 m/s (360 m/min) en Colombia
(Decreto 1886, Art. 56 par. 1). México no fija máxima y no se advierte. Solo
es un aviso: el caudal requerido no cambia. La función `max_air_velocity_mpm`
devuelve esos topes.

### Valores y citas revisados contra el texto oficial de cada norma

Cada valor que la librería atribuye a una norma se volvió a cotejar con el
texto vigente (DS 024-2016-EM consolidado y DS 023-2017-EM; DS 132 y DS 594;
Decreto 1886 de 2015 con el 944 de 2022; NOM-023-STPS-2012 y
NOM-010-STPS-2014). Varios valores compartidos entre normas seguían con
defaults peruanos, y algunos defaults peruanos no eran los de la norma. Los
cambios de resultados van primero, cada uno con a quién afecta.

#### El caudal por persona nunca baja del mínimo de la norma

`PersonnelFlowCalculator.calculate_full` multiplicaba el caudal por persona
por un factor de actividad (0,6 en reposo, 0,8 ligera). Ninguna norma
implementada reduce el mínimo por persona según la actividad: ahora la
actividad y la densidad pueden subirlo, nunca bajarlo.

**A quién afecta:** a quien calcule con `ActivityLevel.Rest` o `Light` en
`VentilationGovernor`. 20 trabajadores en reposo a 0 msnm daban 36 m³/min
(20 × 3 × 0,6); ahora dan 60 (DS 024, Art. 247). Con México, 100
trabajadores en reposo daban 90; ahora 150 (8.4.4 a) 1).

#### Bordes de altitud: rige la banda alta

El Art. 247 del DS 024 pone 1.500 y 3.000 msnm en dos bandas a la vez, y el
Art. 54 del Decreto 1886 hace lo mismo con 1.500. En el umbral exacto ahora
rige la banda de mayor caudal (antes la menor). La cita dice `(>=1500 msnm)`.

**A quién afecta:** a quien calcule exactamente a 1.500, 3.000 o 4.000 msnm
con Perú (3 → 4, 4 → 5 y 5 → 6 m³/min por persona) o a 1.500 msnm con
Colombia (3 → 6).

#### Velocidades mínimas según la norma

`get_min_velocity` tomaba 15 m/min en el frente, 18 en la rampa y 12 en el
tajeo para toda norma, por debajo de los 20 m/min del DS 024, Art. 248.
Ahora devuelve el mayor entre ese valor por tipo de labor (que queda como
criterio de ingeniería) y la mínima de la norma: Perú 20 m/min, 25 con ANFO
u otros agentes de voladura (Art. 248); Chile 15 (DS 132, Art. 138 inc. 3);
México 15,24 donde opere maquinaria diésel (8.4.4 b); Colombia no fija mínima
fuera del carbón y conserva los valores por labor. El barrido de la voladura
(`BlastingParams.min_velocity_mps`) tampoco baja de la mínima de la norma.

**A quién afecta:**
- Perú: el piso de velocidad de `Q_Per` en un frente de 12 m² sube de 180 a
  240 m³/min (12 × 20), y con voladura de ANFO a 300 (12 × 25). El barrido de
  la voladura sube de 18 a 25 m/min con ANFO y a 20 con dinamita. Un frente
  con 10 trabajadores sin flota pasa de 207 a 276 m³/min en el ventilador.
- Chile: el tajeo sube de 12 a 15 m/min.
- México con diésel: frente y tajeo suben a 15,24 m/min.

#### Cobertura: límites de velocidad de la norma configurada

`CoverageParams` traía 20 y 250 m/min (Perú) para todas las normas, y el
piso de 25 m/min con ANFO se aplicaba en cualquier país. Ahora
`min_velocity_mpm` y `max_velocity_mpm` valen 0 por defecto, que significa
«los de la norma», y un valor propio solo puede endurecerlos. Los límites
efectivos los da `CoverageCalculator.velocity_limits(params, config)`:

| Norma | Mínimo | Máximo |
|---|---|---|
| Perú (DS 024, Art. 248) | 20 m/min; 25 con ANFO | 250 m/min |
| Chile (DS 132, Art. 138 inc. 3) | 15 m/min | 150 m/min |
| Colombia (Decreto 1886, Art. 56 par. 1) | 15 m/min, criterio de ingeniería | 360 m/min |
| México (NOM-023, 8.4.4 b) | 15,24 m/min | sin máximo |

**A quién afecta:** a quien evalúe estaciones de aforo con Chile (una
estación a 160 m/min ahora no cumple), Colombia (300 m/min ahora cumple;
el ANFO ya no sube el mínimo) o México (la máxima ya no se aplica). Con
Perú no cambia nada. Quien leía `CoverageParams().min_velocity_mpm` esperando
20 ahora lee 0.

#### Velocidad máxima de Chile

`max_air_velocity_mpm(DS132_Chile)` devuelve 150 m/min (Art. 138 inc. 3: «Las
velocidades, como promedio, no podrán ser mayores de ciento cincuenta metros
por minuto»). Antes no había máxima chilena.

**A quién afecta:** a quien calcule con Chile: el Governor advierte sobre
150 m/min y la cobertura marca las estaciones por encima.

#### Objetivo de dilución: el límite de 8 h de la norma

La dilución de gases de voladura (`BlastingParams.target_co_ppm` y
`target_nox_ppm`) y de escape diésel usaba CO 25 ppm y NOx 5 ppm en toda
norma. Los 5 ppm son el STEL de NO2 del Anexo 15; su TWA es 3 ppm. Ahora el
objetivo es el TWA de la tabla de gases de la norma configurada
(`dilution_target_ppm`); si una norma no fija TWA para un gas, el más
estricto entre su STEL y su techo. El NOx se diluye como NO2. Los objetivos
de `BlastingParams` valen 0 por defecto («el de la norma») y el usuario
puede fijar el suyo; los resultados de voladura y de diésel muestran el
objetivo usado (`target_co_ppm`, `target_nox_ppm`). Las constantes de
referencia `TLV_NO2_PPM` y `TLV_SO2_PPM` pasan a 3 y 2 ppm (Anexo 15, TWA).

| Norma | CO | NO2 | Caudal por NOx frente a la 0.2.1 |
|---|---|---|---|
| Perú (Anexo 15) | 25 ppm | 3 ppm | × 5/3 (+67 %) |
| Chile (DS 594, Art. 66) | 44 ppm | 2,6 ppm | × 5/2,6 |
| Colombia (Art. 39) | 25 ppm | 0,2 ppm | × 25 |
| México (NOM-010, fila 320) | 25 ppm | 0,2 ppm | × 25 |

**A quién afecta:** a todo cálculo con voladura o con la dilución por
contaminante del diésel. Con Colombia y México los caudales por NOx suben
25 veces y pasan a gobernar con frecuencia: es lo que exige su límite de
0,2 ppm. Quien quiera conservar el criterio anterior debe fijar
`target_nox_ppm = 5.0` y asumirlo como criterio propio.

#### Simultaneidad del diésel: 1,0 por defecto

`VentilationInput.simultaneity_factor` valía 0,85. Ninguna norma prevé
simultaneidad; con el DS 024, `Q_Eq = 3 × HP × Dm × Fu` (Anexo 38) ya la
incluye en Dm × Fu. Ahora vale 1,0, y con un valor menor el resultado
advierte que `Q_Eq` queda bajo la norma. El factor por HP tampoco baja ya del
valor de la norma por la corrección volumétrica del nivel del mar (0,99999).

**A quién afecta:** a quien no fijaba la simultaneidad: `Q_Eq` por factor HP
sube 1/0,85 (+17,6 %). Con 100 HP a 0 msnm el Governor daba 255 m³/min;
ahora 300, el mínimo del Anexo 38.

#### Total de mina peruano: madera y temperatura del Anexo 38

El total de mina con el DS 024 (`ZoneType.GeneralMine`) suma ahora los dos
términos del Anexo 38 que faltaban, con entradas opcionales nuevas de
`VentilationInput`:

- `Q_Ma = T × u` (lit. b): `production_t_per_shift` (T, t métricas húmedas
  por guardia) y `timber_consumption_pct`. u = 0 bajo 20 %; 0,60 de 20 a
  40 %; 1,00 de 41 a 70 %; 1,25 sobre 70 % (`timber_production_factor`). El
  num. 2 del anexo escribe «> 71» en la última fila; el num. 1 y el Art. 252
  dicen «> 70», que es lo que se usa. Entre 40 y 41 % rige 1,00.
- `Q_Te = Vm × A × N` (lit. c): `hot_levels` (N, niveles con temperatura
  mayor a 23 °C), A = `face_area_m2` y Vm = 30 m/min.

Entran en `Q_T1 = Q_Tr + Q_Ma + Q_Te + Q_Eq`, con fugas del 15 % de `Q_T1`.
`Q_Te` y el balance térmico (`q_thermal`) responden al mismo concepto, el
aire para la temperatura de confort del Art. 252 lit. d: rige el mayor de
los dos, sin sumarlos. El polvo sigue sumando como criterio de ingeniería.
Sin sus datos cada término vale 0 y el resultado advierte que el total no lo
incluye. El resultado expone `q_timber_m3min` y `q_temperature_m3min`, y la
CLI acepta las tres entradas.

**A quién afecta:** a quien calcule el total de mina peruano. Sin las
entradas nuevas el número no cambia, pero aparecen dos advertencias. Con
ellas el total sube: 100 t por guardia con 30 % de madera agregan 60 m³/min;
dos niveles calientes con 12 m² agregan 720, o nada si el balance térmico ya
pedía más.

#### Polvo: límite de la norma y corrección Fa de Chile

`DustParams.target_concentration_mg_m3` vale 0 por defecto, que significa
«el límite de la norma»: Perú 3 mg/m³ (Art. 111); Chile 2,4 mg/m³ de polvos
no especificados, fracción respirable (DS 594, Art. 66), multiplicado sobre
1.000 msnm por Fa = P/760 con dos decimales (Art. 63); México 3 mg/m³
(NOM-010, fila 609); Colombia 3 mg/m³ como criterio de ingeniería (el Art. 74
remite a la ACGIH). La advertencia de objetivo alto compara con ese límite y
no con 3 fijo. `calculate_dust_flow` acepta la atmósfera (`atm`) para la
corrección por altitud.

**A quién afecta:** a quien calcule polvo con Chile usando el objetivo por
defecto: el caudal sube 25 % al nivel del mar (3 → 2,4 mg/m³) y 81 % a
3.000 msnm (2,4 × 0,69 = 1,656 mg/m³).

#### Chile: óxido nítrico en la tabla de gases

La tabla chilena incluye el NO con LPP de 22 ppm (DS 594, Art. 66).
`lmp_for(DS132_Chile, NO)` ya no lanza error.

**A quién afecta:** a quien consulte el NO con Chile (`ventpy lmp --norma
chile --gas NO` ahora responde) o cuente las filas de la tabla chilena (7 → 8).

#### Citas corregidas (sin cambio de números)

- Perú: el caudal por persona cita el Art. 247 (el Art. 236 vigente trata de
  vibraciones por voladura); el factor diésel, el Art. 252 lit. e y el
  Anexo 38 num. 1 lit. d (no el Art. 246); el reingreso tras la voladura, el
  Art. 240 (no los Arts. 243-244); el O2 de 19,5 %, el Art. 246 lit. b; las
  mangas a 15 m, el Art. 246 lit. f; el circuito de ventilación, el Art. 246
  lit. c; los límites de gases, el Art. 110 y el Anexo 15 (no el Art. 108).
  El tope de 30 min de dilución deja de llamarse «max normativo»: es criterio
  de ingeniería, como el volumen de 0,04 m³/kg.
- Chile: polvo y sílice citan el DS 594, Art. 66 (cuarzo 0,08 mg/m³;
  cristobalita y tridimita 0,04); la carga térmica, el Art. 143 que remite al
  DS 594, Arts. 96 a 98 (TGBH); la cobertura de mina, el Art. 139; la
  velocidad, el Art. 138 inc. 3. El metano del Art. 274 se aclara como del
  Título V (carbón). El factor diésel declara que la pequeña minería
  (Título XV, Art. 627: 3 m³/min por HP) no está cubierta por el preajuste.
- Colombia: la advertencia de velocidad máxima cita el Art. 56 par. 1 y
  menciona los 4,5 m/s de los tajos de carbón (par. 2); ya no remite a
  «límites configurados en CoverageParams».
- México: la corrección por altitud del factor diésel se declara criterio de
  ingeniería; el objetivo térmico no alcanzable también (igual en las otras
  tres normas).

#### API

- Nuevas en C++ y Python: `min_air_velocity_mpm`, `max_air_velocity_mpm`
  (ahora también en Python), `dilution_target_ppm`,
  `timber_production_factor` y `CoverageCalculator.velocity_limits`.
- `get_min_velocity(zone_type, standard=DS024_Peru, diesel=False,
  blasting_agents=False)` depende de la norma; con un solo argumento devuelve
  el valor peruano.
- Campos nuevos: `VentilationInput.production_t_per_shift`,
  `timber_consumption_pct` y `hot_levels`;
  `VentilationDemandResult.q_timber_m3min` y `q_temperature_m3min`;
  `target_co_ppm` y `target_nox_ppm` en los resultados de voladura y diésel.

#### Ejemplos

`01-demanda-peru` pasa de 981 a 1.923 m³/min, `02-preset-chile` de 981 a
2.219 y en `03-cobertura-levantamiento` cada zona requiere 276 m³/min en vez
de 207 (los aforos del ejemplo se ajustaron para conservar el caso). Cada
README explica la derivación.

### Pequeña minería chilena, sílice y tajos de carbón

- **`RegulatoryConfig.chile_small_mining()`**: preajuste para faenas de hasta
  5.000 t/mes (DS 132, Título XV, ámbito del Art. 594). Igual a `chile()` salvo
  3 m³/min por HP (Art. 627); las citas de caudal por persona y por HP nombran
  el Art. 627.
- **Sílice respirable**: con `DustParams.silica_content_percent` > 0, el
  objetivo de polvo por defecto es el menor entre el límite respirable y el
  límite de sílice dividido entre la fracción de sílice. Perú 0,05 mg/m³
  (Anexo 15), Chile cuarzo 0,08 o, con `silica_cristobalite_or_tridymite`,
  0,04 mg/m³ (DS 594, Art. 66, con Fa), México 0,025 mg/m³ (NOM-010, fila 668).
  Colombia remite a la ACGIH sin fijar el valor: solo se advierte.
- **`VentilationInput.coal_mine`**: con Colombia, el máximo en tajos baja a
  4,5 m/s = 270 m/min (Decreto 1886, Art. 56 par. 2). El resultado advierte que
  las demás reglas de carbón no están implementadas.

**A quién afecta:** a quien declare contenido de sílice sin fijar un objetivo
propio: con 10 % de sílice en Perú el objetivo baja de 3 a 0,5 mg/m³ y el
caudal por polvo sube seis veces. Los otros dos cambios solo actúan si se
eligen.

### Metadatos del paquete

- `pyproject.toml` declara los clasificadores de Python 3.9, 3.10, 3.11, 3.12 y 3.13, las
  mismas versiones para las que `cibuildwheel` ya compila ruedas. Así PyPI las muestra en la
  ficha del paquete. No cambia el código.
- Ya no se publican ruedas para Linux de 32 bits (`manylinux_i686`): su contenedor de
  compilación no logra instalar `scikit-build-core`. En esa plataforma `pip` compila desde
  el código fuente publicado.

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
  aplica la banda alta (6 m³/min).
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
