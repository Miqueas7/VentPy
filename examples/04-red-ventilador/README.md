# Ejemplo 4 — Ventilador acoplado a una red (Hardy Cross + curva)

## Que calcula

El punto de operacion de un ventilador axial (curva "AX-RED") instalado en
el ramal `F` de la Red A: una red de ventilacion con una malla en paralelo
(ramales `P1`/`P2` entre los nodos `A` y `B`) alimentada desde el nodo `S`
por el ramal del ventilador (`F`) y devuelta por el ramal de retorno (`R`).

```
S --F(ventilador)--> A --P1--> B --R--> S
                      A --P2--> B
```

`FanCalculator.operating_point_in_network` resuelve simultaneamente:

1. El balance de la red (Hardy Cross, `NetworkSolver`) para una presion de
   ventilador dada.
2. La curva caracteristica del ventilador (interpolacion lineal entre los
   puntos de catalogo).

iterando por punto fijo hasta que ambos convergen al mismo `(Q, P)`.

## Comando CLI

```bash
ventpy ventilador examples/04-red-ventilador/input.json --json
```

## Salida esperada (claves relevantes)

```json
{
  "q_m3min": 2797.4402293280987,
  "converged": true,
  "in_curve_range": true,
  "stall_ok": true,
  "warnings": [
    "Curva monotona decreciente en catalogo: sin zona de stall observable (pico en el primer punto)"
  ]
}
```

`q_m3min ~= 2797.4 m3/min` (dentro de ±1.0 de tolerancia numerica del
punto fijo). `exit code == 0`: los 3 flags de confiabilidad
(`converged`, `stall_ok`, `in_curve_range`) estan en `true`.

## Por que no hay riesgo de stall

La curva de catalogo del ventilador "AX-RED" es **monotona decreciente**
en todo su rango (900 Pa en 1200 m3/min hasta 200 Pa en 3600 m3/min, sin
pico interior): el `FanCalculator` no encuentra ninguna region de la curva
donde la presion suba con el caudal (la firma clasica de una zona de
stall), asi que reporta `stall_ok = true` con la advertencia informativa
"sin zona de stall observable (pico en el primer punto)" en vez de
calcular un margen de stall contra un pico que no existe.

## Fundamento (bibliografia de ingenieria, NO normativa)

McPherson, M.J., *"Subsurface Ventilation Engineering"* (2009):

- **Cap. 7, §7.3.2** — solucion de redes en malla por el metodo de Hardy
  Cross (balance iterativo de caudales en ramales paralelos).
- **Cap. 10** — curvas caracteristicas de ventiladores, punto de operacion
  por interseccion con la curva del sistema, y deteccion de zona de
  stall.

## Ejecutar

```bash
python examples/04-red-ventilador/run.py
```

El script construye la `NetworkDefinition` y la `FanCurve` via API publico
y llama `FanCalculator.operating_point_in_network` directamente — sin
pasar por `ventpy.cli`. Termina en `assert`.
