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
// Valores validados contra el texto normativo
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
// Criterio adoptado: CO lleva el LPP 44 ppm del DS 594 (el umbral
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

// ============================================================================
// Colombia — Decreto 1886 de 2015, Arts. 38 (O2), 39 (tabla de VLP) y 53 (CH4)
// ============================================================================

TEST(LimitesGasesColombia, TablaDelArt39) {
    const auto col = RegulatoryStandard::D1886_Colombia;

    const GasLimit& co = lmp_for(col, GasType::CO);
    EXPECT_DOUBLE_EQ(co.twa_8h.value(), 25.0);
    EXPECT_FALSE(co.stel.has_value());

    const GasLimit& co2 = lmp_for(col, GasType::CO2);
    EXPECT_DOUBLE_EQ(co2.twa_8h.value(), 5000.0);
    EXPECT_DOUBLE_EQ(co2.stel.value(), 30000.0);

    const GasLimit& no2 = lmp_for(col, GasType::NO2);
    EXPECT_DOUBLE_EQ(no2.twa_8h.value(), 0.2);
    EXPECT_FALSE(no2.stel.has_value());

    const GasLimit& no = lmp_for(col, GasType::NO);
    EXPECT_DOUBLE_EQ(no.twa_8h.value(), 25.0);

    const GasLimit& h2s = lmp_for(col, GasType::H2S);
    EXPECT_DOUBLE_EQ(h2s.twa_8h.value(), 1.0);
    EXPECT_DOUBLE_EQ(h2s.stel.value(), 5.0);

    for (GasType g : {GasType::CO, GasType::CO2, GasType::NO2, GasType::NO,
                      GasType::H2S, GasType::SO2}) {
        EXPECT_NE(lmp_for(col, g).regulation_ref.find("Art. 39"),
                  std::string::npos);
        EXPECT_EQ(lmp_for(col, g).unit, ConcentrationUnit::PPM);
    }
}

// El decreto solo fija STEL para el SO2: el TWA va vacío, nunca en cero.
TEST(LimitesGasesColombia, SO2_SoloStel) {
    const GasLimit& g = lmp_for(RegulatoryStandard::D1886_Colombia, GasType::SO2);
    EXPECT_FALSE(g.twa_8h.has_value());
    EXPECT_DOUBLE_EQ(g.stel.value(), 0.25);
}

TEST(LimitesGasesColombia, CH4_Art53_1PorCientoMasEstricto) {
    const GasLimit& g = lmp_for(RegulatoryStandard::D1886_Colombia, GasType::CH4);
    EXPECT_EQ(g.unit, ConcentrationUnit::PercentVolume);
    EXPECT_DOUBLE_EQ(g.ceiling.value(), 1.0);
    EXPECT_NE(g.regulation_ref.find("Art. 53"), std::string::npos);
    EXPECT_NE(g.regulation_ref.find("1.5%"), std::string::npos);
}

TEST(LimitesGasesColombia, O2_Art38_Min19p5Max23p5) {
    const GasLimit& g = lmp_for(RegulatoryStandard::D1886_Colombia, GasType::O2);
    EXPECT_DOUBLE_EQ(g.floor_min.value(), 19.5);
    EXPECT_DOUBLE_EQ(g.ceiling.value(), 23.5);
    EXPECT_NE(g.regulation_ref.find("Art. 38"), std::string::npos);
}

TEST(LimitesGasesColombia, TablaCompleta_8Gases_TodasConRef) {
    const auto& table = gas_limits(RegulatoryStandard::D1886_Colombia);
    EXPECT_EQ(table.size(), 8u);
    for (const GasLimit& g : table) {
        EXPECT_NE(g.regulation_ref.find("Decreto 1886 de 2015"), std::string::npos);
        EXPECT_TRUE(g.twa_8h || g.stel || g.ceiling || g.floor_min);
    }
}

// ============================================================================
// México — NOM-010-STPS-2014, Tabla I.1 (vía remisión de la NOM-023-STPS-2012)
// ============================================================================

TEST(LimitesGasesMexico, TablaI1PorFila) {
    const auto mex = RegulatoryStandard::NOM023_Mexico;
    struct Fila { GasType gas; double ppt; const char* fila; };
    const Fila filas[] = {
        {GasType::CO,  25.0,   "fila 552"},
        {GasType::CO2, 5000.0, "fila 318"},
        {GasType::NO2, 0.2,    "fila 320"},
        {GasType::NO,  25.0,   "fila 599"},
        {GasType::H2S, 1.0,    "fila 683"},
        {GasType::CH4, 1000.0, "fila 500"},
    };
    for (const Fila& f : filas) {
        const GasLimit& g = lmp_for(mex, f.gas);
        EXPECT_DOUBLE_EQ(g.twa_8h.value(), f.ppt) << f.fila;
        EXPECT_EQ(g.unit, ConcentrationUnit::PPM) << f.fila;
        EXPECT_NE(g.regulation_ref.find("NOM-010-STPS-2014"), std::string::npos);
        EXPECT_NE(g.regulation_ref.find(f.fila), std::string::npos);
    }
    EXPECT_DOUBLE_EQ(lmp_for(mex, GasType::CO2).stel.value(), 30000.0);
    EXPECT_DOUBLE_EQ(lmp_for(mex, GasType::H2S).stel.value(), 5.0);
    EXPECT_FALSE(lmp_for(mex, GasType::CO).stel.has_value());
    EXPECT_FALSE(lmp_for(mex, GasType::NO2).stel.has_value());
}

TEST(LimitesGasesMexico, SO2_SoloCortoTiempo) {
    const GasLimit& g = lmp_for(RegulatoryStandard::NOM023_Mexico, GasType::SO2);
    EXPECT_FALSE(g.twa_8h.has_value());
    EXPECT_DOUBLE_EQ(g.stel.value(), 0.25);
    EXPECT_NE(g.regulation_ref.find("fila 317"), std::string::npos);
}

// La NOM-023 no fija el mínimo de oxígeno en labores: el 19,5 % es criterio
// de ingeniería y la cita tiene que decirlo, no atribuirlo a un numeral.
TEST(LimitesGasesMexico, O2_CriterioDeIngenieriaDeclarado) {
    const GasLimit& g = lmp_for(RegulatoryStandard::NOM023_Mexico, GasType::O2);
    EXPECT_DOUBLE_EQ(g.floor_min.value(), 19.5);
    EXPECT_FALSE(g.ceiling.has_value());
    EXPECT_NE(g.regulation_ref.find("Criterio de ingenieria"), std::string::npos);
    EXPECT_NE(g.regulation_ref.find("no fija"), std::string::npos);
}

TEST(LimitesGasesMexico, TablaCompleta_8Gases_TodasConRef) {
    const auto& table = gas_limits(RegulatoryStandard::NOM023_Mexico);
    EXPECT_EQ(table.size(), 8u);
    for (const GasLimit& g : table) {
        EXPECT_FALSE(g.regulation_ref.empty());
        EXPECT_TRUE(g.twa_8h || g.stel || g.ceiling || g.floor_min);
    }
}
