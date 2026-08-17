/**
 * @file test_limites_gases.cpp
 * @brief Tests de las tablas LMP de gases por norma.
 */

#include <gtest/gtest.h>
#include <stdexcept>

#include "ventpy/limites_gases.hpp"

using namespace ventpy;

// ============================================================================
// Perú — DS 024-2016-EM, Anexo 15 (obligatorio vía Art. 246)
// Valores validados en anexo de investigación 2026-08-17
// ============================================================================

TEST(LimitesGasesPeru, CO_Twa25ppm_SinStelNiTecho) {
    const GasLimit& co = lmp_for(RegulatoryStandard::DS024_Peru, GasType::CO);
    EXPECT_EQ(co.unit, ConcentrationUnit::PPM);
    ASSERT_TRUE(co.twa_8h.has_value());
    EXPECT_DOUBLE_EQ(*co.twa_8h, 25.0);
    EXPECT_FALSE(co.stel.has_value());
    EXPECT_FALSE(co.ceiling.has_value());
    EXPECT_NE(co.regulation_ref.find("Anexo 15"), std::string::npos);
}

TEST(LimitesGasesPeru, CO2_Twa5000_Stel30000) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::CO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 5000.0);
    EXPECT_DOUBLE_EQ(*g.stel, 30000.0);
}

TEST(LimitesGasesPeru, NO2_Twa3_Stel5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::NO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 3.0);
    EXPECT_DOUBLE_EQ(*g.stel, 5.0);
}

TEST(LimitesGasesPeru, SO2_Twa2_Stel5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::SO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 2.0);
    EXPECT_DOUBLE_EQ(*g.stel, 5.0);
}

TEST(LimitesGasesPeru, H2S_Twa10_Stel15) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::H2S);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 10.0);
    EXPECT_DOUBLE_EQ(*g.stel, 15.0);
}

TEST(LimitesGasesPeru, CH4_Techo5000ppm_NoTwa) {
    // Anexo 15 fila 32: columna Techo (C), NO TWA (= 0,5%; coherente con Art. 259)
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::CH4);
    EXPECT_FALSE(g.twa_8h.has_value());
    ASSERT_TRUE(g.ceiling.has_value());
    EXPECT_DOUBLE_EQ(*g.ceiling, 5000.0);
}

TEST(LimitesGasesPeru, NO_Twa25ppm) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::NO);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 25.0);
}

TEST(LimitesGasesPeru, O2_Min19p5_Max22p5_PorcentajeVolumen) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS024_Peru, GasType::O2);
    EXPECT_EQ(g.unit, ConcentrationUnit::PercentVolume);
    ASSERT_TRUE(g.floor_min.has_value());
    EXPECT_DOUBLE_EQ(*g.floor_min, 19.5);   // Art. 246.b
    ASSERT_TRUE(g.ceiling.has_value());
    EXPECT_DOUBLE_EQ(*g.ceiling, 22.5);     // Anexo 15, fila 36
}

TEST(LimitesGasesPeru, TablaCompleta_8Gases_TodasConRef) {
    const auto& table = gas_limits(RegulatoryStandard::DS024_Peru);
    EXPECT_EQ(table.size(), 8u);
    for (const auto& g : table) {
        EXPECT_FALSE(g.regulation_ref.empty());
        EXPECT_TRUE(g.twa_8h || g.stel || g.ceiling || g.floor_min);
    }
}
