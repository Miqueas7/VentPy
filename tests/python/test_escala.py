"""Pruebas de escala y condicionamiento del solver de red.

Los tests funcionales usan redes de 2 a 5 ramales. Un modelo real de mina
tiene cientos. Aqui se generan topologias de mina por niveles (rampa de
ingreso, galerias por nivel y retorno) de tamano creciente para verificar
que el balance converge, que lo hace en un tiempo razonable, y que cuando
no converge lo reporta honestamente en vez de entregar numeros invalidos.
"""

import math
import time

import pytest

import ventpy


def _rama(bid, desde, hasta, r, fan=0.0):
    b = ventpy.NetworkBranch()
    b.branch_id = bid
    b.from_node = desde
    b.to_node = hasta
    b.r_manual = r
    b.fan_pressure_pa = fan
    return b


def red_por_niveles(n_niveles, r_rampa=0.02, r_galeria=0.35, fan_pa=2500.0):
    """Mina de `n_niveles`: ingreso por el lado A, galeria por nivel, retorno
    por el lado B. Genera 3*n_niveles + 1 ramales y n_niveles + 1 mallas."""
    ramas = [_rama("FAN", "S", "A0", r_rampa, fan=fan_pa)]
    for i in range(n_niveles):
        ramas.append(_rama(f"GAL{i}", f"A{i}", f"B{i}", r_galeria))
        if i + 1 < n_niveles:
            ramas.append(_rama(f"IN{i}", f"A{i}", f"A{i+1}", r_rampa))
            ramas.append(_rama(f"OUT{i}", f"B{i+1}", f"B{i}", r_rampa))
    ramas.append(_rama("RET", "B0", "S", r_rampa))

    d = ventpy.NetworkDefinition()
    d.branches = ramas
    return d


def _kirchhoff_ok(resultado, tol=1e-6):
    nodos = set()
    for b in resultado.branches:
        nodos.add(b.from_node)
        nodos.add(b.to_node)
    for nodo in nodos:
        balance = 0.0
        for b in resultado.branches:
            if b.to_node == nodo:
                balance += b.q_m3min
            if b.from_node == nodo:
                balance -= b.q_m3min
        if abs(balance) > tol:
            return False, nodo, balance
    return True, None, 0.0


def _resolver(red, max_iter=2000):
    sp = ventpy.SolverParams()
    sp.tolerance_m3min = 0.06
    sp.max_iterations = max_iter
    t0 = time.perf_counter()
    r = ventpy.NetworkSolver.solve(red, ventpy.AtmosphericParams(), sp)
    return r, time.perf_counter() - t0


class TestEscala:
    def test_red_mediana_50_ramales(self):
        red = red_por_niveles(17)          # ~50 ramales
        assert len(red.branches) >= 45
        r, seg = _resolver(red)
        assert r.converged, f"no convergio: residual {r.max_residual_m3min}"
        ok, nodo, bal = _kirchhoff_ok(r)
        assert ok, f"Kirchhoff roto en {nodo}: {bal}"
        assert all(math.isfinite(b.q_m3min) for b in r.branches)
        assert seg < 10.0, f"demasiado lento: {seg:.2f} s"

    @pytest.mark.slow
    def test_red_grande_200_ramales(self):
        red = red_por_niveles(67)          # ~200 ramales
        r, seg = _resolver(red)
        if r.converged:
            ok, nodo, bal = _kirchhoff_ok(r)
            assert ok, f"Kirchhoff roto en {nodo}: {bal}"
        else:
            # falla honesta: residual y advertencia presentes
            assert r.max_residual_m3min > 0.0
            assert any("NO CONVERGIO" in w for w in r.warnings)
        assert seg < 60.0, f"demasiado lento: {seg:.2f} s"

    @pytest.mark.slow
    def test_red_muy_grande_500_ramales(self):
        red = red_por_niveles(167)         # ~500 ramales
        r, seg = _resolver(red)
        assert all(math.isfinite(b.q_m3min) for b in r.branches)
        if r.converged:
            ok, nodo, bal = _kirchhoff_ok(r)
            assert ok, f"Kirchhoff roto en {nodo}: {bal}"
        else:
            assert any("NO CONVERGIO" in w for w in r.warnings)
        assert seg < 120.0, f"demasiado lento: {seg:.2f} s"


class TestCondicionamiento:
    def test_resistencias_en_rangos_extremos(self):
        """Resistencias que difieren en 12 ordenes de magnitud dentro de la
        misma red: el solver debe converger o reportar el fallo, nunca
        entregar valores no finitos."""
        d = ventpy.NetworkDefinition()
        d.branches = [
            _rama("FAN", "S", "A", 1e-6, fan=3000.0),
            _rama("FINA", "A", "B", 1e6),     # labor practicamente cerrada
            _rama("ANCHA", "A", "B", 1e-6),   # by-pass de resistencia minima
            _rama("RET", "B", "S", 0.1),
        ]
        r, _ = _resolver(d)
        for b in r.branches:
            assert math.isfinite(b.q_m3min), f"{b.branch_id} no finito"
            assert math.isfinite(b.pressure_drop_pa)
        if r.converged:
            ok, nodo, bal = _kirchhoff_ok(r)
            assert ok, f"Kirchhoff roto en {nodo}: {bal}"
            # el caudal prefiere abrumadoramente la rama de baja resistencia
            fina = next(b for b in r.branches if b.branch_id == "FINA")
            ancha = next(b for b in r.branches if b.branch_id == "ANCHA")
            assert abs(ancha.q_m3min) > abs(fina.q_m3min)
        else:
            assert any("NO CONVERGIO" in w for w in r.warnings)

    @pytest.mark.parametrize("tolerancia", [0.6, 0.06, 0.006, 0.0006])
    def test_red_sin_ventilador_tiende_a_caudal_nulo(self, tolerancia):
        """Sin fuente de presion no puede haber circulacion sostenida.

        El solver converge cuando la correccion de malla cae por debajo de la
        tolerancia, no cuando el caudal es exactamente cero, de modo que queda
        un residuo acotado por esa tolerancia: pedir una tolerancia diez veces
        menor deja un caudal diez veces menor. Se verifica esa relacion, que
        es lo que el algoritmo garantiza.
        """
        d = ventpy.NetworkDefinition()
        d.branches = [
            _rama("A", "N1", "N2", 0.5),
            _rama("B", "N2", "N1", 0.5),
        ]
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = tolerancia
        sp.max_iterations = 5000
        r = ventpy.NetworkSolver.solve(d, ventpy.AtmosphericParams(), sp)

        assert r.converged
        for b in r.branches:
            assert abs(b.q_m3min) <= tolerancia, (
                f"{b.branch_id}: {b.q_m3min} excede la tolerancia {tolerancia}")
