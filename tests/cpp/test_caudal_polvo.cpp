/**
 * @file test_caudal_polvo.cpp
 * @brief Tests del cálculo de Q_polvo (DS 024, Art. 111).
 */
#include <gtest/gtest.h>
#include <stdexcept>
#include "ventpy/caudal_polvo.hpp"

using namespace ventpy;

namespace {
DustParams base_dust() {
    DustParams p;
    p.dust_generation_rate_mg_s = 50.0;
    p.target_concentration_mg_m3 = 3.0;   // Art. 111
    p.face_area_m2 = 12.0;
    p.water_suppression = true;
    p.suppression_efficiency = 0.7;
    return p;
}
} // namespace

TEST(CaudalPolvo, DilucionConSupresionExacta) {
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(base_dust(), cfg);
    // probe: efectiva = 50×0.3 = 15 mg/s; Q = 15/3 = 5 m³/s = 300 m³/min (ceil no-op)
    EXPECT_DOUBLE_EQ(r.effective_generation, 15.0);
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);
    EXPECT_NE(r.regulation_ref.find("Art. 111"), std::string::npos);
    EXPECT_TRUE(r.warnings.empty());
}

TEST(CaudalPolvo, SinSupresionYSafetyCeil) {
    auto p = base_dust();
    p.water_suppression = false;
    p.target_concentration_mg_m3 = 2.9;
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    // probe: crudo 1034.4827586… → safety_ceil = 1035 (NUNCA 1034)
    EXPECT_DOUBLE_EQ(r.effective_generation, 50.0);
    EXPECT_DOUBLE_EQ(r.q_dust, 1035.0);
    EXPECT_NEAR(r.resulting_velocity_mps, 1035.0 / 60.0 / 12.0, 1e-12);  // 1.4375
}

TEST(CaudalPolvo, SiliceAdvierteRemision) {
    auto p = base_dust();
    p.silica_content_percent = 12.0;
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("Anexo 15") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);   // G2: remisión, sin número inventado
}

TEST(CaudalPolvo, TargetSobreLeoAdvierte) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 4.0;   // > 3 del Art. 111
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    bool aviso = false;
    for (const auto& w : r.warnings)
        if (w.find("Art. 111") != std::string::npos) aviso = true;
    EXPECT_TRUE(aviso);
}

TEST(CaudalPolvo, GeneracionCeroEsCaudalCero) {
    auto p = base_dust();
    p.dust_generation_rate_mg_s = 0.0;
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    EXPECT_DOUBLE_EQ(r.q_dust, 0.0);
}

// Regresion: resulting_velocity_mps queda sin asignar cuando
// face_area_m2 <= 0 (la rama "if (p.face_area_m2 > 0.0)" no corre).
// Con el inicializador "= 0.0" agregado en types.hpp (DustFlowResult), el
// valor debe ser 0.0 en vez de UB.
TEST(CaudalPolvo, AreaCeroVelocidadCero) {
    auto p = base_dust();
    p.face_area_m2 = 0.0;
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);  // caudal normal, no afectado por area
    EXPECT_DOUBLE_EQ(r.resulting_velocity_mps, 0.0);
}

TEST(CaudalPolvo, FronteraFpArtefactoNoInfla) {
    // 50×(1−0.7) = 15.000000000000002 → crudo 300.00000000000006 → 300, no 301
    // Verifica que tolerancia FP sustractiva absorbe el artefacto sin sobre-reportar
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(base_dust(), cfg);
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);
}

TEST(CaudalPolvo, FraccionGenuinaSiempreSubeAlCeil) {
    // target elegido para crudo = 50/2.999×60 = 1000.3334... → ceil 1001
    // Verifica que fracciones genuinas (lejos del epsilon) siempre suben
    auto p = base_dust();
    p.water_suppression = false;      // efectiva 50
    p.target_concentration_mg_m3 = 2.999;
    RegulatoryConfig cfg;
    auto r = DustFlowCalculator::calculate(p, cfg);
    EXPECT_DOUBLE_EQ(r.q_dust, 1001.0);
    // Verifica que NO advierte Art. 111 cuando target < 3 (no es sobre-LEO)
    for (const auto& w : r.warnings)
        EXPECT_EQ(w.find("Art. 111"), std::string::npos);
}

