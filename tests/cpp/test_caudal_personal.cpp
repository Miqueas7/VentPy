/**
 * @file test_caudal_personal.cpp
 * @brief Tests unitarios para cálculo de Q_Per (caudal por personal).
 *
 * Verifica cumplimiento de DS 024-2016-EM, Art. 236.
 */

#include <gtest/gtest.h>

#include "ventpy/caudal_personal.hpp"

using namespace ventpy;

class PersonnelFlowTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;  // Valores DS 024 por defecto
};

// --- Caso base: altitud baja, mínimo normativo ---
TEST_F(PersonnelFlowTest, BasicCalculation_LowAltitude) {
    auto result = PersonnelFlowCalculator::calculate(10, 2500.0, default_config);

    EXPECT_EQ(result.num_workers, 10);
    EXPECT_DOUBLE_EQ(result.altitude_masl, 2500.0);
    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 3.0);  // Mínimo DS 024
    EXPECT_DOUBLE_EQ(result.q_personnel, 30.0);     // 10 × 3
}

// --- Altitud sobre 3000 msnm: escalado ---
TEST_F(PersonnelFlowTest, AltitudeAbove3000_UsesThreshold1) {
    auto result = PersonnelFlowCalculator::calculate(10, 3500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 4.0);  // Estándar corporativo
    EXPECT_DOUBLE_EQ(result.q_personnel, 40.0);     // 10 × 4
}

// --- Altitud sobre 4000 msnm: escalado máximo ---
TEST_F(PersonnelFlowTest, AltitudeAbove4000_UsesThreshold2) {
    auto result = PersonnelFlowCalculator::calculate(10, 4500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 5.0);  // Estándar corporativo alto
    EXPECT_DOUBLE_EQ(result.q_personnel, 50.0);     // 10 × 5
}

// --- Exactamente en el umbral (3000): usa el mínimo, no escala ---
TEST_F(PersonnelFlowTest, ExactlyAtThreshold1_UsesMinimum) {
    auto result = PersonnelFlowCalculator::calculate(5, 3000.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 3.0);
    EXPECT_DOUBLE_EQ(result.q_personnel, 15.0);
}

// --- Un solo trabajador ---
TEST_F(PersonnelFlowTest, SingleWorker) {
    auto result = PersonnelFlowCalculator::calculate(1, 0.0, default_config);

    EXPECT_DOUBLE_EQ(result.q_personnel, 3.0);
}

// --- Config personalizada: empresa usa 6 m³/min sobre 4000 ---
TEST_F(PersonnelFlowTest, CustomConfig_HigherStandard) {
    RegulatoryConfig custom(
        RegulatoryStandard::DS024_Peru,
        3.0,     // min_flow_per_person
        3000.0,  // threshold 1
        4.5,     // flow above t1
        4000.0,  // threshold 2
        6.0,     // flow above t2 (estándar corporativo agresivo)
        3.0, 30.0, 0.04, 0.15
    );

    auto result = PersonnelFlowCalculator::calculate(20, 4200.0, custom);
    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 6.0);
    EXPECT_DOUBLE_EQ(result.q_personnel, 120.0);  // 20 × 6
}

// --- Validación: 0 trabajadores debe lanzar excepción ---
TEST_F(PersonnelFlowTest, ZeroWorkers_ThrowsException) {
    EXPECT_THROW(
        PersonnelFlowCalculator::calculate(0, 3000.0, default_config),
        std::invalid_argument
    );
}

// --- Validación: trabajadores negativos ---
TEST_F(PersonnelFlowTest, NegativeWorkers_ThrowsException) {
    EXPECT_THROW(
        PersonnelFlowCalculator::calculate(-5, 3000.0, default_config),
        std::invalid_argument
    );
}

// --- Validación: altitud negativa ---
TEST_F(PersonnelFlowTest, NegativeAltitude_ThrowsException) {
    EXPECT_THROW(
        PersonnelFlowCalculator::calculate(10, -100.0, default_config),
        std::invalid_argument
    );
}

// --- Referencia normativa presente ---
TEST_F(PersonnelFlowTest, ResultContainsRegulatoryReference) {
    auto result = PersonnelFlowCalculator::calculate(10, 2500.0, default_config);
    EXPECT_FALSE(result.regulation_ref.empty());
    EXPECT_NE(result.regulation_ref.find("DS 024"), std::string::npos);
}
