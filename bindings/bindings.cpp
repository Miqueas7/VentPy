/**
 * @file bindings.cpp
 * @brief Nanobind bindings: expone el core C++ de VentPy a Python.
 *
 * Modulo Python: ventpy._ventpy_core
 *
 * Diseno: Se exponen todas las structs como clases Python con
 * propiedades de solo lectura para resultados, y lectura-escritura
 * para datos de entrada.
 *
 * @copyright 2026 VentPy Project
 */

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "ventpy/governor.hpp"

namespace nb = nanobind;
using namespace ventpy;

NB_MODULE(_ventpy_core, m) {
    m.doc() = "VentPy Core: High-performance mine ventilation calculations.\n"
              "Normativa: DS 024-2016-EM / DS 023-2017-EM (Peru).";

    // ========================================================================
    // Enums
    // ========================================================================
    nb::enum_<ZoneType>(m, "ZoneType",
        "Tipo de zona de calculo segun contexto operativo.")
        .value("DevelopmentFace", ZoneType::DevelopmentFace,
               "Frente de desarrollo (avance, galeria, crucero)")
        .value("Stope", ZoneType::Stope, "Tajeo (explotacion)")
        .value("Ramp", ZoneType::Ramp, "Rampa de acceso")
        .value("GeneralMine", ZoneType::GeneralMine,
               "Mina total (sumatoria global)");

    nb::enum_<RegulatoryStandard>(m, "RegulatoryStandard",
        "Estandar normativo aplicable al calculo.")
        .value("DS024_Peru", RegulatoryStandard::DS024_Peru,
               "DS 024-2016-EM / DS 023-2017-EM (Peru)");

    nb::enum_<ActivityLevel>(m, "ActivityLevel",
        "Nivel de actividad fisica del personal.\n"
        "Afecta consumo de oxigeno y tasa metabolica (ISO 8996).")
        .value("Rest", ActivityLevel::Rest,
               "Descanso, supervision estatica: ~0.3 L O2/min")
        .value("Light", ActivityLevel::Light,
               "Trabajo ligero (operador): ~0.5 L O2/min")
        .value("Moderate", ActivityLevel::Moderate,
               "Trabajo moderado (perforista): ~1.0 L O2/min")
        .value("Heavy", ActivityLevel::Heavy,
               "Trabajo pesado (paleo manual): ~1.5 L O2/min")
        .value("VeryHeavy", ActivityLevel::VeryHeavy,
               "Trabajo muy pesado (rescate): ~2.0 L O2/min");

    nb::enum_<ExplosiveType>(m, "ExplosiveType",
        "Tipo de explosivo para calculo de gases.")
        .value("ANFO", ExplosiveType::ANFO,
               "Nitrato de amonio + fuel oil (estandar)")
        .value("Emulsion", ExplosiveType::Emulsion,
               "Emulsion encartuchada")
        .value("Dynamite", ExplosiveType::Dynamite,
               "Dinamita (mayor CO)")
        .value("WaterGel", ExplosiveType::WaterGel,
               "Hidrogel")
        .value("ElectronicDet", ExplosiveType::ElectronicDet,
               "Solo detonadores electronicos (minimo gas)")
        .value("Custom", ExplosiveType::Custom,
               "Valores personalizados");

    nb::enum_<DuctType>(m, "DuctType",
        "Tipo de ducto/manga de ventilacion.")
        .value("FlexibleFabric", DuctType::FlexibleFabric,
               "Manga flexible de tela (mayor fuga)")
        .value("FlexiblePVC", DuctType::FlexiblePVC,
               "Manga flexible PVC reforzado")
        .value("RigidFiberglass", DuctType::RigidFiberglass,
               "Ducto rigido fibra de vidrio")
        .value("RigidSteel", DuctType::RigidSteel,
               "Ducto rigido acero (menor fuga)")
        .value("SpiralSteel", DuctType::SpiralSteel,
               "Ducto espiral metalico");

    nb::enum_<InstallationQuality>(m, "InstallationQuality",
        "Calidad de instalacion del sistema de ductos.")
        .value("Poor", InstallationQuality::Poor,
               "Instalacion deficiente: fugas ~25-35%")
        .value("Average", InstallationQuality::Average,
               "Instalacion promedio: fugas ~15-25%")
        .value("Good", InstallationQuality::Good,
               "Buena instalacion: fugas ~10-15%")
        .value("Excellent", InstallationQuality::Excellent,
               "Excelente (juntas selladas): fugas ~5-10%");

    nb::enum_<EngineEmissionTier>(m, "EngineEmissionTier",
        "Categoria de emisiones del motor diesel (EPA Tier / EU Stage).")
        .value("Tier0_Unregulated", EngineEmissionTier::Tier0_Unregulated,
               "Sin regulacion (pre-1996)")
        .value("Tier1", EngineEmissionTier::Tier1,
               "Tier 1 (1996-2003)")
        .value("Tier2", EngineEmissionTier::Tier2,
               "Tier 2 (2001-2006)")
        .value("Tier3", EngineEmissionTier::Tier3,
               "Tier 3 (2006-2008)")
        .value("Tier4_Interim", EngineEmissionTier::Tier4_Interim,
               "Tier 4 Interim (2008-2012)")
        .value("Tier4_Final", EngineEmissionTier::Tier4_Final,
               "Tier 4 Final (2012+, DPF obligatorio)");

    // ========================================================================
    // Structs de entrada
    // ========================================================================
    nb::class_<AtmosphericParams>(m, "AtmosphericParams",
        "Parametros atmosfericos de la mina.\n"
        "Critico para correcciones de densidad del aire y rendimiento de equipos.")
        .def(nb::init<>())
        .def_rw("altitude_masl", &AtmosphericParams::altitude_masl,
                "Altitud sobre nivel del mar [msnm]")
        .def_rw("barometric_pressure_kpa", &AtmosphericParams::barometric_pressure_kpa,
                "Presion barometrica [kPa], 0 = calcular")
        .def_rw("dry_bulb_temp_c", &AtmosphericParams::dry_bulb_temp_c,
                "Temperatura bulbo seco [C]")
        .def_rw("wet_bulb_temp_c", &AtmosphericParams::wet_bulb_temp_c,
                "Temperatura bulbo humedo [C]")
        .def_rw("relative_humidity", &AtmosphericParams::relative_humidity,
                "Humedad relativa [0.0 - 1.0]")
        .def("__repr__", [](const AtmosphericParams& p) {
            return "AtmosphericParams(altitude=" + std::to_string(p.altitude_masl) +
                   " msnm, T_db=" + std::to_string(p.dry_bulb_temp_c) + " C)";
        });

    nb::class_<PersonnelParams>(m, "PersonnelParams",
        "Parametros del personal en la zona.")
        .def(nb::init<>())
        .def_rw("num_workers", &PersonnelParams::num_workers,
                "Cantidad de trabajadores")
        .def_rw("activity", &PersonnelParams::activity,
                "Nivel de actividad (ActivityLevel)")
        .def_rw("exposure_hours", &PersonnelParams::exposure_hours,
                "Horas de exposicion por turno")
        .def("__repr__", [](const PersonnelParams& p) {
            return "PersonnelParams(n=" + std::to_string(p.num_workers) + ")";
        });

    nb::class_<DieselEquipment>(m, "DieselEquipment",
        "Informacion de un equipo diesel individual.\n"
        "DS 024-2016-EM, Art. 246.")
        .def(nb::init<>())
        .def_rw("name", &DieselEquipment::name,
                "Identificador del equipo")
        .def_rw("horsepower", &DieselEquipment::horsepower,
                "Potencia nominal del motor [HP]")
        .def_rw("availability", &DieselEquipment::availability,
                "Factor de disponibilidad mecanica [0.0 - 1.0]")
        .def_rw("utilization", &DieselEquipment::utilization,
                "Factor de utilizacion [0.0 - 1.0]")
        .def_rw("emission_tier", &DieselEquipment::emission_tier,
                "Categoria de emisiones EPA")
        .def_rw("fuel_consumption_lph", &DieselEquipment::fuel_consumption_lph,
                "Consumo combustible [L/h], 0 = estimar")
        .def_rw("has_dpf", &DieselEquipment::has_dpf,
                "Tiene filtro de particulas (DPF)?")
        .def_rw("has_doc", &DieselEquipment::has_doc,
                "Tiene catalizador oxidacion (DOC)?")
        .def_rw("co_emission_factor", &DieselEquipment::co_emission_factor,
                "Factor CO [g/kWh], 0 = usar tier")
        .def_rw("nox_emission_factor", &DieselEquipment::nox_emission_factor,
                "Factor NOx [g/kWh], 0 = usar tier")
        .def_rw("pm_emission_factor", &DieselEquipment::pm_emission_factor,
                "Factor PM [g/kWh], 0 = usar tier")
        .def("__repr__", [](const DieselEquipment& e) {
            return "DieselEquipment('" + e.name + "', " +
                   std::to_string(e.horsepower) + " HP, avail=" +
                   std::to_string(e.availability) + ", util=" +
                   std::to_string(e.utilization) + ")";
        });

    nb::class_<BlastingParams>(m, "BlastingParams",
        "Parametros para calculo de caudal por explosivos.\n"
        "DS 024-2016-EM, Art. 243-244.")
        .def(nb::init<>())
        .def_rw("explosive_kg", &BlastingParams::explosive_kg,
                "Cantidad de explosivo por voladura [kg]")
        .def_rw("explosive_type", &BlastingParams::explosive_type,
                "Tipo de explosivo (ExplosiveType)")
        .def_rw("gas_volume_per_kg", &BlastingParams::gas_volume_per_kg,
                "Vol. gases [m3/kg], 0 = usar tipo")
        .def_rw("dilution_time_min", &BlastingParams::dilution_time_min,
                "Tiempo maximo de dilucion [min]")
        .def_rw("face_area_m2", &BlastingParams::face_area_m2,
                "Seccion del frente [m2]")
        .def_rw("face_length_m", &BlastingParams::face_length_m,
                "Longitud hasta el frente [m]")
        .def_rw("co_per_kg_liters", &BlastingParams::co_per_kg_liters,
                "CO generado [L/kg], 0 = usar tipo")
        .def_rw("nox_per_kg_liters", &BlastingParams::nox_per_kg_liters,
                "NOx generado [L/kg], 0 = usar tipo")
        .def_rw("target_co_ppm", &BlastingParams::target_co_ppm,
                "Limite CO objetivo [ppm] (DS024: 25 ppm)")
        .def_rw("target_nox_ppm", &BlastingParams::target_nox_ppm,
                "Limite NOx objetivo [ppm] (DS024: 5 ppm)")
        .def_rw("min_velocity_mps", &BlastingParams::min_velocity_mps,
                "Velocidad minima en el frente [m/s]");

    nb::class_<DuctParams>(m, "DuctParams",
        "Parametros del sistema de ductos.")
        .def(nb::init<>())
        .def_rw("duct_type", &DuctParams::duct_type,
                "Tipo de ducto (DuctType)")
        .def_rw("quality", &DuctParams::quality,
                "Calidad de instalacion (InstallationQuality)")
        .def_rw("duct_diameter_m", &DuctParams::duct_diameter_m,
                "Diametro del ducto [m]")
        .def_rw("duct_length_m", &DuctParams::duct_length_m,
                "Longitud total del ducto [m]")
        .def_rw("num_joints", &DuctParams::num_joints,
                "Numero de juntas/conexiones")
        .def_rw("leakage_per_joint", &DuctParams::leakage_per_joint,
                "Fuga por junta [fraccion]")
        .def_rw("leakage_per_100m", &DuctParams::leakage_per_100m,
                "Fuga por 100m [fraccion], 0 = usar tipo");

    nb::class_<DustParams>(m, "DustParams",
        "Parametros para calculo de dilucion de polvo.\n"
        "DS 024-2016-EM, Art. 103-107.")
        .def(nb::init<>())
        .def_rw("dust_generation_rate_mg_s", &DustParams::dust_generation_rate_mg_s,
                "Tasa generacion polvo [mg/s]")
        .def_rw("silica_content_percent", &DustParams::silica_content_percent,
                "Contenido de silice [%]")
        .def_rw("target_concentration_mg_m3", &DustParams::target_concentration_mg_m3,
                "Limite polvo respirable [mg/m3]")
        .def_rw("face_area_m2", &DustParams::face_area_m2,
                "Seccion para velocidad [m2]")
        .def_rw("water_suppression", &DustParams::water_suppression,
                "Usa supresion con agua?")
        .def_rw("suppression_efficiency", &DustParams::suppression_efficiency,
                "Eficiencia supresion [0-1]");

    nb::class_<ThermalParams>(m, "ThermalParams",
        "Parametros para calculo de carga termica.\n"
        "DS 024-2016-EM, Art. 240: Temperatura efectiva maxima 30C.")
        .def(nb::init<>())
        .def_rw("virgin_rock_temp_c", &ThermalParams::virgin_rock_temp_c,
                "Temperatura roca virgen [C]")
        .def_rw("geothermal_gradient_c_per_100m", &ThermalParams::geothermal_gradient_c_per_100m,
                "Gradiente geotermico [C/100m]")
        .def_rw("depth_below_surface_m", &ThermalParams::depth_below_surface_m,
                "Profundidad [m]")
        .def_rw("auto_compression_c_per_100m", &ThermalParams::auto_compression_c_per_100m,
                "Autocompresion [C/100m]")
        .def_rw("heat_from_equipment_kw", &ThermalParams::heat_from_equipment_kw,
                "Calor de equipos [kW]")
        .def_rw("heat_from_oxidation_kw", &ThermalParams::heat_from_oxidation_kw,
                "Calor de oxidacion mineral [kW]")
        .def_rw("target_effective_temp_c", &ThermalParams::target_effective_temp_c,
                "Temperatura efectiva objetivo [C]")
        .def_rw("face_area_m2", &ThermalParams::face_area_m2,
                "Seccion de la labor [m2]");

    // ========================================================================
    // Structs de resultado
    // ========================================================================
    nb::class_<AtmosphericCorrections>(m, "AtmosphericCorrections",
        "Correcciones atmosfericas calculadas.")
        .def_ro("altitude_masl", &AtmosphericCorrections::altitude_masl)
        .def_ro("pressure_kpa", &AtmosphericCorrections::pressure_kpa)
        .def_ro("density_ratio", &AtmosphericCorrections::density_ratio)
        .def_ro("oxygen_partial_pressure_kpa", &AtmosphericCorrections::oxygen_partial_pressure_kpa)
        .def_ro("air_density_kg_m3", &AtmosphericCorrections::air_density_kg_m3)
        .def_ro("volume_correction_factor", &AtmosphericCorrections::volume_correction_factor)
        .def_ro("notes", &AtmosphericCorrections::notes);

    nb::class_<PersonnelFlowResult>(m, "PersonnelFlowResult",
        "Resultado detallado del calculo de caudal por personal.")
        .def_ro("num_workers", &PersonnelFlowResult::num_workers)
        .def_ro("activity_level", &PersonnelFlowResult::activity_level)
        .def_ro("altitude_masl", &PersonnelFlowResult::altitude_masl)
        .def_ro("density_correction", &PersonnelFlowResult::density_correction)
        .def_ro("o2_consumption_lpm", &PersonnelFlowResult::o2_consumption_lpm)
        .def_ro("flow_per_person_base", &PersonnelFlowResult::flow_per_person_base)
        .def_ro("flow_per_person_corrected", &PersonnelFlowResult::flow_per_person_corrected)
        .def_ro("q_personnel", &PersonnelFlowResult::q_personnel)
        .def_ro("min_velocity_check_mps", &PersonnelFlowResult::min_velocity_check_mps)
        .def_ro("regulation_ref", &PersonnelFlowResult::regulation_ref);

    nb::class_<DieselFlowResult>(m, "DieselFlowResult",
        "Resultado detallado del calculo de caudal por equipo diesel.")
        .def_ro("equipment_names", &DieselFlowResult::equipment_names)
        .def_ro("hp_factor_base", &DieselFlowResult::hp_factor_base)
        .def_ro("hp_factor_corrected", &DieselFlowResult::hp_factor_corrected)
        .def_ro("altitude_derate_factor", &DieselFlowResult::altitude_derate_factor)
        .def_ro("total_rated_hp", &DieselFlowResult::total_rated_hp)
        .def_ro("total_effective_hp", &DieselFlowResult::total_effective_hp)
        .def_ro("total_derated_hp", &DieselFlowResult::total_derated_hp)
        .def_ro("q_for_co_dilution", &DieselFlowResult::q_for_co_dilution)
        .def_ro("q_for_nox_dilution", &DieselFlowResult::q_for_nox_dilution)
        .def_ro("q_for_pm_dilution", &DieselFlowResult::q_for_pm_dilution)
        .def_ro("q_diesel", &DieselFlowResult::q_diesel)
        .def_ro("co_emission_total_g_min", &DieselFlowResult::co_emission_total_g_min)
        .def_ro("nox_emission_total_g_min", &DieselFlowResult::nox_emission_total_g_min)
        .def_ro("regulation_ref", &DieselFlowResult::regulation_ref);

    nb::class_<BlastingFlowResult>(m, "BlastingFlowResult",
        "Resultado detallado del calculo de caudal por explosivos.")
        .def_ro("explosive_type", &BlastingFlowResult::explosive_type)
        .def_ro("explosive_kg", &BlastingFlowResult::explosive_kg)
        .def_ro("co_generated_liters", &BlastingFlowResult::co_generated_liters)
        .def_ro("nox_generated_liters", &BlastingFlowResult::nox_generated_liters)
        .def_ro("total_gas_volume_m3", &BlastingFlowResult::total_gas_volume_m3)
        .def_ro("face_volume_m3", &BlastingFlowResult::face_volume_m3)
        .def_ro("dilution_time_min", &BlastingFlowResult::dilution_time_min)
        .def_ro("q_for_co_dilution", &BlastingFlowResult::q_for_co_dilution)
        .def_ro("q_for_nox_dilution", &BlastingFlowResult::q_for_nox_dilution)
        .def_ro("q_for_volume_exchange", &BlastingFlowResult::q_for_volume_exchange)
        .def_ro("q_for_min_velocity", &BlastingFlowResult::q_for_min_velocity)
        .def_ro("q_blasting", &BlastingFlowResult::q_blasting)
        .def_ro("governing_criterion", &BlastingFlowResult::governing_criterion)
        .def_ro("regulation_ref", &BlastingFlowResult::regulation_ref);

    nb::class_<LeakageFlowResult>(m, "LeakageFlowResult",
        "Resultado detallado del calculo de fugas.")
        .def_ro("duct_type", &LeakageFlowResult::duct_type)
        .def_ro("quality", &LeakageFlowResult::quality)
        .def_ro("duct_length_m", &LeakageFlowResult::duct_length_m)
        .def_ro("duct_diameter_m", &LeakageFlowResult::duct_diameter_m)
        .def_ro("num_joints", &LeakageFlowResult::num_joints)
        .def_ro("base_leakage_factor", &LeakageFlowResult::base_leakage_factor)
        .def_ro("length_leakage_factor", &LeakageFlowResult::length_leakage_factor)
        .def_ro("joint_leakage_factor", &LeakageFlowResult::joint_leakage_factor)
        .def_ro("total_leakage_factor", &LeakageFlowResult::total_leakage_factor)
        .def_ro("base_flow", &LeakageFlowResult::base_flow)
        .def_ro("q_leakage", &LeakageFlowResult::q_leakage)
        .def_ro("q_at_fan", &LeakageFlowResult::q_at_fan)
        .def_ro("notes", &LeakageFlowResult::notes);

    nb::class_<DustFlowResult>(m, "DustFlowResult",
        "Resultado del calculo de dilucion de polvo.")
        .def_ro("dust_generation_mg_s", &DustFlowResult::dust_generation_mg_s)
        .def_ro("target_concentration", &DustFlowResult::target_concentration)
        .def_ro("suppression_efficiency", &DustFlowResult::suppression_efficiency)
        .def_ro("effective_generation", &DustFlowResult::effective_generation)
        .def_ro("q_dust", &DustFlowResult::q_dust)
        .def_ro("resulting_velocity_mps", &DustFlowResult::resulting_velocity_mps)
        .def_ro("regulation_ref", &DustFlowResult::regulation_ref);

    nb::class_<ThermalFlowResult>(m, "ThermalFlowResult",
        "Resultado del calculo de carga termica.")
        .def_ro("heat_from_rock_kw", &ThermalFlowResult::heat_from_rock_kw)
        .def_ro("heat_from_equipment_kw", &ThermalFlowResult::heat_from_equipment_kw)
        .def_ro("heat_from_autocompression_kw", &ThermalFlowResult::heat_from_autocompression_kw)
        .def_ro("heat_from_other_kw", &ThermalFlowResult::heat_from_other_kw)
        .def_ro("total_heat_load_kw", &ThermalFlowResult::total_heat_load_kw)
        .def_ro("inlet_temp_c", &ThermalFlowResult::inlet_temp_c)
        .def_ro("target_temp_c", &ThermalFlowResult::target_temp_c)
        .def_ro("delta_t_available", &ThermalFlowResult::delta_t_available)
        .def_ro("q_thermal", &ThermalFlowResult::q_thermal)
        .def_ro("resulting_velocity_mps", &ThermalFlowResult::resulting_velocity_mps)
        .def_ro("regulation_ref", &ThermalFlowResult::regulation_ref);

    nb::class_<VentilationDemandResult>(m, "VentilationDemandResult",
        "Resultado consolidado de demanda de ventilacion.\n"
        "Estructura de auditoria con desglose completo.")
        .def_ro("zone_type", &VentilationDemandResult::zone_type)
        .def_ro("standard", &VentilationDemandResult::standard)
        // Sub-results (optional)
        .def_ro("atmospheric", &VentilationDemandResult::atmospheric)
        .def_ro("personnel", &VentilationDemandResult::personnel)
        .def_ro("diesel", &VentilationDemandResult::diesel)
        .def_ro("blasting", &VentilationDemandResult::blasting)
        .def_ro("dust", &VentilationDemandResult::dust)
        .def_ro("thermal", &VentilationDemandResult::thermal)
        .def_ro("leakage", &VentilationDemandResult::leakage)
        // Individual flows
        .def_ro("q_personnel_m3min", &VentilationDemandResult::q_personnel_m3min)
        .def_ro("q_diesel_m3min", &VentilationDemandResult::q_diesel_m3min)
        .def_ro("q_blasting_m3min", &VentilationDemandResult::q_blasting_m3min)
        .def_ro("q_dust_m3min", &VentilationDemandResult::q_dust_m3min)
        .def_ro("q_thermal_m3min", &VentilationDemandResult::q_thermal_m3min)
        .def_ro("q_leakage_m3min", &VentilationDemandResult::q_leakage_m3min)
        // Final flows
        .def_ro("q_governing_m3min", &VentilationDemandResult::q_governing_m3min)
        .def_ro("q_at_fan_m3min", &VentilationDemandResult::q_at_fan_m3min)
        .def_ro("q_total_m3min", &VentilationDemandResult::q_total_m3min)
        .def_ro("q_total_m3s", &VentilationDemandResult::q_total_m3s)
        .def_ro("q_total_cfm", &VentilationDemandResult::q_total_cfm)
        // Velocity check
        .def_ro("face_area_m2", &VentilationDemandResult::face_area_m2)
        .def_ro("velocity_at_face_mps", &VentilationDemandResult::velocity_at_face_mps)
        .def_ro("velocity_ok", &VentilationDemandResult::velocity_ok)
        // Metadata
        .def_ro("safety_factor_applied", &VentilationDemandResult::safety_factor_applied)
        .def_ro("governing_factor", &VentilationDemandResult::governing_factor)
        .def_ro("notes", &VentilationDemandResult::notes)
        .def_ro("warnings", &VentilationDemandResult::warnings);

    // ========================================================================
    // DieselFleet
    // ========================================================================
    nb::class_<DieselFleet>(m, "DieselFleet",
        "Flota de equipos diesel para un sector de la mina.")
        .def(nb::init<>())
        .def("add_equipment",
             nb::overload_cast<const DieselEquipment&>(&DieselFleet::add_equipment),
             nb::arg("equipment"),
             "Agrega un equipo diesel a la flota.")
        .def("add_equipment",
             nb::overload_cast<const std::string&, double, double, double>(
                 &DieselFleet::add_equipment),
             nb::arg("name"), nb::arg("hp"),
             nb::arg("availability"), nb::arg("utilization"),
             "Agrega un equipo con parametros directos.")
        .def("size", &DieselFleet::size,
             "Cantidad de equipos en la flota.")
        .def("empty", &DieselFleet::empty,
             "Verifica si la flota esta vacia.")
        .def("clear", &DieselFleet::clear,
             "Limpia la flota.");

    // ========================================================================
    // RegulatoryConfig
    // ========================================================================
    nb::class_<RegulatoryConfig>(m, "RegulatoryConfig",
        "Configuracion normativa inmutable para calculo de ventilacion.\n"
        "DS 024-2016-EM / DS 023-2017-EM (Peru).")
        .def(nb::init<
                RegulatoryStandard, double, double, double,
                double, double, double, double, double, double>(),
             nb::arg("standard") = RegulatoryStandard::DS024_Peru,
             nb::arg("min_flow_per_person_m3min") = 3.0,
             nb::arg("altitude_threshold_1_masl") = 3000.0,
             nb::arg("flow_per_person_above_t1") = 4.0,
             nb::arg("altitude_threshold_2_masl") = 4000.0,
             nb::arg("flow_per_person_above_t2") = 5.0,
             nb::arg("diesel_hp_factor_m3min") = 3.0,
             nb::arg("max_dilution_time_min") = 30.0,
             nb::arg("default_gas_volume_per_kg_m3") = 0.04,
             nb::arg("default_leakage_factor") = 0.15)
        .def_prop_ro("standard", &RegulatoryConfig::standard)
        .def_prop_ro("min_flow_per_person", &RegulatoryConfig::min_flow_per_person)
        .def_prop_ro("altitude_threshold_1", &RegulatoryConfig::altitude_threshold_1)
        .def_prop_ro("flow_above_threshold_1", &RegulatoryConfig::flow_above_threshold_1)
        .def_prop_ro("altitude_threshold_2", &RegulatoryConfig::altitude_threshold_2)
        .def_prop_ro("flow_above_threshold_2", &RegulatoryConfig::flow_above_threshold_2)
        .def_prop_ro("diesel_hp_factor", &RegulatoryConfig::diesel_hp_factor)
        .def_prop_ro("max_dilution_time", &RegulatoryConfig::max_dilution_time)
        .def_prop_ro("default_gas_volume_per_kg", &RegulatoryConfig::default_gas_volume_per_kg)
        .def_prop_ro("default_leakage_factor", &RegulatoryConfig::default_leakage_factor)
        .def_prop_ro("standard_name", &RegulatoryConfig::standard_name);

    // ========================================================================
    // VentilationInput
    // ========================================================================
    nb::class_<VentilationInput>(m, "VentilationInput",
        "Parametros de entrada para el calculo completo de un frente/zona.")
        .def(nb::init<>())
        .def_rw("zone_type", &VentilationInput::zone_type,
                "Tipo de zona (ZoneType)")
        .def_rw("atmospheric", &VentilationInput::atmospheric,
                "Parametros atmosfericos (AtmosphericParams)")
        .def_rw("face_area_m2", &VentilationInput::face_area_m2,
                "Seccion de la labor [m2]")
        .def_rw("face_length_m", &VentilationInput::face_length_m,
                "Longitud de la labor [m]")
        .def_rw("personnel", &VentilationInput::personnel,
                "Parametros de personal (PersonnelParams)")
        .def_rw("diesel_fleet", &VentilationInput::diesel_fleet,
                "Flota de equipos diesel (DieselFleet)")
        .def_rw("simultaneity_factor", &VentilationInput::simultaneity_factor,
                "Factor de simultaneidad [0-1]")
        .def_rw("blasting_params", &VentilationInput::blasting_params,
                "Parametros de voladura (BlastingParams)")
        .def_rw("duct_params", &VentilationInput::duct_params,
                "Parametros de ductos (DuctParams)")
        .def_rw("leakage_factor", &VentilationInput::leakage_factor,
                "Factor de fugas simple (si no se usa duct_params)")
        .def_rw("safety_factor", &VentilationInput::safety_factor,
                "Factor de seguridad adicional [1.0 - 2.0]")
        .def_rw("notes", &VentilationInput::notes,
                "Notas del ingeniero")
        // Backwards compatibility
        .def_rw("num_workers", &VentilationInput::num_workers,
                "(Deprecated) Usar personnel.num_workers")
        .def_rw("altitude_masl", &VentilationInput::altitude_masl,
                "(Deprecated) Usar atmospheric.altitude_masl");

    // ========================================================================
    // VentilationGovernor (API principal)
    // ========================================================================
    nb::class_<VentilationGovernor>(m, "VentilationGovernor",
        "Motor de calculo de demanda de ventilacion.\n\n"
        "API principal de VentPy. Ejemplo de uso:\n\n"
        "    config = ventpy.RegulatoryConfig()\n"
        "    governor = ventpy.VentilationGovernor(config)\n"
        "    input = ventpy.VentilationInput()\n"
        "    input.num_workers = 15\n"
        "    input.altitude_masl = 4200.0\n"
        "    result = governor.calculate_total_demand(input)\n"
        "    print(f'Q_total = {result.q_total_m3min} m3/min')\n")
        .def(nb::init<const RegulatoryConfig&>(),
             nb::arg("config"),
             "Construye el Governor con una configuracion normativa.")
        .def("calculate_total_demand",
             &VentilationGovernor::calculateTotalDemand,
             nb::arg("input"),
             "Calcula la demanda total de ventilacion para una zona.\n"
             "Retorna VentilationDemandResult con desglose completo.")
        .def_prop_ro("config", &VentilationGovernor::config,
             "Configuracion normativa actual (solo lectura).");

    // ========================================================================
    // AtmosphereCalculator (acceso directo)
    // ========================================================================
    m.def("calculate_pressure_kpa",
          &AtmosphereCalculator::calculate_pressure_kpa,
          nb::arg("altitude_masl"),
          "Calcula presion atmosferica a una altitud dada [kPa].\n"
          "Modelo ISA (International Standard Atmosphere).");

    m.def("calculate_density_kg_m3",
          &AtmosphereCalculator::calculate_density_kg_m3,
          nb::arg("altitude_masl"), nb::arg("temperature_c") = 15.0,
          "Calcula densidad del aire a una altitud dada [kg/m3].");

    m.def("calculate_density_ratio",
          &AtmosphereCalculator::calculate_density_ratio,
          nb::arg("altitude_masl"), nb::arg("temperature_c") = 15.0,
          "Calcula ratio de densidad respecto a nivel del mar [0-1].");

    m.def("calculate_volume_correction_factor",
          &AtmosphereCalculator::calculate_volume_correction_factor,
          nb::arg("altitude_masl"), nb::arg("temperature_c") = 15.0,
          "Calcula factor de correccion volumetrica.\n"
          "A mayor altitud, mas volumen de aire necesario.");

    m.def("calculate_o2_partial_pressure_kpa",
          &AtmosphereCalculator::calculate_o2_partial_pressure_kpa,
          nb::arg("altitude_masl"),
          "Calcula presion parcial de O2 [kPa].\n"
          "DS 024-2016-EM, Art. 236: Minimo 19.5% O2.");

    m.def("calculate_diesel_derate_factor",
          &AtmosphereCalculator::calculate_diesel_derate_factor,
          nb::arg("altitude_masl"), nb::arg("is_turbocharged") = true,
          "Calcula factor de de-rating para motores diesel.\n"
          "Motores turbo: ~1.5% perdida por cada 300m sobre 1000m.");

    m.def("calculate_atmospheric_corrections",
          &AtmosphereCalculator::calculate_all,
          nb::arg("params"),
          "Calcula todas las correcciones atmosfericas.\n"
          "Retorna AtmosphericCorrections para auditoria.");

    // ========================================================================
    // Calculadores individuales (acceso directo avanzado)
    // ========================================================================
    m.def("calculate_personnel_flow",
          &PersonnelFlowCalculator::calculate,
          nb::arg("num_workers"), nb::arg("altitude_masl"),
          nb::arg("config"),
          "Calcula Q_Per (caudal por personal) - version simple.\n"
          "DS 024-2016-EM, Art. 236.");

    m.def("calculate_diesel_flow",
          &DieselFlowCalculator::calculate,
          nb::arg("fleet"), nb::arg("config"),
          "Calcula Q_Eq (caudal por equipos diesel) - version simple.\n"
          "DS 024-2016-EM, Art. 246.");

    m.def("calculate_blasting_flow",
          &BlastingFlowCalculator::calculate,
          nb::arg("params"), nb::arg("config"),
          "Calcula Q_Exp (caudal por explosivos) - version simple.\n"
          "DS 024-2016-EM, Art. 243-244.");

    // ========================================================================
    // Constantes normativas DS 024-2016-EM
    // ========================================================================
    auto constants_mod = m.def_submodule("constants",
        "Constantes normativas y de conversion.\n"
        "DS 024-2016-EM (Peru).");

    // Conversiones
    constants_mod.attr("M3MIN_TO_CFM") = constants::M3MIN_TO_CFM;
    constants_mod.attr("M3MIN_TO_M3S") = constants::M3MIN_TO_M3S;
    constants_mod.attr("CFM_TO_M3MIN") = constants::CFM_TO_M3MIN;
    constants_mod.attr("HP_TO_KW") = constants::HP_TO_KW;
    constants_mod.attr("KW_TO_HP") = constants::KW_TO_HP;

    // Atmosfera
    constants_mod.attr("SEA_LEVEL_PRESSURE_KPA") = constants::SEA_LEVEL_PRESSURE_KPA;
    constants_mod.attr("SEA_LEVEL_DENSITY_KG_M3") = constants::SEA_LEVEL_DENSITY_KG_M3;
    constants_mod.attr("O2_FRACTION_AIR") = constants::O2_FRACTION_AIR;

    // TLV (Limites de exposicion ocupacional)
    constants_mod.attr("TLV_CO_PPM") = constants::TLV_CO_PPM;
    constants_mod.attr("TLV_NO2_PPM") = constants::TLV_NO2_PPM;
    constants_mod.attr("TLV_NO_PPM") = constants::TLV_NO_PPM;
    constants_mod.attr("TLV_SO2_PPM") = constants::TLV_SO2_PPM;
    constants_mod.attr("TLV_H2S_PPM") = constants::TLV_H2S_PPM;
    constants_mod.attr("TLV_DUST_RESPIRABLE_MG_M3") = constants::TLV_DUST_RESPIRABLE_MG_M3;
    constants_mod.attr("MIN_O2_PERCENT") = constants::MIN_O2_PERCENT;
    constants_mod.attr("MAX_EFFECTIVE_TEMP_C") = constants::MAX_EFFECTIVE_TEMP_C;

    // Velocidades minimas
    constants_mod.attr("MIN_VELOCITY_DEVELOPMENT_MPS") = constants::MIN_VELOCITY_DEVELOPMENT_MPS;
    constants_mod.attr("MIN_VELOCITY_RAMP_MPS") = constants::MIN_VELOCITY_RAMP_MPS;
    constants_mod.attr("MIN_VELOCITY_STOPE_MPS") = constants::MIN_VELOCITY_STOPE_MPS;

    // Consumo O2 por actividad
    constants_mod.attr("O2_REST_LPM") = constants::O2_REST_LPM;
    constants_mod.attr("O2_LIGHT_LPM") = constants::O2_LIGHT_LPM;
    constants_mod.attr("O2_MODERATE_LPM") = constants::O2_MODERATE_LPM;
    constants_mod.attr("O2_HEAVY_LPM") = constants::O2_HEAVY_LPM;
    constants_mod.attr("O2_VERY_HEAVY_LPM") = constants::O2_VERY_HEAVY_LPM;

    // Gases por tipo explosivo
    constants_mod.attr("ANFO_CO_L_PER_KG") = constants::ANFO_CO_L_PER_KG;
    constants_mod.attr("ANFO_NOX_L_PER_KG") = constants::ANFO_NOX_L_PER_KG;
    constants_mod.attr("EMULSION_CO_L_PER_KG") = constants::EMULSION_CO_L_PER_KG;
    constants_mod.attr("EMULSION_NOX_L_PER_KG") = constants::EMULSION_NOX_L_PER_KG;
    constants_mod.attr("DYNAMITE_CO_L_PER_KG") = constants::DYNAMITE_CO_L_PER_KG;
    constants_mod.attr("DYNAMITE_NOX_L_PER_KG") = constants::DYNAMITE_NOX_L_PER_KG;

    // Emisiones diesel por tier
    constants_mod.attr("DIESEL_TIER0_CO_G_KWH") = constants::DIESEL_TIER0_CO_G_KWH;
    constants_mod.attr("DIESEL_TIER0_NOX_G_KWH") = constants::DIESEL_TIER0_NOX_G_KWH;
    constants_mod.attr("DIESEL_TIER3_CO_G_KWH") = constants::DIESEL_TIER3_CO_G_KWH;
    constants_mod.attr("DIESEL_TIER3_NOX_G_KWH") = constants::DIESEL_TIER3_NOX_G_KWH;
    constants_mod.attr("DIESEL_TIER4F_CO_G_KWH") = constants::DIESEL_TIER4F_CO_G_KWH;
    constants_mod.attr("DIESEL_TIER4F_NOX_G_KWH") = constants::DIESEL_TIER4F_NOX_G_KWH;

    // Para compatibilidad con codigo anterior
    m.attr("M3MIN_TO_CFM") = constants::M3MIN_TO_CFM;

    // ========================================================================
    // Utility functions
    // ========================================================================
    m.def("safety_ceil", &safety_ceil, nb::arg("value"),
          "Redondeo hacia arriba (ceiling) para seguridad minera.\n"
          "En ventilacion de minas NUNCA se redondea hacia abajo.");

    m.def("safety_ceil_decimals", &safety_ceil_decimals,
          nb::arg("value"), nb::arg("decimals"),
          "Redondeo hacia arriba a un numero especifico de decimales.");

    m.def("get_o2_consumption", &get_o2_consumption,
          nb::arg("level"),
          "Obtiene consumo de O2 segun nivel de actividad [L/min].");

    m.def("get_min_velocity", &get_min_velocity,
          nb::arg("zone_type"),
          "Obtiene velocidad minima requerida segun tipo de zona [m/s].");
}
