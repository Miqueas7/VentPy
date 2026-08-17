/**
 * @file test_cobertura.cpp
 * @brief Tests del análisis de déficit/cobertura (DS 024, Art. 252 f/g).
 */

#include <gtest/gtest.h>
#include <stdexcept>

#include "ventpy/cobertura.hpp"

using namespace ventpy;

// ============================================================================
// compare_zone — medición directa (nivel puro)
// ============================================================================

TEST(CoverageZone, DirectaCumpleSinAdvertencias) {
    ZoneMeasurement m;
    m.zone_name = "Rampa 4200";
    m.q_measured_m3min = 250.0;

    auto r = CoverageCalculator::compare_zone(207.0, m);

    EXPECT_EQ(r.zone_name, "Rampa 4200");
    EXPECT_DOUBLE_EQ(r.q_required_m3min, 207.0);
    EXPECT_DOUBLE_EQ(r.q_measured_m3min, 250.0);
    EXPECT_NEAR(r.coverage_ratio, 250.0 / 207.0, 1e-9);   // 1.2077 crudo
    EXPECT_TRUE(r.compliant);
    EXPECT_FALSE(r.near_deficit_warning);                 // 1.2077 > 1.10
    EXPECT_FALSE(r.overventilated);                       // 1.2077 < 1.5
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 0.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

TEST(CoverageZone, DeficitUsaSafetyCeil) {
    ZoneMeasurement m;
    m.zone_name = "Frente N-02";
    m.q_measured_m3min = 90.2;

    auto r = CoverageCalculator::compare_zone(100.0, m);

    EXPECT_FALSE(r.compliant);
    // deficit = ceil(100 - 90.2) = ceil(9.8) = 10 — NUNCA 9
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 10.0);
    EXPECT_NEAR(r.coverage_ratio, 0.902, 1e-9);
}

TEST(CoverageZone, CoberturaJustaGeneraAdvertenciaDeMargen) {
    ZoneMeasurement m;
    m.zone_name = "Tajeo 380";
    m.q_measured_m3min = 500.0;

    auto r = CoverageCalculator::compare_zone(500.0, m);

    EXPECT_TRUE(r.compliant);            // medido >= requerido
    EXPECT_TRUE(r.near_deficit_warning); // 1.00 < 1.10 (margen ingenieril 10%)
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 0.0);
}

TEST(CoverageZone, SobreVentilacionAdvierte) {
    ZoneMeasurement m;
    m.zone_name = "Bypass 12";
    m.q_measured_m3min = 160.0;

    auto r = CoverageCalculator::compare_zone(100.0, m);

    EXPECT_TRUE(r.compliant);
    EXPECT_TRUE(r.overventilated);       // 1.6 > 1.5 (factor ingenieril)
}

TEST(CoverageZone, UmbralesPersonalizados) {
    CoverageParams p;
    p.warning_margin = 0.30;             // exigir 130% para no advertir
    p.overventilation_factor = 2.0;

    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 120.0;

    auto r = CoverageCalculator::compare_zone(100.0, m, p);
    EXPECT_TRUE(r.near_deficit_warning);  // 1.2 < 1.3
    EXPECT_FALSE(r.overventilated);       // 1.2 < 2.0
}

// ============================================================================
// compare_zone — validación en frontera
// ============================================================================

TEST(CoverageZone, FuenteAmbiguaLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;
    m.stations.push_back({"E-1", 10.0, 1.0});   // ambas fuentes

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, SinFuenteLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, RequeridoNoPositivoLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;

    EXPECT_THROW(CoverageCalculator::compare_zone(0.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, MedidoNegativoLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = -5.0;

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, ParamsInvalidosLanzan) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;

    CoverageParams margen_negativo;
    margen_negativo.warning_margin = -0.1;
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, margen_negativo),
                 std::invalid_argument);

    CoverageParams factor_bajo;
    factor_bajo.overventilation_factor = 1.0;   // debe ser > 1
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, factor_bajo),
                 std::invalid_argument);

    CoverageParams velocidades_invertidas;
    velocidades_invertidas.min_velocity_mpm = 300.0;   // min >= max
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, velocidades_invertidas),
                 std::invalid_argument);
}
