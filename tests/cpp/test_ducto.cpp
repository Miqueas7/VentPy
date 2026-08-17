/**
 * @file test_ducto.cpp
 * @brief Tests de dimensionamiento de ducto (técnico y económico).
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "ventpy/atkinson.hpp"
#include "ventpy/ducto.hpp"

using namespace ventpy;

namespace {
DuctSizingParams base_params() {
    DuctSizingParams p;
    p.q_m3min = 1200.0;                      // 20 m³/s
    p.length_m = 400.0;
    p.duct_lining = AirwayLining::DuctFlexibleSpiral;   // k = 0.011 (Tabla 5.1)
    return p;
}
} // namespace

TEST(DuctoTecnico, EligeMenorDiametroQueCumpleVelocidad) {
    AtmosphericParams atm;
    auto r = DuctSizingCalculator::calculate(base_params(), atm);

    // probe_sp3a.py: solo 1.22 baja de 20 m/s (1.07 da 22.24)
    EXPECT_TRUE(r.feasible);
    EXPECT_DOUBLE_EQ(r.selected_diameter_m, 1.22);
    EXPECT_EQ(r.options.size(), 8u);         // evaluó toda la lista default
    // opción seleccionada auditable
    const auto& sel = r.options.back();
    EXPECT_NEAR(sel.velocity_mps, 17.1088356, 1e-6);
    EXPECT_NEAR(sel.r_total, 10.5930663, 1e-6);
    EXPECT_NEAR(sel.pressure_drop_pa, 4237.2263043, 1e-5);
    // las descartadas dicen por qué
    EXPECT_FALSE(r.options[6].velocity_ok);   // 1.07 → 22.24 m/s
    EXPECT_FALSE(r.options[6].rejection_reason.empty());
}

TEST(DuctoTecnico, RestriccionDePresionPuedeDejarloInviable) {
    auto p = base_params();
    p.available_pressure_pa = 3000.0;         // 1.22 necesita 4237 Pa
    AtmosphericParams atm;
    auto r = DuctSizingCalculator::calculate(p, atm);
    EXPECT_FALSE(r.feasible);
    EXPECT_DOUBLE_EQ(r.selected_diameter_m, 0.0);
    ASSERT_FALSE(r.warnings.empty());         // nunca elegir "el menos malo" en silencio
}

TEST(DuctoTecnico, EspejoCircularAtkinson) {
    // Mismo caso físico por ambas rutas: D=0.60 ⇒ per=πD, A=πD²/4
    AtmosphericParams atm;
    AirwayParams a;
    a.airway_id = "espejo";
    a.length_m = 400.0;
    a.perimeter_m = 1.8849555921538759;
    a.area_m2 = 0.2827433388230814;
    a.lining = AirwayLining::DuctFlexibleSpiral;
    auto ra = AtkinsonCalculator::calculate_resistance(a, atm);

    auto p = base_params();
    p.diameters_m = {0.60};
    p.max_velocity_mps = 1000.0;              // no restringir: solo comparar R
    auto rd = DuctSizingCalculator::calculate(p, atm);

    ASSERT_EQ(rd.options.size(), 1u);
    EXPECT_NEAR(rd.options[0].r_total, ra.r_total, 1e-9);
    EXPECT_NEAR(rd.options[0].r_total, 368.18371019628114, 1e-6);
}

TEST(DuctoTecnico, Validaciones) {
    AtmosphericParams atm;
    auto p = base_params(); p.q_m3min = 0.0;
    EXPECT_THROW(DuctSizingCalculator::calculate(p, atm), std::invalid_argument);
    p = base_params(); p.length_m = -1.0;
    EXPECT_THROW(DuctSizingCalculator::calculate(p, atm), std::invalid_argument);
    p = base_params(); p.duct_lining = AirwayLining::Manual;  // sin k
    EXPECT_THROW(DuctSizingCalculator::calculate(p, atm), std::invalid_argument);
    p = base_params(); p.diameters_m = {0.6, -0.3};
    EXPECT_THROW(DuctSizingCalculator::calculate(p, atm), std::invalid_argument);
}
