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

#include "ventpy/atkinson.hpp"
#include "ventpy/cobertura.hpp"
#include "ventpy/ducto.hpp"
#include "ventpy/governor.hpp"
#include "ventpy/limites_gases.hpp"
#include "ventpy/red.hpp"
#include "ventpy/ventilador.hpp"

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
               "DS 024-2016-EM / DS 023-2017-EM (Peru)")
        .value("DS132_Chile", RegulatoryStandard::DS132_Chile,
               "DS 132 Reglamento de Seguridad Minera (Chile)");

    nb::enum_<GasType>(m, "GasType",
        "Gases regulados en interior mina.")
        .value("CO", GasType::CO, "Monoxido de carbono")
        .value("CO2", GasType::CO2, "Dioxido de carbono")
        .value("NO2", GasType::NO2, "Dioxido de nitrogeno")
        .value("SO2", GasType::SO2, "Dioxido de azufre")
        .value("H2S", GasType::H2S, "Acido sulfhidrico")
        .value("CH4", GasType::CH4, "Metano")
        .value("NO", GasType::NO, "Monoxido de nitrogeno")
        .value("O2", GasType::O2, "Oxigeno (limites minimo/maximo)");

    nb::enum_<ConcentrationUnit>(m, "ConcentrationUnit",
        "Unidad en que la norma expresa un limite de concentracion.")
        .value("PPM", ConcentrationUnit::PPM, "Partes por millon (volumen)")
        .value("PercentVolume", ConcentrationUnit::PercentVolume,
               "Porcentaje en volumen (O2, CH4)");

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
        "DS 024-2016-EM, Art. 111 (LEO 3 mg/m3).")
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
        "DS 024-2016-EM: criterio ingenieril (herencia DS 055-2010-EM "
        "derogado); normativo real: Art. 252.d y Art. 104/Anexo 13.")
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
        .def_ro("heat_from_oxidation_kw", &ThermalFlowResult::heat_from_oxidation_kw)
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
    // GasLimit / tablas LMP
    // ========================================================================
    nb::class_<GasLimit>(m, "GasLimit",
        "Limite de exposicion ocupacional de un gas bajo una norma.\n"
        "Estructura auditable: cada entrada cita su fuente normativa exacta\n"
        "en regulation_ref. Los campos de valor son opcionales porque cada\n"
        "norma define combinaciones distintas (TWA+STEL, solo techo, minimo\n"
        "para O2). Unidad canonica: ppm / % vol.")
        .def_ro("gas", &GasLimit::gas,
                "Gas regulado (GasType)")
        .def_ro("unit", &GasLimit::unit,
                "Unidad del limite (ConcentrationUnit: PPM o porcentaje en volumen)")
        .def_ro("twa_8h", &GasLimit::twa_8h,
                "Promedio ponderado 8 h (TWA / LPP)")
        .def_ro("stel", &GasLimit::stel,
                "Corta duracion (STEL / LPT)")
        .def_ro("ceiling", &GasLimit::ceiling,
                "Techo absoluto (C) - o maximo para O2")
        .def_ro("floor_min", &GasLimit::floor_min,
                "Minimo permitido (solo O2)")
        .def_ro("regulation_ref", &GasLimit::regulation_ref,
                "Cita normativa exacta");

    m.def("gas_limits", &gas_limits, nb::arg("standard"),
          nb::rv_policy::reference,
          "Tabla LMP completa de la norma indicada.\n"
          "Lanza ValueError si la norma no tiene tabla implementada.");

    m.def("lmp_for", &lmp_for, nb::arg("standard"), nb::arg("gas"),
          nb::rv_policy::reference,
          "LMP de un gas bajo una norma.\n"
          "Lanza ValueError si el gas no esta regulado en esa norma\n"
          "(nunca se retorna un valor por defecto silencioso).");

    // ========================================================================
    // RegulatoryConfig
    // ========================================================================
    nb::class_<RegulatoryConfig>(m, "RegulatoryConfig",
        "Configuracion normativa inmutable para calculo de ventilacion.\n"
        "DS 024-2016-EM / DS 023-2017-EM (Peru) o DS 132 (Chile).")
        .def(nb::init<
                RegulatoryStandard, double, double, double,
                double, double, double, double, double, double,
                double, double>(),
             nb::arg("standard") = RegulatoryStandard::DS024_Peru,
             nb::arg("min_flow_per_person_m3min") = 3.0,
             nb::arg("altitude_threshold_1_masl") = 1500.0,
             nb::arg("flow_per_person_above_t1") = 4.0,
             nb::arg("altitude_threshold_2_masl") = 3000.0,
             nb::arg("flow_per_person_above_t2") = 5.0,
             nb::arg("altitude_threshold_3_masl") = 4000.0,
             nb::arg("flow_per_person_above_t3") = 6.0,
             nb::arg("diesel_hp_factor_m3min") = 3.0,
             nb::arg("max_dilution_time_min") = 30.0,
             nb::arg("default_gas_volume_per_kg_m3") = 0.04,
             nb::arg("default_leakage_factor") = 0.15)
        .def_static("peru", &RegulatoryConfig::peru,
             "Preset oficial peruano - DS 024-2016-EM / DS 023-2017-EM.\n"
             "Equivale exactamente a los defaults del constructor (Art. 247).")
        .def_static("chile", &RegulatoryConfig::chile,
             "Preset oficial chileno - DS 132, Reglamento de Seguridad Minera.\n"
             "3.0 m3/min por persona (Art. 138, sin escalon por altitud) y\n"
             "2.83 m3/min por HP efectivo al freno (Art. 132).")
        .def_static("for_standard", &RegulatoryConfig::for_standard,
             nb::arg("standard"),
             "Construye el preset oficial de la norma indicada.\n"
             "Lanza ValueError si la norma no tiene preset implementado.")
        .def_prop_ro("standard", &RegulatoryConfig::standard)
        .def_prop_ro("min_flow_per_person", &RegulatoryConfig::min_flow_per_person)
        .def_prop_ro("altitude_threshold_1", &RegulatoryConfig::altitude_threshold_1)
        .def_prop_ro("flow_above_threshold_1", &RegulatoryConfig::flow_above_threshold_1)
        .def_prop_ro("altitude_threshold_2", &RegulatoryConfig::altitude_threshold_2)
        .def_prop_ro("flow_above_threshold_2", &RegulatoryConfig::flow_above_threshold_2)
        .def_prop_ro("altitude_threshold_3", &RegulatoryConfig::altitude_threshold_3)
        .def_prop_ro("flow_above_threshold_3", &RegulatoryConfig::flow_above_threshold_3)
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
    // Cobertura / deficit (analisis medido vs requerido - Art. 252 f/g)
    // ========================================================================
    nb::class_<AirflowStation>(m, "AirflowStation",
        "Estacion de aforo de un levantamiento de ventilacion.\n"
        "El caudal de la estacion se calcula como Q = area x velocidad x 60.")
        .def(nb::init<>())
        .def_rw("station_id", &AirflowStation::station_id,
                "Identificador de la estacion")
        .def_rw("area_m2", &AirflowStation::area_m2,
                "Seccion de la labor en la estacion [m2] (> 0)")
        .def_rw("velocity_mps", &AirflowStation::velocity_mps,
                "Velocidad promedio medida [m/s] (>= 0; 0 = sin flujo)")
        .def("__repr__", [](const AirflowStation& s) {
            return "AirflowStation('" + s.station_id + "', area=" +
                   std::to_string(s.area_m2) + " m2, v=" +
                   std::to_string(s.velocity_mps) + " m/s)";
        });

    nb::class_<ZoneMeasurement>(m, "ZoneMeasurement",
        "Medicion de caudal de una zona: exactamente UNA fuente.\n"
        "O bien el caudal ya aforado (q_measured_m3min), o bien la lista de\n"
        "estaciones (entradas PARALELAS de la zona: sus caudales se suman;\n"
        "si se aforo la misma labor varias veces, promediar antes de ingresar).")
        .def(nb::init<>())
        .def_rw("zone_name", &ZoneMeasurement::zone_name,
                "Nombre de la zona")
        .def_rw("q_measured_m3min", &ZoneMeasurement::q_measured_m3min,
                "Caudal ya aforado [m3/min] (fuente directa)")
        .def_rw("stations", &ZoneMeasurement::stations,
                "Estaciones de aforo (fuente alternativa)");

    nb::class_<CoverageParams>(m, "CoverageParams",
        "Umbrales del analisis de cobertura.\n"
        "warning_margin y overventilation_factor son criterios ingenieriles\n"
        "(no normados). Los limites de velocidad provienen del DS 024-2016-EM,\n"
        "Art. 248 (texto original vigente).")
        .def(nb::init<>())
        .def_rw("warning_margin", &CoverageParams::warning_margin,
                "Advertir si cobertura < 1+margen (ingenieril)")
        .def_rw("overventilation_factor", &CoverageParams::overventilation_factor,
                "Advertir si cobertura > factor (ingenieril)")
        .def_rw("min_velocity_mpm", &CoverageParams::min_velocity_mpm,
                "DS 024, Art. 248: minimo 20 m/min")
        .def_rw("max_velocity_mpm", &CoverageParams::max_velocity_mpm,
                "DS 024, Art. 248: maximo 250 m/min")
        .def_rw("anfo_or_blasting_agents", &CoverageParams::anfo_or_blasting_agents,
                "Art. 248: con ANFO el minimo es 25 m/min");

    nb::class_<ZoneSurvey>(m, "ZoneSurvey",
        "Zona del levantamiento: demanda (entrada del Governor) + medicion.\n"
        "input.zone_type NO puede ser GeneralMine: el total de mina lo calcula\n"
        "el propio analisis (evita doble conteo).\n"
        "El zone_name del ZoneSurvey es el que manda en el informe; el de\n"
        "measurement.zone_name se ignora en la ruta analyze_survey.")
        .def(nb::init<>())
        .def_rw("zone_name", &ZoneSurvey::zone_name,
                "Nombre de la zona (el que manda en el informe)")
        .def_rw("input", &ZoneSurvey::input,
                "Entrada de demanda (VentilationInput)")
        .def_rw("measurement", &ZoneSurvey::measurement,
                "Medicion de campo (ZoneMeasurement)");

    nb::class_<StationResult>(m, "StationResult",
        "Resultado de una estacion de aforo (auditable).")
        .def_ro("station_id", &StationResult::station_id)
        .def_ro("area_m2", &StationResult::area_m2)
        .def_ro("velocity_mps", &StationResult::velocity_mps)
        .def_ro("velocity_mpm", &StationResult::velocity_mpm)
        .def_ro("q_station_m3min", &StationResult::q_station_m3min)
        .def_ro("velocity_ok", &StationResult::velocity_ok)
        .def_ro("warning", &StationResult::warning);

    nb::class_<ZoneCoverageResult>(m, "ZoneCoverageResult",
        "Cobertura de una zona: requerido vs medido (auditable).")
        .def_ro("zone_name", &ZoneCoverageResult::zone_name)
        .def_ro("q_required_m3min", &ZoneCoverageResult::q_required_m3min)
        .def_ro("q_measured_m3min", &ZoneCoverageResult::q_measured_m3min)
        .def_ro("coverage_ratio", &ZoneCoverageResult::coverage_ratio)
        .def_ro("deficit_m3min", &ZoneCoverageResult::deficit_m3min)
        .def_ro("compliant", &ZoneCoverageResult::compliant)
        .def_ro("near_deficit_warning", &ZoneCoverageResult::near_deficit_warning)
        .def_ro("overventilated", &ZoneCoverageResult::overventilated)
        .def_ro("stations", &ZoneCoverageResult::stations)
        .def_ro("demand", &ZoneCoverageResult::demand,
                "Desglose completo del Governor (solo via analyze_survey)")
        .def_ro("regulation_ref", &ZoneCoverageResult::regulation_ref);

    nb::class_<MineCoverageResult>(m, "MineCoverageResult",
        "Balance de cobertura de la mina completa (auditable).")
        .def_ro("zones", &MineCoverageResult::zones)
        .def_ro("q_required_total_m3min", &MineCoverageResult::q_required_total_m3min)
        .def_ro("q_measured_total_m3min", &MineCoverageResult::q_measured_total_m3min)
        .def_ro("coverage_ratio", &MineCoverageResult::coverage_ratio)
        .def_ro("deficit_total_m3min", &MineCoverageResult::deficit_total_m3min)
        .def_ro("global_compliant", &MineCoverageResult::global_compliant)
        .def_ro("all_zones_compliant", &MineCoverageResult::all_zones_compliant)
        .def_ro("compliant", &MineCoverageResult::compliant)
        .def_ro("warnings", &MineCoverageResult::warnings)
        .def_ro("regulation_ref", &MineCoverageResult::regulation_ref);

    nb::class_<CoverageCalculator>(m, "CoverageCalculator",
        "Calculador de deficit/cobertura: caudal medido vs requerido.\n"
        "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252.")
        .def_static("compare_zone", &CoverageCalculator::compare_zone,
             nb::arg("q_required_m3min"), nb::arg("measurement"),
             nb::arg("params") = CoverageParams{},
             "Nivel puro: compara un requerido ya calculado contra la medicion.\n"
             "Lanza ValueError si la medicion no tiene exactamente una fuente,\n"
             "o ante cualquier dato fuera de dominio.")
        .def_static("analyze_survey", &CoverageCalculator::analyze_survey,
             nb::arg("zones"), nb::arg("config"),
             nb::arg("params") = CoverageParams{},
             "Orquestador: corre el Governor por zona y agrega el balance.\n"
             "Cumplimiento estricto (safety-first): compliant exige cobertura\n"
             "global (Art. 252.f) Y todas las zonas cubiertas (Art. 252.g).\n"
             "Lanza ValueError si el levantamiento esta vacio, hay nombres de\n"
             "zona duplicados o alguna zona es GeneralMine.");

    // ========================================================================
    // Red de ventilacion (SP-3): enums de Atkinson
    // ========================================================================
    nb::enum_<AirwayLining>(m, "AirwayLining",
        "Tipo de labor/revestimiento para el factor de friccion de Atkinson.\n"
        "McPherson (2009), Cap. 5, Tabla 5.1 (p. 5-6).")
        .value("SmoothLined", AirwayLining::SmoothLined,
               "Concreto liso (rectangular) - k=0.004")
        .value("Shotcrete", AirwayLining::Shotcrete,
               "Shotcrete (rectangular) - k=0.0055")
        .value("UnlinedMinorIrreg", AirwayLining::UnlinedMinorIrreg,
               "Sin revestir, irregularidades menores - k=0.009")
        .value("UnlinedTypical", AirwayLining::UnlinedTypical,
               "Sin revestir, condiciones tipicas - k=0.012")
        .value("UnlinedRough", AirwayLining::UnlinedRough,
               "Sin revestir, rugoso/irregular - k=0.016")
        .value("ArchedDriftBolted", AirwayLining::ArchedDriftBolted,
               "Galeria arqueada, pernos y malla (metal mines) - k=0.010")
        .value("ArchedRampBolted", AirwayLining::ArchedRampBolted,
               "Rampa arqueada, pernos y malla (metal mines) - k=0.014")
        .value("TimberedCribbed", AirwayLining::TimberedCribbed,
               "Entibado/cribbed (coal mines) - k=0.14 (extremo conservador del rango)")
        .value("DuctFabricCollapsible", AirwayLining::DuctFabricCollapsible,
               "Ducto de tela colapsable (forzado) - k=0.0037, ducto nuevo")
        .value("DuctFlexibleSpiral", AirwayLining::DuctFlexibleSpiral,
               "Ducto flexible con espiral estirado - k=0.011, ducto nuevo")
        .value("DuctFiberglass", AirwayLining::DuctFiberglass,
               "Ducto fibra de vidrio - k=0.0024, ducto nuevo")
        .value("DuctSteelSpiral", AirwayLining::DuctSteelSpiral,
               "Ducto espiral acero galvanizado - k=0.0021, ducto nuevo")
        .value("Manual", AirwayLining::Manual,
               "k provisto por el usuario (atkinson_k obligatorio > 0)");

    nb::enum_<SingularityType>(m, "SingularityType",
        "Tipo de singularidad de choque en un ramal.\n"
        "McPherson (2009), Apendice A5 (p. 5-26 a 5-38).")
        .value("Bend90", SingularityType::Bend90,
               "Codo 90 grados - manual-only v1 (McPherson solo publica graficos)")
        .value("Bend45", SingularityType::Bend45,
               "Codo 45 grados - manual-only v1 (correccion angular por grafico)")
        .value("Entrance", SingularityType::Entrance,
               "Entrada - X=0.5 exacto (Ap. A5.2(b), A1 -> infinito)")
        .value("Exit", SingularityType::Exit,
               "Salida - X=1.0 exacto (Ap. A5.2(a), A2 -> infinito)")
        .value("Expansion", SingularityType::Expansion,
               "Expansion brusca - X por formula (Ap. A5.2(a))")
        .value("Contraction", SingularityType::Contraction,
               "Contraccion brusca - X por formula (Ap. A5.2(b))")
        .value("Junction", SingularityType::Junction,
               "Union - manual-only en SP-3a (la formula exige velocidades de red)")
        .value("Manual", SingularityType::Manual,
               "X provisto por el usuario (shock_factor_x obligatorio > 0)");

    // ========================================================================
    // Red de ventilacion (SP-3): structs de entrada
    // ========================================================================
    nb::class_<AirwaySingularity>(m, "AirwaySingularity",
        "Singularidad de choque de un ramal (McPherson Apendice A5).")
        .def(nb::init<>())
        .def_rw("type", &AirwaySingularity::type,
                "Tipo de singularidad (SingularityType)")
        .def_rw("shock_factor_x", &AirwaySingularity::shock_factor_x,
                "X manual (> 0 para Manual/Bend90/Bend45/Junction)")
        .def_rw("area_ratio", &AirwaySingularity::area_ratio,
                "Expansion: A1/A2; Contraction: A2/A1 (0 < r < 1)")
        .def_rw("description", &AirwaySingularity::description,
                "Descripcion libre de la singularidad");

    nb::class_<AirwayParams>(m, "AirwayParams",
        "Parametros geometricos y de revestimiento de un ramal.\n"
        "Fuente de R (friccion + choque) via AtkinsonCalculator.")
        .def(nb::init<>())
        .def_rw("airway_id", &AirwayParams::airway_id,
                "Identificador del ramal")
        .def_rw("length_m", &AirwayParams::length_m,
                "Longitud del ramal [m]")
        .def_rw("perimeter_m", &AirwayParams::perimeter_m,
                "Perimetro de la seccion [m]")
        .def_rw("area_m2", &AirwayParams::area_m2,
                "Seccion del ramal [m2]")
        .def_rw("lining", &AirwayParams::lining,
                "Tipo de labor/revestimiento (AirwayLining)")
        .def_rw("atkinson_k", &AirwayParams::atkinson_k,
                "k manual [kg/m3 a rho=1.2]; > 0 obligatorio si lining=Manual")
        .def_rw("singularities", &AirwayParams::singularities,
                "Singularidades de choque del ramal");

    nb::class_<DuctSizingParams>(m, "DuctSizingParams",
        "Parametros para dimensionamiento tecnico/economico de ducto.")
        .def(nb::init<>())
        .def_rw("q_m3min", &DuctSizingParams::q_m3min,
                "Caudal requerido [m3/min]")
        .def_rw("length_m", &DuctSizingParams::length_m,
                "Longitud del ducto [m]")
        .def_rw("duct_lining", &DuctSizingParams::duct_lining,
                "Tipo de ducto (AirwayLining)")
        .def_rw("atkinson_k", &DuctSizingParams::atkinson_k,
                "k manual [kg/m3], > 0 si duct_lining=Manual")
        .def_rw("singularities", &DuctSizingParams::singularities,
                "Singularidades de choque")
        .def_rw("max_velocity_mps", &DuctSizingParams::max_velocity_mps,
                "Velocidad maxima [m/s]; 0 = default 20")
        .def_rw("available_pressure_pa", &DuctSizingParams::available_pressure_pa,
                "Presion disponible [Pa]; 0 = sin restriccion")
        .def_rw("diameters_m", &DuctSizingParams::diameters_m,
                "Diametros comerciales a evaluar [m]; vacio = default comercial");

    nb::class_<EconomicParams>(m, "EconomicParams",
        "Parametros economicos para optimizacion de ducto.")
        .def(nb::init<>())
        .def_rw("energy_cost_per_kwh", &EconomicParams::energy_cost_per_kwh,
                "Costo de energia [USD/kWh]")
        .def_rw("duct_cost_per_m_per_m_diam", &EconomicParams::duct_cost_per_m_per_m_diam,
                "Costo lineal de ducto [USD/(m*m_diam)]")
        .def_rw("operating_hours", &EconomicParams::operating_hours,
                "Horas de operacion anuales [h/ano]")
        .def_rw("fan_efficiency", &EconomicParams::fan_efficiency,
                "Eficiencia del ventilador (0, 1]");

    nb::class_<NetworkBranch>(m, "NetworkBranch",
        "Ramal de la red. Resistencia: exactamente UNA fuente (XOR):\n"
        "airway (R calculada via AtkinsonCalculator) o r_manual.")
        .def(nb::init<>())
        .def_rw("branch_id", &NetworkBranch::branch_id,
                "Identificador del ramal")
        .def_rw("from_node", &NetworkBranch::from_node,
                "Nodo de origen")
        .def_rw("to_node", &NetworkBranch::to_node,
                "Nodo de destino")
        .def_rw("airway", &NetworkBranch::airway,
                "Parametros del ramal para R via AtkinsonCalculator (opcional)")
        .def_rw("r_manual", &NetworkBranch::r_manual,
                "R manual [Ns2/m8]; > 0 si no hay airway")
        .def_rw("fan_pressure_pa", &NetworkBranch::fan_pressure_pa,
                "Presion de ventilador en sentido from->to [Pa] (>= 0)")
        .def_rw("q_initial_m3min", &NetworkBranch::q_initial_m3min,
                "Caudal inicial [m3/min]; 0 = estimacion automatica (solo cuerdas)");

    nb::class_<NetworkDefinition>(m, "NetworkDefinition",
        "Red completa. La red se modela CERRADA: el nodo 'superficie'\n"
        "cierra el circuito de admision/retorno.")
        .def(nb::init<>())
        .def_rw("branches", &NetworkDefinition::branches,
                "Ramales de la red");

    nb::class_<SolverParams>(m, "SolverParams",
        "Parametros de convergencia del solver Hardy Cross (ingenieriles).")
        .def(nb::init<>())
        .def_rw("tolerance_m3min", &SolverParams::tolerance_m3min,
                "max|deltaQ| de malla para converger [m3/min]")
        .def_rw("max_iterations", &SolverParams::max_iterations,
                "Iteraciones maximas");

    nb::class_<FanCurvePoint>(m, "FanCurvePoint",
        "Punto de la curva de catalogo del ventilador.")
        .def(nb::init<>())
        .def_rw("q_m3min", &FanCurvePoint::q_m3min,
                "Caudal del punto de catalogo [m3/min]")
        .def_rw("pressure_pa", &FanCurvePoint::pressure_pa,
                "Presion total del ventilador [Pa] (>= 0)");

    nb::class_<FanCurve>(m, "FanCurve",
        "Curva de catalogo del fabricante. Puntos ESTRICTAMENTE crecientes\n"
        "en Q (minimo 2). Referida a rated_density_kg_m3 (fan laws, ec. 10.28).")
        .def(nb::init<>())
        .def_rw("fan_id", &FanCurve::fan_id,
                "Identificador del ventilador")
        .def_rw("points", &FanCurve::points,
                "Puntos de catalogo (FanCurvePoint)")
        .def_rw("rated_density_kg_m3", &FanCurve::rated_density_kg_m3,
                "Densidad de referencia de la curva [kg/m3] (default 1.2)");

    nb::class_<FanOperatingParams>(m, "FanOperatingParams",
        "Parametros del punto de operacion del ventilador.")
        .def(nb::init<>())
        .def_rw("stall_margin", &FanOperatingParams::stall_margin,
                "Q_op >= Q_pico*(1+margen) - ingenieril")
        .def_rw("under_relaxation", &FanOperatingParams::under_relaxation,
                "Amortiguacion del punto fijo (modo red) (0, 1]")
        .def_rw("max_iterations", &FanOperatingParams::max_iterations,
                "Iteraciones del punto fijo (modo red)");

    // ========================================================================
    // Red de ventilacion (SP-3): structs de resultado (auditables)
    // ========================================================================
    nb::class_<FrictionFactorEntry>(m, "FrictionFactorEntry",
        "Entrada de la tabla de friccion de Atkinson (auditable, con cita).")
        .def_ro("lining", &FrictionFactorEntry::lining)
        .def_ro("k", &FrictionFactorEntry::k,
                "[kg/m3] a rho = 1.2")
        .def_ro("biblio_ref", &FrictionFactorEntry::biblio_ref);

    nb::class_<ShockFactorEntry>(m, "ShockFactorEntry",
        "Entrada informativa de la tabla de factores de choque.")
        .def_ro("type", &ShockFactorEntry::type)
        .def_ro("x", &ShockFactorEntry::x,
                "0 si es formula/manual (ver note)")
        .def_ro("biblio_ref", &ShockFactorEntry::biblio_ref)
        .def_ro("note", &ShockFactorEntry::note);

    nb::class_<AirwayResistanceResult>(m, "AirwayResistanceResult",
        "Resultado auditable de resistencia de ramal (Atkinson + choque).\n"
        "SIN safety_ceil: R y deltaP crudos (redondearlos falsearia el balance).")
        .def_ro("airway_id", &AirwayResistanceResult::airway_id)
        .def_ro("k_used", &AirwayResistanceResult::k_used,
                "k a densidad estandar 1.2 kg/m3")
        .def_ro("k_corrected", &AirwayResistanceResult::k_corrected,
                "k x rho/1.2")
        .def_ro("air_density_kg_m3", &AirwayResistanceResult::air_density_kg_m3)
        .def_ro("r_friction", &AirwayResistanceResult::r_friction,
                "[Ns2/m8]")
        .def_ro("r_shock", &AirwayResistanceResult::r_shock)
        .def_ro("r_total", &AirwayResistanceResult::r_total)
        .def_ro("q_m3min", &AirwayResistanceResult::q_m3min)
        .def_ro("velocity_mps", &AirwayResistanceResult::velocity_mps)
        .def_ro("pressure_drop_pa", &AirwayResistanceResult::pressure_drop_pa)
        .def_ro("pressure_drop_mmh2o", &AirwayResistanceResult::pressure_drop_mmh2o)
        .def_ro("biblio_ref", &AirwayResistanceResult::biblio_ref)
        .def_ro("warnings", &AirwayResistanceResult::warnings,
                "Ej. velocidad fuera de rango, DS 024-2016-EM Art. 248");

    nb::class_<DuctOptionResult>(m, "DuctOptionResult",
        "Resultado de una opcion de diametro comercial de ducto.")
        .def_ro("diameter_m", &DuctOptionResult::diameter_m,
                "Diametro evaluado [m]")
        .def_ro("area_m2", &DuctOptionResult::area_m2,
                "Area de seccion [m2]")
        .def_ro("velocity_mps", &DuctOptionResult::velocity_mps,
                "Velocidad de aire [m/s]")
        .def_ro("r_total", &DuctOptionResult::r_total,
                "Resistencia total [Ns2/m8]")
        .def_ro("pressure_drop_pa", &DuctOptionResult::pressure_drop_pa,
                "Caida de presion [Pa]")
        .def_ro("velocity_ok", &DuctOptionResult::velocity_ok,
                "Cumple v <= vmax")
        .def_ro("pressure_ok", &DuctOptionResult::pressure_ok,
                "Cumple deltaP <= disponible (si aplica)")
        .def_ro("energy_cost", &DuctOptionResult::energy_cost,
                "Costo energetico anual [USD]")
        .def_ro("capital_cost", &DuctOptionResult::capital_cost,
                "Costo capital [USD]")
        .def_ro("total_cost", &DuctOptionResult::total_cost,
                "Costo total [USD]")
        .def_ro("rejection_reason", &DuctOptionResult::rejection_reason,
                "Razon de rechazo; vacio si viable");

    nb::class_<DuctSizingResult>(m, "DuctSizingResult",
        "Resultado consolidado del dimensionamiento de ducto.")
        .def_ro("selected_diameter_m", &DuctSizingResult::selected_diameter_m,
                "Diametro elegido [m]")
        .def_ro("options", &DuctSizingResult::options,
                "Todas las opciones evaluadas")
        .def_ro("feasible", &DuctSizingResult::feasible,
                "Existe opcion viable?")
        .def_ro("selection_criterion", &DuctSizingResult::selection_criterion,
                "Criterio de seleccion aplicado")
        .def_ro("biblio_ref", &DuctSizingResult::biblio_ref)
        .def_ro("warnings", &DuctSizingResult::warnings,
                "Ej. ninguna opcion viable");

    nb::class_<BranchFlowResult>(m, "BranchFlowResult",
        "Resultado de balance de un ramal de red (auditable, crudo).")
        .def_ro("branch_id", &BranchFlowResult::branch_id)
        .def_ro("from_node", &BranchFlowResult::from_node)
        .def_ro("to_node", &BranchFlowResult::to_node)
        .def_ro("r_ns2m8", &BranchFlowResult::r_ns2m8)
        .def_ro("q_m3min", &BranchFlowResult::q_m3min,
                "Signo: + = from->to")
        .def_ro("pressure_drop_pa", &BranchFlowResult::pressure_drop_pa,
                "R*Q*|Q| (con signo, crudo)")
        .def_ro("fan_pressure_pa", &BranchFlowResult::fan_pressure_pa)
        .def_ro("velocity_mps", &BranchFlowResult::velocity_mps,
                "Solo si el ramal tiene airway con area")
        .def_ro("warnings", &BranchFlowResult::warnings,
                "DS 024-2016-EM Art. 248 si el area es conocida");

    nb::class_<NetworkSolveResult>(m, "NetworkSolveResult",
        "Resultado del balance Hardy Cross de la red completa.\n"
        "McPherson (2009), Cap. 7, sec. 7.3.2.")
        .def_ro("branches", &NetworkSolveResult::branches)
        .def_ro("converged", &NetworkSolveResult::converged)
        .def_ro("iterations", &NetworkSolveResult::iterations)
        .def_ro("max_residual_m3min", &NetworkSolveResult::max_residual_m3min)
        .def_ro("mesh_count", &NetworkSolveResult::mesh_count)
        .def_ro("node_count", &NetworkSolveResult::node_count)
        .def_ro("warnings", &NetworkSolveResult::warnings)
        .def_ro("biblio_ref", &NetworkSolveResult::biblio_ref);

    nb::class_<FanOperatingResult>(m, "FanOperatingResult",
        "Resultado del punto de operacion del ventilador (simple o en red).\n"
        "McPherson (2009), Cap. 10 'Fans'.")
        .def_ro("fan_id", &FanOperatingResult::fan_id)
        .def_ro("q_m3min", &FanOperatingResult::q_m3min,
                "Caudal de operacion")
        .def_ro("pressure_pa", &FanOperatingResult::pressure_pa,
                "Presion de operacion (a densidad de sitio)")
        .def_ro("air_density_kg_m3", &FanOperatingResult::air_density_kg_m3,
                "Rho de sitio usada (ec. 10.28)")
        .def_ro("density_factor", &FanOperatingResult::density_factor,
                "rho_sitio / rated_density aplicado a la curva")
        .def_ro("in_curve_range", &FanOperatingResult::in_curve_range,
                "La solucion cayo dentro del catalogo")
        .def_ro("q_peak_m3min", &FanOperatingResult::q_peak_m3min,
                "Pico del catalogo (a densidad de sitio)")
        .def_ro("pressure_peak_pa", &FanOperatingResult::pressure_peak_pa,
                "Presion del pico (a densidad de sitio)")
        .def_ro("stall_ok", &FanOperatingResult::stall_ok,
                "Q_op >= Q_pico*(1+margen)")
        .def_ro("stall_margin_actual", &FanOperatingResult::stall_margin_actual,
                "(Q_op - Q_pico)/Q_pico (crudo, con signo)")
        .def_ro("converged", &FanOperatingResult::converged,
                "Punto fijo convergio (red); true en modo simple si hay solucion")
        .def_ro("iterations", &FanOperatingResult::iterations)
        .def_ro("network", &FanOperatingResult::network,
                "Desglose de red en el punto de operacion (solo modo red, opcional)")
        .def_ro("warnings", &FanOperatingResult::warnings)
        .def_ro("biblio_ref", &FanOperatingResult::biblio_ref);

    // ========================================================================
    // AtkinsonCalculator: tablas y calculador de resistencia de ramal
    // ========================================================================
    m.def("atkinson_friction_factors", &atkinson_friction_factors,
          nb::rv_policy::reference,
          "Tabla k de Atkinson - McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6.\n"
          "12 entradas, todas con cita bibliografica.");

    m.def("shock_factors", &shock_factors,
          nb::rv_policy::reference,
          "Tabla informativa de factores de choque - McPherson (2009),\n"
          "Apendice A5 (p. 5-26 a 5-38). NO incluye Manual.");

    m.def("friction_factor_for", &friction_factor_for,
          nb::arg("lining"),
          "k de la tabla para un tipo de labor.\n"
          "Lanza ValueError si lining=Manual (el usuario debe proveer k).");

    m.def("resolve_shock_factor", &resolve_shock_factor,
          nb::arg("singularity"),
          "Resuelve el factor de choque X de una singularidad.\n"
          "Lanza ValueError si falta el dato requerido (area_ratio o\n"
          "shock_factor_x segun el tipo).");

    nb::class_<AtkinsonCalculator>(m, "AtkinsonCalculator",
        "Calculador de resistencia de ramal (Atkinson + choque).\n"
        "Unidades: Q en m3/min (API); internamente ley cuadratica en m3/s.\n"
        "SIN safety_ceil: R y deltaP crudos (spec).")
        .def_static("calculate_resistance", &AtkinsonCalculator::calculate_resistance,
             nb::arg("params"), nb::arg("atm"),
             "Calcula R_friccion + R_choque de un ramal (sin caudal).")
        .def_static("calculate", &AtkinsonCalculator::calculate,
             nb::arg("params"), nb::arg("atm"), nb::arg("q_m3min"),
             "Calcula R, deltaP (Pa y mmH2O) y velocidad de un ramal para un\n"
             "caudal dado. Advierte fuera de [20, 250] m/min\n"
             "(DS 024-2016-EM, Art. 248).");

    // ========================================================================
    // DuctSizingCalculator: dimensionamiento tecnico/economico de ducto
    // ========================================================================
    nb::class_<DuctSizingCalculator>(m, "DuctSizingCalculator",
        "Dimensionamiento de ducto de ventilacion auxiliar.\n"
        "Diametros comerciales default (gate 2026-08-17, NO normativos):\n"
        "{0.30, 0.40, 0.50, 0.60, 0.76, 0.91, 1.07, 1.22} m.")
        .def_static("calculate", &DuctSizingCalculator::calculate,
             nb::arg("params"), nb::arg("atm"),
             "Tecnico: menor diametro comercial con v <= vmax y\n"
             "deltaP <= presion disponible.")
        .def_static("calculate_full", &DuctSizingCalculator::calculate_full,
             nb::arg("params"), nb::arg("atm"), nb::arg("economics"),
             "Economico: entre los diametros viables, costo total\n"
             "(energia + capital) minimo.");

    // ========================================================================
    // NetworkSolver: balance de red por Hardy Cross
    // ========================================================================
    nb::class_<NetworkSolver>(m, "NetworkSolver",
        "Solver de red de ventilacion - balance por Hardy Cross.\n"
        "McPherson (2009), Cap. 7, sec. 7.3.2 (pp. 7-13 a 7-19).\n"
        "La red se modela CERRADA (nodo 'superficie' cierra el circuito).")
        .def_static("solve", &NetworkSolver::solve,
             nb::arg("network"), nb::arg("atm"),
             nb::arg("params") = SolverParams{},
             "Resuelve la red por Hardy Cross.\n"
             "Lanza ValueError si la red esta vacia, no es conexa, no tiene\n"
             "mallas (arbol sin circulacion) o hay datos fuera de dominio.");

    // ========================================================================
    // FanCalculator: curva de ventilador, punto de operacion y stall
    // ========================================================================
    nb::class_<FanCalculator>(m, "FanCalculator",
        "Curva de ventilador, punto de operacion y margen de stall.\n"
        "McPherson (2009), Cap. 10 'Fans' - ec. 10.28 (fan laws, densidad).\n"
        "SIN safety_ceil: puntos de equilibrio crudos.")
        .def_static("pressure_at", &FanCalculator::pressure_at,
             nb::arg("curve"), nb::arg("q_m3min"), nb::arg("air_density_kg_m3"),
             "Presion de la curva a densidad de sitio (interpolacion lineal).\n"
             "Lanza ValueError fuera del rango del catalogo (nunca extrapola).")
        .def_static("operating_point", &FanCalculator::operating_point,
             nb::arg("curve"), nb::arg("r_system_ns2m8"), nb::arg("atm"),
             nb::arg("params") = FanOperatingParams{},
             "Punto de operacion contra resistencia de sistema\n"
             "P(Q) = R*(Q/60)^2 (biseccion sobre el rango del catalogo).")
        .def_static("operating_point_in_network",
             &FanCalculator::operating_point_in_network,
             nb::arg("network"), nb::arg("fan_branch_id"), nb::arg("curve"),
             nb::arg("atm"), nb::arg("solver_params") = SolverParams{},
             nb::arg("params") = FanOperatingParams{},
             "Punto de operacion del ventilador DENTRO de una red: punto fijo\n"
             "con sub-relajacion sobre NetworkSolver::solve. El\n"
             "fan_pressure_pa declarado en el ramal se ignora (lo gobierna\n"
             "la curva); se advierte si venia > 0.");

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
