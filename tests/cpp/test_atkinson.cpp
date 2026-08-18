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

TEST(TablaChoque, TablaInformativa7EntradasTodasConCita) {
    const auto& t = shock_factors();
    EXPECT_EQ(t.size(), 7u);
    for (const auto& e : t) {
        EXPECT_NE(e.biblio_ref.find("A5"), std::string::npos);
    }
    int exactos = 0;
    for (const auto& e : t) {
        if (e.type == SingularityType::Exit) {
            EXPECT_DOUBLE_EQ(e.x, 1.0);
            ++exactos;
        } else if (e.type == SingularityType::Entrance) {
            EXPECT_DOUBLE_EQ(e.x, 0.5);
            ++exactos;
        } else {
            EXPECT_DOUBLE_EQ(e.x, 0.0);
            EXPECT_FALSE(e.note.empty());
        }
    }
    EXPECT_EQ(exactos, 2);
}

// ============================================================================
// AtkinsonCalculator — esperados derivados con probe_sp3a.py (2026-08-17)
// Galería: L=500 m, per=15 m, A=14 m², k manual 0.012, T bulbo seco 20 °C.
// ============================================================================

namespace {
AirwayParams galeria_base() {
    AirwayParams p;
    p.airway_id = "GAL-500";
    p.length_m = 500.0;
    p.perimeter_m = 15.0;
    p.area_m2 = 14.0;
    p.lining = AirwayLining::Manual;
    p.atkinson_k = 0.012;
    return p;
}
} // namespace

TEST(AtkinsonRamal, ResistenciaFriccionNivelDelMar) {
    AtmosphericParams atm;   // alt 0, T 20 °C → ρ = 1.20411831637
    auto r = AtkinsonCalculator::calculate_resistance(galeria_base(), atm);

    EXPECT_NEAR(r.air_density_kg_m3, 1.2041183163746156, 1e-12);
    EXPECT_DOUBLE_EQ(r.k_used, 0.012);
    EXPECT_NEAR(r.k_corrected, 0.0120411831637462, 1e-12);
    // R = k_corr·L·per/A³ = k_corr·500·15/2744
    EXPECT_NEAR(r.r_friction, 0.0329113971312304, 1e-12);
    EXPECT_DOUBLE_EQ(r.r_shock, 0.0);
    EXPECT_NEAR(r.r_total, 0.0329113971312304, 1e-12);
}

TEST(AtkinsonRamal, ChoqueExitSumaResistencia) {
    auto p = galeria_base();
    p.singularities.push_back({SingularityType::Exit});
    AtmosphericParams atm;
    auto r = AtkinsonCalculator::calculate_resistance(p, atm);

    // R_x = X·ρ/(2A²) = 1.0×1.20411.../(2×196)
    EXPECT_NEAR(r.r_shock, 0.0030717303989148, 1e-12);
    EXPECT_NEAR(r.r_total, 0.0359831275301452, 1e-12);
}

TEST(AtkinsonRamal, ContraccionPorFormulaSuma) {
    auto p = galeria_base();
    AirwaySingularity c{SingularityType::Contraction};
    c.area_ratio = 0.5;   // X = 0.125
    p.singularities.push_back(c);
    AtmosphericParams atm;
    auto r = AtkinsonCalculator::calculate_resistance(p, atm);
    EXPECT_NEAR(r.r_shock, 0.0003839662998644, 1e-12);
}

TEST(AtkinsonRamal, DensidadEnAltitudReduceR) {
    AtmosphericParams atm;
    atm.altitude_masl = 4200.0;   // ρ = 0.71350295977866
    auto r = AtkinsonCalculator::calculate_resistance(galeria_base(), atm);
    EXPECT_NEAR(r.air_density_kg_m3, 0.7135029597786612, 1e-12);
    EXPECT_NEAR(r.r_friction, 0.0195017208394313, 1e-12);
}

TEST(AtkinsonRamal, CaudalDaPresionVelocidadYUnidades) {
    auto p = galeria_base();
    p.singularities.push_back({SingularityType::Exit});
    AtmosphericParams atm;
    auto r = AtkinsonCalculator::calculate(p, atm, 3000.0);   // 50 m³/s

    EXPECT_DOUBLE_EQ(r.q_m3min, 3000.0);
    EXPECT_NEAR(r.velocity_mps, 3.5714285714285716, 1e-12);
    // ΔP = R_total·(Q/60)² — crudo, sin redondeo (spec: sin safety_ceil)
    EXPECT_NEAR(r.pressure_drop_pa, 89.9578188253631, 1e-9);
    EXPECT_NEAR(r.pressure_drop_mmh2o, 9.1731446340354, 1e-9);
}

TEST(AtkinsonRamal, VelocidadBajaAdvierteArt248) {
    AtmosphericParams atm;
    auto r = AtkinsonCalculator::calculate(galeria_base(), atm, 150.0);
    // v = 150/14 = 10.71 m/min < 20 m/min (DS 024, Art. 248)
    ASSERT_FALSE(r.warnings.empty());
    bool cita = false;
    for (const auto& w : r.warnings)
        if (w.find("248") != std::string::npos) cita = true;
    EXPECT_TRUE(cita);
}

TEST(AtkinsonRamal, LiningDeTablaUsaKDeTablaYCita) {
    auto p = galeria_base();
    p.lining = AirwayLining::UnlinedTypical;   // k = 0.012 de Tabla 5.1
    p.atkinson_k = 0.0;
    AtmosphericParams atm;
    auto r = AtkinsonCalculator::calculate_resistance(p, atm);
    EXPECT_DOUBLE_EQ(r.k_used, 0.012);
    EXPECT_NE(r.biblio_ref.find("Tabla 5.1"), std::string::npos);
}

TEST(AtkinsonRamal, Validaciones) {
    AtmosphericParams atm;
    auto p = galeria_base(); p.length_m = 0.0;
    EXPECT_THROW(AtkinsonCalculator::calculate_resistance(p, atm),
                 std::invalid_argument);
    p = galeria_base(); p.atkinson_k = 0.0;   // Manual sin k
    EXPECT_THROW(AtkinsonCalculator::calculate_resistance(p, atm),
                 std::invalid_argument);
    p = galeria_base();
    EXPECT_THROW(AtkinsonCalculator::calculate(p, atm, -10.0),
                 std::invalid_argument);
}
