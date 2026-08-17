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

// Escala del Art. 247 DS 024-2016-EM (texto original, vigente):
//   hasta 1,500 msnm: 3 m³/min · >1,500: 4 · >3,000: 5 · >4,000: 6
// Semántica de borde: '>' estricto — en el umbral exacto rige la banda inferior.

// --- Hasta 1500 msnm: mínimo normativo ---
TEST_F(PersonnelFlowTest, AtOrBelow1500_UsesMinimum) {
    auto result = PersonnelFlowCalculator::calculate(10, 1500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 3.0);  // Art. 247: hasta 1,500
    EXPECT_DOUBLE_EQ(result.q_personnel, 30.0);     // 10 × 3
}

// --- Banda 1500–3000 msnm ---
TEST_F(PersonnelFlowTest, Between1500And3000_Uses4) {
    auto result = PersonnelFlowCalculator::calculate(10, 2500.0, default_config);

    EXPECT_EQ(result.num_workers, 10);
    EXPECT_DOUBLE_EQ(result.altitude_masl, 2500.0);
    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 4.0);  // Art. 247: +40%
    EXPECT_DOUBLE_EQ(result.q_personnel, 40.0);     // 10 × 4
}

// --- Banda 3000–4000 msnm ---
TEST_F(PersonnelFlowTest, Between3000And4000_Uses5) {
    auto result = PersonnelFlowCalculator::calculate(10, 3500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 5.0);  // Art. 247: +70%
    EXPECT_DOUBLE_EQ(result.q_personnel, 50.0);     // 10 × 5
}

// --- Sobre 4000 msnm: máximo de la escala ---
TEST_F(PersonnelFlowTest, Above4000_Uses6) {
    auto result = PersonnelFlowCalculator::calculate(10, 4500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 6.0);  // Art. 247: +100%
    EXPECT_DOUBLE_EQ(result.q_personnel, 60.0);     // 10 × 6
}

// --- Exactamente en un umbral (3000): rige la banda inferior ---
TEST_F(PersonnelFlowTest, ExactlyAt3000_UsesLowerBand) {
    auto result = PersonnelFlowCalculator::calculate(5, 3000.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 4.0);
    EXPECT_DOUBLE_EQ(result.q_personnel, 20.0);
}

// --- Un solo trabajador ---
TEST_F(PersonnelFlowTest, SingleWorker) {
    auto result = PersonnelFlowCalculator::calculate(1, 0.0, default_config);

    EXPECT_DOUBLE_EQ(result.q_personnel, 3.0);
}

// --- Config personalizada: empresa usa 7 m³/min sobre 4000 ---
TEST_F(PersonnelFlowTest, CustomConfig_HigherStandard) {
    RegulatoryConfig custom(
        RegulatoryStandard::DS024_Peru,
        3.0,     // min_flow_per_person
        1500.0,  // threshold 1
        4.5,     // flow above t1 (estándar corporativo)
        3000.0,  // threshold 2
        5.5,     // flow above t2
        4000.0,  // threshold 3
        7.0,     // flow above t3 (estándar corporativo agresivo)
        3.0, 30.0, 0.04, 0.15
    );

    auto result = PersonnelFlowCalculator::calculate(20, 4200.0, custom);
    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 7.0);
    EXPECT_DOUBLE_EQ(result.q_personnel, 140.0);  // 20 × 7
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
