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
    // normativo 15 × 6 × 1.772 = 159.5 < piso de velocidad con ANFO,
    // 12 m² × 25 m/min (Art. 248) = 300 → Q_Per = 300)
    input.num_workers = 15;
    input.altitude_masl = 4200.0;

    // Equipos: un scoop de 150 HP al 85%/70%
    DieselFleet fleet;
    fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70);
    input.diesel_fleet = fleet;
    // calculate_full (criterio gobernante: dilución de NOx al TWA de NO2 del
    // Anexo 15, 3 ppm; Tier3; simultaneidad por defecto 1,0):
    //   HP_eff = 150 × 0.85 × 0.70 = 89.25 ; derate(4200) = 0.84 → HP_der = 74.97
    //   NOx = 6.0 g/kWh × (74.97 × 0.7457) kW / 60 = 5.5905 g/min
    //   Q_NOx = dilución(5.5905, 3 ppm, 46.01) × corr_vol(4200 = 1.6876) = 1671.2
    //   (método HP: 89.25 × 3 × 1.6876 = 451.85 — NO gobierna)
    //   Q_Eq = safety_ceil(1671.2) = 1672

    // Explosivos: 50 kg ANFO
    BlastingParams blast{
        .explosive_kg = 50.0,
        .gas_volume_per_kg = 0.04,
        .dilution_time_min = 30.0,
        .face_area_m2 = 12.0,
        .face_length_m = 200.0
    };
    input.blasting_params = blast;
    // Dilución de NOx tras la voladura (gobierna dentro de Q_Exp):
    //   50 × 10 = 500 L → 500 / (3e-6 × 30) / 1000 × 1,2 = 6.666,67
    //   × corr_vol(4200 = 1.6876) = 11250.6 → Q_Exp = safety_ceil = 11251

    auto result = governor.calculateTotalDemand(input);

    // Gobernante: max(300, 1672, 11251) = 11251 (voladura)
    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 300.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 1672.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 11251.0);
    EXPECT_EQ(result.governing_factor, "blasting (Q_Exp)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 11251.0);

    // Con fugas 15%: 11251 × 1.15 = 12938.65 → ceil = 12939
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 12939.0);

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
    // queda por debajo del piso de velocidad mínima (face_area default 12 m² ×
    // 20 m/min del Art. 248 = 240), que gobierna dentro de Q_Per.
    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 240.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 0.0);

    // Gobernante: 240 (personal)
    EXPECT_EQ(result.governing_factor, "personnel (Q_Per)");
    // Total: ceil(240 × 1.15) = ceil(276) = 276
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 276.0);
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
    // Q_Eq:  dilución NOx Tier3 al TWA de NO2 (3 ppm), simultaneidad 1,0:
    //        1404.94 × (5/3) / 0.85 = 2754.8 → ceil = 2755 (antes se usaba el
    //        STEL de 5 ppm y simultaneidad 0,85)
    //        (método HP: 144 × 3 × 1.7555 = 758.38 — NO gobierna)
    // Q_Exp = 0 (sin explosivos)
    // GeneralMine: Q = 1106 + 2755 + 0 = 3861
    EXPECT_EQ(result.governing_factor, "summation (mine total)");
    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 1106.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 2755.0);
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 3861.0);
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
// q_governing = max(240, 9391) = 9391 (domina termico; Q_Per = 12 m² ×
// 20 m/min del Art. 248 = 240).
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

    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 240.0);
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
//   q_per     = 300   (piso velocidad con ANFO: 12 m² × 25 m/min, Art. 248)
//   q_eq      = 0     (sin flota diesel; simplificacion documentada)
//   q_exp     = 300   (DS 024, Anexo 38 num. 2 lit. e, sin diesel:
//                       Q_Ex = A x V x N = 12 x 25 (Art. 248, ANFO) x 1;
//                       no entra en la suma, se compara con el total)
//   q_dust    = 300   (caso A de DilucionConSupresionExacta en
//                       test_caudal_polvo.cpp: 50 mg/s con supresion 0.7,
//                       target 3.0 => exacto, sin resto)
//   q_thermal = 9391  (mismo caso de test_caudal_termico.cpp que TermicoGobiernaE2E;
//                       sin hot_levels, Q_Te = 0 y rige max(9391, 0))
// suma = 300+0+300+9391 = 9991.
// total = max(ceil(9991 x 1.15), Q_Ex) = max(ceil(11489.65), 300) = 11490.
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

    EXPECT_DOUBLE_EQ(result.q_personnel_m3min, 300.0);
    EXPECT_DOUBLE_EQ(result.q_diesel_m3min, 0.0);
    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 300.0);
    EXPECT_DOUBLE_EQ(result.q_dust_m3min, 300.0);
    EXPECT_DOUBLE_EQ(result.q_thermal_m3min, 9391.0);

    EXPECT_EQ(result.governing_factor, "summation (mine total)");
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 9991.0);
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 11490.0);
}

