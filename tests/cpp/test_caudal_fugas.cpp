/**
 * @file test_caudal_fugas.cpp
 * @brief Tests unitarios para cálculo de Q_Fug (caudal por fugas).
 *
 * Verifica DS 024-2016-EM, Art. 252 (mangas de ventilación).
 */

#include <gtest/gtest.h>

#include "ventpy/caudal_fugas.hpp"

using namespace ventpy;

class LeakageFlowTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;
};

// --- Factor por defecto (15%) ---
TEST_F(LeakageFlowTest, DefaultLeakageFactor) {
    auto result = LeakageFlowCalculator::calculate(100.0, default_config);

    EXPECT_DOUBLE_EQ(result.base_flow, 100.0);
    EXPECT_DOUBLE_EQ(result.base_leakage_factor, 0.15);
    EXPECT_DOUBLE_EQ(result.q_leakage, 15.0);
    EXPECT_DOUBLE_EQ(result.q_at_fan, 115.0);
}

// --- Factor personalizado ---
TEST_F(LeakageFlowTest, CustomLeakageFactor) {
    auto result = LeakageFlowCalculator::calculate(
        200.0, 0.25, default_config);

    EXPECT_DOUBLE_EQ(result.base_leakage_factor, 0.25);
    EXPECT_DOUBLE_EQ(result.q_leakage, 50.0);
    EXPECT_DOUBLE_EQ(result.q_at_fan, 250.0);
}

// --- Fugas cero (ducto perfecto teórico) ---
TEST_F(LeakageFlowTest, ZeroLeakage) {
    auto result = LeakageFlowCalculator::calculate(
        100.0, 0.0, default_config);

    EXPECT_DOUBLE_EQ(result.q_leakage, 0.0);
    EXPECT_DOUBLE_EQ(result.q_at_fan, 100.0);
}

// --- Caudal base cero ---
TEST_F(LeakageFlowTest, ZeroBaseFlow) {
    auto result = LeakageFlowCalculator::calculate(0.0, default_config);
    EXPECT_DOUBLE_EQ(result.q_at_fan, 0.0);
}

// --- Validación: factor > 1 ---
TEST_F(LeakageFlowTest, LeakageFactorAboveOne_ThrowsException) {
    EXPECT_THROW(
        LeakageFlowCalculator::calculate(100.0, 1.5, default_config),
        std::invalid_argument
    );
}

// --- Validación: factor negativo ---
TEST_F(LeakageFlowTest, NegativeLeakageFactor_ThrowsException) {
    EXPECT_THROW(
        LeakageFlowCalculator::calculate(100.0, -0.1, default_config),
        std::invalid_argument
    );
}

// --- Validación: caudal base negativo ---
TEST_F(LeakageFlowTest, NegativeBaseFlow_ThrowsException) {
    EXPECT_THROW(
        LeakageFlowCalculator::calculate(-50.0, default_config),
        std::invalid_argument
    );
}
