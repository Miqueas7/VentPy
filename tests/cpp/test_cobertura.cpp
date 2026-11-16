/**
 * @file test_cobertura.cpp
 * @brief Tests del análisis de déficit/cobertura (DS 024, Art. 252 f/g).
 */

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

#include "ventpy/cobertura.hpp"

using namespace ventpy;

// ============================================================================
// compare_zone — medición directa (nivel puro)
// ============================================================================

TEST(CoverageZone, DirectaCumpleSinAdvertencias) {
    ZoneMeasurement m;
    m.zone_name = "Rampa 4200";
    m.q_measured_m3min = 250.0;

    auto r = CoverageCalculator::compare_zone(207.0, m);

    EXPECT_EQ(r.zone_name, "Rampa 4200");
    EXPECT_DOUBLE_EQ(r.q_required_m3min, 207.0);
    EXPECT_DOUBLE_EQ(r.q_measured_m3min, 250.0);
    EXPECT_NEAR(r.coverage_ratio, 250.0 / 207.0, 1e-9);   // 1.2077 crudo
    EXPECT_TRUE(r.compliant);
    EXPECT_FALSE(r.near_deficit_warning);                 // 1.2077 > 1.10
    EXPECT_FALSE(r.overventilated);                       // 1.2077 < 1.5
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 0.0);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
}

TEST(CoverageZone, DeficitUsaSafetyCeil) {
    ZoneMeasurement m;
    m.zone_name = "Frente N-02";
    m.q_measured_m3min = 90.2;

    auto r = CoverageCalculator::compare_zone(100.0, m);

    EXPECT_FALSE(r.compliant);
    // deficit = ceil(100 - 90.2) = ceil(9.8) = 10 — NUNCA 9
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 10.0);
    EXPECT_NEAR(r.coverage_ratio, 0.902, 1e-9);
}

TEST(CoverageZone, CoberturaJustaGeneraAdvertenciaDeMargen) {
    ZoneMeasurement m;
    m.zone_name = "Tajeo 380";
    m.q_measured_m3min = 500.0;

    auto r = CoverageCalculator::compare_zone(500.0, m);

    EXPECT_TRUE(r.compliant);            // medido >= requerido
    EXPECT_TRUE(r.near_deficit_warning); // 1.00 < 1.10 (margen ingenieril 10%)
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 0.0);
}

TEST(CoverageZone, MedidoCeroDirectoEsValido) {
    // Medición directa Q=0 (labor sin ventilar): dato real, no debe lanzar.
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 0.0;

    auto r = CoverageCalculator::compare_zone(100.0, m);

    EXPECT_FALSE(r.compliant);
    EXPECT_DOUBLE_EQ(r.coverage_ratio, 0.0);
    EXPECT_DOUBLE_EQ(r.deficit_m3min, 100.0);
}

TEST(CoverageZone, SobreVentilacionAdvierte) {
    ZoneMeasurement m;
    m.zone_name = "Bypass 12";
    m.q_measured_m3min = 160.0;

    auto r = CoverageCalculator::compare_zone(100.0, m);

    EXPECT_TRUE(r.compliant);
    EXPECT_TRUE(r.overventilated);       // 1.6 > 1.5 (factor ingenieril)
}

TEST(CoverageZone, UmbralesPersonalizados) {
    CoverageParams p;
    p.warning_margin = 0.30;             // exigir 130% para no advertir
    p.overventilation_factor = 2.0;

    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 120.0;

    auto r = CoverageCalculator::compare_zone(100.0, m, p);
    EXPECT_TRUE(r.near_deficit_warning);  // 1.2 < 1.3
    EXPECT_FALSE(r.overventilated);       // 1.2 < 2.0
}

// ============================================================================
// compare_zone — validación en frontera
// ============================================================================

