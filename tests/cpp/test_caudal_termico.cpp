/**
 * @file test_caudal_termico.cpp
 * @brief Tests del cálculo de Q_termico (DS 024, Art. 252.d + Art. 104/Anexo 13).
 *
 * Todos los valores esperados se derivaron con una réplica Python EXACTA del
 * modelo (misma fórmula ISA de atmosphere.hpp, mismo ternario de densidad de
 * sitio que atkinson.hpp/ventilador.hpp) ANTES de escribir estos tests —
 * script: .superpowers/sdd/2026-08-18-sp4-polvo-termico/../../../../
 * (guardado en el workspace SDD como probe-termico.py; salida completa en
 * task-2-report.md). Regla de oro del anexo del controlador: si el binario
 * difiere de estos valores, es BLOCKED — nunca se ajustan números aquí.
 *
 * Derivación (resumen; ver probe-termico.py para el detalle):
 *   pressure_kpa(2500)              = 74.6751618663247
 *   rho(2500, dry_bulb=12C)         = 0.912315903449939
 *   rho(2500, dry_bulb=16C)         = 0.8996952442287743
 *   rho(2500, dry_bulb=22C)         = 0.8814056576952399
 *   cp = constants::AIR_CP_KJ_KG_K  = 1.005
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "ventpy/caudal_termico.hpp"

using namespace ventpy;

namespace {
ThermalParams base_thermal() {
    ThermalParams p;
    p.virgin_rock_temp_c = 25.0;
    p.geothermal_gradient_c_per_100m = 1.0;
    p.depth_below_surface_m = 800.0;
    p.auto_compression_c_per_100m = 0.98;
    p.heat_from_equipment_kw = 400.0;
    p.heat_from_oxidation_kw = 50.0;
    p.target_effective_temp_c = 28.0;
    p.face_area_m2 = 12.0;
    return p;
}
AtmosphericParams atm_2500_12C() {
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 12.0; return a;
}
} // namespace

// Caso 1: inlet = 12 + 0.98*800/100 = 19.84 (< 24 => sin criterio 252.d).
// delta_t = 28 - 19.84 = 8.16. total_heat = 400+50 = 450.
// rho(2500, 12C) = 0.912315903449939. q_raw = 450/(rho*1.005*8.16)*60
//                = 3608.7957124912564 m3/min => safety_ceil(q_raw - 1e-9) = 3609.
TEST(CaudalTermico, BalanceSensibleFactible) {
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(base_thermal(), atm_2500_12C(), cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 19.84);
    EXPECT_NEAR(r.delta_t_available, 8.16, 1e-12);
    EXPECT_DOUBLE_EQ(r.total_heat_load_kw, 450.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 3609.0);
    EXPECT_EQ(r.regulation_ref.find("252"), std::string::npos);
    EXPECT_GT(r.resulting_velocity_mps, 0.0);
    // FIX 3 (Task 1 pre-bindings): la oxidacion debe aparecer en el desglose
    // de auditoria (antes se sumaba a total_heat_load_kw pero no se exponia).
    EXPECT_DOUBLE_EQ(r.heat_from_oxidation_kw, 50.0);
    EXPECT_DOUBLE_EQ(
        r.heat_from_equipment_kw + r.heat_from_oxidation_kw +
            r.heat_from_rock_kw + r.heat_from_autocompression_kw +
            r.heat_from_other_kw,
        r.total_heat_load_kw);
}

// Caso 2: depth=1000, atm dry_bulb=18 (alt 2500). inlet = 18+0.98*1000/100 = 27.8.
// delta_t = 28-27.8 = 0.2 (artefacto FP: 0.1999999999999993) <= 0.5 => infactible
// por balance. PERO inlet 27.8 esta en [24,29] y face_area=12 (base_thermal) =>
// FIX 1 (revision final SP-4): el piso legal Art. 252.d NUNCA se descarta, ni
// siquiera cuando el balance termico es infactible: q_thermal =
// safety_ceil(12*0.5*60 - FP_TOL) = 360 (derivado en probe-fixwave.py).
TEST(CaudalTermico, InfactiblePorAutocompresion) {
    auto p = base_thermal();
    p.depth_below_surface_m = 1000.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 18.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 27.8);
    EXPECT_NEAR(r.delta_t_available, 0.2, 1e-9);
    EXPECT_DOUBLE_EQ(r.q_thermal, 360.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("refrigeracion") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}

// FIX 1 (I2 - revision final SP-4): superficie 25, depth 1000 => inlet =
// 25+0.98*1000/100 = 34.8 (> 29, fuera de [24,29]) => delta_t = 28-34.8 =
// -6.8 (infactible). face_area=0 => el piso 252.d NO aplica (requiere
// face_area>0) => q_thermal = 0. El chequeo "inlet>29 => advertencia 104"
// debe ejecutarse TAMBIEN en la rama infactible: advertencias deben contener
// tanto "refrigeracion" (piso 252.d/infactibilidad) como "104" (Art. 104 +
// Anexo 13). Derivado en probe-fixwave.py.
TEST(CaudalTermico, InfactibleCalienteAdvierte104) {
    auto p = base_thermal();
    p.face_area_m2 = 0.0;
    p.depth_below_surface_m = 1000.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 25.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 34.8);
    EXPECT_DOUBLE_EQ(r.q_thermal, 0.0);
    EXPECT_DOUBLE_EQ(r.resulting_velocity_mps, 0.0);
    bool aviso_refrigeracion = false;
    bool aviso_104 = false;
    for (const auto& w : r.warnings) {
        if (w.find("refrigeracion") != std::string::npos) aviso_refrigeracion = true;
        if (w.find("104") != std::string::npos) aviso_104 = true;
    }
    EXPECT_TRUE(aviso_refrigeracion);
    EXPECT_TRUE(aviso_104);
}

// Caso 3: atm dry_bulb=16, depth=900 => inlet = 16+0.98*900/100 = 24.82 (en
// [24,29] => 252.d aplica). equipos=15, oxidacion=0 => total_heat=15.
// rho(2500,16C)=0.8996952442287743. delta_t=28-24.82=3.18 (artefacto FP).
// balance crudo = 15/(rho*1.005*3.18)*60 = 313.0068939028916 => ceil-eps = 314.
// 252.d: face_area(12)*0.5*60 = 360 > 314 => domina 252.d => q_thermal = 360.
TEST(CaudalTermico, Criterio252dDominaConCargaBaja) {
    auto p = base_thermal();
    p.heat_from_equipment_kw = 15.0;
    p.heat_from_oxidation_kw = 0.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 16.0;
    p.depth_below_surface_m = 900.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 24.82);
    EXPECT_NEAR(r.delta_t_available, 3.18, 1e-9);
    EXPECT_DOUBLE_EQ(r.total_heat_load_kw, 15.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 360.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

// Caso 4: igual que 3 pero equipos=400, oxidacion=50 (defaults de base_thermal()).
// total_heat=450. balance crudo = 450/(rho(2500,16C)*1.005*3.18)*60
//                = 9390.206817086748 => ceil-eps = 9391 > 360 => domina balance.
// La cita 252.d sigue presente (aplica por rango de temperatura, gobierne o no).
TEST(CaudalTermico, BalanceDominaSobre252d) {
    auto p = base_thermal();   // equipos=400, oxidacion=50
    p.depth_below_surface_m = 900.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 16.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 24.82);
    EXPECT_DOUBLE_EQ(r.total_heat_load_kw, 450.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 9391.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

// Caso 5: virgin=30, gradiente=1.5, depth=900 => VRT = 30+1.5*900/100 = 43.5.
// target+10 = 38 => 43.5 > 38 => advertencia de estudio geotermico.
// (resto de params: base_thermal() default + atm_2500_12C(), depth override
//  a 900 => inlet = 12+0.98*900/100 = 20.82, delta_t = 7.18, total_heat=450,
//  balance crudo = 450/(rho(2500,12C)*1.005*7.18)*60 = 4101.361144001205
//  => ceil-eps = 4102; inlet 20.82 < 24 => sin criterio 252.d).
TEST(CaudalTermico, VrtAltaAdvierte) {
    auto p = base_thermal();
    p.virgin_rock_temp_c = 30.0;
    p.geothermal_gradient_c_per_100m = 1.5;
    p.depth_below_surface_m = 900.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("estudio") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 20.82);
    EXPECT_NEAR(r.delta_t_available, 7.18, 1e-12);
    EXPECT_DOUBLE_EQ(r.total_heat_load_kw, 450.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 4102.0);
}

// Caso 6: atm dry_bulb=22, depth=800 => inlet = 22+0.98*800/100 = 29.84 (> 29).
// target override a 32 para que sea factible (delta_t = 32-29.84 = 2.16 > 0.5).
// total_heat = 400+50 = 450 (defaults). rho(2500,22C)=0.8814056576952399.
// balance crudo = 450/(rho*1.005*2.16)*60 = 14111.335497660493 => ceil-eps = 14112.
// inlet > 29 (fuera de [24,29]) => sin cita 252.d, advertencia Art. 104/Anexo 13.
TEST(CaudalTermico, MayorA29AdvierteEstresTermico) {
    auto p = base_thermal();
    p.target_effective_temp_c = 32.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 22.0;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 29.84);
    EXPECT_NEAR(r.delta_t_available, 2.16, 1e-9);
    EXPECT_DOUBLE_EQ(r.total_heat_load_kw, 450.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 14112.0);
    EXPECT_EQ(r.regulation_ref.find("252"), std::string::npos);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("104") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}

TEST(CaudalTermico, Validaciones) {
    RegulatoryConfig cfg;
    auto p = base_thermal(); p.heat_from_equipment_kw = -1.0;
    EXPECT_THROW(ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg),
                 std::invalid_argument);
    p = base_thermal(); p.depth_below_surface_m = -1.0;
    EXPECT_THROW(ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg),
                 std::invalid_argument);
    p = base_thermal(); p.target_effective_temp_c = 0.0;
    EXPECT_THROW(ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg),
                 std::invalid_argument);
    p = base_thermal(); p.geothermal_gradient_c_per_100m = -1.0;
    EXPECT_THROW(ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg),
                 std::invalid_argument);
    p = base_thermal(); p.auto_compression_c_per_100m = -1.0;
    EXPECT_THROW(ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg),
                 std::invalid_argument);
}

// ============================================================================
// FIX 4 (revision final SP-4): fronteras exactas del rango [24,29] del
// Art. 252.d. Derivado en probe-fixwave.py (regla de oro: replica Python
// EXACTA antes de escribir el test).
// ============================================================================

// superficie=16.16, depth=800, autoc=0.98 => inlet = 16.16+7.84 = 24.0 EXACTO
// (== inclusivo por el lado bajo del rango [24,29]). equipos=15, oxidacion=0,
// target=28 => delta_t=4.0. rho(2500,16.16C)=0.8991976767783694.
// balance crudo = 248.97817554093456 => ceil-eps = 249. 252.d = 12*0.5*60=360.
// 360 > 249 => domina el piso 252.d => q_thermal = 360, ref contiene "252".
TEST(CaudalTermico, Frontera24Activa252d) {
    auto p = base_thermal();
    p.heat_from_equipment_kw = 15.0;
    p.heat_from_oxidation_kw = 0.0;
    p.depth_below_surface_m = 800.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 16.16;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 24.0);
    EXPECT_DOUBLE_EQ(r.q_thermal, 360.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

// superficie=21.16, depth=800, autoc=0.98 => inlet = 21.16+7.84 = 29.0 EXACTO
// (== inclusivo por el lado alto del rango [24,29]). target override a 32
// para que el balance sea factible (delta_t = 32-29 = 3.0 > 0.5). equipos=400,
// oxidacion=50 (defaults). rho(2500,21.16C)=0.8839213070189598.
// balance crudo = 10131.245631807053 => ceil-eps = 10132 > 252d (360) =>
// domina el balance, pero la cita 252.d sigue presente por estar en rango.
TEST(CaudalTermico, Frontera29Activa252d) {
    auto p = base_thermal();
    p.target_effective_temp_c = 32.0;
    p.depth_below_surface_m = 800.0;
    AtmosphericParams a; a.altitude_masl = 2500.0; a.dry_bulb_temp_c = 21.16;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, a, cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 29.0);
    EXPECT_NEAR(r.delta_t_available, 3.0, 1e-9);
    EXPECT_DOUBLE_EQ(r.q_thermal, 10132.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

// target = inlet + 0.5 EXACTO => delta_t_available == 0.5 exacto, que cae en
// la frontera inclusiva "<= 0.5" del gate de infactibilidad. atm_2500_12C() +
// depth=800 (default) => inlet = 12+0.98*800/100 = 19.84; target = 20.34 =>
// delta_t = 0.5 EXACTO => infactible (q_thermal=0, advertencia refrigeracion).
// inlet 19.84 no esta en [24,29] => el piso 252.d no aplica (independiente de
// face_area).
TEST(CaudalTermico, DeltaTMedioExactoInfactible) {
    auto p = base_thermal();
    p.target_effective_temp_c = 20.34;
    RegulatoryConfig cfg;
    auto r = ThermalFlowCalculator::calculate(p, atm_2500_12C(), cfg);
    EXPECT_DOUBLE_EQ(r.inlet_temp_c, 19.84);
    EXPECT_DOUBLE_EQ(r.delta_t_available, 0.5);
    EXPECT_DOUBLE_EQ(r.q_thermal, 0.0);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("refrigeracion") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}
