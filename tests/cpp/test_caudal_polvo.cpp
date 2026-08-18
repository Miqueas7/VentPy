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

// FIX 2 (revision final SP-4): resulting_velocity_mps queda sin asignar
// cuando face_area_m2 <= 0 (la rama "if (p.face_area_m2 > 0.0)" no corre).
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
    p = base_dust(); p.target_concentration_mg_m3 = 0.0;
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
    p = base_dust(); p.suppression_efficiency = 1.0;   // [0,1)
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
    p = base_dust(); p.silica_content_percent = 130.0;
    EXPECT_THROW(DustFlowCalculator::calculate(p, cfg), std::invalid_argument);
}
