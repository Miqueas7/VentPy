# Ejemplo 1 — Demanda de ventilacion, frente de desarrollo (Peru)

## Que calcula

La demanda total de ventilacion (`Q_total`) de un frente de desarrollo
(`DevelopmentFace`) con:

- 15 trabajadores, a 4200 msnm
- 1 scoop diesel de 150 HP (disponibilidad 0.85, utilizacion 0.70)
- sin voladura (ver la nota al final)

usando la norma peruana `DS 024-2016-EM` (el preset por defecto de
`RegulatoryConfig`).

## Comando CLI

```bash
ventpy demanda examples/01-demanda-peru/input.json --json
```

## Salida esperada (claves relevantes)

```json
{
  "standard": "DS024_Peru",
  "q_personnel_m3min": 240.0,
  "q_diesel_m3min": 1672.0,
  "q_blasting_m3min": 0.0,
  "governing_factor": "diesel (Q_Eq)",
  "q_total_m3min": 1923.0,
  "velocity_at_face_mps": 2.3222222222222224,
  "velocity_ok": true
}
```

`q_total_m3min == 1923.0` = ceil(1672 × 1.15) = ceil(1922.8).

## Por que gobierna el diesel, y por que gobierna NOx dentro del diesel

El Governor toma el `max()` de los caudales de personal, diesel y
explosivos para una zona de tipo frente/rampa (`select_governing_flow`,
`include/ventpy/governor.hpp`). Aqui `Q_Eq` (1672 m3/min) es mayor que
`Q_Per` (240) y no hay voladura, asi que **"diesel (Q_Eq)"** es el criterio
gobernante.

Dentro del calculo de `Q_Eq` (`DieselFlowCalculator::calculate_full`,
`include/ventpy/caudal_equipo.hpp`) se evaluan 3 sub-criterios y se toma el
maximo:

1. Factor HP normativo (Art. 252 lit. e y Anexo 38: Q_Eq = 3 × HP × Dm × Fu):
   ~451.85 m3/min
   (`total_effective_hp * hp_factor_corrected * simultaneity_factor` =
   `89.25 HP * 5.0628 m3/min/HP * 1.0` — HP con disponibilidad/utilizacion
   pero SIN de-rating por altitud, ya que `hp_factor_corrected` ya trae su
   propia correccion volumetrica, que es criterio de ingenieria; la
   simultaneidad vale 1.0 porque la norma no la preve: Dm × Fu ya lo es)
2. Dilucion de CO a su TWA del Anexo 15 (25 ppm): ~137 m3/min
3. **Dilucion de NOx (como NO2) a su TWA del Anexo 15 (3 ppm): ~1671.19
   m3/min ← gobierna**

Derivacion del criterio 3: HP derateado = 89.25 × 0.84 = 74.97 HP; NOx =
6.0 g/kWh × 74.97 × 0.7457 kW / 60 = 5.5905 g/min; volumen de NO2 = 5.5905 /
46.01 × 24.45 = 2.9708 L/min; aire = 2.9708 × 10^6 / 3 / 1000 = 990.27
m3/min; × correccion volumetrica a 4200 msnm (1.6876) = 1671.19.

El scoop usa el motor por defecto `EngineEmissionTier.Tier3` (motores 2006-
2008, sin reduccion adicional de NOx tipo SCR): sus emisiones de NOx
dominan sobre el factor HP normativo y sobre CO. El `regulation_ref` del
resultado lo deja explicito: `"... [Gobernante: dilucion NOx]"`.

**DS 024-2016-EM, Art. 252 lit. e**: el aire para equipos petroleros no debe
ser menor de 3 m3/min por HP, y la evaluacion debe considerar tambien la
altitud y las emisiones. El factor HP es el minimo normativo; el caudal real
puede ser mayor si las emisiones del motor exigen mas aire para diluir CO y
NOx a su limite de exposicion (Art. 110 + Anexo 15) — exactamente lo que
pasa en este caso.

### Por que cambiaron los numeros (0.3.0)

Hasta la 0.2.1 este ejemplo daba 981 m3/min: diluia el NOx a 5 ppm (el STEL
de NO2, no su TWA de 3 ppm) y aplicaba una simultaneidad de 0.85 que la
norma no tiene. El piso de velocidad era 0.25 m/s (15 m/min), por debajo de
los 20 m/min del Art. 248. Ver `CHANGELOG.md`.

## El piso de velocidad (no es lo que gobierna aqui)

Independientemente del criterio anterior, el Governor verifica que la
velocidad resultante en el frente (`q_governing / area`) supere el minimo
de **DS 024-2016-EM, Art. 248** (20 m/min en las labores de explotacion,
desarrollo y preparacion; `get_min_velocity(ZoneType.DevelopmentFace)`).
`Q_Per` ya lo incluye: 12 m2 × 20 m/min = 240 m3/min. En este caso
`velocity_at_face_mps ~= 2.32 m/s` (139 m/min), entre los 20 y los 250 m/min
del Art. 248, asi que `velocity_ok = true`.

## Nota: por que este ejemplo no incluye voladura

Hasta la 0.2.1 este caso llevaba un disparo de 50 kg de ANFO con
`gas_volume_per_kg = 0.04`, y `Q_Exp` salia en 1 m3/min: la libreria dividia
el volumen de gases entre el tiempo de dilucion, sin llevarlo a un limite de
exposicion. Desde la 0.3.0 la voladura se calcula siempre por dilucion de CO
y NOx. Con ese mismo disparo, `Q_Exp` vale 11251 m3/min y pasa a gobernar
(`Q_total` = 12939 m3/min). Se quito del ejemplo para que siga mostrando el
caso gobernado por el diesel; el detalle del cambio esta en `CHANGELOG.md`.

## Ejecutar

```bash
python examples/01-demanda-peru/run.py
```

El script usa solo el API publico de `ventpy` (sin pasar por `ventpy.cli`)
y termina en `assert` — si corre sin errores, los numeros de arriba siguen
vigentes.
