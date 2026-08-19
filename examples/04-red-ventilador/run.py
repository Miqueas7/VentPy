"""Ejemplo 4 — Punto de operacion de un ventilador dentro de una red (Red A).

Usa SOLO el API publico de `ventpy` (`NetworkBranch`, `NetworkDefinition`,
`FanCurve`/`FanCurvePoint`, `FanCalculator.operating_point_in_network`) para
acoplar la curva de un ventilador axial (AX-RED) al ramal "F" de la Red A:
una malla en paralelo (P1/P2 entre nodos A-B) alimentada desde S y devuelta
por R. El solver de red (Hardy Cross) y la curva del ventilador se resuelven
juntos por punto fijo hasta converger.

Bibliografia (NO normativa): McPherson, M.J., "Subsurface Ventilation
Engineering" (2009) - Cap. 7 (redes, Hardy Cross, seccion 7.3.2) y Cap. 10
(ventiladores, curvas caracteristicas y deteccion de stall).

Equivalente por CLI:
    ventpy ventilador examples/04-red-ventilador/input.json --json
"""
import ventpy


def branch(branch_id, from_node, to_node, r_manual):
    b = ventpy.NetworkBranch()
    b.branch_id = branch_id
    b.from_node = from_node
    b.to_node = to_node
    b.r_manual = r_manual
    return b


network = ventpy.NetworkDefinition()
network.branches = [
    branch("F", "S", "A", 0.05),   # ramal del ventilador (sin fan_pressure_pa:
                                    # la presion la impone la curva acoplada)
    branch("P1", "A", "B", 0.2),   # malla en paralelo
    branch("P2", "A", "B", 0.8),
    branch("R", "B", "S", 0.1),    # retorno
]

curve = ventpy.FanCurve()
curve.fan_id = "AX-RED"
points = [(1200.0, 900.0), (1800.0, 800.0), (2400.0, 650.0),
          (3000.0, 450.0), (3600.0, 200.0)]
curve_points = []
for q_m3min, pressure_pa in points:
    p = ventpy.FanCurvePoint()
    p.q_m3min = q_m3min
    p.pressure_pa = pressure_pa
    curve_points.append(p)
curve.points = curve_points

atm = ventpy.AtmosphericParams()
solver_params = ventpy.SolverParams()
solver_params.tolerance_m3min = 0.006
solver_params.max_iterations = 1000
fan_params = ventpy.FanOperatingParams()

result = ventpy.FanCalculator.operating_point_in_network(
    network, "F", curve, atm, solver_params, fan_params)

print(f"q_m3min          = {result.q_m3min}")
print(f"pressure_pa       = {result.pressure_pa}")
print(f"converged          = {result.converged}")
print(f"in_curve_range     = {result.in_curve_range}")
print(f"stall_ok            = {result.stall_ok}")
print(f"warnings            = {list(result.warnings)}")
print(f"network.converged  = {result.network.converged}")

# --- Punto de operacion Q ~= 2797.4 m3/min (McPherson Cap. 7/10) ---
assert result.converged is True
assert abs(result.q_m3min - 2797.4) < 1.0

# --- Curva monotona decreciente en todo su rango de catalogo: no hay pico
# interior, por lo tanto no existe zona de stall observable en la curva ---
assert result.stall_ok is True
assert result.in_curve_range is True
assert any("monotona" in w for w in result.warnings)

# --- La red asociada tambien convergio (Hardy Cross) ---
assert result.network.converged is True

print("OK: q_m3min ~= 2797.4 m3/min, sin zona de stall (curva monotona)")