TEST(CoverageZone, FuenteAmbiguaLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;
    m.stations.push_back({"E-1", 10.0, 1.0});   // ambas fuentes

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, SinFuenteLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, RequeridoNoPositivoLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;

    EXPECT_THROW(CoverageCalculator::compare_zone(0.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, MedidoNegativoLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = -5.0;

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, MedidoInfinitoLanza) {
    // Regresion: ANTES de isfinite en la frontera, un
    // q_measured_m3min = inf pasaba require_non_negative (inf >= 0) y
    // reportaba compliant=true de forma espuria. Debe lanzar.
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = std::numeric_limits<double>::infinity();

    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageZone, ParamsInvalidosLanzan) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;

    CoverageParams margen_negativo;
    margen_negativo.warning_margin = -0.1;
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, margen_negativo),
                 std::invalid_argument);

    CoverageParams factor_bajo;
    factor_bajo.overventilation_factor = 1.0;   // debe ser > 1
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, factor_bajo),
                 std::invalid_argument);

    CoverageParams velocidades_invertidas;
    velocidades_invertidas.min_velocity_mpm = 300.0;   // min >= max (250, Art. 248)
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, velocidades_invertidas),
                 std::invalid_argument);
}

// 0 significa "el límite de la norma"; un valor negativo no tiene sentido.
TEST(CoverageZone, VelocidadNegativaEnParamsLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.q_measured_m3min = 100.0;

    CoverageParams p;
    p.min_velocity_mpm = -1.0;
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, p),
                 std::invalid_argument);
    CoverageParams q;
    q.max_velocity_mpm = -1.0;
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m, q),
                 std::invalid_argument);
}

// ============================================================================
// compare_zone — estaciones de aforo + velocidad Art. 248
// ============================================================================

TEST(CoverageStations, SumaEstacionesParalelas) {
    ZoneMeasurement m;
    m.zone_name = "Nivel 380";
    m.stations.push_back({"E-1", 10.0, 1.0});   // Q = 10 × 1.0 × 60 = 600
    m.stations.push_back({"E-2", 5.0, 0.5});    // Q = 5 × 0.5 × 60 = 150

    auto r = CoverageCalculator::compare_zone(700.0, m);

    ASSERT_EQ(r.stations.size(), 2u);
    EXPECT_DOUBLE_EQ(r.stations[0].q_station_m3min, 600.0);
    EXPECT_DOUBLE_EQ(r.stations[0].velocity_mpm, 60.0);
    EXPECT_TRUE(r.stations[0].velocity_ok);       // 20 <= 60 <= 250
    EXPECT_DOUBLE_EQ(r.stations[1].q_station_m3min, 150.0);
    EXPECT_DOUBLE_EQ(r.q_measured_m3min, 750.0);  // suma paralela
    EXPECT_TRUE(r.compliant);                     // 750 >= 700
}

TEST(CoverageStations, VelocidadBajoMinimoAdvierte) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 12.0, 0.30});  // 18 m/min < 20 (Art. 248)

    auto r = CoverageCalculator::compare_zone(100.0, m);

    ASSERT_EQ(r.stations.size(), 1u);
    EXPECT_FALSE(r.stations[0].velocity_ok);
    EXPECT_NE(r.stations[0].warning.find("248"), std::string::npos);
    // El caudal SÍ se contabiliza aunque la velocidad esté fuera de rango
    EXPECT_DOUBLE_EQ(r.q_measured_m3min, 216.0);
}

TEST(CoverageStations, VelocidadSobreMaximoAdvierte) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 4.0, 4.5});    // 270 m/min > 250

    auto r = CoverageCalculator::compare_zone(100.0, m);
    EXPECT_FALSE(r.stations[0].velocity_ok);
    EXPECT_NE(r.stations[0].warning.find("248"), std::string::npos);
}

TEST(CoverageStations, AnfoElevaMinimoA25) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 10.0, 0.35});  // 21 m/min

    // Sin ANFO: 21 >= 20 → ok
    auto sin_anfo = CoverageCalculator::compare_zone(100.0, m);
    EXPECT_TRUE(sin_anfo.stations[0].velocity_ok);

    // Con ANFO: mínimo efectivo 25 (Art. 248) → 21 < 25 advierte
    CoverageParams p;
    p.anfo_or_blasting_agents = true;
    auto con_anfo = CoverageCalculator::compare_zone(100.0, m, p);
    EXPECT_FALSE(con_anfo.stations[0].velocity_ok);
    EXPECT_NE(con_anfo.stations[0].warning.find("ANFO"), std::string::npos);
}

