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

    // Dilución de NOx: 50 × 10 = 500 L → 500 / (5e-6 × 30) / 1000 × 1,2 = 4.000
    // Dilución de CO:  50 × 40 = 2.000 L → 2.000 / (25e-6 × 30) / 1000 × 1,2 = 3.200
    EXPECT_NEAR(result.q_for_nox_dilution, 4000.0, 0.01);
    EXPECT_NEAR(result.q_for_co_dilution, 3200.0, 0.01);
    // Recambio: 12 × 200 = 2.400 m³ × 5 (labor de más de 100 m) / 30 = 400
    EXPECT_NEAR(result.q_for_volume_exchange, 400.0, 0.0001);
    EXPECT_DOUBLE_EQ(result.q_blasting, 4000.0);
    EXPECT_EQ(result.governing_criterion, "dilucion NOx");
    EXPECT_DOUBLE_EQ(result.explosive_kg, 50.0);
    // gas_volume_per_kg solo fija el volumen reportado: 50 × 0,04 = 2 m³
    EXPECT_DOUBLE_EQ(result.total_gas_volume_m3, 2.0);
}

// --- gas_volume_per_kg no cambia el caudal ---
// Hasta la 0.2.1 declararlo desviaba a volumen total / tiempo (1 m³/min).
TEST_F(BlastingFlowTest, GasVolumeNoCambiaElCaudal) {
    BlastingParams sin_volumen{.explosive_kg = 50.0, .dilution_time_min = 30.0,
                               .face_area_m2 = 12.0, .face_length_m = 200.0};
    BlastingParams con_volumen = sin_volumen;
    con_volumen.gas_volume_per_kg = 0.04;

    auto a = BlastingFlowCalculator::calculate(sin_volumen, default_config);
    auto b = BlastingFlowCalculator::calculate(con_volumen, default_config);

    EXPECT_DOUBLE_EQ(a.q_blasting, b.q_blasting);
    EXPECT_EQ(a.governing_criterion, b.governing_criterion);
    EXPECT_DOUBLE_EQ(b.total_gas_volume_m3, 2.0);
    // Sin declararlo se estima: (2.000 + 500 + 50 × 20) L = 3,5 m³
    EXPECT_DOUBLE_EQ(a.total_gas_volume_m3, 3.5);
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

    // NOx: 200 × 10 = 2.000 L → 2.000 / (5e-6 × 20) / 1000 × 1,2 = 24.000
    // Recambio: 16 × 150 = 2.400 m³ × 5 / 20 = 600
    EXPECT_NEAR(result.q_for_volume_exchange, 600.0, 0.0001);
    EXPECT_DOUBLE_EQ(result.q_blasting, 24000.0);
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
    BlastingParams params{.explosive_kg = 0.0, .gas_volume_per_kg = 0.04,
                          .dilution_time_min = 30.0, .face_area_m2 = 12.0,
                          .face_length_m = 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- CRITICO: tiempo de dilución = 0 (división por cero) ---
TEST_F(BlastingFlowTest, ZeroDilutionTime_ThrowsException) {
    BlastingParams params{.explosive_kg = 50.0, .gas_volume_per_kg = 0.04,
                          .dilution_time_min = 0.0, .face_area_m2 = 12.0,
                          .face_length_m = 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- Validación: gas_volume negativo ---
TEST_F(BlastingFlowTest, NegativeGasVolume_ThrowsException) {
    BlastingParams params{.explosive_kg = 50.0, .gas_volume_per_kg = -0.04,
                          .dilution_time_min = 30.0, .face_area_m2 = 12.0,
                          .face_length_m = 200.0};
    EXPECT_THROW(
        BlastingFlowCalculator::calculate(params, default_config),
        std::invalid_argument
    );
}

// --- Área de frente = 0: válido en la API simplificada ---
// (0 = "completar después": el Governor la rellena con input.face_area_m2.
// Sin sección no se evalúan recambio ni velocidad; calculate_full sí la exige > 0.)
TEST_F(BlastingFlowTest, ZeroFaceArea_AllowedInSimplifiedApi) {
    BlastingParams params{.explosive_kg = 50.0, .gas_volume_per_kg = 0.04,
                          .dilution_time_min = 30.0, .face_area_m2 = 0.0,
                          .face_length_m = 200.0};
    auto result = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_DOUBLE_EQ(result.q_blasting, 4000.0);   // dilución de NOx
    EXPECT_DOUBLE_EQ(result.q_for_min_velocity, 0.0);
    EXPECT_DOUBLE_EQ(result.face_volume_m3, 0.0);
}

// --- Referencia normativa Art. 243 ---
TEST_F(BlastingFlowTest, ResultContainsArticle243) {
    BlastingParams params{.explosive_kg = 50.0, .gas_volume_per_kg = 0.04,
                          .dilution_time_min = 30.0, .face_area_m2 = 12.0,
                          .face_length_m = 200.0};
    auto result = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_NE(result.regulation_ref.find("Art. 243"), std::string::npos);
}

// --- Sin sección ni longitud: solo los criterios de dilución ---
// 60 kg de ANFO, 30 min:
//   NOx = 60 × 10 = 600 L → 600 / (5e-6 × 30) / 1000 × 1,2 = 4.800 m³/min
//   CO  = 60 × 40 = 2.400 L → 2.400 / (25e-6 × 30) / 1000 × 1,2 = 3.840 m³/min
TEST_F(BlastingFlowTest, SinVolumenDeGases_ModeloPorContaminante) {
    BlastingParams params{.explosive_kg = 60.0, .dilution_time_min = 30.0};

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    EXPECT_NEAR(result.q_for_co_dilution, 3840.0, 0.01);
    EXPECT_NEAR(result.q_for_nox_dilution, 4800.0, 0.01);
    EXPECT_DOUBLE_EQ(result.q_blasting, 4800.0);
    EXPECT_EQ(result.governing_criterion, "dilucion NOx");
    // Sin sección ni longitud no hay criterios de frente que evaluar.
    EXPECT_DOUBLE_EQ(result.q_for_volume_exchange, 0.0);
    EXPECT_DOUBLE_EQ(result.q_for_min_velocity, 0.0);
}

// La versión simple coincide con calculate_full al nivel del mar: la única
// diferencia entre ambas es la corrección por altitud.
TEST_F(BlastingFlowTest, SinVolumenDeGases_CoincideConCompletoAlNivelDelMar) {
    BlastingParams params{.explosive_kg = 60.0, .dilution_time_min = 30.0,
                          .face_area_m2 = 12.0, .face_length_m = 100.0};
    AtmosphericParams nivel_del_mar;
    nivel_del_mar.altitude_masl = 0.0;

    auto simple = BlastingFlowCalculator::calculate(params, default_config);
    auto completo = BlastingFlowCalculator::calculate_full(
        params, nivel_del_mar, default_config);

    EXPECT_NEAR(simple.q_blasting, completo.q_blasting, 1.0);
    EXPECT_EQ(simple.governing_criterion, completo.governing_criterion);
    EXPECT_GT(simple.q_for_min_velocity, 0.0);
}
