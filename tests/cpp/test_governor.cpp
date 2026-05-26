/**
 * @file test_governor.cpp
 * @brief Tests unitarios para el Governor (motor de cálculo total).
 *
 * Verifica la lógica de selección y el cálculo consolidado.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "ventpy/governor.hpp"

using namespace ventpy;

class GovernorTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;
    VentilationGovernor governor{default_config};
};

// ============================================================================
// Frente de desarrollo: caso completo
// ============================================================================

TEST_F(GovernorTest, DevelopmentFace_FullCalculation) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;

    // Personal: 15 trabajadores a 4200 msnm → 15 × 5 = 75 m³/min
    input.num_workers = 15;
    input.altitude_masl = 4200.0;

    // Equipos: un scoop de 150 HP al 85%/70%
    DieselFleet fleet;
    fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70);
    input.diesel_fleet = fleet;
    // HP_eff = 150 × 0.85 × 0.70 = 89.25
    // Q_Eq = 89.25 × 3 = 267.75

    // Explosivos: 50 kg ANFO
    BlastingParams blast{
        .explosive_kg = 50.0,
        .gas_volume_per_kg = 0.04,
        .dilution_time_min = 30.0,
        .face_area_m2 = 12.0,
        .face_length_m = 200.0
    };
    input.blasting_params = blast;
    // Q_Exp = (50 × 0.04) / 30 = 0.0667

    auto result = governor.calculateTotalDemand(input);

    // Gobernante: max(75, 267.75, 0.0667) = 267.75 (diésel)
    EXPECT_EQ(result.governing_factor, "diesel (Q_Eq)");
    EXPECT_NEAR(result.q_governing_m3min, 267.75, 0.01);

    // Con fugas 15%: 267.75 × 1.15 = 307.9125 → ceil = 308
    EXPECT_DOUBLE_EQ(result.q_total_m3min, std::ceil(267.75 * 1.15));

    // Verificar que todos los componentes están presentes
    EXPECT_TRUE(result.personnel.has_value());
    EXPECT_TRUE(result.diesel.has_value());
    EXPECT_TRUE(result.blasting.has_value());
    EXPECT_TRUE(result.leakage.has_value());

    // CFM conversion
    EXPECT_GT(result.q_total_cfm, 0.0);
}

// ============================================================================
// Frente solo con personal (sin equipos ni explosivos)
// ============================================================================

TEST_F(GovernorTest, PersonnelOnly) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 10;
    input.altitude_masl = 2500.0;

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = 10 × 3 = 30
    EXPECT_NEAR(result.q_personnel_m3min, 30.0, 0.01);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 0.0);

    // Gobernante: 30 (personal)
    EXPECT_EQ(result.governing_factor, "personnel (Q_Per)");
    // Total: ceil(30 × 1.15) = ceil(34.5) = 35
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 35.0);
}

// ============================================================================
// Mina total: sumatoria
// ============================================================================

TEST_F(GovernorTest, GeneralMine_Summation) {
    VentilationInput input;
    input.zone_type = ZoneType::GeneralMine;
    input.num_workers = 100;
    input.altitude_masl = 4500.0;  // 5 m³/min por persona

    DieselFleet fleet;
    fleet.add_equipment("Scoop1", 200.0, 0.9, 0.8);
    input.diesel_fleet = fleet;

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = 100 × 5 = 500
    // Q_Eq = 200 × 0.9 × 0.8 × 3 = 432
    // Q_Exp = 0 (no explosives)
    // GeneralMine: Q = 500 + 432 + 0 = 932
    EXPECT_EQ(result.governing_factor, "summation (mine total)");
    EXPECT_NEAR(result.q_governing_m3min, 932.0, 0.01);
}

// ============================================================================
// Safety ceil: nunca redondea abajo
// ============================================================================

TEST_F(GovernorTest, SafetyCeil_NeverRoundsDown) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 1;
    input.altitude_masl = 0.0;

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = 1 × 3 = 3.0
    // Con fugas: 3.0 × 1.15 = 3.45 → ceil = 4
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 4.0);
}

// ============================================================================
// Factor de fugas personalizado
// ============================================================================

TEST_F(GovernorTest, CustomLeakageFactor) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 10;
    input.altitude_masl = 2500.0;
    input.leakage_factor = 0.25;  // 25% fugas

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = 30, fugas 25%: 30 × 1.25 = 37.5 → ceil = 38
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 38.0);
}

// ============================================================================
// Entrada vacía: sin parámetros
// ============================================================================

TEST_F(GovernorTest, EmptyInput_ReturnsZero) {
    VentilationInput input;
    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_total_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_total_cfm, 0.0);
}

// ============================================================================
// Config inmutable: verificar que el governor preserva la config
// ============================================================================

TEST_F(GovernorTest, ConfigIsPreserved) {
    EXPECT_DOUBLE_EQ(governor.config().min_flow_per_person(), 3.0);
    EXPECT_DOUBLE_EQ(governor.config().diesel_hp_factor(), 3.0);
    EXPECT_DOUBLE_EQ(governor.config().default_leakage_factor(), 0.15);
}
