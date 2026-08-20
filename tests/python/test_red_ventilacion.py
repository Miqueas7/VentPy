"""
Tests de integracion Python de la red de ventilacion: Atkinson,
dimensionamiento de ducto, solver de red (Hardy Cross) y ventilador.

Verifica que los bindings nanobind exponen correctamente AtkinsonCalculator,
DuctSizingCalculator, NetworkSolver y FanCalculator, junto con las structs de
entrada/resultado y los enums AirwayLining / SingularityType.

Fuente NO NORMATIVA (bibliografia de ingenieria): McPherson, M.J.,
"Subsurface Ventilation Engineering", ed. 2009 (SRK) - Cap. 5 (Atkinson),
Cap. 7 (red, Hardy Cross), Cap. 10 (ventiladores).
Numeros tomados de tests/cpp/{test_atkinson,test_ducto,
test_red,test_ventilador}.cpp (ya validados).
"""

import pytest

import ventpy


# ============================================================================
# AtkinsonCalculator - tablas y resistencia de ramal
# ============================================================================


class TestTablaFriccion:
    def test_tabla_completa_12_entradas_con_cita(self):
        tabla = ventpy.atkinson_friction_factors()
        assert len(tabla) == 12
        for entry in tabla:
            assert entry.k > 0.0
            assert "Tabla 5.1" in entry.biblio_ref
            assert "McPherson" in entry.biblio_ref

    def test_friction_factor_for_valores_conocidos(self):
        assert ventpy.friction_factor_for(ventpy.AirwayLining.UnlinedTypical) == pytest.approx(0.012)
        assert ventpy.friction_factor_for(ventpy.AirwayLining.DuctFlexibleSpiral) == pytest.approx(0.011)

    def test_friction_factor_for_manual_lanza(self):
        with pytest.raises(ValueError):
            ventpy.friction_factor_for(ventpy.AirwayLining.Manual)


class TestTablaChoque:
    def test_tabla_informativa_7_entradas(self):
        tabla = ventpy.shock_factors()
        assert len(tabla) == 7
        for entry in tabla:
            assert "A5" in entry.biblio_ref

    def test_resolve_shock_factor_exit_entrance(self):
        exit_s = ventpy.AirwaySingularity()
        exit_s.type = ventpy.SingularityType.Exit
        assert ventpy.resolve_shock_factor(exit_s) == pytest.approx(1.0)

        entr_s = ventpy.AirwaySingularity()
        entr_s.type = ventpy.SingularityType.Entrance
        assert ventpy.resolve_shock_factor(entr_s) == pytest.approx(0.5)

    def test_resolve_shock_factor_expansion_sin_ratio_lanza(self):
        exp_s = ventpy.AirwaySingularity()
        exp_s.type = ventpy.SingularityType.Expansion
        with pytest.raises(ValueError):
            ventpy.resolve_shock_factor(exp_s)


def galeria_base():
    """Espejo de galeria_base() en tests/cpp/test_atkinson.cpp."""
    p = ventpy.AirwayParams()
    p.airway_id = "GAL-500"
    p.length_m = 500.0
    p.perimeter_m = 15.0
    p.area_m2 = 14.0
    p.lining = ventpy.AirwayLining.Manual
    p.atkinson_k = 0.012
    return p


class TestAtkinsonRamal:
    def test_resistencia_friccion_nivel_del_mar(self):
        atm = ventpy.AtmosphericParams()
        r = ventpy.AtkinsonCalculator.calculate_resistance(galeria_base(), atm)

        assert r.air_density_kg_m3 == pytest.approx(1.2041183163746156, rel=1e-9)
        assert r.k_used == pytest.approx(0.012)
        # R = k_corr*L*per/A^3
        assert r.r_friction == pytest.approx(0.0329113971312304, rel=1e-9)
        assert r.r_shock == 0.0
        assert r.r_total == pytest.approx(0.0329113971312304, rel=1e-9)

    def test_choque_exit_suma_resistencia(self):
        p = galeria_base()
        exit_s = ventpy.AirwaySingularity()
        exit_s.type = ventpy.SingularityType.Exit
        p.singularities = [exit_s]
        atm = ventpy.AtmosphericParams()
        r = ventpy.AtkinsonCalculator.calculate_resistance(p, atm)

        assert r.r_shock == pytest.approx(0.0030717303989148, rel=1e-9)
        assert r.r_total == pytest.approx(0.0359831275301452, rel=1e-9)

    def test_caudal_da_presion_velocidad_y_unidades(self):
        p = galeria_base()
        exit_s = ventpy.AirwaySingularity()
        exit_s.type = ventpy.SingularityType.Exit
        p.singularities = [exit_s]
        atm = ventpy.AtmosphericParams()
        r = ventpy.AtkinsonCalculator.calculate(p, atm, 3000.0)

        assert r.q_m3min == pytest.approx(3000.0)
        assert r.velocity_mps == pytest.approx(3.5714285714285716, rel=1e-9)
        assert r.pressure_drop_pa == pytest.approx(89.9578188253631, abs=1e-6)
        assert r.pressure_drop_mmh2o == pytest.approx(9.1731446340354, abs=1e-6)

    def test_velocidad_baja_advierte_art_248(self):
        atm = ventpy.AtmosphericParams()
        r = ventpy.AtkinsonCalculator.calculate(galeria_base(), atm, 150.0)
        assert len(r.warnings) > 0
        assert any("248" in w for w in r.warnings)

    def test_validaciones_lanzan(self):
        atm = ventpy.AtmosphericParams()
        p = galeria_base()
        p.length_m = 0.0
        with pytest.raises(ValueError):
            ventpy.AtkinsonCalculator.calculate_resistance(p, atm)


