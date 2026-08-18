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
