/**
 * @file test_caudal_equipo.cpp
 * @brief Tests unitarios para cálculo de Q_Eq (caudal por equipos diésel).
 *
 * Verifica cumplimiento de DS 024-2016-EM, Art. 246.
 */

#include <gtest/gtest.h>

#include "ventpy/caudal_equipo.hpp"

using namespace ventpy;

class DieselFlowTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;
    DieselFleet fleet;

    void SetUp() override {
        // Flota típica de mina peruana
        fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70);
        fleet.add_equipment("Dumper AD30", 300.0, 0.90, 0.60);
    }
};

// --- Cálculo básico con flota de 2 equipos ---
TEST_F(DieselFlowTest, BasicFleetCalculation) {
    auto result = DieselFlowCalculator::calculate(fleet, default_config);

    // Scoop: 150 × 0.85 × 0.70 = 89.25 HP_eff
    // Dumper: 300 × 0.90 × 0.60 = 162.0  HP_eff
    // Total HP_eff = 251.25
    // Q_Eq = 251.25 × 3.0 = 753.75 → safety_ceil = 754 m³/min

    EXPECT_DOUBLE_EQ(result.hp_factor_base, 3.0);
    EXPECT_NEAR(result.total_effective_hp, 251.25, 0.01);
    EXPECT_DOUBLE_EQ(result.q_diesel, 754.0);
    EXPECT_EQ(result.equipment_names.size(), 2u);
}

// --- Flota vacía: caudal cero (válido) ---
TEST_F(DieselFlowTest, EmptyFleet_ReturnsZero) {
    DieselFleet empty_fleet;
    auto result = DieselFlowCalculator::calculate(empty_fleet, default_config);

    EXPECT_DOUBLE_EQ(result.q_diesel, 0.0);
    EXPECT_DOUBLE_EQ(result.total_effective_hp, 0.0);
}

// --- Factor HP personalizado (5 m³/min/HP) ---
TEST_F(DieselFlowTest, CustomHPFactor) {
    RegulatoryConfig custom(
        RegulatoryStandard::DS024_Peru,
        3.0, 3000.0, 4.0, 4000.0, 5.0,
        5.0,    // diesel_hp_factor = 5.0 (estándar corporativo)
        30.0, 0.04, 0.15
    );

    auto result = DieselFlowCalculator::calculate(fleet, custom);
    EXPECT_DOUBLE_EQ(result.hp_factor_base, 5.0);
    // 251.25 × 5.0 = 1256.25 → safety_ceil = 1257
    EXPECT_DOUBLE_EQ(result.q_diesel, 1257.0);
}

// --- Equipo con 100% disponibilidad y utilización ---
TEST_F(DieselFlowTest, FullAvailabilityUtilization) {
    DieselFleet single;
    single.add_equipment("Jumbo DD321", 200.0, 1.0, 1.0);

    auto result = DieselFlowCalculator::calculate(single, default_config);
    EXPECT_DOUBLE_EQ(result.total_effective_hp, 200.0);
    EXPECT_DOUBLE_EQ(result.q_diesel, 600.0);  // 200 × 3
}

// --- Validación: HP negativo ---
TEST_F(DieselFlowTest, NegativeHP_ThrowsException) {
    DieselFleet bad_fleet;
    EXPECT_THROW(
        bad_fleet.add_equipment("Bad", -100.0, 0.85, 0.70),
        std::invalid_argument
    );
}

// --- Validación: disponibilidad > 1 ---
TEST_F(DieselFlowTest, AvailabilityOverOne_ThrowsException) {
    DieselFleet bad_fleet;
    EXPECT_THROW(
        bad_fleet.add_equipment("Bad", 100.0, 1.5, 0.70),
        std::invalid_argument
    );
}

// --- Validación: utilización negativa ---
TEST_F(DieselFlowTest, NegativeUtilization_ThrowsException) {
    DieselFleet bad_fleet;
    EXPECT_THROW(
        bad_fleet.add_equipment("Bad", 100.0, 0.85, -0.1),
        std::invalid_argument
    );
}

// --- Referencia normativa ---
TEST_F(DieselFlowTest, ResultContainsRegReference) {
    auto result = DieselFlowCalculator::calculate(fleet, default_config);
    EXPECT_NE(result.regulation_ref.find("Art. 246"), std::string::npos);
}
