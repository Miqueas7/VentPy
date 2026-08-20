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

// ============================================================================
// Cableo de dust_params/thermal_params al Governor.
//
// Valores verificados con un cálculo independiente EXACTO ANTES de escribir
// estos tests. Reutiliza resultados ya testeados y en verde de
// test_caudal_polvo.cpp y test_caudal_termico.cpp: no se rederivan distinto.
// ============================================================================

class GovernorPolvoTermico : public ::testing::Test {
protected:
    RegulatoryConfig default_config;
    VentilationGovernor governor{default_config};
};

// DevelopmentFace, 1 trabajador, altitude 0, face_area_m2 = 0.1 (neutraliza
// el piso de velocidad de personal: 0.1 x 0.25 x 60 = 1.5 < normativo 3 =>
// Q_per = 3, igual que GovernorTest.SafetyCeil_NeverRoundsDown).
// dust_params = caso "SinSupresionYSafetyCeil" de test_caudal_polvo.cpp:
// 50 mg/s SIN supresion, target 2.9 => q_dust = 1035 (NO se fija face_area
// propio: hereda 0.1 del input, mismo patron que blasting_params).
// q_governing = max(3, 1035) = 1035 (domina polvo).
// total = ceil(1035 x 1.15) = ceil(1190.25) = 1191.
TEST_F(GovernorPolvoTermico, PolvoGobiernaConPisoNeutralizado) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 1;
    input.altitude_masl = 0.0;
    input.face_area_m2 = 0.1;

    DustParams dust;
    dust.dust_generation_rate_mg_s = 50.0;
    dust.water_suppression = false;
    dust.target_concentration_mg_m3 = 2.9;
    // dust.face_area_m2 queda en 0 => el governor lo completa con input.face_area_m2.
    input.dust_params = dust;

    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 3.0);
    EXPECT_DOUBLE_EQ(result.q_dust_m3min, 1035.0);
    EXPECT_EQ(result.governing_factor, "dust (Q_Dust)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 1035.0);
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 1191.0);
    ASSERT_TRUE(result.dust.has_value());
    EXPECT_NEAR(result.dust->resulting_velocity_mps, 1035.0 / 60.0 / 0.1, 1e-9);
}

// Regresion: las advertencias de los sub-calculadores deben
// subir al Governor con el prefijo del factor que las origino ("Q_polvo: ",
// "Q_termico: "; ver governor.hpp lineas 171-188). Caso base de polvo
// (DilucionConSupresionExacta de test_caudal_polvo.cpp) + silica 12% => el
// calculador de polvo advierte remision al Anexo 15; esa advertencia debe
// aparecer en result.warnings prefijada con "Q_polvo: ".
TEST_F(GovernorPolvoTermico, WarningsPrefijadasSubenAlGovernor) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 1;

    DustParams dust;
    dust.dust_generation_rate_mg_s = 50.0;
    dust.target_concentration_mg_m3 = 3.0;
    dust.face_area_m2 = 12.0;
    dust.water_suppression = true;
    dust.suppression_efficiency = 0.7;
    dust.silica_content_percent = 12.0;
    input.dust_params = dust;

    auto result = governor.calculateTotalDemand(input);

    bool aviso = false;
    for (const auto& w : result.warnings) {
        if (w.find("Q_polvo: ") != std::string::npos &&
            w.find("Anexo 15") != std::string::npos) {
            aviso = true;
        }
    }
    EXPECT_TRUE(aviso);
}

