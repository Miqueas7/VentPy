/**
 * @file test_red.cpp
 * @brief Tests del solver de red (Hardy Cross — McPherson Cap. 7).
 */
#include <gtest/gtest.h>
#include <cmath>
#include <stdexcept>
#include "ventpy/red.hpp"

using namespace ventpy;

namespace {
NetworkBranch mk(const std::string& id, const std::string& f, const std::string& t,
                 double r, double fan = 0.0) {
    NetworkBranch b;
    b.branch_id = id; b.from_node = f; b.to_node = t;
    b.r_manual = r; b.fan_pressure_pa = fan;
    return b;
}
NetworkDefinition red_a() {   // paralelo analítico (verificado con calculo independiente)
    NetworkDefinition d;
    d.branches = { mk("F","S","A",0.05,500.0), mk("P1","A","B",0.2),
                   mk("P2","A","B",0.8),       mk("R","B","S",0.1) };
    return d;
}
} // namespace

// ============================================================================
// Topología y validaciones
// ============================================================================

TEST(RedTopologia, CuentaNodosYMallas) {
    AtmosphericParams atm;
    auto r = NetworkSolver::solve(red_a(), atm);
    EXPECT_EQ(r.node_count, 3);   // S, A, B
    EXPECT_EQ(r.mesh_count, 2);   // B - N + 1 = 4 - 3 + 1
    ASSERT_EQ(r.branches.size(), 4u);
    // R resuelta por ramal (XOR manual)
    EXPECT_DOUBLE_EQ(r.branches[0].r_ns2m8, 0.05);
    EXPECT_DOUBLE_EQ(r.branches[0].fan_pressure_pa, 500.0);
    EXPECT_NE(r.biblio_ref.find("7.3.2"), std::string::npos);
}

TEST(RedValidacion, RedVaciaLanza) {
    AtmosphericParams atm;
    EXPECT_THROW(NetworkSolver::solve({}, atm), std::invalid_argument);
}

