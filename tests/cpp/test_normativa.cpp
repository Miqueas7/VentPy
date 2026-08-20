/**
 * @file test_normativa.cpp
 * @brief Tests de los presets normativos multi-norma (factory).
 */

#include <gtest/gtest.h>
#include <cmath>
#include <stdexcept>

#include "ventpy/governor.hpp"
#include "ventpy/normativa.hpp"

using namespace ventpy;

// ============================================================================
// peru(): no-regresión — idéntico a los defaults del constructor
// ============================================================================

TEST(RegulatoryPresets, Peru_MatchesConstructorDefaults) {
    const RegulatoryConfig def{};
    const RegulatoryConfig peru = RegulatoryConfig::peru();

    EXPECT_EQ(peru.standard(), RegulatoryStandard::DS024_Peru);
    EXPECT_DOUBLE_EQ(peru.min_flow_per_person(),      def.min_flow_per_person());
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_1(),     def.altitude_threshold_1());
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_1(),   def.flow_above_threshold_1());
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_2(),     def.altitude_threshold_2());
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_2(),   def.flow_above_threshold_2());
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_3(),     def.altitude_threshold_3());
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_3(),   def.flow_above_threshold_3());
    EXPECT_DOUBLE_EQ(peru.diesel_hp_factor(),         def.diesel_hp_factor());
    EXPECT_DOUBLE_EQ(peru.max_dilution_time(),        def.max_dilution_time());
    EXPECT_DOUBLE_EQ(peru.default_gas_volume_per_kg(),def.default_gas_volume_per_kg());
    EXPECT_DOUBLE_EQ(peru.default_leakage_factor(),   def.default_leakage_factor());
}

// ============================================================================
// peru(): escala del Art. 247 (texto original, no modificado por DS 023-2017)
// "hasta 1,500 msnm: 3 m³/min; de 1,500 a 3,000: 4; de 3,000 a 4,000: 5;
//  sobre los 4,000: 6" (corrección normativa)
// ============================================================================

TEST(RegulatoryPresets, Peru_EscalaPorPersonaArt247) {
    const RegulatoryConfig peru = RegulatoryConfig::peru();

    EXPECT_DOUBLE_EQ(peru.min_flow_per_person(),    3.0);   // hasta 1,500 msnm
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_1(),   1500.0);
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_1(), 4.0);   // 1,500–3,000 (+40%)
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_2(),   3000.0);
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_2(), 5.0);   // 3,000–4,000 (+70%)
    EXPECT_DOUBLE_EQ(peru.altitude_threshold_3(),   4000.0);
    EXPECT_DOUBLE_EQ(peru.flow_above_threshold_3(), 6.0);   // sobre 4,000 (+100%)
}

TEST(RegulatoryPresets, ThresholdsNoCrecientes_Lanza) {
    // Umbrales deben ser estrictamente crecientes (t1 < t2 < t3)
    EXPECT_THROW(
        RegulatoryConfig(RegulatoryStandard::DS024_Peru,
                         3.0, 3000.0, 4.0, 3000.0, 5.0, 4000.0, 6.0,
                         3.0, 30.0, 0.04, 0.15),
        std::invalid_argument);
}

// ============================================================================
// chile(): valores del DS 132 validados contra el texto normativo
// ============================================================================

TEST(RegulatoryPresets, Chile_ValuesFromDS132) {
    const RegulatoryConfig chile = RegulatoryConfig::chile();

    EXPECT_EQ(chile.standard(), RegulatoryStandard::DS132_Chile);
    // DS 132, Art. 138: 3 m³/min por persona en cualquier sitio del interior mina
    EXPECT_DOUBLE_EQ(chile.min_flow_per_person(), 3.0);
    // DS 132 NO escala por altitud: escalones neutralizados (los tres)
    EXPECT_DOUBLE_EQ(chile.flow_above_threshold_1(), 3.0);
    EXPECT_DOUBLE_EQ(chile.flow_above_threshold_2(), 3.0);
    EXPECT_DOUBLE_EQ(chile.flow_above_threshold_3(), 3.0);
    // DS 132, Art. 132: 2,83 m³/min por HP efectivo al freno
    EXPECT_DOUBLE_EQ(chile.diesel_hp_factor(), 2.83);
    // No regulados por DS 132 — defaults ingenieriles conservados (criterio adoptado)
    EXPECT_DOUBLE_EQ(chile.max_dilution_time(), 30.0);
    EXPECT_DOUBLE_EQ(chile.default_gas_volume_per_kg(), 0.04);
    EXPECT_DOUBLE_EQ(chile.default_leakage_factor(), 0.15);
}