TEST(CoverageStations, AnfoConFalloPorMaximoNoMencionaAnfo) {
    // El sufijo de ANFO solo aplica cuando el fallo es por el MÍNIMO
    // (Art. 248: con ANFO el mínimo sube a 25 m/min). Si el fallo es por
    // exceder el máximo, mencionar ANFO es engañoso.
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 4.0, 4.5});    // 270 m/min > 250

    CoverageParams p;
    p.anfo_or_blasting_agents = true;

    auto r = CoverageCalculator::compare_zone(100.0, m, p);
    EXPECT_FALSE(r.stations[0].velocity_ok);
    EXPECT_NE(r.stations[0].warning.find("248"), std::string::npos);
    EXPECT_EQ(r.stations[0].warning.find("ANFO"), std::string::npos);
}

TEST(CoverageStations, FronteraExacta20y250EsValida) {
    // Semántica inclusiva: 20 y 250 m/min son válidos (limites del Art. 248).
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-20", 6.0, 20.0 / 60.0});   // exactamente 20 m/min
    m.stations.push_back({"E-250", 1.0, 250.0 / 60.0}); // exactamente 250 m/min

    auto r = CoverageCalculator::compare_zone(100.0, m);

    ASSERT_EQ(r.stations.size(), 2u);
    EXPECT_TRUE(r.stations[0].velocity_ok);
    EXPECT_TRUE(r.stations[1].velocity_ok);
}

TEST(CoverageStations, AnfoConMinimoCustomMayorUsaElMayor) {
    // Mínimo efectivo = max(min_velocity_mpm, 25). Si el custom ya es mayor
    // que 25, debe prevalecer el custom.
    CoverageParams p;
    p.anfo_or_blasting_agents = true;
    p.min_velocity_mpm = 30.0;

    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 10.0, 28.0 / 60.0});   // 28 m/min

    auto r = CoverageCalculator::compare_zone(100.0, m, p);

    ASSERT_EQ(r.stations.size(), 1u);
    EXPECT_FALSE(r.stations[0].velocity_ok);   // 28 < max(30, 25) = 30
}

TEST(CoverageStations, VelocidadCeroEsMedicionValida) {
    // Labor sin flujo: dato real de levantamiento — Q = 0, advertencia por
    // velocidad, sin excepción.
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 8.0, 0.0});

    auto r = CoverageCalculator::compare_zone(100.0, m);
    EXPECT_DOUBLE_EQ(r.q_measured_m3min, 0.0);
    EXPECT_FALSE(r.compliant);
    EXPECT_FALSE(r.stations[0].velocity_ok);
}

TEST(CoverageStations, AreaNoPositivaLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 0.0, 1.0});
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

TEST(CoverageStations, VelocidadNegativaLanza) {
    ZoneMeasurement m;
    m.zone_name = "Z";
    m.stations.push_back({"E-1", 10.0, -0.5});
    EXPECT_THROW(CoverageCalculator::compare_zone(100.0, m),
                 std::invalid_argument);
}

// ============================================================================
// analyze_survey — balance de mina vía Governor (E2E)
// Números derivados a mano: zona tipo = 10 trabajadores,
// DevelopmentFace, sin flota → Q_Per = 240 (piso de velocidad: 12 m² ×
// 20 m/min, Art. 248) → req = 240 × 1,15 = 276.
// ============================================================================

namespace {
ZoneSurvey make_zone(const std::string& name, double altitude,
                     double measured_direct) {
    ZoneSurvey z;
    z.zone_name = name;
    z.input.zone_type = ZoneType::DevelopmentFace;
    z.input.num_workers = 10;
    z.input.altitude_masl = altitude;
    z.measurement.zone_name = name;
    z.measurement.q_measured_m3min = measured_direct;
    return z;
}
} // namespace