// DevelopmentFace, 10 trabajadores, atmospheric.altitude_masl=2500,
// atmospheric.dry_bulb_temp_c=16, face_area_m2 default (12): Q_per = 180
// (piso de velocidad; igual que GovernorTest.PersonnelOnly a altitude=2500).
// thermal_params = caso 4 de test_caudal_termico.cpp
// (BalanceDominaSobre252d): equipos 400 kW + oxidacion 50 kW, depth 900 m,
// autocompresion 0.98, target 28 C, face_area 12 => q_thermal = 9391
// (REUTILIZADO, no rederivado distinto).
// q_governing = max(180, 9391) = 9391 (domina termico).
// total = ceil(9391 x 1.15) = ceil(10799.65) = 10800.
// VRT del caso: virgen 25 + 1.0x900/100 = 34 <= target+10 (38) => sin
// advertencia de estudio geotermico. inlet=24.82 en [24,29] => regulation_ref
// del resultado termico contiene "252" (Art. 252.d aplica).
TEST_F(GovernorPolvoTermico, TermicoGobiernaE2E) {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 10;
    input.atmospheric.altitude_masl = 2500.0;
    input.atmospheric.dry_bulb_temp_c = 16.0;

    ThermalParams thermal;
    thermal.virgin_rock_temp_c = 25.0;
    thermal.geothermal_gradient_c_per_100m = 1.0;
    thermal.depth_below_surface_m = 900.0;
    thermal.auto_compression_c_per_100m = 0.98;
    thermal.heat_from_equipment_kw = 400.0;
    thermal.heat_from_oxidation_kw = 50.0;
    thermal.target_effective_temp_c = 28.0;
    thermal.face_area_m2 = 12.0;
    input.thermal_params = thermal;

    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 180.0);
    EXPECT_DOUBLE_EQ(result.q_thermal_m3min, 9391.0);
    EXPECT_EQ(result.governing_factor, "thermal (Q_Thermal)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 9391.0);
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 10800.0);
    ASSERT_TRUE(result.thermal.has_value());
    EXPECT_NE(result.thermal->regulation_ref.find("252"), std::string::npos);
    // Sin advertencia de estudio geotermico (VRT 34 <= target+10 = 38).
    for (const auto& w : result.thermal->warnings)
        EXPECT_EQ(w.find("estudio"), std::string::npos);
}

// GeneralMine: sumatoria de los factores. 10 trabajadores, atmospheric
// altitude=2500/dry_bulb=16, face_area_m2 default (12) para TODO el input
// (piso de velocidad de personal Y criterio 252.d termico).
// SIMPLIFICACION documentada: SIN
// flota diesel (q_eq = 0) -- suma de 4 factores en vez de 5, para no
// rederivar aqui el modelo Tier3 de dilucion NOx del diesel.
//   q_per     = 180   (piso velocidad; igual que TermicoGobiernaE2E)
//   q_eq      = 0     (sin flota diesel; simplificacion documentada)
//   q_exp     = 1     (blasting simple 50 kg, 0.04 m3/kg, 30 min:
//                       ceil((50*0.04)/30) = ceil(0.0667) = 1)
//   q_dust    = 300   (caso A de DilucionConSupresionExacta en
//                       test_caudal_polvo.cpp: 50 mg/s con supresion 0.7,
//                       target 3.0 => exacto, sin resto)
//   q_thermal = 9391  (mismo caso de test_caudal_termico.cpp que TermicoGobiernaE2E)
// suma = 180+0+1+300+9391 = 9872.
// total = ceil(9872 x 1.15) = ceil(11352.8) = 11353.
TEST_F(GovernorPolvoTermico, GeneralMineSumaSinFlota) {
    VentilationInput input;
    input.zone_type = ZoneType::GeneralMine;
    input.num_workers = 10;
    input.atmospheric.altitude_masl = 2500.0;
    input.atmospheric.dry_bulb_temp_c = 16.0;

    BlastingParams blast;
    blast.explosive_kg = 50.0;
    blast.gas_volume_per_kg = 0.04;
    blast.dilution_time_min = 30.0;
    input.blasting_params = blast;

    DustParams dust;
    dust.dust_generation_rate_mg_s = 50.0;
    dust.target_concentration_mg_m3 = 3.0;
    dust.face_area_m2 = 12.0;
    dust.water_suppression = true;
    dust.suppression_efficiency = 0.7;
    input.dust_params = dust;

    ThermalParams thermal;
    thermal.virgin_rock_temp_c = 25.0;
    thermal.geothermal_gradient_c_per_100m = 1.0;
    thermal.depth_below_surface_m = 900.0;
    thermal.auto_compression_c_per_100m = 0.98;
    thermal.heat_from_equipment_kw = 400.0;
    thermal.heat_from_oxidation_kw = 50.0;
    thermal.target_effective_temp_c = 28.0;
    thermal.face_area_m2 = 12.0;
    input.thermal_params = thermal;

    // Sin input.diesel_fleet (simplificacion documentada arriba).

    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 180.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 1.0);
    EXPECT_DOUBLE_EQ(result.q_dust_m3min, 300.0);
    EXPECT_DOUBLE_EQ(result.q_thermal_m3min, 9391.0);

    double suma_manual = result.q_personnel_m3min + result.q_diesel_m3min +
                          result.q_blasting_m3min + result.q_dust_m3min +
                          result.q_thermal_m3min;
    EXPECT_DOUBLE_EQ(suma_manual, 9872.0);
    EXPECT_EQ(result.governing_factor, "summation (mine total)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 9872.0);
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 11353.0);
}