// ============================================================================
// Total de mina con el DS 024: voladura segun el Anexo 38
// ============================================================================

// Con equipos diesel (Anexo 38 num. 1) la voladura no entra en el total:
// mismo caso que GeneralMine_Summation mas un disparo, mismo resultado.
TEST_F(GovernorTest, GeneralMine_Anexo38_ConDieselLaVoladuraNoSuma) {
    VentilationInput input;
    input.zone_type = ZoneType::GeneralMine;
    input.num_workers = 100;
    input.altitude_masl = 4500.0;

    DieselFleet fleet;
    fleet.add_equipment("Scoop1", 200.0, 0.9, 0.8);
    input.diesel_fleet = fleet;

    BlastingParams blast;
    blast.explosive_kg = 50.0;
    blast.dilution_time_min = 30.0;
    input.blasting_params = blast;

    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 0.0);
    EXPECT_FALSE(result.blasting.has_value());
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, 3861.0);
    bool aviso = false;
    for (const auto& w : result.warnings)
        aviso = aviso || w.find("Anexo 38") != std::string::npos;
    EXPECT_TRUE(aviso);
}

// Sin diesel (Anexo 38 num. 2) rige el mayor entre el total con fugas y
// Q_Ex = A x V x N, que no lleva fugas: 12 m2 x 25 m/min (ANFO) x 3 = 900.
TEST_F(GovernorTest, GeneralMine_Anexo38_SinDieselGobiernaQEx) {
    VentilationInput input;
    input.zone_type = ZoneType::GeneralMine;

    BlastingParams blast;
    blast.explosive_kg = 50.0;
    blast.dilution_time_min = 30.0;
    blast.blasting_levels = 3;
    input.blasting_params = blast;

    auto result = governor.calculateTotalDemand(input);

    EXPECT_DOUBLE_EQ(result.q_blasting_m3min, 900.0);
    EXPECT_DOUBLE_EQ(result.q_total_m3min, 900.0);
    EXPECT_EQ(result.governing_factor, "blasting (Q_Ex, Anexo 38)");
    ASSERT_TRUE(result.blasting.has_value());
    EXPECT_NE(result.blasting->regulation_ref.find("Anexo 38"), std::string::npos);
}

// Fuera del DS 024 no hay Anexo 38: el total sigue sumando la dilucion.
TEST(GovernorAnexo38, OtrasNormasSumanLaDilucion) {
    VentilationGovernor governor{RegulatoryConfig::colombia()};
    VentilationInput input;
    input.zone_type = ZoneType::GeneralMine;
    BlastingParams blast;
    blast.explosive_kg = 50.0;
    blast.dilution_time_min = 30.0;
    input.blasting_params = blast;

    auto result = governor.calculateTotalDemand(input);

    EXPECT_GT(result.q_blasting_m3min, 1000.0);
    EXPECT_DOUBLE_EQ(result.q_governing_m3min, result.q_blasting_m3min);
}