# ============================================================================
# DuctSizingCalculator - tecnico y economico
# ============================================================================


def duct_base_params():
    p = ventpy.DuctSizingParams()
    p.q_m3min = 1200.0
    p.length_m = 400.0
    p.duct_lining = ventpy.AirwayLining.DuctFlexibleSpiral
    return p


def eco_base():
    e = ventpy.EconomicParams()
    e.energy_cost_per_kwh = 0.12
    e.duct_cost_per_m_per_m_diam = 40.0
    e.operating_hours = 4000.0
    e.fan_efficiency = 0.65
    return e


class TestDuctoTecnico:
    def test_elige_menor_diametro_que_cumple_velocidad(self):
        atm = ventpy.AtmosphericParams()
        r = ventpy.DuctSizingCalculator.calculate(duct_base_params(), atm)

        assert r.feasible is True
        assert r.selected_diameter_m == pytest.approx(1.22)
        assert len(r.options) == 8

    def test_validaciones_lanzan(self):
        atm = ventpy.AtmosphericParams()
        p = duct_base_params()
        p.q_m3min = 0.0
        with pytest.raises(ValueError):
            ventpy.DuctSizingCalculator.calculate(p, atm)


class TestDuctoEconomico:
    def test_elige_costo_total_minimo_no_el_menor_diametro(self):
        p = duct_base_params()
        p.max_velocity_mps = 25.0
        atm = ventpy.AtmosphericParams()

        tec = ventpy.DuctSizingCalculator.calculate(p, atm)
        assert tec.selected_diameter_m == pytest.approx(1.07)

        eco = ventpy.DuctSizingCalculator.calculate_full(p, atm, eco_base())
        assert eco.feasible is True
        assert eco.selected_diameter_m == pytest.approx(1.22)
        assert "economico" in eco.selection_criterion

    def test_validaciones_economicas_lanzan(self):
        p = duct_base_params()
        atm = ventpy.AtmosphericParams()
        e = eco_base()
        e.energy_cost_per_kwh = 0.0
        with pytest.raises(ValueError):
            ventpy.DuctSizingCalculator.calculate_full(p, atm, e)


# ============================================================================
# NetworkSolver - Hardy Cross (Red A: paralelo analitico)
# ============================================================================


def mk_branch(branch_id, from_node, to_node, r_manual, fan_pressure_pa=0.0):
    b = ventpy.NetworkBranch()
    b.branch_id = branch_id
    b.from_node = from_node
    b.to_node = to_node
    b.r_manual = r_manual
    b.fan_pressure_pa = fan_pressure_pa
    return b


def red_a():
    d = ventpy.NetworkDefinition()
    d.branches = [
        mk_branch("F", "S", "A", 0.05, 500.0),
        mk_branch("P1", "A", "B", 0.2),
        mk_branch("P2", "A", "B", 0.8),
        mk_branch("R", "B", "S", 0.1),
    ]
    return d


class TestNetworkSolver:
    def test_red_paralela_converge_a_la_solucion_analitica(self):
        atm = ventpy.AtmosphericParams()
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = 0.006
        sp.max_iterations = 1000
        r = ventpy.NetworkSolver.solve(red_a(), atm, sp)

        assert r.converged is True
        f = r.branches[0]
        p1 = r.branches[1]
        p2 = r.branches[2]
        assert f.q_m3min == pytest.approx(2744.974, abs=0.05)
        # ratio paralelo sqrt(R2/R1) = 2
        assert p1.q_m3min / p2.q_m3min == pytest.approx(2.0, rel=1e-3)

    def test_red_vacia_lanza(self):
        atm = ventpy.AtmosphericParams()
        with pytest.raises(ValueError):
            ventpy.NetworkSolver.solve(ventpy.NetworkDefinition(), atm)


