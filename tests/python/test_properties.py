"""Pruebas basadas en propiedades (Hypothesis).

A diferencia de los tests de caso puntual, aqui se declaran invariantes que
deben cumplirse para CUALQUIER entrada dentro del dominio fisico, y la
libreria de generacion busca contraejemplos.

Los rangos de las estrategias son fisicamente razonables para mineria
subterranea: altitudes hasta 5.000 msnm, potencias 10-1000 HP, secciones
1-100 m2, resistencias 1e-3 a 10 Ns2/m8.
"""

import math

import pytest
from hypothesis import HealthCheck, assume, given, settings
from hypothesis import strategies as st

import ventpy

# Perfil comun: las llamadas al nucleo son rapidas, pero construir structs
# nanobind tiene coste; se limita el numero de ejemplos por propiedad.
PROP = settings(max_examples=60, deadline=None,
                suppress_health_check=[HealthCheck.too_slow])

altitudes = st.floats(min_value=0.0, max_value=5000.0, allow_nan=False,
                      allow_infinity=False)
resistencias = st.floats(min_value=1e-3, max_value=10.0, allow_nan=False,
                         allow_infinity=False)
areas = st.floats(min_value=1.0, max_value=100.0, allow_nan=False,
                  allow_infinity=False)


# ---------------------------------------------------------------------------
# Monotonias fisicas
# ---------------------------------------------------------------------------

class TestMonotonia:
    @given(n=st.integers(min_value=1, max_value=500), alt=altitudes)
    @settings(PROP)
    def test_caudal_personal_no_decrece_con_trabajadores(self, n, alt):
        """Un trabajador mas nunca puede exigir menos aire."""
        cfg = ventpy.RegulatoryConfig.peru()
        q_n = ventpy.calculate_personnel_flow(n, alt, cfg).q_personnel
        q_n1 = ventpy.calculate_personnel_flow(n + 1, alt, cfg).q_personnel
        assert q_n1 >= q_n

    @given(n=st.integers(min_value=1, max_value=200),
           alt=st.floats(min_value=0.0, max_value=4000.0, allow_nan=False,
                         allow_infinity=False))
    @settings(PROP)
    def test_caudal_personal_no_decrece_con_altitud(self, n, alt):
        """La escala del Art. 247 es no decreciente en altitud."""
        cfg = ventpy.RegulatoryConfig.peru()
        q_bajo = ventpy.calculate_personnel_flow(n, alt, cfg).q_personnel
        q_alto = ventpy.calculate_personnel_flow(n, alt + 500.0, cfg).q_personnel
        assert q_alto >= q_bajo

    @given(r1=resistencias, r2=resistencias, delta=st.floats(
        min_value=0.1, max_value=5.0, allow_nan=False, allow_infinity=False))
    @settings(PROP)
    def test_mas_resistencia_no_aumenta_su_caudal(self, r1, r2, delta):
        """En dos ramas en paralelo, subir la resistencia de una no puede
        aumentar el caudal que pasa por ella."""
        def resolver(ra):
            d = ventpy.NetworkDefinition()
            d.branches = [
                _rama("F", "S", "A", 0.05, fan=800.0),
                _rama("P1", "A", "B", ra),
                _rama("P2", "A", "B", r2),
                _rama("R", "B", "S", 0.1),
            ]
            sp = ventpy.SolverParams()
            sp.tolerance_m3min = 0.006
            sp.max_iterations = 500
            return ventpy.NetworkSolver.solve(d, ventpy.AtmosphericParams(), sp)

        base = resolver(r1)
        subida = resolver(r1 + delta)
        assume(base.converged and subida.converged)
        assert abs(subida.branches[1].q_m3min) <= abs(base.branches[1].q_m3min) + 1e-6
        # y la rama gemela no puede perder caudal cuando su competidora empeora
        assert abs(subida.branches[2].q_m3min) >= abs(base.branches[2].q_m3min) - 1e-6


# ---------------------------------------------------------------------------
# Redondeo de seguridad
# ---------------------------------------------------------------------------

class TestRedondeoSeguridad:
    @given(gen=st.floats(min_value=0.1, max_value=500.0, allow_nan=False,
                         allow_infinity=False),
           target=st.floats(min_value=0.5, max_value=3.0, allow_nan=False,
                            allow_infinity=False),
           eficiencia=st.floats(min_value=0.0, max_value=0.95, allow_nan=False,
                                allow_infinity=False))
    @settings(PROP)
    def test_q_dust_nunca_por_debajo_del_crudo(self, gen, target, eficiencia):
        """El caudal reportado cubre siempre la dilucion teorica, y no la
        excede en mas de 1 m3/min (redondeo hacia arriba, no inflado)."""
        p = ventpy.DustParams()
        p.dust_generation_rate_mg_s = gen
        p.target_concentration_mg_m3 = target
        p.water_suppression = True
        p.suppression_efficiency = eficiencia
        r = ventpy.calculate_dust_flow(p, ventpy.RegulatoryConfig.peru())

        crudo = gen * (1.0 - eficiencia) / target * 60.0
        assert r.q_dust >= crudo - 1e-6
        assert r.q_dust < crudo + 1.0


