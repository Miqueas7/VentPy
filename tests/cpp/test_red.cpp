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
NetworkDefinition red_a() {   // paralelo analítico (probe_sp3b.py)
    NetworkDefinition d;
    d.branches = { mk("F","S","A",0.05,500.0), mk("P1","A","B",0.2),
                   mk("P2","A","B",0.8),       mk("R","B","S",0.1) };
    return d;
}
} // namespace

// ============================================================================
// Topología y validaciones (Task 1)
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