TEST(CaudalPolvo, Validaciones) {
    RegulatoryConfig cfg;
    auto p = base_dust(); p.dust_generation_rate_mg_s = -1.0;
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
    p = base_dust(); p.target_concentration_mg_m3 = -1.0;  // 0 = limite de la norma
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
    p = base_dust(); p.suppression_efficiency = 1.0;   // [0,1)
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
    p = base_dust(); p.silica_content_percent = 130.0;
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
}

// Con target 0 rige el límite de la norma configurada.
//   Perú: 3 mg/m³ (DS 024-2016-EM, Art. 111) → 15 / 3 × 60 = 300 m³/min.
//   Chile al nivel del mar: 2,4 mg/m³ (DS 594, Art. 66) → 15 / 2,4 × 60 = 375.
//   México: 3 mg/m³ (NOM-010-STPS-2014, fila 609); Colombia: 3 (criterio).
TEST(CaudalPolvo, ObjetivoPorDefectoSigueALaNorma) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;
    const auto pe = DustFlowCalculator::calculate(p, RegulatoryConfig::peru());
    const auto cl = DustFlowCalculator::calculate(p, RegulatoryConfig::chile());
    const auto mx = DustFlowCalculator::calculate(p, RegulatoryConfig::mexico());
    const auto co = DustFlowCalculator::calculate(p, RegulatoryConfig::colombia());
    EXPECT_DOUBLE_EQ(pe.target_concentration, 3.0);
    EXPECT_DOUBLE_EQ(pe.q_dust, 300.0);
    EXPECT_DOUBLE_EQ(cl.target_concentration, 2.4);
    EXPECT_DOUBLE_EQ(cl.q_dust, 375.0);
    EXPECT_DOUBLE_EQ(mx.target_concentration, 3.0);
    EXPECT_DOUBLE_EQ(co.target_concentration, 3.0);
    EXPECT_TRUE(cl.warnings.empty());
}

// Chile sobre 1.000 msnm: LPP × Fa, Fa = P/760 con dos decimales (DS 594,
// Art. 63). A 3.000 msnm, ISA: P = 70,100 kPa = 525,79 mmHg → Fa = 0,6918 →
// 0,69 → 2,4 × 0,69 = 1,656 mg/m³. Q = 15 / 1,656 × 60 = 543,48 → 544.
// A 1.000 msnm exactos no aplica (el artículo dice «superior a 1.000»).
TEST(CaudalPolvo, ChileCorrigePorFaSobre1000msnm) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;
    AtmosphericParams atm;
    atm.altitude_masl = 3000.0;
    const auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::chile(), atm);
    EXPECT_NEAR(r.target_concentration, 1.656, 1e-12);
    EXPECT_DOUBLE_EQ(r.q_dust, 544.0);

    atm.altitude_masl = 1000.0;
    EXPECT_DOUBLE_EQ(DustFlowCalculator::respirable_limit_mg_m3(
        RegulatoryConfig::chile(), atm), 2.4);
    // Presión medida: 80,0 kPa → Fa = 0,7895 → 0,79 → 1,896 mg/m³.
    atm.altitude_masl = 2000.0;
    atm.barometric_pressure_kpa = 80.0;
    EXPECT_NEAR(DustFlowCalculator::respirable_limit_mg_m3(
        RegulatoryConfig::chile(), atm), 1.896, 1e-12);
    // Fuera de Chile no hay corrección por altitud.
    EXPECT_DOUBLE_EQ(DustFlowCalculator::respirable_limit_mg_m3(
        RegulatoryConfig::peru(), atm), 3.0);
}

// Un objetivo del usuario sobre el límite de la norma se advierte: con Chile
// 3 mg/m³ supera los 2,4 del DS 594, Art. 66.
TEST(CaudalPolvo, ChileAdvierteObjetivoSobreElLimite) {
    const auto r = DustFlowCalculator::calculate(base_dust(), RegulatoryConfig::chile());
    ASSERT_EQ(r.warnings.size(), 1u);
    EXPECT_NE(r.warnings[0].find("por ENCIMA de 2.4 mg/m3"), std::string::npos)
        << r.warnings[0];
    EXPECT_NE(r.warnings[0].find("DS 594, Art. 66"), std::string::npos);
}