# ---------------------------------------------------------------------------
# Conservacion de masa en la red (invariante mas fuerte)
# ---------------------------------------------------------------------------

def _rama(bid, desde, hasta, r, fan=0.0):
    b = ventpy.NetworkBranch()
    b.branch_id = bid
    b.from_node = desde
    b.to_node = hasta
    b.r_manual = r
    b.fan_pressure_pa = fan
    return b


@st.composite
def redes_conexas(draw):
    """Genera una red conexa con al menos una malla: un ciclo base de n nodos
    mas cuerdas aleatorias entre nodos existentes."""
    n_nodos = draw(st.integers(min_value=3, max_value=8))
    n_cuerdas = draw(st.integers(min_value=1, max_value=4))
    nodos = [f"N{i}" for i in range(n_nodos)]

    ramas = []
    for i in range(n_nodos):  # ciclo base
        r = draw(st.floats(min_value=0.01, max_value=5.0, allow_nan=False,
                           allow_infinity=False))
        ramas.append(_rama(f"C{i}", nodos[i], nodos[(i + 1) % n_nodos], r))

    for k in range(n_cuerdas):  # cuerdas: crean mallas adicionales
        i = draw(st.integers(min_value=0, max_value=n_nodos - 1))
        j = draw(st.integers(min_value=0, max_value=n_nodos - 1))
        assume(i != j)
        r = draw(st.floats(min_value=0.01, max_value=5.0, allow_nan=False,
                           allow_infinity=False))
        ramas.append(_rama(f"X{k}", nodos[i], nodos[j], r))

    fan = draw(st.floats(min_value=100.0, max_value=3000.0, allow_nan=False,
                         allow_infinity=False))
    ramas[0].fan_pressure_pa = fan

    d = ventpy.NetworkDefinition()
    d.branches = ramas
    return d


class TestKirchhoff:
    @given(red=redes_conexas())
    @settings(max_examples=80, deadline=None,
              suppress_health_check=[HealthCheck.too_slow, HealthCheck.data_too_large])
    def test_conservacion_en_todos_los_nodos(self, red):
        """Para CUALQUIER red que converja, la suma de caudales en cada nodo
        debe anularse (primera ley de Kirchhoff)."""
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = 0.006
        sp.max_iterations = 500
        r = ventpy.NetworkSolver.solve(red, ventpy.AtmosphericParams(), sp)
        assume(r.converged)

        nodos = set()
        for b in r.branches:
            nodos.add(b.from_node)
            nodos.add(b.to_node)

        for nodo in nodos:
            balance = 0.0
            for b in r.branches:
                if b.to_node == nodo:
                    balance += b.q_m3min
                if b.from_node == nodo:
                    balance -= b.q_m3min
            assert abs(balance) < 1e-6, f"nodo {nodo}: balance {balance}"

    @given(red=redes_conexas())
    @settings(max_examples=40, deadline=None,
              suppress_health_check=[HealthCheck.too_slow, HealthCheck.data_too_large])
    def test_resultados_finitos_o_no_convergido(self, red):
        """Nunca se entregan NaN/infinito: o los numeros son finitos, o el
        resultado viene marcado como no convergido."""
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = 0.006
        sp.max_iterations = 200
        r = ventpy.NetworkSolver.solve(red, ventpy.AtmosphericParams(), sp)
        if r.converged:
            for b in r.branches:
                assert math.isfinite(b.q_m3min)
                assert math.isfinite(b.pressure_drop_pa)


# ---------------------------------------------------------------------------
# Cobertura: relaciones internas del resultado
# ---------------------------------------------------------------------------

class TestCoberturaInvariantes:
    @given(req=st.floats(min_value=1.0, max_value=1e5, allow_nan=False,
                         allow_infinity=False),
           med=st.floats(min_value=0.0, max_value=1e5, allow_nan=False,
                         allow_infinity=False))
    @settings(PROP)
    def test_relaciones_del_resultado(self, req, med):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Z"
        m.q_measured_m3min = med
        r = ventpy.CoverageCalculator.compare_zone(req, m)

        assert r.coverage_ratio == pytest.approx(med / req, rel=1e-12)
        assert r.compliant == (med >= req)
        assert (r.deficit_m3min > 0.0) == (not r.compliant)
        if not r.compliant:
            assert r.deficit_m3min >= req - med - 1e-9  # redondeo hacia arriba