TEST(RedValidacion, IdDuplicadoLanza) {
    auto d = red_a();
    d.branches[1].branch_id = "F";
    AtmosphericParams atm;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

TEST(RedValidacion, SelfLoopLanza) {
    auto d = red_a();
    d.branches[1].to_node = "A";   // A→A
    AtmosphericParams atm;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

TEST(RedValidacion, XorResistencia) {
    AtmosphericParams atm;
    auto d = red_a();
    d.branches[1].airway = AirwayParams{};   // ambos (airway + r_manual>0)
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
    d = red_a();
    d.branches[1].r_manual = 0.0;            // ninguno
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

TEST(RedValidacion, DesconexaLanza) {
    // dos triángulos sin nodos comunes
    NetworkDefinition d;
    d.branches = { mk("A1","N1","N2",0.1), mk("A2","N2","N3",0.1), mk("A3","N3","N1",0.1),
                   mk("B1","M1","M2",0.1), mk("B2","M2","M3",0.1), mk("B3","M3","M1",0.1) };
    AtmosphericParams atm;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

TEST(RedValidacion, ArbolSinMallasLanza) {
    NetworkDefinition d;
    d.branches = { mk("T1","S","A",0.1), mk("T2","A","B",0.1) };   // B-N+1 = 0
    AtmosphericParams atm;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

TEST(RedValidacion, ParamsInvalidosLanzan) {
    AtmosphericParams atm;
    SolverParams p; p.tolerance_m3min = 0.0;
    EXPECT_THROW(NetworkSolver::solve(red_a(), atm, p), std::invalid_argument);
    p = SolverParams{}; p.max_iterations = 0;
    EXPECT_THROW(NetworkSolver::solve(red_a(), atm, p), std::invalid_argument);
    auto d = red_a(); d.branches[0].fan_pressure_pa = -10.0;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
    d = red_a(); d.branches[0].q_initial_m3min = -5.0;
    EXPECT_THROW(NetworkSolver::solve(d, atm), std::invalid_argument);
}

// ============================================================================
// Hardy Cross — Red A: solución ANALÍTICA (ley cuadrática):
//   R_eq paralelo = 1/(1/√0.2 + 1/√0.8)² = 0.088889; R_serie = 0.238889
//   Q_total = √(500/0.238889) = 45.74957 m³/s = 2744.9743 m³/min
//   Q_P1 = 2/3·Q_total = 1829.9828 ; Q_P2 = 1/3·Q_total = 914.9914
//   ΔP_P1 = ΔP_P2 = 186.0465 Pa (paralelo ⇒ misma caída)
// Tolerancia de test ±0.05 m³/min con SolverParams{0.006, 1000}: la aserción
// es sobre la SOLUCIÓN, robusta al orden interno de mallas.
// ============================================================================

TEST(RedHardyCross, RedParalelaConvergeALaSolucionAnalitica) {
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(red_a(), atm, sp);

    EXPECT_TRUE(r.converged);
    EXPECT_LE(r.max_residual_m3min, sp.tolerance_m3min);
    EXPECT_GT(r.iterations, 0);

    const auto& f  = r.branches[0];
    const auto& p1 = r.branches[1];
    const auto& p2 = r.branches[2];
    const auto& rr = r.branches[3];
    EXPECT_NEAR(f.q_m3min,  2744.9743, 0.05);
    EXPECT_NEAR(p1.q_m3min, 1829.9828, 0.05);
    EXPECT_NEAR(p2.q_m3min,  914.9914, 0.05);
    EXPECT_NEAR(rr.q_m3min, 2744.9743, 0.05);
    // ratio paralelo √(R2/R1) = 2
    EXPECT_NEAR(p1.q_m3min / p2.q_m3min, 2.0, 1e-3);
    // caídas de presión iguales en paralelo (Kirchhoff II)
    EXPECT_NEAR(p1.pressure_drop_pa, 186.0465, 0.05);
    EXPECT_NEAR(p2.pressure_drop_pa, 186.0465, 0.05);
}

TEST(RedHardyCross, ConservacionEnNodos) {
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(red_a(), atm, sp);
    // Kirchhoff I en cada nodo: Σ entradas − Σ salidas ≈ 0
    for (const std::string& node : {std::string("S"), std::string("A"), std::string("B")}) {
        double balance = 0.0;
        for (const auto& b : r.branches) {
            if (b.to_node == node)   balance += b.q_m3min;
            if (b.from_node == node) balance -= b.q_m3min;
        }
        EXPECT_NEAR(balance, 0.0, 1e-6) << "nodo " << node;
    }
}

TEST(RedHardyCross, RedDosMallasCoincideConProbe) {
    // Red B: F S→A r0.03 fan800 | B1 A→B r0.5 | B2 A→C r0.9
    //                      | B3 B→C r0.6 | B5 C→S r0.25
    NetworkDefinition d;
    d.branches = { mk("F","S","A",0.03,800.0), mk("B1","A","B",0.5),
                   mk("B2","A","C",0.9), mk("B3","B","C",0.6),
                   mk("B5","C","S",0.25) };
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(d, atm, sp);

    EXPECT_TRUE(r.converged);
    EXPECT_EQ(r.mesh_count, 2);
    EXPECT_NEAR(r.branches[0].q_m3min, 2335.2274, 0.05);   // F
    EXPECT_NEAR(r.branches[1].q_m3min, 1109.0863, 0.05);   // B1
    EXPECT_NEAR(r.branches[2].q_m3min, 1226.1411, 0.05);   // B2
    EXPECT_NEAR(r.branches[3].q_m3min, 1109.0863, 0.05);   // B3
    EXPECT_NEAR(r.branches[4].q_m3min, 2335.2274, 0.05);   // B5
}

TEST(RedHardyCross, NoConvergenciaEsAuditable) {
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 1e-9; sp.max_iterations = 1;
    auto r = NetworkSolver::solve(red_a(), atm, sp);

    EXPECT_FALSE(r.converged);
    EXPECT_EQ(r.iterations, 1);
    EXPECT_GT(r.max_residual_m3min, sp.tolerance_m3min);
    bool warned = false;
    for (const auto& w : r.warnings)
        if (w.find("NO CONVERGIO") != std::string::npos) warned = true;
    EXPECT_TRUE(warned);
}

TEST(RedHardyCross, QInicialCustomRespetado) {
    // Con q_initial en una cuerda la solución final es la MISMA (invariante).
    auto d = red_a();
    for (auto& b : d.branches) b.q_initial_m3min = 600.0;
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(d, atm, sp);
    EXPECT_TRUE(r.converged);
    EXPECT_NEAR(r.branches[1].q_m3min, 1829.9828, 0.05);
}

// ============================================================================
// Integración con atkinson + advertencias de velocidad por ramal
// ============================================================================

TEST(RedIntegracion, RamalPorAirwayUsaRTotalDeAtkinson) {
    // Galería del caso de la red: L=500, per=15, A=14, k=0.012 manual
    AirwayParams gal;
    gal.airway_id = "GAL"; gal.length_m = 500.0; gal.perimeter_m = 15.0;
    gal.area_m2 = 14.0; gal.lining = AirwayLining::Manual; gal.atkinson_k = 0.012;

    NetworkDefinition d = red_a();
    d.branches[1].r_manual = 0.0;
    d.branches[1].airway = gal;     // P1 ahora es la galería
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(d, atm, sp);

    // R de P1 = la de AtkinsonCalculator (verificado con calculo independiente): 0.0329113971312304
    EXPECT_NEAR(r.branches[1].r_ns2m8, 0.0329113971312304, 1e-12);
    EXPECT_TRUE(r.converged);
    // velocidad reportada = Q/(60·A)
    EXPECT_NEAR(r.branches[1].velocity_mps,
                r.branches[1].q_m3min / 60.0 / 14.0, 1e-9);
}

TEST(RedIntegracion, VelocidadFueraDeRangoAdvierteArt248) {
    // Red mínima con galería de área enorme → velocidad < 20 m/min
    AirwayParams gal;
    gal.airway_id = "GAL"; gal.length_m = 100.0; gal.perimeter_m = 40.0;
    gal.area_m2 = 100.0; gal.lining = AirwayLining::Manual; gal.atkinson_k = 0.012;

    NetworkDefinition d;
    NetworkBranch b1 = mk("FAN","S","A",0.5, 50.0);
    NetworkBranch b2; b2.branch_id = "GAL"; b2.from_node = "A"; b2.to_node = "S";
    b2.airway = gal;
    d.branches = { b1, b2 };
    AtmosphericParams atm;
    SolverParams sp; sp.tolerance_m3min = 0.006; sp.max_iterations = 1000;
    auto r = NetworkSolver::solve(d, atm, sp);

    ASSERT_TRUE(r.converged);
    bool cita = false;
    for (const auto& w : r.branches[1].warnings)
        if (w.find("248") != std::string::npos) cita = true;
    EXPECT_TRUE(cita);
    // y la advertencia sube al nivel de red con el id del ramal
    bool arriba = false;
    for (const auto& w : r.warnings)
        if (w.find("GAL") != std::string::npos && w.find("248") != std::string::npos)
            arriba = true;
    EXPECT_TRUE(arriba);
}