class TestValidacionVisibleEnFrontera:
    def test_airway_params_length_cero_lanza(self):
        # Validacion visible desde Python de una entrada cruda invalida
        # en frontera: AirwayParams con length 0 lanza ValueError (la
        # convencion de validacion en frontera del proyecto: valida en
        # frontera, no en interior).
        p = ventpy.AirwayParams()
        p.airway_id = "x"
        p.length_m = 0.0
        p.perimeter_m = 15.0
        p.area_m2 = 14.0
        p.lining = ventpy.AirwayLining.Manual
        p.atkinson_k = 0.012
        atm = ventpy.AtmosphericParams()
        with pytest.raises(ValueError):
            ventpy.AtkinsonCalculator.calculate_resistance(p, atm)


# ============================================================================
# FanCalculator - punto de operacion (analitico, stall, en red)
# ============================================================================


def curva_lineal():
    c = ventpy.FanCurve()
    c.fan_id = "LIN"
    p1 = ventpy.FanCurvePoint()
    p1.q_m3min = 600.0
    p1.pressure_pa = 3000.0
    p2 = ventpy.FanCurvePoint()
    p2.q_m3min = 3000.0
    p2.pressure_pa = 600.0
    c.points = [p1, p2]
    return c


def curva_pico():
    c = ventpy.FanCurve()
    c.fan_id = "PICO"
    puntos = [(600.0, 1500.0), (1200.0, 2000.0), (1800.0, 1900.0),
              (2400.0, 1200.0), (3000.0, 400.0)]
    pts = []
    for q, p in puntos:
        fp = ventpy.FanCurvePoint()
        fp.q_m3min = q
        fp.pressure_pa = p
        pts.append(fp)
    c.points = pts
    return c


class TestFanOperatingPoint:
    def test_interseccion_analitica(self):
        atm = ventpy.AtmosphericParams()
        r = ventpy.FanCalculator.operating_point(curva_lineal(), 0.5, atm)

        assert r.converged is True
        assert r.in_curve_range is True
        assert r.q_m3min == pytest.approx(2637.29, abs=0.01)
        assert r.stall_ok is True

    def test_zona_de_stall_detectada(self):
        atm = ventpy.AtmosphericParams()
        r = ventpy.FanCalculator.operating_point(
            curva_pico(), 5.722049850540529, atm)

        assert r.converged is True
        assert r.stall_ok is False
        assert any("ZONA DE STALL" in w for w in r.warnings)


def curva_red():
    c = ventpy.FanCurve()
    c.fan_id = "AX-RED"
    puntos = [(1200.0, 900.0), (1800.0, 800.0), (2400.0, 650.0),
              (3000.0, 450.0), (3600.0, 200.0)]
    pts = []
    for q, p in puntos:
        fp = ventpy.FanCurvePoint()
        fp.q_m3min = q
        fp.pressure_pa = p
        pts.append(fp)
    c.points = pts
    return c


def red_a_sin_fan():
    d = ventpy.NetworkDefinition()
    d.branches = [
        mk_branch("F", "S", "A", 0.05),
        mk_branch("P1", "A", "B", 0.2),
        mk_branch("P2", "A", "B", 0.8),
        mk_branch("R", "B", "S", 0.1),
    ]
    return d


class TestFanOperatingPointInNetwork:
    def test_punto_fijo_converge_al_equilibrio(self):
        atm = ventpy.AtmosphericParams()
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = 0.006
        sp.max_iterations = 1000
        r = ventpy.FanCalculator.operating_point_in_network(
            red_a_sin_fan(), "F", curva_red(), atm, sp)

        assert r.converged is True
        assert r.in_curve_range is True
        assert r.q_m3min == pytest.approx(2797.44, abs=1.0)
        assert r.network is not None
        assert r.network.converged is True

    def test_presion_reportada_coincide_con_el_solve_embebido(self):
        atm = ventpy.AtmosphericParams()
        sp = ventpy.SolverParams()
        sp.tolerance_m3min = 0.006
        sp.max_iterations = 1000
        r = ventpy.FanCalculator.operating_point_in_network(
            red_a_sin_fan(), "F", curva_red(), atm, sp)

        assert r.converged is True
        assert r.network is not None
        assert r.pressure_pa == r.network.branches[0].fan_pressure_pa

    def test_ramal_inexistente_lanza(self):
        atm = ventpy.AtmosphericParams()
        with pytest.raises(ValueError):
            ventpy.FanCalculator.operating_point_in_network(
                red_a_sin_fan(), "NO-EXISTE", curva_red(), atm)