# ---------------------------------------------------------------------------
# Escalado dimensional de Atkinson
# ---------------------------------------------------------------------------

class TestAtkinsonEscalado:
    @given(largo=st.floats(min_value=10.0, max_value=2000.0, allow_nan=False,
                           allow_infinity=False),
           area=areas,
           k=st.floats(min_value=0.002, max_value=0.05, allow_nan=False,
                       allow_infinity=False))
    @settings(PROP)
    def test_resistencia_proporcional_a_longitud(self, largo, area, k):
        def r_de(L):
            p = ventpy.AirwayParams()
            p.airway_id = "A"
            p.length_m = L
            p.perimeter_m = 4.0 * math.sqrt(area)
            p.area_m2 = area
            p.lining = ventpy.AirwayLining.Manual
            p.atkinson_k = k
            return ventpy.AtkinsonCalculator.calculate_resistance(
                p, ventpy.AtmosphericParams()).r_friction

        assert r_de(2.0 * largo) == pytest.approx(2.0 * r_de(largo), rel=1e-9)

    @given(area=areas, k=st.floats(min_value=0.002, max_value=0.05,
                                   allow_nan=False, allow_infinity=False))
    @settings(PROP)
    def test_resistencia_inversa_al_cubo_del_area(self, area, k):
        def r_de(A):
            p = ventpy.AirwayParams()
            p.airway_id = "A"
            p.length_m = 500.0
            p.perimeter_m = 15.0
            p.area_m2 = A
            p.lining = ventpy.AirwayLining.Manual
            p.atkinson_k = k
            return ventpy.AtkinsonCalculator.calculate_resistance(
                p, ventpy.AtmosphericParams()).r_friction

        assert r_de(2.0 * area) == pytest.approx(r_de(area) / 8.0, rel=1e-9)


# ---------------------------------------------------------------------------
# Curva de ventilador: interpolacion acotada
# ---------------------------------------------------------------------------

class TestCurvaVentilador:
    @given(q=st.floats(min_value=600.0, max_value=3000.0, allow_nan=False,
                       allow_infinity=False),
           rho=st.floats(min_value=0.6, max_value=1.3, allow_nan=False,
                         allow_infinity=False))
    @settings(PROP)
    def test_interpolacion_dentro_del_rango_de_catalogo(self, q, rho):
        c = ventpy.FanCurve()
        c.fan_id = "AX"
        c.points = [ventpy.FanCurvePoint(), ventpy.FanCurvePoint(),
                    ventpy.FanCurvePoint()]
        for punto, (qq, pp) in zip(c.points, [(600.0, 2000.0), (1800.0, 1400.0),
                                              (3000.0, 200.0)]):
            punto.q_m3min = qq
            punto.pressure_pa = pp

        p = ventpy.FanCalculator.pressure_at(c, q, rho)
        factor = rho / c.rated_density_kg_m3
        assert 200.0 * factor - 1e-9 <= p <= 2000.0 * factor + 1e-9


# ---------------------------------------------------------------------------
# Determinismo y rechazo de valores no finitos
# ---------------------------------------------------------------------------

class TestDeterminismoYDominio:
    @given(n=st.integers(min_value=1, max_value=300), alt=altitudes)
    @settings(PROP)
    def test_misma_entrada_mismo_resultado(self, n, alt):
        cfg = ventpy.RegulatoryConfig.peru()
        a = ventpy.calculate_personnel_flow(n, alt, cfg)
        b = ventpy.calculate_personnel_flow(n, alt, cfg)
        assert a.q_personnel == b.q_personnel
        assert a.flow_per_person_base == b.flow_per_person_base

    @given(malo=st.sampled_from([float("nan"), float("inf"), float("-inf")]))
    @settings(PROP)
    def test_altitud_no_finita_es_rechazada(self, malo):
        cfg = ventpy.RegulatoryConfig.peru()
        with pytest.raises(ValueError):
            ventpy.calculate_personnel_flow(10, malo, cfg)

    @given(malo=st.sampled_from([float("nan"), float("inf"), float("-inf")]))
    @settings(PROP)
    def test_medicion_no_finita_es_rechazada(self, malo):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Z"
        m.q_measured_m3min = malo
        with pytest.raises(ValueError):
            ventpy.CoverageCalculator.compare_zone(100.0, m)

    @given(malo=st.sampled_from([float("nan"), float("inf"), float("-inf")]))
    @settings(PROP)
    def test_resistencia_no_finita_es_rechazada(self, malo):
        d = ventpy.NetworkDefinition()
        d.branches = [_rama("A", "N1", "N2", malo), _rama("B", "N2", "N1", 0.5)]
        with pytest.raises(ValueError):
            ventpy.NetworkSolver.solve(d, ventpy.AtmosphericParams())
