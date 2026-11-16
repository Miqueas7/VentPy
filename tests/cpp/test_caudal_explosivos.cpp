/**
 * @file test_caudal_explosivos.cpp
 * @brief Tests unitarios para cálculo de Q_Exp (caudal por explosivos).
 *
 * Con el DS 024-2016-EM la dilución se lleva al TWA del Anexo 15 (CO 25 ppm,
 * NO2 3 ppm) y el barrido nunca baja de la mínima del Art. 248.
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
        .dilution_time_min  = 30.0,   // 30 min (tope configurado)
        .face_area_m2       = 12.0,   // 4m × 3m
        .face_length_m      = 200.0   // 200 m de labor
    };

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    // Objetivos por defecto: TWA del Anexo 15 (CO 25 ppm, NO2 3 ppm).
    // Dilución de NOx: 50 × 10 = 500 L → 500 / (3e-6 × 30) / 1000 × 1,2 = 6.666,67
    // Dilución de CO:  50 × 40 = 2.000 L → 2.000 / (25e-6 × 30) / 1000 × 1,2 = 3.200
    EXPECT_DOUBLE_EQ(result.target_nox_ppm, 3.0);
    EXPECT_DOUBLE_EQ(result.target_co_ppm, 25.0);
    EXPECT_NEAR(result.q_for_nox_dilution, 6666.67, 0.01);
    EXPECT_NEAR(result.q_for_co_dilution, 3200.0, 0.01);
    // Recambio: 12 × 200 = 2.400 m³ × 5 (labor de más de 100 m) / 30 = 400
    EXPECT_NEAR(result.q_for_volume_exchange, 400.0, 0.0001);
    // Barrido: ANFO → 25 m/min (Art. 248) × 12 m² = 300
    EXPECT_NEAR(result.q_for_min_velocity, 300.0, 1e-9);
    EXPECT_DOUBLE_EQ(result.q_blasting, 6667.0);  // ceil(6.666,67)
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

    // NOx: 200 × 10 = 2.000 L → 2.000 / (3e-6 × 20) / 1000 × 1,2 = 40.000
    // Recambio: 16 × 150 = 2.400 m³ × 5 / 20 = 600
    EXPECT_NEAR(result.q_for_volume_exchange, 600.0, 0.0001);
    EXPECT_DOUBLE_EQ(result.q_blasting, 40000.0);
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
    EXPECT_DOUBLE_EQ(result.q_blasting, 6667.0);   // dilución de NOx a 3 ppm
    EXPECT_DOUBLE_EQ(result.q_for_min_velocity, 0.0);
    EXPECT_DOUBLE_EQ(result.face_volume_m3, 0.0);
}

// --- Referencia normativa: reingreso del Art. 240 (el Art. 243 trata de
// diseños de perforación y voladura, no de dilución) ---
TEST_F(BlastingFlowTest, ResultCitesReentryArticle240) {
    BlastingParams params{.explosive_kg = 50.0, .gas_volume_per_kg = 0.04,
                          .dilution_time_min = 30.0, .face_area_m2 = 12.0,
                          .face_length_m = 200.0};
    auto result = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_NE(result.regulation_ref.find("Art. 240"), std::string::npos);
    EXPECT_EQ(result.regulation_ref.find("Art. 243"), std::string::npos);
}

// --- Sin sección ni longitud: solo los criterios de dilución ---
// 60 kg de ANFO, 30 min:
//   NOx = 60 × 10 = 600 L → 600 / (3e-6 × 30) / 1000 × 1,2 = 8.000 m³/min
//   CO  = 60 × 40 = 2.400 L → 2.400 / (25e-6 × 30) / 1000 × 1,2 = 3.840 m³/min
TEST_F(BlastingFlowTest, SinVolumenDeGases_ModeloPorContaminante) {
    BlastingParams params{.explosive_kg = 60.0, .dilution_time_min = 30.0};

    auto result = BlastingFlowCalculator::calculate(params, default_config);

    EXPECT_NEAR(result.q_for_co_dilution, 3840.0, 0.01);
    EXPECT_NEAR(result.q_for_nox_dilution, 8000.0, 0.01);
    EXPECT_DOUBLE_EQ(result.q_blasting, 8000.0);
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

// --- Q_Ex del DS 024, Anexo 38: A x V x N con V del Art. 248 ---
TEST_F(BlastingFlowTest, Anexo38_AnfoUsa25PorMinuto) {
    BlastingParams params{.explosive_kg = 50.0, .face_area_m2 = 12.0};
    params.blasting_levels = 2;
    auto r = BlastingFlowCalculator::calculate_annex38(params);
    EXPECT_DOUBLE_EQ(r.q_blasting, 12.0 * 25.0 * 2);
    EXPECT_DOUBLE_EQ(r.q_for_min_velocity, r.q_blasting);
    EXPECT_NE(r.regulation_ref.find("Anexo 38"), std::string::npos);
}

// La dinamita es explosivo, no agente de voladura: rige el minimo general.
TEST_F(BlastingFlowTest, Anexo38_DinamitaUsa20PorMinuto) {
    BlastingParams params{.explosive_kg = 50.0,
                          .explosive_type = ExplosiveType::Dynamite,
                          .face_area_m2 = 10.0};
    auto r = BlastingFlowCalculator::calculate_annex38(params);
    EXPECT_DOUBLE_EQ(r.q_blasting, 200.0);
}

// Objetivo de dilución por defecto = TWA de NO2 de la norma configurada.
// 60 kg de ANFO, 30 min: NOx = 600 L → 600 / (TWA × 1e-6 × 30) / 1000 × 1,2.
//   Perú (3 ppm): 8.000; Chile (2,6): 9.230,77; Colombia y México (0,2): 120.000.
TEST_F(BlastingFlowTest, DefaultTargetsFollowTheStandard) {
    BlastingParams params{.explosive_kg = 60.0, .dilution_time_min = 30.0};
    const auto pe = BlastingFlowCalculator::calculate(params, RegulatoryConfig::peru());
    const auto cl = BlastingFlowCalculator::calculate(params, RegulatoryConfig::chile());
    const auto co = BlastingFlowCalculator::calculate(params, RegulatoryConfig::colombia());
    const auto mx = BlastingFlowCalculator::calculate(params, RegulatoryConfig::mexico());

    EXPECT_DOUBLE_EQ(pe.target_nox_ppm, 3.0);
    EXPECT_DOUBLE_EQ(cl.target_nox_ppm, 2.6);
    EXPECT_DOUBLE_EQ(cl.target_co_ppm, 44.0);
    EXPECT_DOUBLE_EQ(co.target_nox_ppm, 0.2);
    EXPECT_DOUBLE_EQ(mx.target_nox_ppm, 0.2);
    EXPECT_NEAR(cl.q_for_nox_dilution, 9230.77, 0.01);
    EXPECT_NEAR(mx.q_for_nox_dilution, 120000.0, 0.01);
    EXPECT_DOUBLE_EQ(mx.q_blasting, 120000.0);
}

// El usuario puede fijar su propio objetivo: 5 ppm → 600 / (5e-6 × 30)
// / 1000 × 1,2 = 4.800.
TEST_F(BlastingFlowTest, UserTargetIsHonored) {
    BlastingParams params{.explosive_kg = 60.0, .dilution_time_min = 30.0};
    params.target_nox_ppm = 5.0;
    const auto r = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_DOUBLE_EQ(r.target_nox_ppm, 5.0);
    EXPECT_NEAR(r.q_for_nox_dilution, 4800.0, 0.01);

    params.target_nox_ppm = -1.0;
    EXPECT_THROW((void)BlastingFlowCalculator::calculate(params, default_config),
                 std::invalid_argument);
}

// El barrido nunca queda bajo la mínima del Art. 248 aunque el usuario pida
// menos: ANFO 25 m/min, dinamita 20 m/min. Frente de 10 m², 0,1 m/s pedidos.
TEST_F(BlastingFlowTest, SweepVelocityNeverBelowNormativeMinimum) {
    BlastingParams params{.explosive_kg = 1.0, .dilution_time_min = 30.0,
                          .face_area_m2 = 10.0, .face_length_m = 10.0};
    params.min_velocity_mps = 0.1;
    const auto anfo = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_NEAR(anfo.q_for_min_velocity, 250.0, 1e-9);   // 10 × 25

    params.explosive_type = ExplosiveType::Dynamite;
    const auto dinamita = BlastingFlowCalculator::calculate(params, default_config);
    EXPECT_NEAR(dinamita.q_for_min_velocity, 200.0, 1e-9);  // 10 × 20

    // Chile: 15 m/min (Art. 138) coincide con el valor de ingeniería de un
    // frente (0,25 m/s): 10 × 15 = 150.
    const auto chile = BlastingFlowCalculator::calculate(params, RegulatoryConfig::chile());
    EXPECT_NEAR(chile.q_for_min_velocity, 150.0, 1e-9);
    // México: 15,24 m/min (8.4.4 b) con diésel, que se supone: 10 × 15,24.
    const auto mexico = BlastingFlowCalculator::calculate(params, RegulatoryConfig::mexico());
    EXPECT_NEAR(mexico.q_for_min_velocity, 152.4, 1e-9);
}

TEST_F(BlastingFlowTest, Anexo38_ValidaAreaYNiveles) {
    BlastingParams sin_area{.explosive_kg = 50.0};
    EXPECT_THROW((void)BlastingFlowCalculator::calculate_annex38(sin_area),
                 std::invalid_argument);
    BlastingParams sin_niveles{.explosive_kg = 50.0, .face_area_m2 = 12.0};
    sin_niveles.blasting_levels = 0;
    EXPECT_THROW((void)BlastingFlowCalculator::calculate_annex38(sin_niveles),
                 std::invalid_argument);
}
