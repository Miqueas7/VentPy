# Ejemplo 1 — Demanda de ventilacion, frente de desarrollo (Peru)

## Que calcula

La demanda total de ventilacion (`Q_total`) de un frente de desarrollo
(`DevelopmentFace`) con:

- 15 trabajadores, a 4200 msnm
- 1 scoop diesel de 150 HP (disponibilidad 0.85, utilizacion 0.70)
- 1 disparo de 50 kg de ANFO (0.04 m3 de gases/kg, 30 min de dilucion)

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
  "q_personnel_m3min": 180.0,
  "q_diesel_m3min": 853.0,
  "q_blasting_m3min": 1.0,
  "governing_factor": "diesel (Q_Eq)",
  "q_total_m3min": 981.0,
  "velocity_at_face_mps": 1.1847222222222222,
  "velocity_ok": true
}
```

`q_total_m3min == 981.0`.

## Por que gobierna el diesel, y por que gobierna NOx dentro del diesel

El Governor toma el `max()` de los caudales de personal, diesel y
explosivos para una zona de tipo frente/rampa (`select_governing_flow`,
`include/ventpy/governor.hpp`). Aqui `Q_Eq` (853 m3/min) es mayor que
`Q_Per` (180) y `Q_Exp` (1), asi que **"diesel (Q_Eq)"** es el criterio
gobernante.

Dentro del calculo de `Q_Eq` (`DieselFlowCalculator::calculate_full`,
`include/ventpy/caudal_equipo.hpp`) se evaluan 3 sub-criterios y se toma el
maximo:

1. Factor HP normativo (Art. 246): ~323 m3/min
2. Dilucion de CO a su TLV: ~117 m3/min
3. **Dilucion de NOx a su TLV: ~852 m3/min ← gobierna**

El scoop usa el motor por defecto `EngineEmissionTier.Tier3` (motores 2006-
2008, sin reduccion adicional de NOx tipo SCR): sus emisiones de NOx
dominan sobre el factor HP normativo y sobre CO. El `regulation_ref` del
resultado lo deja explicito: `"... [Gobernante: dilucion NOx]"`.

**DS 024-2016-EM, Art. 246**: exige ventilacion para diluir gases de
combustion de equipos diesel; el factor HP (3 m3/min/HP) es el minimo
normativo, pero el caudal real requerido puede ser mayor si las emisiones
del motor (segun su tier de emision) exigen mas aire para diluir CO/NOx a
su limite de exposicion — exactamente lo que pasa en este caso.

## El piso de velocidad (no es lo que gobierna aqui)

Independientemente del criterio anterior, el Governor tambien verifica que
la velocidad resultante en el frente (`q_governing / area`) supere el
minimo de **DS 024-2016-EM, Art. 236** (0.25 m/s para un frente de
desarrollo, `get_min_velocity(ZoneType.DevelopmentFace)`). En este caso
`velocity_at_face_mps ~= 1.18 m/s > 0.25 m/s`, asi que `velocity_ok = true`:
el piso de velocidad esta cubierto de sobra por el caudal que ya exige el
diesel — no es un criterio adicional que aumente `Q_total` en este
ejemplo, pero forma parte de la auditoria obligatoria de todo resultado
(regla 4 de `CLAUDE.md`: estructura de auditoria completa, sin `double`
suelto).

## Ejecutar

```bash
python examples/01-demanda-peru/run.py
```

El script usa solo el API publico de `ventpy` (sin pasar por `ventpy.cli`)
y termina en `assert` — si corre sin errores, los numeros de arriba siguen
vigentes.