TEST(CoverageSurvey, GlobalCubreConZonaEnDeficit) {
    std::vector<ZoneSurvey> zones;
    zones.push_back(make_zone("Rampa 4200", 4200.0, 350.0));
    zones.push_back(make_zone("Frente N-02", 2500.0, 210.0));

    auto r = CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru());

    ASSERT_EQ(r.zones.size(), 2u);
    EXPECT_DOUBLE_EQ(r.zones[0].q_required_m3min, 276.0);
    EXPECT_DOUBLE_EQ(r.zones[1].q_required_m3min, 276.0);
    EXPECT_TRUE(r.zones[0].compliant);              // 350 >= 276
    EXPECT_FALSE(r.zones[1].compliant);             // 210 < 276
    EXPECT_DOUBLE_EQ(r.zones[1].deficit_m3min, 66.0);

    EXPECT_DOUBLE_EQ(r.q_required_total_m3min, 552.0);
    EXPECT_DOUBLE_EQ(r.q_measured_total_m3min, 560.0);
    EXPECT_NEAR(r.coverage_ratio, 560.0 / 552.0, 1e-9);
    EXPECT_TRUE(r.global_compliant);        // Art. 252.f: Σmed >= Σreq
    EXPECT_FALSE(r.all_zones_compliant);    // Art. 252.g: B en déficit
    EXPECT_FALSE(r.compliant);              // criterio estricto: ambos
    EXPECT_DOUBLE_EQ(r.deficit_total_m3min, 0.0);

    // La zona en déficit aparece en las advertencias agregadas
    bool deficit_warned = false;
    for (const auto& w : r.warnings) {
        if (w.find("Frente N-02") != std::string::npos) deficit_warned = true;
    }
    EXPECT_TRUE(deficit_warned);
    EXPECT_NE(r.regulation_ref.find("252"), std::string::npos);
    // Cada zona lleva su desglose completo del Governor
    ASSERT_TRUE(r.zones[0].demand.has_value());
    EXPECT_EQ(r.zones[0].demand->standard, RegulatoryStandard::DS024_Peru);
}

TEST(CoverageSurvey, DeficitGlobal) {
    std::vector<ZoneSurvey> zones;
    zones.push_back(make_zone("A", 2500.0, 100.0));   // req 276
    zones.push_back(make_zone("B", 2500.0, 100.0));   // req 276

    auto r = CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru());

    EXPECT_FALSE(r.global_compliant);       // 200 < 552
    // deficit_total = ceil(552 - 200) = 352
    EXPECT_DOUBLE_EQ(r.deficit_total_m3min, 352.0);
    EXPECT_FALSE(r.compliant);
}

TEST(CoverageSurvey, ZonaConEstacionesPropagaAdvertencias) {
    ZoneSurvey z;
    z.zone_name = "Galeria 100";
    z.input.zone_type = ZoneType::DevelopmentFace;
    z.input.num_workers = 10;
    z.input.altitude_masl = 1400.0;         // req = 276 (piso de velocidad)
    z.measurement.zone_name = "Galeria 100";
    z.measurement.stations.push_back({"E-1", 16.0, 0.30});  // 288 m³/min; 18 m/min < 20

    std::vector<ZoneSurvey> zones{z};
    auto r = CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru());

    EXPECT_TRUE(r.zones[0].compliant);              // 288 >= 276
    EXPECT_TRUE(r.zones[0].near_deficit_warning);   // 288/276 = 1.043 < 1.10
    ASSERT_EQ(r.zones[0].stations.size(), 1u);
    EXPECT_FALSE(r.zones[0].stations[0].velocity_ok);
    // La advertencia de velocidad de la estación sube al informe de mina
    bool velocity_warned = false;
    for (const auto& w : r.warnings) {
        if (w.find("248") != std::string::npos) velocity_warned = true;
    }
    EXPECT_TRUE(velocity_warned);
}

