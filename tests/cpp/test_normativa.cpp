/**
 * @file test_normativa.cpp
 * @brief Tests de los presets normativos multi-norma (factory).
 */

#include <gtest/gtest.h>
#include <stdexcept>

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
    EXPECT_DOUBLE_EQ(peru.diesel_hp_factor(),         def.diesel_hp_factor());
    EXPECT_DOUBLE_EQ(peru.max_dilution_time(),        def.max_dilution_time());
    EXPECT_DOUBLE_EQ(peru.default_gas_volume_per_kg(),def.default_gas_volume_per_kg());
    EXPECT_DOUBLE_EQ(peru.default_leakage_factor(),   def.default_leakage_factor());
}

// ============================================================================
// chile(): valores del DS 132 validados (anexo de investigación 2026-08-17)
// ============================================================================

TEST(RegulatoryPresets, Chile_ValuesFromDS132) {
    const RegulatoryConfig chile = RegulatoryConfig::chile();

    EXPECT_EQ(chile.standard(), RegulatoryStandard::DS132_Chile);
    // DS 132, Art. 138: 3 m³/min por persona en cualquier sitio del interior mina
    EXPECT_DOUBLE_EQ(chile.min_flow_per_person(), 3.0);
    // DS 132 NO escala por altitud: escalones neutralizados
    EXPECT_DOUBLE_EQ(chile.flow_above_threshold_1(), 3.0);
    EXPECT_DOUBLE_EQ(chile.flow_above_threshold_2(), 3.0);
    // DS 132, Art. 132: 2,83 m³/min por HP efectivo al freno
    EXPECT_DOUBLE_EQ(chile.diesel_hp_factor(), 2.83);
    // No regulados por DS 132 — defaults ingenieriles conservados (gate 2026-08-17)
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
