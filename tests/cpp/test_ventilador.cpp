/**
 * @file test_ventilador.cpp
 * @brief Tests de curva de ventilador y punto de operación (McPherson Cap. 10).
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "ventpy/ventilador.hpp"

using namespace ventpy;

namespace {
FanCurve curva_tipica() {
    FanCurve c;
    c.fan_id = "AX-1200";
    c.points = { {600.0,2000.0}, {1200.0,1800.0}, {1800.0,1400.0},
                 {2400.0,800.0}, {3000.0,200.0} };
    return c;
}
} // namespace

// ============================================================================
// pressure_at — interpolación + fan laws (ec. 10.28)
// ============================================================================

TEST(FanCurva, InterpolaYCorrigeDensidad) {
    // probe_sp3c.py: P_cat(1500)=1600; ρ(0,20°C)=1.20412 → factor 1.00343
    const double p0 = FanCalculator::pressure_at(curva_tipica(), 1500.0,
                                                 1.2041183163746156);
    EXPECT_NEAR(p0, 1605.4910884994874, 1e-9);
    // ρ(4200) = 0.71350 → factor 0.59459
    const double p42 = FanCalculator::pressure_at(curva_tipica(), 1500.0,
                                                  0.7135029597786612);
    EXPECT_NEAR(p42, 951.3372797048817, 1e-9);
}

TEST(FanCurva, ExtremosExactos) {
    const double rho = 1.2;   // factor 1.0 → valores de catálogo puros
    EXPECT_DOUBLE_EQ(FanCalculator::pressure_at(curva_tipica(), 600.0, rho), 2000.0);
    EXPECT_DOUBLE_EQ(FanCalculator::pressure_at(curva_tipica(), 3000.0, rho), 200.0);
}

TEST(FanCurva, FueraDeRangoLanza) {
    EXPECT_THROW(FanCalculator::pressure_at(curva_tipica(), 599.9, 1.2),
                 std::invalid_argument);
    EXPECT_THROW(FanCalculator::pressure_at(curva_tipica(), 3000.1, 1.2),
                 std::invalid_argument);
}

TEST(FanCurva, ValidacionesDeCurva) {
    FanCurve c = curva_tipica();
    c.points.resize(1);                          // < 2 puntos
    EXPECT_THROW(FanCalculator::pressure_at(c, 600.0, 1.2), std::invalid_argument);

    c = curva_tipica();
    c.points[2].q_m3min = c.points[1].q_m3min;   // no estrictamente creciente
    EXPECT_THROW(FanCalculator::pressure_at(c, 1500.0, 1.2), std::invalid_argument);

    c = curva_tipica();
    c.points[1].pressure_pa = -5.0;              // presión negativa
    EXPECT_THROW(FanCalculator::pressure_at(c, 1500.0, 1.2), std::invalid_argument);

    c = curva_tipica();
    c.rated_density_kg_m3 = 0.0;
    EXPECT_THROW(FanCalculator::pressure_at(c, 1500.0, 1.2), std::invalid_argument);
}

TEST(FanCurva, RatedDensityDistintaEscalaCorrecto) {
    FanCurve c = curva_tipica();
    c.rated_density_kg_m3 = 1.0;   // curva referida a 1.0 kg/m3
    // P_cat(1500)=1600; factor = 1.2041183163746156/1.0
    const double p = FanCalculator::pressure_at(c, 1500.0, 1.2041183163746156);
    EXPECT_NEAR(p, 1600.0 * 1.2041183163746156, 1e-9);
}

// ============================================================================
// operating_point — probe_sp3c.py (analítico y stall)
// ============================================================================

namespace {
FanCurve curva_lineal() {           // analítica: P_cat(Q) = 3600 − Q
    FanCurve c; c.fan_id = "LIN";
    c.points = { {600.0,3000.0}, {3000.0,600.0} };
    return c;
}
FanCurve curva_pico() {             // pico interior en Q=1200
    FanCurve c; c.fan_id = "PICO";
    c.points = { {600.0,1500.0}, {1200.0,2000.0}, {1800.0,1900.0},
                 {2400.0,1200.0}, {3000.0,400.0} };
    return c;
}
} // namespace

TEST(FanOperacion, InterseccionAnalitica) {
    // R=0.5: Q²/7200 + f·Q − 3600f = 0 → Q_op = 2637.290153772097 (probe)
    AtmosphericParams atm;   // ρ(0,20°C)
    auto r = FanCalculator::operating_point(curva_lineal(), 0.5, atm);

    EXPECT_TRUE(r.converged);
    EXPECT_TRUE(r.in_curve_range);
    EXPECT_NEAR(r.q_m3min, 2637.290153772097, 0.01);
    EXPECT_NEAR(r.pressure_pa, 966.013799331007, 0.01);
    EXPECT_NEAR(r.air_density_kg_m3, 1.2041183163746156, 1e-12);
    EXPECT_NE(r.biblio_ref.find("10.28"), std::string::npos);
    // curva monótona decreciente → pico en el primer punto → sin zona de stall
    EXPECT_TRUE(r.stall_ok);
}

TEST(FanOperacion, StallOkConMargenHolgado) {
    // R = 1.5051478954682693 → Q_op ≈ 2000; pico en 1200 → margen 0.667 > 0.10
    AtmosphericParams atm;
    auto r = FanCalculator::operating_point(curva_pico(), 1.5051478954682693, atm);
    EXPECT_TRUE(r.converged);
    EXPECT_NEAR(r.q_m3min, 2000.0, 0.5);
    EXPECT_DOUBLE_EQ(r.q_peak_m3min, 1200.0);
    EXPECT_TRUE(r.stall_ok);
    EXPECT_NEAR(r.stall_margin_actual, 800.0 / 1200.0, 1e-3);
}

TEST(FanOperacion, ZonaDeStallDetectada) {
    // R = 5.722049850540529 → Q_op ≈ 1100 < pico 1200
    AtmosphericParams atm;
    auto r = FanCalculator::operating_point(curva_pico(), 5.722049850540529, atm);
    EXPECT_TRUE(r.converged);
    EXPECT_NEAR(r.q_m3min, 1100.0, 0.5);
    EXPECT_FALSE(r.stall_ok);
    bool fuerte = false;
    for (const auto& w : r.warnings)
        if (w.find("ZONA DE STALL") != std::string::npos) fuerte = true;
    EXPECT_TRUE(fuerte);
}

TEST(FanOperacion, SinInterseccionDiagnostica) {
    AtmosphericParams atm;
    // R gigantesco: opera antes del catálogo (f(Qmin) < 0)
    auto muy_resistivo = FanCalculator::operating_point(curva_lineal(), 1e4, atm);
    EXPECT_FALSE(muy_resistivo.converged);
    EXPECT_FALSE(muy_resistivo.in_curve_range);
    ASSERT_FALSE(muy_resistivo.warnings.empty());
    // R ínfimo: opera más allá del catálogo (f(Qmax) > 0)
    auto muy_abierto = FanCalculator::operating_point(curva_lineal(), 1e-6, atm);
    EXPECT_FALSE(muy_abierto.converged);
    EXPECT_FALSE(muy_abierto.in_curve_range);
}

TEST(FanOperacion, Validaciones) {
    AtmosphericParams atm;
    EXPECT_THROW(FanCalculator::operating_point(curva_lineal(), 0.0, atm),
                 std::invalid_argument);
    FanOperatingParams p; p.stall_margin = -0.1;
    EXPECT_THROW(FanCalculator::operating_point(curva_lineal(), 0.5, atm, p),
                 std::invalid_argument);
}

// ============================================================================
// operating_point_in_network — Red A de SP-3b + curva (probe_sp3c.py):
// equilibrio Q_F = 2797.4401 m³/min, P = 519.2961 Pa; Q_P1 = 1864.96, Q_P2 = 932.48
// ============================================================================

namespace {
NetworkBranch mkb(const std::string& id, const std::string& f, const std::string& t,
                  double r, double fan = 0.0) {
    NetworkBranch b;
    b.branch_id = id; b.from_node = f; b.to_node = t;
    b.r_manual = r; b.fan_pressure_pa = fan;
    return b;
}
NetworkDefinition red_a_sin_fan() {
    NetworkDefinition d;
    d.branches = { mkb("F","S","A",0.05), mkb("P1","A","B",0.2),
                   mkb("P2","A","B",0.8), mkb("R","B","S",0.1) };
    return d;
}
FanCurve curva_red() {
    FanCurve c; c.fan_id = "AX-RED";
    c.points = { {1200.0,900.0}, {1800.0,800.0}, {2400.0,650.0},
                 {3000.0,450.0}, {3600.0,200.0} };
    return c;
}
} // namespace

TEST(FanEnRed, PuntoFijoConvergeAlEquilibrio) {
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = FanCalculator::operating_point_in_network(
        red_a_sin_fan(), "F", curva_red(), atm, sp);

    EXPECT_TRUE(r.converged);
    EXPECT_TRUE(r.in_curve_range);
    EXPECT_NEAR(r.q_m3min, 2797.440074748137, 1.0);
    EXPECT_NEAR(r.pressure_pa, 519.2960675736035, 1.0);
    // La red embebida trae el balance completo en el punto de operación
    ASSERT_TRUE(r.network.has_value());
    EXPECT_TRUE(r.network->converged);
    EXPECT_NEAR(r.network->branches[1].q_m3min, 1864.96, 1.0);   // P1
    EXPECT_NEAR(r.network->branches[2].q_m3min,  932.48, 1.0);   // P2
    // Curva monótona decreciente → stall_ok con nota informativa
    EXPECT_TRUE(r.stall_ok);
}

TEST(FanEnRed, FanPressureDeclaradoSeIgnoraConAdvertencia) {
    auto d = red_a_sin_fan();
    d.branches[0].fan_pressure_pa = 500.0;   // declarado: debe ignorarse
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = FanCalculator::operating_point_in_network(d, "F", curva_red(), atm, sp);

    EXPECT_TRUE(r.converged);
    EXPECT_NEAR(r.q_m3min, 2797.440074748137, 1.0);   // mismo equilibrio
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("ignora") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}

TEST(FanEnRed, RamalInexistenteLanza) {
    AtmosphericParams atm;
    EXPECT_THROW(FanCalculator::operating_point_in_network(
                     red_a_sin_fan(), "NO-EXISTE", curva_red(), atm),
                 std::invalid_argument);
}

TEST(FanEnRed, PresionReportadaCoincideConElSolveEmbebido) {
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = FanCalculator::operating_point_in_network(
        red_a_sin_fan(), "F", curva_red(), atm, sp);
    ASSERT_TRUE(r.converged);
    ASSERT_TRUE(r.network.has_value());
    // Trazabilidad exacta: la presión reportada ES la que alimentó el solve final
    EXPECT_DOUBLE_EQ(r.pressure_pa, r.network->branches[0].fan_pressure_pa);
}

TEST(FanEnRed, OmegaExtremoNoConvergeEspurio) {
    // FIX 2 (Task 1 pre-bindings): con under_relaxation extrema (0.001), p_fan
    // se mueve muy poco por iteracion, pero el sistema es lo bastante sensible
    // en este punto de la curva para que |Δq| entre iteraciones sucesivas caiga
    // por debajo de la tolerancia DEFAULT (0.6 m3/min de SolverParams) alrededor
    // de la iteracion 19 (verificado por barrido exhaustivo, no supuesto) --
    // MUY lejos del equilibrio real de la Red A (Q≈2797.44, P≈519.30 Pa):
    // ANTES del fix, con max_iterations=25 el codigo actual converge de forma
    // ESPURIA en Q≈3124.17, P≈647.68 Pa (residual ≈248 Pa frente al target de
    // la curva en ese Q). El criterio residual
    // |p_fan_usado - p_target(q_raw)| <= FAN_RESIDUAL_TOL_PA (1 Pa) debe
    // impedir esta convergencia espuria.
    AtmosphericParams atm;
    SolverParams sp;   // default: tolerance_m3min=0.6, max_iterations=100
    FanOperatingParams params_usados;
    params_usados.under_relaxation = 0.001;
    params_usados.max_iterations = 25;
    auto r = FanCalculator::operating_point_in_network(
        red_a_sin_fan(), "F", curva_red(), atm, sp, params_usados);

    EXPECT_FALSE(r.converged);
    // Camino esperado: en it=19 el |Δq| cae bajo tolerancia (convergencia
    // espuria del criterio VIEJO), pero el residual |p_fan_usado -
    // p_target(q_raw)| (~248 Pa) lo RECHAZA -> el loop NO rompe ahi y sigue
    // hasta agotar max_iterations (25). Si en el futuro cambia la dinamica
    // del solver y el punto fijo SI llega a converger de verdad antes de
    // agotar el presupuesto, EXPECT_FALSE(converged) de arriba ya fallaria;
    // este assert adicional distingue explicitamente "rechazado por
    // residual, agota iteraciones" de cualquier otro camino de no-
    // convergencia (red interna no balancea, flujo invertido, etc.), que
    // NO llegarian a agotar r.iterations == max_iterations ni dejarian el
    // warning "NO CONVERGIO" generico de agotamiento del punto fijo.
    EXPECT_EQ(r.iterations, params_usados.max_iterations);
    bool no_convergio = false;
    for (const auto& w : r.warnings)
        if (w.find("NO CONVERGIO") != std::string::npos) no_convergio = true;
    EXPECT_TRUE(no_convergio);
}

TEST(FanEnRed, FlujoInvertidoAdvierteYNoConverge) {
    // Red de 1 malla (S<->A) con 2 ramales paralelos: BIG impone 5000 Pa
    // S->A (dominante frente a la curva, max ~900 Pa) forzando la circulacion
    // de retorno A->S por el ramal del ventilador ("FAN") -> q_FAN < 0 en la
    // convencion from(S)->to(A). Las fan laws no aplican a flujo invertido.
    NetworkDefinition d;
    d.branches = { mkb("BIG", "S", "A", 0.05, 5000.0), mkb("FAN", "S", "A", 0.5) };
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = FanCalculator::operating_point_in_network(d, "FAN", curva_red(), atm, sp);

    EXPECT_FALSE(r.converged);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("FLUJO INVERTIDO") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}