TEST(CoverageSurvey, ZonaSinDemandaLanzaConMensajeDeZona) {
    // Zona sin trabajadores, flota ni voladura -> q_total = 0. No tiene
    // sentido comparar cobertura contra una demanda nula: debe lanzar con
    // el nombre de la zona en el mensaje para facilitar el diagnóstico.
    ZoneSurvey z;
    z.zone_name = "Zona muerta";
    z.input.zone_type = ZoneType::DevelopmentFace;
    z.input.num_workers = 0;
    z.measurement.zone_name = "Zona muerta";
    z.measurement.q_measured_m3min = 100.0;

    std::vector<ZoneSurvey> zones{z};

    bool threw = false;
    try {
        CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru());
    } catch (const std::invalid_argument& e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("Zona muerta"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

TEST(CoverageSurvey, AdvertenciasDeMargenYSobreVentilacionAgregadas) {
    std::vector<ZoneSurvey> zones;
    zones.push_back(make_zone("Justa", 1400.0, 280.0));    // req 276; 280/276=1.0145 (<1.10)
    zones.push_back(make_zone("Sobrada", 1400.0, 500.0));  // req 276; 500/276=1.812 (>1.5)

    auto r = CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru());

    bool justa_warned = false;
    bool sobrada_warned = false;
    for (const auto& w : r.warnings) {
        if (w.find("Justa") != std::string::npos &&
            w.find("cobertura justa") != std::string::npos) {
            justa_warned = true;
        }
        if (w.find("Sobrada") != std::string::npos &&
            w.find("sobre-ventilada") != std::string::npos) {
            sobrada_warned = true;
        }
    }
    EXPECT_TRUE(justa_warned);
    EXPECT_TRUE(sobrada_warned);
    EXPECT_TRUE(r.compliant);
}

TEST(CoverageSurvey, LevantamientoVacioLanza) {
    EXPECT_THROW(
        CoverageCalculator::analyze_survey({}, RegulatoryConfig::peru()),
        std::invalid_argument);
}

TEST(CoverageSurvey, ZonaGeneralMineLanza) {
    ZoneSurvey z = make_zone("Mina total", 2500.0, 1000.0);
    z.input.zone_type = ZoneType::GeneralMine;   // doble conteo → prohibido
    std::vector<ZoneSurvey> zones{z};
    EXPECT_THROW(
        CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru()),
        std::invalid_argument);
}

TEST(CoverageSurvey, NombreDuplicadoLanza) {
    std::vector<ZoneSurvey> zones;
    zones.push_back(make_zone("Rampa", 2500.0, 250.0));
    zones.push_back(make_zone("Rampa", 4200.0, 250.0));
    EXPECT_THROW(
        CoverageCalculator::analyze_survey(zones, RegulatoryConfig::peru()),
        std::invalid_argument);
}

// ============================================================================
// Límites de velocidad según la norma configurada
// ============================================================================

// {mínimo, máximo} efectivos con CoverageParams por defecto:
//   Perú 20 / 250 (DS 024, Art. 248); con ANFO 25 / 250.
//   Chile 15 / 150 (DS 132, Art. 138 inc. 3).
//   Colombia 15 (criterio de ingeniería: el Decreto 1886 fija mínimas solo
//   para el carbón) / 360 (Art. 56 par. 1).
//   México 15,24 (NOM-023-STPS-2012, 8.4.4 b, con diésel) / sin máximo.
TEST(CoverageVelocityLimits, PorDefectoSiguenALaNorma) {
    const CoverageParams p;
    using L = std::pair<double, double>;
    EXPECT_EQ(CoverageCalculator::velocity_limits(p, RegulatoryConfig::peru()),
              L(20.0, 250.0));
    EXPECT_EQ(CoverageCalculator::velocity_limits(p, RegulatoryConfig::chile()),
              L(15.0, 150.0));
    EXPECT_EQ(CoverageCalculator::velocity_limits(p, RegulatoryConfig::colombia()),
              L(15.0, 360.0));
    const auto mx = CoverageCalculator::velocity_limits(p, RegulatoryConfig::mexico());
    EXPECT_DOUBLE_EQ(mx.first, 15.24);
    EXPECT_TRUE(std::isinf(mx.second));

    CoverageParams anfo;
    anfo.anfo_or_blasting_agents = true;
    EXPECT_EQ(CoverageCalculator::velocity_limits(anfo, RegulatoryConfig::peru()),
              L(25.0, 250.0));
    // El ANFO solo sube el mínimo con el DS 024.
    EXPECT_EQ(CoverageCalculator::velocity_limits(anfo, RegulatoryConfig::colombia()),
              L(15.0, 360.0));
}