// ============================================================================
// Velocidad maxima en el frente: solo advierte, no cambia el caudal
// ============================================================================

namespace {
VentilationInput frente_con_voladura() {
    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.atmospheric.altitude_masl = 4200.0;
    BlastingParams blast;
    blast.explosive_kg = 50.0;
    blast.dilution_time_min = 30.0;
    input.blasting_params = blast;
    return input;
}

bool hay_aviso(const VentilationDemandResult& r, const std::string& texto) {
    for (const auto& w : r.warnings)
        if (w.find("maximo") != std::string::npos &&
            w.find(texto) != std::string::npos) return true;
    return false;
}
}  // namespace

TEST(GovernorVelocidadMaxima, PeruAdvierteSobre250PorMinuto) {
    VentilationGovernor governor{RegulatoryConfig::peru()};
    auto r = governor.calculateTotalDemand(frente_con_voladura());
    ASSERT_GT(r.velocity_at_face_mps * 60.0, 250.0);
    EXPECT_TRUE(hay_aviso(r, "Art. 248"));
}

TEST(GovernorVelocidadMaxima, ColombiaAdvierteSobre360PorMinuto) {
    VentilationGovernor governor{RegulatoryConfig::colombia()};
    auto r = governor.calculateTotalDemand(frente_con_voladura());
    ASSERT_GT(r.velocity_at_face_mps * 60.0, 360.0);
    EXPECT_TRUE(hay_aviso(r, "Art. 56"));
}

TEST(GovernorVelocidadMaxima, MexicoNoFijaMaxima) {
    VentilationGovernor governor{RegulatoryConfig::mexico()};
    auto r = governor.calculateTotalDemand(frente_con_voladura());
    EXPECT_FALSE(hay_aviso(r, ""));
}

TEST(GovernorVelocidadMaxima, BajoElMaximoNoAdvierte) {
    VentilationGovernor governor{RegulatoryConfig::peru()};
    VentilationInput input;
    input.num_workers = 10;
    auto r = governor.calculateTotalDemand(input);
    ASSERT_LT(r.velocity_at_face_mps * 60.0, 250.0);
    EXPECT_FALSE(hay_aviso(r, ""));
}

// ============================================================================
// Simultaneidad: por defecto 1,0; menos de 1,0 se advierte
// ============================================================================

namespace {
VentilationInput mina_100hp(double simultaneidad) {
    VentilationInput vi;
    vi.zone_type = ZoneType::GeneralMine;
    vi.num_workers = 1;
    vi.altitude_masl = 0.0;
    vi.face_area_m2 = 1.0;
    vi.simultaneity_factor = simultaneidad;
    DieselFleet f;
    f.add_equipment("E1", 100.0, 1.0, 1.0, EngineEmissionTier::Tier4_Final);
    vi.diesel_fleet = f;
    return vi;
}
}  // namespace

// DS 024-2016-EM, Anexo 38 num. 1 lit. d: Q_Eq = 3 × HP × Dm × Fu =
// 3 × 100 × 1 × 1 = 300 m³/min. Antes el 0,85 por defecto daba 255.
TEST(GovernorSimultaneidad, PorDefectoRespetaLaNorma) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi = mina_100hp(1.0);
    EXPECT_DOUBLE_EQ(VentilationInput{}.simultaneity_factor, 1.0);
    const auto r = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(r.q_diesel_m3min, 300.0);
    for (const auto& w : r.warnings)
        EXPECT_EQ(w.find("simultaneidad"), std::string::npos) << w;
}

TEST(GovernorSimultaneidad, MenorQueUnoAdvierteQueQuedaBajoLaNorma) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    const auto r = g.calculateTotalDemand(mina_100hp(0.85));
    EXPECT_DOUBLE_EQ(r.q_diesel_m3min, 255.0);  // 300 × 0,85
    bool aviso = false;
    for (const auto& w : r.warnings)
        aviso = aviso || (w.find("simultaneidad 0.85") != std::string::npos &&
                          w.find("Anexo 38") != std::string::npos);
    EXPECT_TRUE(aviso);
}

