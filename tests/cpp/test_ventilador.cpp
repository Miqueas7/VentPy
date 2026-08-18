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
