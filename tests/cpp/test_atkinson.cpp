/**
 * @file test_atkinson.cpp
 * @brief Tests de resistencias Atkinson: tablas bibliográficas y calculador.
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "ventpy/atkinson.hpp"

using namespace ventpy;

// ============================================================================
// Tabla k — McPherson 2009, Cap. 5, Tabla 5.1 (p. 5-6), a ρ = 1.2 kg/m³
// ============================================================================

TEST(TablaFriccion, ValoresMcPhersonTabla51) {
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::SmoothLined),        0.004);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::Shotcrete),          0.0055);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::UnlinedMinorIrreg),  0.009);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::UnlinedTypical),     0.012);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::UnlinedRough),       0.016);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::ArchedDriftBolted),  0.010);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::ArchedRampBolted),   0.014);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::TimberedCribbed),    0.14);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::DuctFabricCollapsible), 0.0037);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::DuctFlexibleSpiral), 0.011);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::DuctFiberglass),     0.0024);
    EXPECT_DOUBLE_EQ(friction_factor_for(AirwayLining::DuctSteelSpiral),    0.0021);
}

TEST(TablaFriccion, TablaCompleta12EntradasTodasConCita) {
    const auto& t = atkinson_friction_factors();
    EXPECT_EQ(t.size(), 12u);
    for (const auto& e : t) {
        EXPECT_GT(e.k, 0.0);
        EXPECT_NE(e.biblio_ref.find("Tabla 5.1"), std::string::npos);
        EXPECT_NE(e.biblio_ref.find("McPherson"), std::string::npos);
    }
}

TEST(TablaFriccion, ManualLanza) {
    EXPECT_THROW(friction_factor_for(AirwayLining::Manual), std::invalid_argument);
}

// ============================================================================
// Choque X — McPherson 2009, Apéndice A5.2 (p. 5-28)
// ============================================================================

TEST(TablaChoque, ExitYEntranceExactos) {
    AirwaySingularity exit_s{SingularityType::Exit};
    AirwaySingularity entr_s{SingularityType::Entrance};
    EXPECT_DOUBLE_EQ(resolve_shock_factor(exit_s), 1.0);   // A5.2(a), A2→∞
    EXPECT_DOUBLE_EQ(resolve_shock_factor(entr_s), 0.5);   // A5.2(b), A1→∞
}

TEST(TablaChoque, ExpansionYContraccionPorFormula) {
    // A5.2(a): X = (1 - A1/A2)² con area_ratio = A1/A2 = 0.4 → 0.36
    AirwaySingularity exp_s{SingularityType::Expansion};
    exp_s.area_ratio = 0.4;
    EXPECT_DOUBLE_EQ(resolve_shock_factor(exp_s), 0.36);
    // A5.2(b): X = 0.5·(1 - A2/A1)² con area_ratio = A2/A1 = 0.5 → 0.125
    AirwaySingularity con_s{SingularityType::Contraction};
    con_s.area_ratio = 0.5;
    EXPECT_DOUBLE_EQ(resolve_shock_factor(con_s), 0.125);
}

TEST(TablaChoque, ExpansionSinRatioLanza) {
    AirwaySingularity s{SingularityType::Expansion};   // area_ratio = 0
    EXPECT_THROW(resolve_shock_factor(s), std::invalid_argument);
}

TEST(TablaChoque, BendsYJunctionSonManualOnly) {
    // Gate 2026-08-17: McPherson solo trae gráficos (Figs. A5.1-A5.3) para codos
    // y la fórmula de junction requiere velocidades de red (SP-3b) → manual.
    for (auto t : {SingularityType::Bend90, SingularityType::Bend45,
                   SingularityType::Junction}) {
        AirwaySingularity sin_x{t};
        EXPECT_THROW(resolve_shock_factor(sin_x), std::invalid_argument);
        AirwaySingularity con_x{t};
        con_x.shock_factor_x = 0.8;
        EXPECT_DOUBLE_EQ(resolve_shock_factor(con_x), 0.8);
    }
}

TEST(TablaChoque, ManualExigeXPositivo) {
    AirwaySingularity s{SingularityType::Manual};
    EXPECT_THROW(resolve_shock_factor(s), std::invalid_argument);
    s.shock_factor_x = 1.3;
    EXPECT_DOUBLE_EQ(resolve_shock_factor(s), 1.3);
}