// ============================================================================
// DS 024-2016-EM, Anexo 38: Q_Ma (madera) y Q_Te (temperatura) en el total
// ============================================================================

namespace {
// 10 trabajadores a 0 msnm, área promedio 1 m²: Q_Tr = 10 × 3 = 30
// (el piso de velocidad, 1 × 20 = 20, no gobierna).
VentilationInput mina_peru_10() {
    VentilationInput vi;
    vi.zone_type = ZoneType::GeneralMine;
    vi.num_workers = 10;
    vi.face_area_m2 = 1.0;
    vi.hot_levels = 0;
    return vi;
}

bool tiene(const VentilationDemandResult& r, const std::string& t) {
    for (const auto& w : r.warnings)
        if (w.find(t) != std::string::npos) return true;
    return false;
}
}  // namespace

// Q_Ma = T × u con T = 100 t/guardia: < 20 % → 0; 20-40 % → 0,60 × 100 = 60;
// 41-70 % → 100; > 70 % → 125 (Anexo 38 lit. b; Art. 252 lit. d).
TEST(GovernorAnexo38Madera, LosCuatroTramos) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    const std::pair<double, double> casos[] = {
        {10.0, 0.0}, {20.0, 60.0}, {40.0, 60.0}, {41.0, 100.0},
        {70.0, 100.0}, {71.0, 125.0}};
    for (const auto& [pct, q_ma] : casos) {
        VentilationInput vi = mina_peru_10();
        vi.production_t_per_shift = 100.0;
        vi.timber_consumption_pct = pct;
        const auto r = g.calculateTotalDemand(vi);
        EXPECT_DOUBLE_EQ(r.q_timber_m3min, q_ma) << pct;
        EXPECT_DOUBLE_EQ(r.q_governing_m3min, 30.0 + q_ma) << pct;
        EXPECT_FALSE(tiene(r, "no incluye Q_Ma")) << pct;
    }
}

// Q_T1 = 30 + 60 = 90; Q_Fu = 15 % de Q_T1 = 13,5 (Anexo 38 num. 1 lit. e);
// Q_To = 103,5 → ceil 104. El factor por defecto del preset peruano da
// exactamente el 15 % del total de mina.
TEST(GovernorAnexo38Madera, FugasSonEl15PorCientoDeQT1) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi = mina_peru_10();
    vi.production_t_per_shift = 100.0;
    vi.timber_consumption_pct = 30.0;
    const auto r = g.calculateTotalDemand(vi);
    ASSERT_TRUE(r.leakage.has_value());
    EXPECT_DOUBLE_EQ(r.leakage->total_leakage_factor, 0.15);
    EXPECT_DOUBLE_EQ(r.q_leakage_m3min, 0.15 * 90.0);
    EXPECT_DOUBLE_EQ(r.q_total_m3min, 104.0);
}

TEST(GovernorAnexo38Madera, SinDatosAdvierteYValeCero) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi = mina_peru_10();
    const auto r = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(r.q_timber_m3min, 0.0);
    EXPECT_TRUE(tiene(r, "Anexo 38 lit. b: el total no incluye Q_Ma"));
}

TEST(GovernorAnexo38Madera, Validaciones) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput solo_t = mina_peru_10();
    solo_t.production_t_per_shift = 100.0;
    EXPECT_THROW((void)g.calculateTotalDemand(solo_t), std::invalid_argument);
    VentilationInput pct_malo = mina_peru_10();
    pct_malo.production_t_per_shift = 100.0;
    pct_malo.timber_consumption_pct = 120.0;
    EXPECT_THROW((void)g.calculateTotalDemand(pct_malo), std::invalid_argument);
    VentilationInput t_negativa = mina_peru_10();
    t_negativa.production_t_per_shift = -1.0;
    t_negativa.timber_consumption_pct = 30.0;
    EXPECT_THROW((void)g.calculateTotalDemand(t_negativa), std::invalid_argument);
}

