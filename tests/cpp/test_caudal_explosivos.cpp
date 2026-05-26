/**
 * @file test_caudal_explosivos.cpp
 * @brief Tests unitarios para cálculo de Q_Exp (caudal por explosivos).
 *
 * Verifica cumplimiento de DS 024-2016-EM, Art. 243-244.
 */

#include <gtest/gtest.h>

#include "ventpy/caudal_explosivos.hpp"

using namespace ventpy;

class BlastingFlowTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;
};

// --- Caso típico: voladura en frente ciego ---
TEST_F(BlastingFlowTest, TypicalBlasting) {
    BlastingParams params{
        .explosive_kg       = 50.0,   // 50 kg de ANFO
        .gas_volume_per_kg  = 0.04,   // 0.04 m³/kg
        .dilution_time_min  = 30.0,   // 30 min (máximo normativo)
        .face_area_m2       = 12.0,   // 4m × 3m
        .face_length_m      = 200.0   // 200 m de labor
    };

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    // Q_Exp = (50 × 0.04) / 30 = 2.0 / 30 = 0.0667 m³/min
    EXPECT_NEAR(result.q_blasting, 50.0 * 0.04 / 30.0, 0.0001);
    EXPECT_DOUBLE_EQ(result.explosive_kg, 50.0);
    EXPECT_DOUBLE_EQ(result.total_gas_volume, 2.0);
}

// --- Voladura grande: 200 kg ---
TEST_F(BlastingFlowTest, LargeBlasting) {
    BlastingParams params{
        .explosive_kg       = 200.0,
        .gas_volume_per_kg  = 0.04,
        .dilution_time_min  = 20.0,
        .face_area_m2       = 16.0,
        .face_length_m      = 150.0
    };

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    // Q_Exp = (200 × 0.04) / 20 = 8.0 / 20 = 0.4 m³/min
    EXPECT_NEAR(result.q_blasting, 0.4, 0.0001);
}

// --- Tiempo de dilución excede el máximo normativo: advertencia ---
TEST_F(BlastingFlowTest, DilutionTimeExceedsMax_ContainsWarning) {
    BlastingParams params{
        .explosive_kg       = 50.0,
        .gas_volume_per_kg  = 0.04,
        .dilution_time_min  = 45.0,  // > 30 min
        .face_area_m2       = 12.0,
        .face_length_m      = 200.0
    };

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    EXPECT_NE(result.regulation_ref.find("ADVERTENCIA"), std::string::npos);
}

// --- Validación: explosivo = 0 kg ---
TEST_F(BlastingFlowTest, ZeroExplosive_ThrowsException) {
    BlastingParams params{0.0, 0.04, 30.0, 12.0, 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- CRITICO: tiempo de dilución = 0 (división por cero) ---
TEST_F(BlastingFlowTest, ZeroDilutionTime_ThrowsException) {
    BlastingParams params{50.0, 0.04, 0.0, 12.0, 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- Validación: gas_volume negativo ---
TEST_F(BlastingFlowTest, NegativeGasVolume_ThrowsException) {
    BlastingParams params{50.0, -0.04, 30.0, 12.0, 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- Validación: área de frente = 0 ---
TEST_F(BlastingFlowTest, ZeroFaceArea_ThrowsException) {
    BlastingParams params{50.0, 0.04, 30.0, 0.0, 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- Referencia normativa Art. 243 ---
TEST_F(BlastingFlowTest, ResultContainsArticle243) {
    BlastingParams params{50.0, 0.04, 30.0, 12.0, 200.0};
    auto result = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_NE(result.regulation_ref.find("Art. 243"), std::string::npos);
}
