/**
 * @file test_caudal_personal.cpp
 * @brief Tests unitarios para cálculo de Q_Per (caudal por personal).
 *
 * Verifica cumplimiento de DS 024-2016-EM, Art. 247 (y el piso normativo
 * del caudal por persona en las demás normas).
 */

#include <gtest/gtest.h>

#include "ventpy/caudal_personal.hpp"

using namespace ventpy;

class PersonnelFlowTest : public ::testing::Test {
protected:
    RegulatoryConfig default_config;  // Valores DS 024 por defecto
};

// Escala del Art. 247 DS 024-2016-EM (texto original, vigente):
//   hasta 1,500 msnm: 3 m³/min · 1,500-3,000: 4 · 3,000-4,000: 5 · sobre 4,000: 6
// Semántica de borde: el texto pone cada umbral en dos bandas; en el umbral
// exacto rige la banda alta (caudal mayor, lado seguro).

// --- Bajo 1500 msnm: mínimo normativo ---
TEST_F(PersonnelFlowTest, Below1500_UsesMinimum) {
    auto result = PersonnelFlowCalculator::calculate(10, 1499.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 3.0);  // Art. 247: hasta 1,500
    EXPECT_DOUBLE_EQ(result.q_personnel, 30.0);     // 10 × 3
}

// --- Exactamente 1500 msnm: rige la banda alta (antes se fijaba 3 × 10 = 30) ---
TEST_F(PersonnelFlowTest, ExactlyAt1500_UsesUpperBand) {
    auto result = PersonnelFlowCalculator::calculate(10, 1500.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 4.0);  // Art. 247 num. 1
    EXPECT_DOUBLE_EQ(result.q_personnel, 40.0);     // 10 × 4
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

// --- Exactamente en un umbral (3000 y 4000): rige la banda alta ---
TEST_F(PersonnelFlowTest, ExactlyAt3000_UsesUpperBand) {
    auto result = PersonnelFlowCalculator::calculate(5, 3000.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 5.0);  // Art. 247 num. 2
    EXPECT_DOUBLE_EQ(result.q_personnel, 25.0);          // 5 × 5
}

TEST_F(PersonnelFlowTest, ExactlyAt4000_UsesUpperBand) {
    auto result = PersonnelFlowCalculator::calculate(5, 4000.0, default_config);

    EXPECT_DOUBLE_EQ(result.flow_per_person_base, 6.0);  // Art. 247 num. 3
    EXPECT_DOUBLE_EQ(result.q_personnel, 30.0);          // 5 × 6
}

// La cita del borde también dice ">=".
TEST_F(PersonnelFlowTest, BorderCitationUsesGreaterOrEqual) {
    PersonnelParams per;
    per.num_workers = 10;
    AtmosphericParams atm;
    atm.altitude_masl = 1500.0;
    const auto r = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 12.0, default_config);
    EXPECT_NE(r.regulation_ref.find("(>=1500 msnm)"), std::string::npos)
        << r.regulation_ref;
}

// --- El nivel de actividad nunca baja el caudal del mínimo de la norma ---
// 20 trabajadores en reposo a 0 msnm, frente de 1 m² (la velocidad mínima no
// gobierna: 1 × 20/60 × 60 = 20 m³/min). Antes: 20 × 3 × 0,6 = 36 m³/min, bajo
// el mínimo legal. Ahora: 20 × 3 = 60 (DS 024-2016-EM, Art. 247).
TEST_F(PersonnelFlowTest, RestActivityNeverBelowNormativeMinimum) {
    PersonnelParams per;
    per.num_workers = 20;
    per.activity = ActivityLevel::Rest;
    const AtmosphericParams atm;  // 0 msnm
    const auto r = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 1.0, default_config);
    EXPECT_DOUBLE_EQ(r.flow_per_person_corrected, 3.0);
    EXPECT_DOUBLE_EQ(r.q_personnel, 60.0);

    per.activity = ActivityLevel::Light;  // antes 20 × 3 × 0,8 = 48
    EXPECT_DOUBLE_EQ(PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 1.0, default_config).q_personnel, 60.0);
}

// Mismo piso con las otras normas: México 100 × 1,5 = 150 (8.4.4 a) 1);
// Colombia 20 × 3 = 60 (Art. 54 num. 1).
TEST_F(PersonnelFlowTest, RestActivityFloorFollowsTheStandard) {
    PersonnelParams per;
    per.activity = ActivityLevel::Rest;
    const AtmosphericParams atm;
    per.num_workers = 100;
    EXPECT_DOUBLE_EQ(PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::GeneralMine, 1.0, RegulatoryConfig::mexico()).q_personnel,
        150.0);
    per.num_workers = 20;
    EXPECT_DOUBLE_EQ(PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 1.0, RegulatoryConfig::colombia()).q_personnel,
        60.0);
}

// La actividad pesada sigue subiendo el caudal: 10 × 3 × 1,3 = 39 → ceil 39.
TEST_F(PersonnelFlowTest, HeavyActivityStillRaisesFlow) {
    PersonnelParams per;
    per.num_workers = 10;
    per.activity = ActivityLevel::Heavy;
    const AtmosphericParams atm;
    const auto r = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 1.0, default_config);
    EXPECT_GT(r.q_personnel, 30.0);
}

// La velocidad mínima que dimensiona sigue a la norma: Perú 20 m/min en un
// frente de 12 m² da 12 × 20 = 240 m³/min, sobre los 2 × 3 = 6 normativos.
TEST_F(PersonnelFlowTest, VelocityFloorUsesNormativeMinimum) {
    PersonnelParams per;
    per.num_workers = 2;
    const AtmosphericParams atm;
    const auto r = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::Stope, 12.0, default_config);
    EXPECT_DOUBLE_EQ(r.q_personnel, 240.0);  // antes 12 × 12 m/min = 144
    // Con ANFO u otros agentes de voladura: 12 × 25 = 300.
    EXPECT_DOUBLE_EQ(PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::Stope, 12.0, default_config, false, true).q_personnel, 300.0);
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