// Q_Te = Vm × A × N = 30 × 12 × 2 = 720 (Anexo 38 lit. c). 10 trabajadores a
// 0 msnm en 12 m²: Q_Tr = max(30, 12 × 20) = 240. Q_T1 = 240 + 720 = 960.
TEST(GovernorAnexo38Temperatura, QTeEntraEnElTotal) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi = mina_peru_10();
    vi.face_area_m2 = 12.0;
    vi.hot_levels = 2;
    const auto r = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(r.q_temperature_m3min, 720.0);
    EXPECT_DOUBLE_EQ(r.q_governing_m3min, 960.0);
    EXPECT_DOUBLE_EQ(r.q_total_m3min, 1104.0);  // ceil(960 × 1,15)
    EXPECT_FALSE(tiene(r, "no incluye Q_Te"));
}

TEST(GovernorAnexo38Temperatura, SinNivelesAdvierte) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi = mina_peru_10();
    vi.hot_levels.reset();
    const auto r = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(r.q_temperature_m3min, 0.0);
    EXPECT_TRUE(tiene(r, "Anexo 38 lit. c: el total no incluye Q_Te"));
    VentilationInput negativo = mina_peru_10();
    negativo.hot_levels = -1;
    EXPECT_THROW((void)g.calculateTotalDemand(negativo), std::invalid_argument);
}

// Q_Te y el balance térmico responden al mismo concepto: rige el mayor, no
// la suma. Balance térmico del caso TermicoGobiernaE2E: 9391 m³/min.
//   N = 2  → Q_Te = 720   < 9391 → término térmico 9391
//   N = 50 → Q_Te = 18000 > 9391 → término térmico 18000
TEST(GovernorAnexo38Temperatura, RigeElMayorEntreQTeYElBalance) {
    VentilationGovernor g{RegulatoryConfig::peru()};
    VentilationInput vi;
    vi.zone_type = ZoneType::GeneralMine;
    vi.num_workers = 10;
    vi.atmospheric.altitude_masl = 2500.0;
    vi.atmospheric.dry_bulb_temp_c = 16.0;
    ThermalParams t;
    t.virgin_rock_temp_c = 25.0;
    t.geothermal_gradient_c_per_100m = 1.0;
    t.depth_below_surface_m = 900.0;
    t.auto_compression_c_per_100m = 0.98;
    t.heat_from_equipment_kw = 400.0;
    t.heat_from_oxidation_kw = 50.0;
    t.target_effective_temp_c = 28.0;
    t.face_area_m2 = 12.0;
    vi.thermal_params = t;

    vi.hot_levels = 2;
    const auto bajo = g.calculateTotalDemand(vi);
    ASSERT_DOUBLE_EQ(bajo.q_thermal_m3min, 9391.0);
    EXPECT_DOUBLE_EQ(bajo.q_temperature_m3min, 720.0);
    EXPECT_DOUBLE_EQ(bajo.q_governing_m3min, bajo.q_personnel_m3min + 9391.0);

    vi.hot_levels = 50;
    const auto alto = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(alto.q_temperature_m3min, 18000.0);
    EXPECT_DOUBLE_EQ(alto.q_governing_m3min, alto.q_personnel_m3min + 18000.0);
}

// Fuera del total de mina peruano los datos se ignoran, con aviso.
TEST(GovernorAnexo38Temperatura, OtrasNormasLosIgnoran) {
    VentilationGovernor g{RegulatoryConfig::chile()};
    VentilationInput vi = mina_peru_10();
    vi.production_t_per_shift = 100.0;
    vi.timber_consumption_pct = 80.0;
    vi.hot_levels = 3;
    const auto r = g.calculateTotalDemand(vi);
    EXPECT_DOUBLE_EQ(r.q_timber_m3min, 0.0);
    EXPECT_DOUBLE_EQ(r.q_temperature_m3min, 0.0);
    EXPECT_TRUE(tiene(r, "se ignoran"));
}