// Con sílice declarada, el objetivo es el menor entre el límite respirable y
// el límite de sílice dividido entre la fracción de sílice del polvo.
TEST(CaudalPolvo, SiliceGobiernaElObjetivoEnPeru) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;   // el de la norma
    p.silica_content_percent = 10.0;
    auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::peru());
    // Anexo 15: 0,05 mg/m3 / 0,10 = 0,5 < 3 → Q = 15 / 0,5 = 30 m3/s = 1800
    EXPECT_DOUBLE_EQ(r.target_concentration, 0.5);
    EXPECT_DOUBLE_EQ(r.q_dust, 1800.0);
    EXPECT_NE(r.regulation_ref.find("Anexo 15"), std::string::npos);
}

TEST(CaudalPolvo, PocaSiliceNoGobierna) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;
    p.silica_content_percent = 1.0;       // 0,05 / 0,01 = 5 > 3
    auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::peru());
    EXPECT_DOUBLE_EQ(r.target_concentration, 3.0);
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);
}

TEST(CaudalPolvo, SiliceChileYMexico) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;
    p.silica_content_percent = 10.0;
    // DS 594 Art. 66, cuarzo 0,08 (Fa = 1 a nivel del mar) / 0,10 = 0,8
    auto cl = DustFlowCalculator::calculate(p, RegulatoryConfig::chile());
    EXPECT_DOUBLE_EQ(cl.target_concentration, 0.8);
    EXPECT_DOUBLE_EQ(cl.q_dust, 1125.0);
    // NOM-010 fila 668: 0,025 / 0,10 = 0,25 → 15 / 0,25 = 60 m3/s
    auto mx = DustFlowCalculator::calculate(p, RegulatoryConfig::mexico());
    EXPECT_DOUBLE_EQ(mx.target_concentration, 0.25);
    EXPECT_DOUBLE_EQ(mx.q_dust, 3600.0);
}

// El Decreto 1886 remite a la ACGIH sin fijar un número: no se cuantifica.
TEST(CaudalPolvo, SiliceColombiaSoloAdvierte) {
    auto p = base_dust();
    p.silica_content_percent = 10.0;
    auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::colombia());
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);
    ASSERT_FALSE(r.warnings.empty());
    EXPECT_NE(r.warnings[0].find("no cuantificado"), std::string::npos);
}

TEST(CaudalPolvo, ObjetivoDelUsuarioSobreLaSiliceAdvierte) {
    auto p = base_dust();                 // objetivo explícito de 3 mg/m3
    p.silica_content_percent = 10.0;
    auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::peru());
    EXPECT_DOUBLE_EQ(r.q_dust, 300.0);
    bool aviso = false;
    for (const auto& w : r.warnings) aviso = aviso || w.find("silice") != std::string::npos;
    EXPECT_TRUE(aviso);
}

// DS 594 Art. 66: cristobalita y tridimita 0,04 mg/m3, la mitad que el cuarzo.
TEST(CaudalPolvo, CristobalitaChileUsa004) {
    auto p = base_dust();
    p.target_concentration_mg_m3 = 0.0;
    p.silica_content_percent = 10.0;
    p.silica_cristobalite_or_tridymite = true;
    auto r = DustFlowCalculator::calculate(p, RegulatoryConfig::chile());
    // 0,04 / 0,10 = 0,4 → 15 / 0,4 = 37,5 m3/s = 2250 m3/min
    EXPECT_DOUBLE_EQ(r.target_concentration, 0.4);
    EXPECT_DOUBLE_EQ(r.q_dust, 2250.0);
    // En Perú el Anexo 15 da 0,05 para las tres formas: no cambia.
    auto pe = DustFlowCalculator::calculate(p, RegulatoryConfig::peru());
    EXPECT_DOUBLE_EQ(pe.target_concentration, 0.5);
}