// Un valor del usuario solo endurece: el más estricto entre norma y params.
TEST(CoverageVelocityLimits, ParamsSoloEndurecen) {
    CoverageParams laxo;
    laxo.min_velocity_mpm = 10.0;
    laxo.max_velocity_mpm = 400.0;
    EXPECT_EQ(CoverageCalculator::velocity_limits(laxo, RegulatoryConfig::chile()),
              (std::pair<double, double>(15.0, 150.0)));
    CoverageParams estricto;
    estricto.min_velocity_mpm = 30.0;
    estricto.max_velocity_mpm = 120.0;
    EXPECT_EQ(CoverageCalculator::velocity_limits(estricto, RegulatoryConfig::chile()),
              (std::pair<double, double>(30.0, 120.0)));
    // Con Colombia, un mínimo propio reemplaza el de ingeniería.
    EXPECT_EQ(CoverageCalculator::velocity_limits(laxo, RegulatoryConfig::colombia()),
              (std::pair<double, double>(10.0, 360.0)));
    // México sin máximo: el del usuario rige.
    EXPECT_EQ(CoverageCalculator::velocity_limits(laxo, RegulatoryConfig::mexico()),
              (std::pair<double, double>(15.24, 400.0)));
}

// Estaciones: Chile 160 m/min > 150 (Art. 138 inc. 3); Colombia 300 m/min
// es legal (máx. 360) y 20 m/min con ANFO también (sin piso de 25);
// México 400 m/min no tiene máximo y 15,0 < 15,24.
TEST(CoverageVelocityLimits, EstacionesPorNorma) {
    const auto estacion = [](double v_mpm) {
        ZoneMeasurement m;
        m.zone_name = "Z";
        m.stations.push_back({"E-1", 10.0, v_mpm / 60.0});
        return m;
    };
    CoverageParams anfo;
    anfo.anfo_or_blasting_agents = true;

    const auto cl = CoverageCalculator::compare_zone(
        100.0, estacion(160.0), CoverageParams{}, RegulatoryConfig::chile());
    EXPECT_FALSE(cl.stations[0].velocity_ok);
    EXPECT_NE(cl.stations[0].warning.find("Art. 138 inc. 3"), std::string::npos);

    EXPECT_TRUE(CoverageCalculator::compare_zone(
        100.0, estacion(300.0), CoverageParams{}, RegulatoryConfig::colombia())
        .stations[0].velocity_ok);
    const auto co_anfo = CoverageCalculator::compare_zone(
        100.0, estacion(20.0), anfo, RegulatoryConfig::colombia());
    EXPECT_TRUE(co_anfo.stations[0].velocity_ok);

    EXPECT_TRUE(CoverageCalculator::compare_zone(
        100.0, estacion(400.0), CoverageParams{}, RegulatoryConfig::mexico())
        .stations[0].velocity_ok);
    const auto mx = CoverageCalculator::compare_zone(
        100.0, estacion(15.0), CoverageParams{}, RegulatoryConfig::mexico());
    EXPECT_FALSE(mx.stations[0].velocity_ok);
    EXPECT_NE(mx.stations[0].warning.find("sin maximo"), std::string::npos);
    EXPECT_NE(mx.stations[0].warning.find("8.4.4 b)"), std::string::npos);
    EXPECT_EQ(mx.stations[0].warning.find("ANFO"), std::string::npos);
}

// La validación no cita el Art. 248 peruano con otra norma.
TEST(CoverageVelocityLimits, ValidacionSinArticuloPeruano) {
    CoverageParams p;
    p.min_velocity_mpm = 200.0;  // > 150 de Chile
    try {
        (void)CoverageCalculator::velocity_limits(p, RegulatoryConfig::chile());
        FAIL() << "debia lanzar";
    } catch (const std::invalid_argument& e) {
        EXPECT_EQ(std::string(e.what()).find("248"), std::string::npos);
    }
}
