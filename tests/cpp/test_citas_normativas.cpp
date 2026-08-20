/**
 * @file test_citas_normativas.cpp
 * @brief La referencia normativa de cada resultado sigue a la norma configurada.
 *
 * Con el preajuste peruano la cita apunta al DS 024-2016-EM / DS 023-2017-EM.
 * Con el preajuste chileno apunta al DS 132 o declara explícitamente que el
 * DS 132 no fija el valor; en ningún caso aparece la normativa peruana.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ventpy/cobertura.hpp"
#include "ventpy/governor.hpp"
#include "ventpy/normativa.hpp"

using namespace ventpy;

namespace {

/// Marcas de la normativa peruana que jamás deben aparecer bajo marco chileno.
const std::vector<std::string> kMarcasPeru = {
    "DS 024", "DS 023", "DS 015-2005-SA", "Anexo 15", "Anexo 13"
};

/// Devuelve la primera marca peruana hallada, o "" si el texto está limpio.
[[nodiscard]] std::string marca_peruana(const std::string& texto) {
    for (const std::string& m : kMarcasPeru) {
        if (texto.find(m) != std::string::npos) return m;
    }
    return "";
}

[[nodiscard]] bool contiene(const std::string& texto, const std::string& aguja) {
    return texto.find(aguja) != std::string::npos;
}

[[nodiscard]] DieselFleet flota_diesel() {
    DieselFleet f;
    f.add_equipment("Scooptram", 200.0, 0.85, 0.70);
    return f;
}

[[nodiscard]] BlastingParams params_voladura() {
    BlastingParams p;
    p.explosive_kg = 50.0;
    p.dilution_time_min = 45.0;   // > 30 => dispara la ADVERTENCIA
    p.face_area_m2 = 12.0;
    p.face_length_m = 100.0;
    p.gas_volume_per_kg = 0.04;
    return p;
}

[[nodiscard]] DustParams params_polvo() {
    DustParams p;
    p.dust_generation_rate_mg_s = 50.0;
    p.target_concentration_mg_m3 = 4.0;  // > 3 => advertencia de LEO
    p.silica_content_percent = 12.0;     // => remisión de sílice
    p.face_area_m2 = 12.0;
    return p;
}

[[nodiscard]] ThermalParams params_termicos() {
    ThermalParams p;
    p.depth_below_surface_m = 900.0;
    p.auto_compression_c_per_100m = 0.98;
    p.heat_from_equipment_kw = 400.0;
    p.heat_from_oxidation_kw = 50.0;
    p.target_effective_temp_c = 28.0;
    p.face_area_m2 = 12.0;
    return p;
}

[[nodiscard]] AtmosphericParams atmosfera_2500_16c() {
    AtmosphericParams a;
    a.altitude_masl = 2500.0;
    a.dry_bulb_temp_c = 16.0;
    return a;
}

[[nodiscard]] VentilationInput entrada_completa() {
    VentilationInput in;
    in.zone_type = ZoneType::DevelopmentFace;
    in.face_area_m2 = 12.0;
    in.face_length_m = 100.0;
    in.num_workers = 15;
    in.atmospheric = atmosfera_2500_16c();
    in.diesel_fleet = flota_diesel();
    in.blasting_params = params_voladura();
    in.dust_params = params_polvo();
    in.thermal_params = params_termicos();
    return in;
}

/// Toda cadena de auditoría alcanzable desde un resultado del Governor.
[[nodiscard]] std::vector<std::string> textos_auditables(
    const VentilationDemandResult& d
) {
    std::vector<std::string> t = d.warnings;
    if (d.personnel) t.push_back(d.personnel->regulation_ref);
    if (d.diesel)    t.push_back(d.diesel->regulation_ref);
    if (d.blasting)  t.push_back(d.blasting->regulation_ref);
    if (d.dust) {
        t.push_back(d.dust->regulation_ref);
        t.insert(t.end(), d.dust->warnings.begin(), d.dust->warnings.end());
    }
    if (d.thermal) {
        t.push_back(d.thermal->regulation_ref);
        t.insert(t.end(), d.thermal->warnings.begin(), d.thermal->warnings.end());
    }
    if (d.leakage) t.push_back(d.leakage->notes);
    return t;
}

}  // namespace

// ---------------------------------------------------------------------------
// Helper compartido
// ---------------------------------------------------------------------------

TEST(CitasNormativas, HelperSigueALaNorma) {
    EXPECT_EQ(regulation_reference(RegulatoryTopic::PersonnelFlow,
                                   RegulatoryStandard::DS024_Peru),
              "DS 024-2016-EM, Art. 236");
    EXPECT_EQ(regulation_reference(RegulatoryTopic::PersonnelFlow,
                                   RegulatoryStandard::DS132_Chile),
              "DS 132, Art. 138");
    EXPECT_EQ(regulation_reference(RegulatoryTopic::DieselHpFactor,
                                   RegulatoryStandard::DS132_Chile),
              "DS 132, Art. 132");
}

TEST(CitasNormativas, HelperAceptaLaConfiguracion) {
    EXPECT_EQ(regulation_reference(RegulatoryTopic::PersonnelFlow,
                                   RegulatoryConfig::chile()),
              "DS 132, Art. 138");
}

// El DS 132 fija 3,0 m³/min sin escalón por altitud (Art. 138): no hay
// cláusula de escala que citar, y la ausencia es la respuesta correcta.
TEST(CitasNormativas, ChileNoTieneEscalaDeAltitud) {
    EXPECT_EQ(regulation_reference(RegulatoryTopic::PersonnelAltitudeScale,
                                   RegulatoryStandard::DS132_Chile),
              "");
    EXPECT_FALSE(regulation_reference(RegulatoryTopic::PersonnelAltitudeScale,
                                      RegulatoryStandard::DS024_Peru).empty());
}

TEST(CitasNormativas, NingunaCitaChilenaMencionaNormativaPeruana) {
    const RegulatoryTopic temas[] = {
        RegulatoryTopic::PersonnelFlow,
        RegulatoryTopic::PersonnelAltitudeScale,
        RegulatoryTopic::DieselHpFactor,
        RegulatoryTopic::BlastingDilution,
        RegulatoryTopic::BlastingDilutionTimeLimit,
        RegulatoryTopic::DustRespirableLimit,
        RegulatoryTopic::DustLimitBasis,
        RegulatoryTopic::DustSilicaReferral,
        RegulatoryTopic::ThermalBalance,
        RegulatoryTopic::ThermalWithVelocityFloor,
        RegulatoryTopic::ThermalInfeasibleWithFloor,
        RegulatoryTopic::ThermalInfeasibleNoFloor,
        RegulatoryTopic::ThermalStressReferral,
        RegulatoryTopic::AirVelocityLimits,
        RegulatoryTopic::CoverageZone,
        RegulatoryTopic::CoverageMine
    };
    for (RegulatoryTopic t : temas) {
        const std::string ref =
            regulation_reference(t, RegulatoryStandard::DS132_Chile);
        EXPECT_EQ(marca_peruana(ref), "")
            << "cita chilena con normativa peruana: " << ref;
    }
}

// ---------------------------------------------------------------------------
// Cita por calculador
// ---------------------------------------------------------------------------

TEST(CitasNormativas, PersonalPeruYChile) {
    const auto peru = PersonnelFlowCalculator::calculate(
        15, 4200.0, RegulatoryConfig::peru());
    const auto chile = PersonnelFlowCalculator::calculate(
        15, 4200.0, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 236"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132, Art. 138"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
    EXPECT_FALSE(contiene(chile.regulation_ref, "247"));
}

TEST(CitasNormativas, PersonalCompletoConEscalaDeAltitud) {
    PersonnelParams per;
    per.num_workers = 15;
    const auto atm = [] { AtmosphericParams a; a.altitude_masl = 4200.0; return a; }();

    const auto peru = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 12.0, RegulatoryConfig::peru());
    const auto chile = PersonnelFlowCalculator::calculate_full(
        per, atm, ZoneType::DevelopmentFace, 12.0, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "Art. 247 escala altitud"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132, Art. 138"));
    EXPECT_FALSE(contiene(chile.regulation_ref, "247"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
}

TEST(CitasNormativas, DieselPeruYChile) {
    const auto peru = DieselFlowCalculator::calculate(
        flota_diesel(), RegulatoryConfig::peru());
    const auto chile = DieselFlowCalculator::calculate(
        flota_diesel(), RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 246"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132, Art. 132"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
}

TEST(CitasNormativas, DieselCompletoPeruYChile) {
    const auto atm = atmosfera_2500_16c();
    const auto peru = DieselFlowCalculator::calculate_full(
        flota_diesel(), atm, 0.85, RegulatoryConfig::peru());
    const auto chile = DieselFlowCalculator::calculate_full(
        flota_diesel(), atm, 0.85, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 246"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132, Art. 132"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
}

TEST(CitasNormativas, ExplosivosChileCitaReingresoYDeclaraVacio) {
    const auto peru = BlastingFlowCalculator::calculate(
        params_voladura(), RegulatoryConfig::peru());
    const auto chile = BlastingFlowCalculator::calculate(
        params_voladura(), RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 243-244"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132, Arts. 156, 571 y 585"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "no fija tiempo de dilucion"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
}

// El tope de 30 min es normativo en Perú (Art. 243) y meramente configurado
// en Chile: la advertencia no puede llamarlo "normativo" bajo el DS 132.
TEST(CitasNormativas, ExplosivosAdvertenciaDeTiempoPorNorma) {
    const auto peru = BlastingFlowCalculator::calculate(
        params_voladura(), RegulatoryConfig::peru());
    const auto chile = BlastingFlowCalculator::calculate(
        params_voladura(), RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "ADVERTENCIA"));
    EXPECT_TRUE(contiene(peru.regulation_ref, "max normativo"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "ADVERTENCIA"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "max configurado"));
    EXPECT_FALSE(contiene(chile.regulation_ref, "max normativo"));
}

TEST(CitasNormativas, PolvoChileDeclaraVacio) {
    const auto peru = DustFlowCalculator::calculate(
        params_polvo(), RegulatoryConfig::peru());
    const auto chile = DustFlowCalculator::calculate(
        params_polvo(), RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 111"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(chile.regulation_ref,
                         "sin limite de polvo respirable verificado"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
    EXPECT_DOUBLE_EQ(peru.q_dust, chile.q_dust);  // la correccion es de cita
}

TEST(CitasNormativas, PolvoAdvertenciasChilenasSinArticuloPeruano) {
    const auto chile = DustFlowCalculator::calculate(
        params_polvo(), RegulatoryConfig::chile());
    ASSERT_FALSE(chile.warnings.empty());
    for (const std::string& w : chile.warnings) {
        EXPECT_EQ(marca_peruana(w), "") << w;
        EXPECT_FALSE(contiene(w, "Art. 111")) << w;
    }
}

TEST(CitasNormativas, TermicoChileDeclaraCriterioDeIngenieria) {
    const auto atm = atmosfera_2500_16c();
    const auto peru = ThermalFlowCalculator::calculate(
        params_termicos(), atm, RegulatoryConfig::peru());
    const auto chile = ThermalFlowCalculator::calculate(
        params_termicos(), atm, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM, Art. 252.d"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "criterio de ingenieria"));
    EXPECT_FALSE(contiene(chile.regulation_ref, "252"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
    EXPECT_DOUBLE_EQ(peru.q_thermal, chile.q_thermal);  // el piso no cambia
}

// Rama infactible por autocompresión, con el piso de velocidad exigible.
TEST(CitasNormativas, TermicoInfactibleConPisoPorNorma) {
    auto p = params_termicos();
    p.depth_below_surface_m = 1000.0;
    AtmosphericParams a;
    a.altitude_masl = 2500.0;
    a.dry_bulb_temp_c = 18.0;  // inlet 27.8 en [24,29]

    const auto peru = ThermalFlowCalculator::calculate(
        p, a, RegulatoryConfig::peru());
    const auto chile = ThermalFlowCalculator::calculate(
        p, a, RegulatoryConfig::chile());

    EXPECT_DOUBLE_EQ(peru.q_thermal, 360.0);
    EXPECT_DOUBLE_EQ(chile.q_thermal, 360.0);
    EXPECT_TRUE(contiene(peru.regulation_ref, "252"));
    EXPECT_FALSE(contiene(chile.regulation_ref, "252"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
}

// Rama infactible sin piso de velocidad aplicable (face_area = 0) y remisión
// a estrés térmico: ninguna de las dos puede citar el Anexo 13 peruano.
TEST(CitasNormativas, TermicoInfactibleSinPisoYRemisionPorNorma) {
    auto p = params_termicos();
    p.face_area_m2 = 0.0;
    p.depth_below_surface_m = 1000.0;
    AtmosphericParams a;
    a.altitude_masl = 2500.0;
    a.dry_bulb_temp_c = 25.0;  // inlet 34.8 > 29

    const auto peru = ThermalFlowCalculator::calculate(
        p, a, RegulatoryConfig::peru());
    const auto chile = ThermalFlowCalculator::calculate(
        p, a, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "DS 024-2016-EM"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");

    bool peru_cita_104 = false;
    for (const std::string& w : peru.warnings) {
        if (contiene(w, "Art. 104")) peru_cita_104 = true;
    }
    EXPECT_TRUE(peru_cita_104);

    bool chile_remite = false;
    for (const std::string& w : chile.warnings) {
        EXPECT_EQ(marca_peruana(w), "") << w;
        EXPECT_FALSE(contiene(w, "Art. 104")) << w;
        if (contiene(w, "estres termico")) chile_remite = true;
    }
    EXPECT_TRUE(chile_remite);
}

TEST(CitasNormativas, CoberturaZonaPorNorma) {
    ZoneMeasurement m;
    m.zone_name = "Frente N-02";
    m.q_measured_m3min = 90.0;

    const auto peru = CoverageCalculator::compare_zone(
        100.0, m, CoverageParams{}, RegulatoryConfig::peru());
    const auto chile = CoverageCalculator::compare_zone(
        100.0, m, CoverageParams{}, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "Art. 252 lit. g"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "criterio de ingenieria"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
    EXPECT_DOUBLE_EQ(peru.deficit_m3min, chile.deficit_m3min);
}

// Compatibilidad: sin configuración explícita rige el preset peruano, igual
// que el constructor por defecto de RegulatoryConfig.
TEST(CitasNormativas, CoberturaZonaSinConfigUsaPeru) {
    ZoneMeasurement m;
    m.zone_name = "Frente N-02";
    m.q_measured_m3min = 90.0;
    const auto r = CoverageCalculator::compare_zone(100.0, m);
    EXPECT_TRUE(contiene(r.regulation_ref, "DS 024-2016-EM"));
}

TEST(CitasNormativas, EstacionFueraDeRangoPorNorma) {
    ZoneMeasurement m;
    m.zone_name = "Frente N-02";
    m.stations.push_back({"E-1", 12.0, 0.30});  // 18 m/min

    const auto peru = CoverageCalculator::compare_zone(
        100.0, m, CoverageParams{}, RegulatoryConfig::peru());
    const auto chile = CoverageCalculator::compare_zone(
        100.0, m, CoverageParams{}, RegulatoryConfig::chile());

    ASSERT_FALSE(peru.stations[0].velocity_ok);
    ASSERT_FALSE(chile.stations[0].velocity_ok);
    EXPECT_TRUE(contiene(peru.stations[0].warning, "Art. 248"));
    EXPECT_FALSE(contiene(chile.stations[0].warning, "248"));
    EXPECT_EQ(marca_peruana(chile.stations[0].warning), "");
}

TEST(CitasNormativas, BalanceDeMinaPorNorma) {
    ZoneSurvey z;
    z.zone_name = "Frente N-02";
    z.input.zone_type = ZoneType::DevelopmentFace;
    z.input.num_workers = 10;
    z.input.altitude_masl = 4200.0;
    z.measurement.zone_name = "Frente N-02";
    z.measurement.q_measured_m3min = 1000.0;

    const auto peru = CoverageCalculator::analyze_survey(
        {z}, RegulatoryConfig::peru());
    const auto chile = CoverageCalculator::analyze_survey(
        {z}, RegulatoryConfig::chile());

    EXPECT_TRUE(contiene(peru.regulation_ref, "Art. 252"));
    EXPECT_TRUE(contiene(chile.regulation_ref, "DS 132"));
    EXPECT_EQ(marca_peruana(chile.regulation_ref), "");
    EXPECT_EQ(marca_peruana(chile.zones[0].regulation_ref), "");
}

// ---------------------------------------------------------------------------
// Red transversal: atrapa a un calculador nuevo que reintroduzca el defecto
// ---------------------------------------------------------------------------

TEST(CitasNormativas, BarridoChilenoDelGovernor) {
    const VentilationDemandResult d =
        VentilationGovernor{RegulatoryConfig::chile()}
            .calculateTotalDemand(entrada_completa());

    ASSERT_TRUE(d.personnel.has_value());
    ASSERT_TRUE(d.diesel.has_value());
    ASSERT_TRUE(d.blasting.has_value());
    ASSERT_TRUE(d.dust.has_value());
    ASSERT_TRUE(d.thermal.has_value());

    for (const std::string& texto : textos_auditables(d)) {
        EXPECT_EQ(marca_peruana(texto), "")
            << "texto con normativa peruana bajo marco chileno: " << texto;
    }
}

TEST(CitasNormativas, BarridoPeruanoConservaSuCita) {
    const VentilationDemandResult d =
        VentilationGovernor{RegulatoryConfig::peru()}
            .calculateTotalDemand(entrada_completa());

    EXPECT_TRUE(contiene(d.personnel->regulation_ref, "DS 024"));
    EXPECT_TRUE(contiene(d.diesel->regulation_ref, "DS 024"));
    EXPECT_TRUE(contiene(d.blasting->regulation_ref, "DS 024"));
    EXPECT_TRUE(contiene(d.dust->regulation_ref, "DS 024"));
    EXPECT_TRUE(contiene(d.thermal->regulation_ref, "DS 024"));
}

TEST(CitasNormativas, CadaCitaChilenaNombraSuMarco) {
    const VentilationDemandResult d =
        VentilationGovernor{RegulatoryConfig::chile()}
            .calculateTotalDemand(entrada_completa());

    EXPECT_TRUE(contiene(d.personnel->regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(d.diesel->regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(d.blasting->regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(d.dust->regulation_ref, "DS 132"));
    EXPECT_TRUE(contiene(d.thermal->regulation_ref, "DS 132"));
}
