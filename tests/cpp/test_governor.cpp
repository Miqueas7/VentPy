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

    // Personal: 15 trabajadores a 4200 msnm (Art. 247: base 6 m³/min;
    // normativo 15 × 6 × 1.772 = 159.5 < piso de velocidad 180 → Q_Per = 180)
    input.num_workers = 15;
    input.altitude_masl = 4200.0;

    // Equipos: un scoop de 150 HP al 85%/70%
    DieselFleet fleet;
    fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70);
    input.diesel_fleet = fleet;
    // calculate_full (criterio gobernante: dilución de NOx a TLV 5 ppm, Tier3):
    //   HP_eff = 150 × 0.85 × 0.70 = 89.25 ; derate(4200) = 0.84 → HP_der = 74.97
    //   NOx = 6.0 g/kWh × (74.97 × 0.7457) kW / 60 × 0.85 = 4.7519 g/min
    //   Q_NOx = dilución(4.7519, 5 ppm, 46.01) × corr_vol(4200 = 1.6876) = 852.31
    //   (método HP: 89.25 × 3 × 1.6876 × 0.85 = 384.08 — NO gobierna)
    //   Q_Eq = safety_ceil(852.31) = 853

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

    // Q_Per = 180 (piso de velocidad: 12 m² × 0.25 m/s × 60; ver PersonnelOnly)
    // Gobernante: max(180, 853, 0.0667) = 853 (diésel)
    EXPECT_EQ(result.governing_factor, "diesel (Q_Eq)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 853.0);

    // Con fugas 15%: 853 × 1.15 = 980.95 → ceil = 981
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 981.0);

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

    // calculate_full: el criterio normativo (10 × 4 (Art. 247) × 1.3569 = 54.27)
    // queda por debajo del piso de velocidad mínima de galería (face_area default
    // 12 m² × 0.25 m/s × 60 = 180), que gobierna dentro de Q_Per.
    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 180.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 0.0);

    // Gobernante: 180 (personal)
    EXPECT_EQ(result.governing_factor, "personnel (Q_Per)");
    // Total: ceil(180 × 1.15) = ceil(207) = 207
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 207.0);
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

    // calculate_full:
    // Q_Per: 100 × 6 (Art. 247, >4000) × corr(4500 = 1.7555 × 1.05) = 1105.97 → ceil = 1106
    //        (aquí SÍ gobierna el criterio normativo; el piso de velocidad es 180)
    // Q_Eq:  dilución NOx Tier3 → 1404.94 → ceil = 1405
    //        (método HP: 144 × 3 × 1.7555 × 0.85 = 644.62 — NO gobierna)
    // Q_Exp = 0 (sin explosivos)
    // GeneralMine: Q = 1106 + 1405 + 0 = 2511
    EXPECT_EQ(result.governing_factor, "summation (mine total)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 2511.0);
}

// ============================================================================
// Safety ceil: nunca redondea abajo
// ============================================================================

TEST_F(GovernorTest, SafetyCeil_NeverRoundsDown) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 1;
    input.altitude_masl = 0.0;
    // Sección mínima para que el piso de velocidad (0.1 × 0.25 × 60 = 1.5) no
    // tape la aritmética fraccionaria que este test verifica.
    input.face_area_m2 = 0.1;

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = ceil(1 × 3 × ~1.0) = 3
    // Con fugas: 3.0 × 1.15 = 3.45 → ceil = 4 (nunca 3)
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
    // Sección mínima: neutraliza el piso de velocidad (= 1.5) para que el
    // factor de fugas actúe sobre el criterio normativo, no sobre el piso.
    input.face_area_m2 = 0.1;

    auto result = governor.calculateTotalDemand(input);

    // Q_Per = ceil(10 × 4 (Art. 247, >1500) × 1.3569 = 54.27) = 55
    // Fugas 25%: 55 × 1.25 = 68.75 → ceil = 69
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 69.0);
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
