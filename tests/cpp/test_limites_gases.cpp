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

// ============================================================================
// Chile — DS 594 Art. 66 (vía remisión del DS 132) y DS 132
// Decisiones del gate 2026-08-17: CO lleva el LPP 44 ppm del DS 594 (el umbral
// de paralización diésel de 40 ppm del DS 132 Art. 135 queda en la cita);
// NO2 2,6 ppm del DS 594 gobierna (NOx 20 ppm queda en la cita).
// Los ppm chilenos NO se corrigen por altitud (Fa aplica solo a mg/m³, Art. 63).
// ============================================================================

TEST(LimitesGasesChile, CO_Lpp44ppm_RefMencionaParalizacion40) {
    const GasLimit& co = lmp_for(RegulatoryStandard::DS132_Chile, GasType::CO);
    EXPECT_DOUBLE_EQ(*co.twa_8h, 44.0);
    EXPECT_NE(co.regulation_ref.find("DS 594"), std::string::npos);
    EXPECT_NE(co.regulation_ref.find("40 ppm"), std::string::npos);
}

TEST(LimitesGasesChile, CO2_Lpp4375_Lpt30000) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::CO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 4375.0);
    EXPECT_DOUBLE_EQ(*g.stel, 30000.0);
}

TEST(LimitesGasesChile, NO2_Lpp2p6_Lpt5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::NO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 2.6);
    EXPECT_DOUBLE_EQ(*g.stel, 5.0);
}

TEST(LimitesGasesChile, SO2_Lpp1p7_Lpt5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::SO2);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 1.7);
    EXPECT_DOUBLE_EQ(*g.stel, 5.0);
}

TEST(LimitesGasesChile, H2S_Lpp8p8_Lpt15) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::H2S);
    EXPECT_DOUBLE_EQ(*g.twa_8h, 8.8);
    EXPECT_DOUBLE_EQ(*g.stel, 15.0);
}

TEST(LimitesGasesChile, CH4_Techo0p75PorcentajeConservador) {
    // DS 132 Art. 274: 0,75% en retorno general (conservador); 2% en frentes
    // de arranque documentado en la cita.
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::CH4);
    EXPECT_EQ(g.unit, ConcentrationUnit::PercentVolume);
    EXPECT_DOUBLE_EQ(*g.ceiling, 0.75);
    EXPECT_NE(g.regulation_ref.find("2%"), std::string::npos);
}

TEST(LimitesGasesChile, O2_Min19p5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::DS132_Chile, GasType::O2);
    EXPECT_EQ(g.unit, ConcentrationUnit::PercentVolume);
    EXPECT_DOUBLE_EQ(*g.floor_min, 19.5);   // DS 132, Art. 144
    EXPECT_FALSE(g.ceiling.has_value());    // Chile no fija máximo de O2
}

TEST(LimitesGasesChile, NO_NoReguladoLanza) {
    // El NO no se extrajo/validó para Chile: debe lanzar, nunca inventar valor.
    EXPECT_THROW(lmp_for(RegulatoryStandard::DS132_Chile, GasType::NO),
                 std::invalid_argument);
}

TEST(LimitesGasesChile, TablaCompleta_7Gases_TodasConRef) {
    const auto& table = gas_limits(RegulatoryStandard::DS132_Chile);
    EXPECT_EQ(table.size(), 7u);
    for (const auto& g : table) {
        EXPECT_FALSE(g.regulation_ref.empty());
        EXPECT_TRUE(g.twa_8h || g.stel || g.ceiling || g.floor_min);
    }
}