// ============================================================================
// for_standard(): dispatch
// ============================================================================

TEST(RegulatoryPresets, ForStandard_DispatchesPeru) {
    const RegulatoryConfig cfg = RegulatoryConfig::for_standard(RegulatoryStandard::DS024_Peru);
    EXPECT_EQ(cfg.standard(), RegulatoryStandard::DS024_Peru);
    EXPECT_DOUBLE_EQ(cfg.diesel_hp_factor(), 3.0);
}

TEST(RegulatoryPresets, ForStandard_DispatchesChile) {
    const RegulatoryConfig cfg = RegulatoryConfig::for_standard(RegulatoryStandard::DS132_Chile);
    EXPECT_EQ(cfg.standard(), RegulatoryStandard::DS132_Chile);
    EXPECT_DOUBLE_EQ(cfg.diesel_hp_factor(), 2.83);
}

// ============================================================================
// standard_name(): reporte auditable
// ============================================================================

TEST(RegulatoryPresets, StandardName_Chile) {
    const std::string name = RegulatoryConfig::chile().standard_name();
    EXPECT_NE(name.find("DS 132"), std::string::npos);
    EXPECT_NE(name.find("Chile"), std::string::npos);
}

TEST(RegulatoryPresets, StandardName_PeruUnchanged) {
    EXPECT_EQ(RegulatoryConfig::peru().standard_name(),
              "DS 024-2016-EM / DS 023-2017-EM (Peru)");
}

// ============================================================================
// End-to-end: preset chileno a través del Governor (calculate_full)
// Derivación:
//   Desglose normativo chileno:
//     flow_per_person_base = 3,0 (Art. 138 — SIN escalón a 4.200 msnm)
//     hp_factor_base = 2,83 (Art. 132)
//     método HP: 150 × 0,85 × 0,70 × 2,83 = 252,5775 m³/min
//   Criterios robustos de calculate_full():
//     Q_Per = max(personal corregido, piso velocidad 12 m² × 0,25 m/s × 60) = 180
//     Q_Eq  = máx(método HP, dilución NOx Tier3) ≈ 853  → gobierna diésel
//   Total = safety_ceil(q_gobernante × 1,15) ≈ 981 m³/min
// ============================================================================

TEST(RegulatoryPresets, Chile_EndToEnd_Governor) {
    VentilationGovernor governor{RegulatoryConfig::chile()};

    VentilationInput input;
    input.zone_type = ZoneType::DevelopmentFace;
    input.num_workers = 10;
    input.altitude_masl = 4200.0;  // en Chile la altitud NO cambia el caudal/persona

    DieselFleet fleet;
    fleet.add_equipment("Scoop ST7", 150.0, 0.85, 0.70);
    input.diesel_fleet = fleet;

    auto result = governor.calculateTotalDemand(input);

    ASSERT_TRUE(result.personnel.has_value());
    ASSERT_TRUE(result.diesel.has_value());

    // El preset chileno es visible en el desglose auditable:
    EXPECT_DOUBLE_EQ(result.personnel->flow_per_person_base, 3.0);  // sin escalón
    EXPECT_DOUBLE_EQ(result.diesel->hp_factor_base, 2.83);          // Art. 132

    // Criterios robustos de calculate_full() (no normativos chilenos):
    EXPECT_NEAR(result.q_personnel_m3min, 180.0, 0.01);   // piso de velocidad
    EXPECT_NEAR(result.diesel->q_for_nox_dilution, 853.0, 2.0);
    EXPECT_EQ(result.governing_factor, "diesel (Q_Eq)");

    // Cadena de fugas + redondeo de seguridad exacta:
    EXPECT_DOUBLE_EQ(result.q_total_m3min,
                     std::ceil(result.q_governing_m3min * 1.15));
    EXPECT_NEAR(result.q_total_m3min, 981.0, 3.0);
    EXPECT_EQ(result.standard, RegulatoryStandard::DS132_Chile);
}