// ============================================================================
// Velocidad mínima según la norma
// ============================================================================

// México: 15,24 m/min donde opere diésel (NOM-023-STPS-2012, 8.4.4 b).
// 1 trabajador en 12 m²: Q_Per = ceil(12 × 15,24) = ceil(182,88) = 183; sin
// diésel rige el valor de ingeniería de un frente: 12 × 15 = 180.
TEST(GovernorVelocidadMinima, MexicoConDieselUsa15_24) {
    VentilationGovernor g{RegulatoryConfig::mexico()};
    VentilationInput vi;
    vi.num_workers = 1;
    DieselFleet f;
    f.add_equipment("Jumbo", 10.0, 1.0, 1.0);
    vi.diesel_fleet = f;
    EXPECT_DOUBLE_EQ(g.calculateTotalDemand(vi).q_personnel_m3min, 183.0);
    vi.diesel_fleet.reset();
    EXPECT_DOUBLE_EQ(g.calculateTotalDemand(vi).q_personnel_m3min, 180.0);
}

// Bajo la mínima, la advertencia cita la norma si es la norma la que rige, o
// declara criterio de ingeniería. Tajeo de 12 m² con un equipo Tier 4 Final
// de 10 HP: Chile, método HP 2,83 × 10 = 28,3 m³/min → 2,4 m/min < 15
// (Art. 138 inc. 3); Colombia, dilución de NOx a 0,2 ppm (Art. 39) = 132,1
// → 133 m³/min → 11,1 m/min < 12 (valor de ingeniería de un tajeo).
TEST(GovernorVelocidadMinima, AdvertenciaCitaLaNormaOElCriterio) {
    VentilationInput vi;
    vi.zone_type = ZoneType::Stope;
    DieselFleet f;
    f.add_equipment("Jumbo", 10.0, 1.0, 1.0, EngineEmissionTier::Tier4_Final);
    vi.diesel_fleet = f;

    const auto cl = VentilationGovernor{RegulatoryConfig::chile()}.calculateTotalDemand(vi);
    EXPECT_FALSE(cl.velocity_ok);
    EXPECT_TRUE(tiene(cl, "< minimo 15 m/min (DS 132, Art. 138 inc. 3"));

    const auto co = VentilationGovernor{RegulatoryConfig::colombia()}.calculateTotalDemand(vi);
    EXPECT_FALSE(co.velocity_ok);
    EXPECT_TRUE(tiene(co, "< minimo 12 m/min (criterio de ingenieria"));
}

// Decreto 1886, Art. 56 par. 2: en tajos de carbón el máximo es 4,5 m/s.
namespace {
VentilationInput tajo_rapido(bool carbon) {
    VentilationInput input;
    input.zone_type = ZoneType::Stope;
    input.num_workers = 1200;   // 1200 x 3 m3/min = 3600 → 300 m/min en 12 m2
    input.coal_mine = carbon;
    return input;
}
}  // namespace

TEST(GovernorCarbon, ColombiaTajoDeCarbonTopa270) {
    VentilationGovernor governor{RegulatoryConfig::colombia()};
    auto r = governor.calculateTotalDemand(tajo_rapido(true));
    const double v = r.velocity_at_face_mps * 60.0;
    ASSERT_GT(v, 270.0);
    ASSERT_LT(v, 360.0);
    EXPECT_TRUE(hay_aviso(r, "maximo 270"));
    bool nota = false;
    for (const auto& w : r.warnings) nota = nota || w.find("carbon") != std::string::npos;
    EXPECT_TRUE(nota);
}

TEST(GovernorCarbon, SinCarbonRigen360) {
    VentilationGovernor governor{RegulatoryConfig::colombia()};
    auto r = governor.calculateTotalDemand(tajo_rapido(false));
    EXPECT_FALSE(hay_aviso(r, ""));
}
