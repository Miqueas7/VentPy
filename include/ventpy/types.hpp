/**
 * @file types.hpp
 * @brief Tipos fundamentales para cálculos de ventilación minera.
 *
 * Define structs, enums y result types usados a lo largo de toda la librería.
 * Referencia normativa: DS 024-2016-EM (Reglamento de Seguridad y Salud
 * Ocupacional en Minería) y sus modificatorias DS 023-2017-EM.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace ventpy {

// ============================================================================
// Enumeraciones
// ============================================================================

/**
 * @brief Tipo de zona de cálculo según contexto operativo.
 *
 * DS 024-2016-EM, Art. 236: La ventilación debe cubrir todos los
 * frentes de trabajo, rampas, tajeos y áreas donde transiten personas.
 */
enum class ZoneType {
    DevelopmentFace,  ///< Frente de desarrollo (avance, galería, crucero)
    Stope,            ///< Tajeo (explotación)
    Ramp,             ///< Rampa de acceso
    GeneralMine       ///< Mina total (sumatoria global)
};

/**
 * @brief Estándar normativo aplicable al cálculo.
 */
enum class RegulatoryStandard {
    DS024_Peru,     ///< DS 024-2016-EM + DS 023-2017-EM (Perú)
    DS132_Chile,    ///< DS 132 Reglamento de Seguridad Minera (Chile)
};

/**
 * @brief Gases regulados en interior mina.
 */
enum class GasType {
    CO,    ///< Monóxido de carbono
    CO2,   ///< Dióxido de carbono
    NO2,   ///< Dióxido de nitrógeno
    SO2,   ///< Dióxido de azufre
    H2S,   ///< Ácido sulfhídrico
    CH4,   ///< Metano
    NO,    ///< Monóxido de nitrógeno
    O2,    ///< Oxígeno (límites mínimo/máximo)
};

/// Unidad en que la norma expresa un límite de concentración.
enum class ConcentrationUnit {
    PPM,            ///< Partes por millón (volumen)
    PercentVolume,  ///< Porcentaje en volumen (O2, CH4)
};

/**
 * @brief Límite de exposición ocupacional de un gas bajo una norma.
 *
 * Estructura auditable: cada entrada cita su fuente normativa exacta en
 * `regulation_ref`. Los campos de valor son opcionales porque cada norma
 * define combinaciones distintas (TWA+STEL, solo techo, mínimo para O2);
 * toda entrada publicada garantiza al menos un valor presente.
 * Unidad canónica: ppm / % vol (decisión de diseño 2026-08-17 — comparar en
 * la unidad en que se mide; los pares en mg/m³ quedan en la cita).
 */
struct GasLimit {
    GasType gas;
    ConcentrationUnit unit;
    std::optional<double> twa_8h;    ///< Promedio ponderado 8 h (TWA / LPP)
    std::optional<double> stel;      ///< Corta duración (STEL / LPT)
    std::optional<double> ceiling;   ///< Techo absoluto (C) — o máximo para O2
    std::optional<double> floor_min; ///< Mínimo permitido (solo O2)
    std::string regulation_ref;      ///< Cita normativa exacta
};

/**
 * @brief Nivel de actividad física del personal.
 *
 * Afecta el consumo de oxígeno y la tasa metabólica.
 * Basado en ISO 8996 (Ergonomics of the thermal environment).
 */
enum class ActivityLevel {
    Rest,           ///< Descanso, supervisión estática: ~0.3 L O2/min
    Light,          ///< Trabajo ligero (operador, supervisión móvil): ~0.5 L O2/min
    Moderate,       ///< Trabajo moderado (perforista, sostenimiento): ~1.0 L O2/min
    Heavy,          ///< Trabajo pesado (paleo manual, instalación): ~1.5 L O2/min
    VeryHeavy       ///< Trabajo muy pesado (rescate, emergencia): ~2.0 L O2/min
};

/**
 * @brief Tipo de explosivo para cálculo de gases.
 *
 * Cada tipo tiene diferentes volúmenes de gases nocivos por kg.
 */
enum class ExplosiveType {
    ANFO,           ///< Nitrato de amonio + fuel oil (estándar)
    Emulsion,       ///< Emulsión encartuchada
    Dynamite,       ///< Dinamita (mayor CO)
    WaterGel,       ///< Hidrogel
    ElectronicDet,  ///< Solo detonadores electrónicos (mínimo gas)
    Custom          ///< Valores personalizados
};

/**
 * @brief Tipo de ducto/manga de ventilación.
 *
 * Afecta el factor de fugas y pérdidas por fricción.
 */
enum class DuctType {
    FlexibleFabric,     ///< Manga flexible de tela (mayor fuga)
    FlexiblePVC,        ///< Manga flexible PVC reforzado
    RigidFiberglass,    ///< Ducto rígido fibra de vidrio
    RigidSteel,         ///< Ducto rígido acero (menor fuga)
    SpiralSteel         ///< Ducto espiral metálico
};

/**
 * @brief Calidad de instalación del sistema de ductos.
 *
 * Afecta directamente las fugas en juntas y conexiones.
 */
enum class InstallationQuality {
    Poor,       ///< Instalación deficiente: fugas ~25-35%
    Average,    ///< Instalación promedio: fugas ~15-25%
    Good,       ///< Buena instalación: fugas ~10-15%
    Excellent   ///< Excelente (juntas selladas): fugas ~5-10%
};

/**
 * @brief Categoría de emisiones del motor diésel.
 *
 * Según EPA Tier / EU Stage para equipos non-road.
 */
enum class EngineEmissionTier {
    Tier0_Unregulated,  ///< Sin regulación (pre-1996)
    Tier1,              ///< Tier 1 (1996-2003)
    Tier2,              ///< Tier 2 (2001-2006)
    Tier3,              ///< Tier 3 (2006-2008)
    Tier4_Interim,      ///< Tier 4 Interim (2008-2012)
    Tier4_Final         ///< Tier 4 Final (2012+, DPF obligatorio)
};

// ============================================================================
// Structs de entrada
// ============================================================================

/**
 * @brief Información de un equipo diésel individual (versión robusta).
 *
 * DS 024-2016-EM, Art. 246: Todo equipo con motor de combustión interna
 * que opere en interior mina requiere un caudal mínimo de aire.
 */
struct DieselEquipment {
    std::string name;               ///< Identificador del equipo
    double      horsepower = 0.0;   ///< Potencia nominal del motor [HP]
    double      availability = 1.0; ///< Factor de disponibilidad mecánica [0.0 - 1.0]
    double      utilization = 1.0;  ///< Factor de utilización [0.0 - 1.0]

    // Campos extendidos (opcionales, con defaults razonables)
    EngineEmissionTier emission_tier = EngineEmissionTier::Tier3;
    double fuel_consumption_lph = 0.0;   ///< Consumo combustible [L/h], 0 = estimar
    bool   has_dpf = false;              ///< ¿Tiene filtro de partículas (DPF)?
    bool   has_doc = false;              ///< ¿Tiene catalizador oxidación (DOC)?
    double co_emission_factor = 0.0;     ///< Factor CO [g/kWh], 0 = usar tier
    double nox_emission_factor = 0.0;    ///< Factor NOx [g/kWh], 0 = usar tier
    double pm_emission_factor = 0.0;     ///< Factor PM [g/kWh], 0 = usar tier
};

/**
 * @brief Parámetros atmosféricos de la mina.
 *
 * Crítico para correcciones de densidad del aire y rendimiento de equipos.
 */
struct AtmosphericParams {
    double altitude_masl = 0.0;          ///< Altitud sobre nivel del mar [msnm]
    double barometric_pressure_kpa = 0.0;///< Presión barométrica [kPa], 0 = calcular
    double dry_bulb_temp_c = 20.0;       ///< Temperatura bulbo seco [°C]
    double wet_bulb_temp_c = 15.0;       ///< Temperatura bulbo húmedo [°C]
    double relative_humidity = 0.60;     ///< Humedad relativa [0.0 - 1.0]
};

/**
 * @brief Parámetros del personal en la zona.
 */
struct PersonnelParams {
    int num_workers = 0;                     ///< Cantidad de trabajadores
    ActivityLevel activity = ActivityLevel::Moderate;  ///< Nivel de actividad
    double exposure_hours = 8.0;             ///< Horas de exposición por turno
};

/**
 * @brief Parámetros para cálculo de caudal por explosivos (versión robusta).
 *
 * DS 024-2016-EM, Art. 243: Tiempo máximo de dilución 30 minutos.
 */
struct BlastingParams {
    double explosive_kg = 0.0;          ///< Cantidad de explosivo por voladura [kg]
    ExplosiveType explosive_type = ExplosiveType::ANFO;
    double gas_volume_per_kg = 0.0;     ///< Vol. gases [m³/kg], 0 = usar tipo
    double dilution_time_min = 0.0;     ///< Tiempo máximo de dilución [min]
    double face_area_m2 = 0.0;          ///< Sección del frente [m²]
    double face_length_m = 0.0;         ///< Longitud hasta el frente [m]

    // Gases específicos (para cálculo detallado de dilución)
    double co_per_kg_liters = 0.0;      ///< CO generado [L/kg], 0 = usar tipo
    double nox_per_kg_liters = 0.0;     ///< NOx generado [L/kg], 0 = usar tipo

    // Límites de exposición objetivo
    double target_co_ppm = 25.0;        ///< Límite CO objetivo [ppm] (DS024: 25 ppm)
    double target_nox_ppm = 5.0;        ///< Límite NOx objetivo [ppm] (DS024: 5 ppm)

    // Velocidad mínima de barrido
    double min_velocity_mps = 0.3;      ///< Velocidad mínima en el frente [m/s]
};

/**
 * @brief Parámetros del sistema de ductos.
 */
struct DuctParams {
    DuctType duct_type = DuctType::FlexiblePVC;
    InstallationQuality quality = InstallationQuality::Average;
    double duct_diameter_m = 0.6;       ///< Diámetro del ducto [m]
    double duct_length_m = 0.0;         ///< Longitud total del ducto [m]
    double num_joints = 0;              ///< Número de juntas/conexiones
    double leakage_per_joint = 0.005;   ///< Fuga por junta [fracción]
    double leakage_per_100m = 0.0;      ///< Fuga por 100m [fracción], 0 = usar tipo
};

/**
 * @brief Parámetros para cálculo de dilución de polvo.
 *
 * DS 024-2016-EM, Art. 111: LEO polvo respirable 3 mg/m³ (8 h).
 */
struct DustParams {
    double dust_generation_rate_mg_s = 0.0;  ///< Tasa generación polvo [mg/s]
    double silica_content_percent = 0.0;     ///< Contenido de sílice [%]
    double target_concentration_mg_m3 = 3.0; ///< Límite polvo respirable [mg/m³]
    double face_area_m2 = 0.0;               ///< Sección para velocidad [m²]
    bool   water_suppression = true;         ///< ¿Usa supresión con agua?
    double suppression_efficiency = 0.7;     ///< Eficiencia supresión [0-1]
};

/**
 * @brief Parámetros para cálculo de carga térmica.
 *
 * INGENIERIL — la norma vigente (DS 024-2016-EM / DS 023-2017-EM) NO fija una
 * temperatura efectiva máxima; `target_effective_temp_c` (default 28°C) es un
 * objetivo de diseño heredado del derogado DS 055-2010-EM. El criterio
 * normativo real es el Art. 252.d (velocidad mínima 30 m/min con temperatura
 * seca 24-29°C) y el Art. 104 + Anexo 13 (remisión a evaluación WBGT de
 * estrés térmico) — ver `ThermalFlowCalculator` (caudal_termico.hpp).
 */
struct ThermalParams {
    double virgin_rock_temp_c = 25.0;       ///< Temperatura roca virgen [°C]
    double geothermal_gradient_c_per_100m = 1.0;  ///< Gradiente [°C/100m]
    double depth_below_surface_m = 0.0;     ///< Profundidad [m]
    double auto_compression_c_per_100m = 0.98;    ///< Autocompresión [°C/100m]
    double heat_from_equipment_kw = 0.0;    ///< Calor de equipos [kW]
    double heat_from_oxidation_kw = 0.0;    ///< Calor de oxidación mineral [kW]
    double target_effective_temp_c = 28.0;  ///< Temperatura efectiva objetivo [°C]
    double face_area_m2 = 0.0;              ///< Sección de la labor [m²]
};

/**
 * @brief Estación de aforo de un levantamiento de ventilación.
 *
 * El caudal de la estación se calcula como Q = area × velocidad × 60.
 */
struct AirflowStation {
    std::string station_id;
    double area_m2 = 0.0;       ///< Sección de la labor en la estación [m²] (> 0)
    double velocity_mps = 0.0;  ///< Velocidad promedio medida [m/s] (≥ 0; 0 = sin flujo)
};

/**
 * @brief Medición de caudal de una zona: exactamente UNA fuente.
 *
 * O bien el caudal ya aforado (`q_measured_m3min`), o bien la lista de
 * estaciones (entradas PARALELAS de la zona: sus caudales se suman; si se
 * aforó la misma labor varias veces, promediar antes de ingresar).
 */
struct ZoneMeasurement {
    std::string zone_name;
    std::optional<double> q_measured_m3min;   ///< Caudal ya aforado [m³/min]
    std::vector<AirflowStation> stations;     ///< O estaciones de aforo
};

/**
 * @brief Umbrales del análisis de cobertura.
 *
 * `warning_margin` y `overventilation_factor` son criterios INGENIERILES
 * (no normados — verificación negativa del gate 2026-08-17). Los límites de
 * velocidad provienen del DS 024-2016-EM, Art. 248 (texto original vigente).
 */
struct CoverageParams {
    double warning_margin = 0.10;         ///< Advertir si cobertura < 1+margen (ingenieril)
    double overventilation_factor = 1.5;  ///< Advertir si cobertura > factor (ingenieril)
    double min_velocity_mpm = 20.0;       ///< DS 024, Art. 248: mínimo 20 m/min
    double max_velocity_mpm = 250.0;      ///< DS 024, Art. 248: máximo 250 m/min
    bool anfo_or_blasting_agents = false; ///< Art. 248: con ANFO el mínimo es 25 m/min
};

/// Tipo de labor/revestimiento — mapeo 1:1 a McPherson (2009) Tabla 5.1 (gate 2026-08-17).
enum class AirwayLining {
    SmoothLined, Shotcrete, UnlinedMinorIrreg, UnlinedTypical, UnlinedRough,
    ArchedDriftBolted, ArchedRampBolted, TimberedCribbed,
    DuctFabricCollapsible, DuctFlexibleSpiral, DuctFiberglass, DuctSteelSpiral,
    Manual,   ///< k provisto por el usuario
};

/// Singularidad de choque. Bend90/Bend45/Junction son Manual-only en v1
/// (McPherson solo publica gráficos/fórmulas de red — ver atkinson.hpp).
enum class SingularityType {
    Bend90, Bend45, Entrance, Exit, Expansion, Contraction, Junction, Manual,
};

struct AirwaySingularity {
    SingularityType type = SingularityType::Manual;
    double shock_factor_x = 0.0;  ///< X manual (> 0 para Manual/Bend*/Junction)
    double area_ratio = 0.0;      ///< Expansion: A1/A2; Contraction: A2/A1 (0<r<1)
    std::string description;
};

struct AirwayParams {
    std::string airway_id;
    double length_m = 0.0;
    double perimeter_m = 0.0;
    double area_m2 = 0.0;
    AirwayLining lining = AirwayLining::Manual;
    double atkinson_k = 0.0;      ///< [kg/m³ a ρ=1.2]; > 0 obligatorio si Manual
    std::vector<AirwaySingularity> singularities;
};

/// Entrada de la tabla de fricción (auditable, con cita bibliográfica).
struct FrictionFactorEntry {
    AirwayLining lining;
    double k;                     ///< [kg/m³] a ρ = 1.2
    std::string biblio_ref;
};

/// Entrada informativa de la tabla de choque.
struct ShockFactorEntry {
    SingularityType type;
    double x;                     ///< 0 si es fórmula/manual
    std::string biblio_ref;
    std::string note;
};

// ============================================================================
// Structs de resultado (para auditoría)
// ============================================================================

/**
 * @brief Correcciones atmosféricas calculadas.
 */
struct AtmosphericCorrections {
    double altitude_masl;               ///< Altitud usada [msnm]
    double pressure_kpa;                ///< Presión atmosférica [kPa]
    double density_ratio;               ///< ρ/ρ₀ (ratio vs nivel del mar)
    double oxygen_partial_pressure_kpa; ///< Presión parcial O2 [kPa]
    double air_density_kg_m3;           ///< Densidad del aire [kg/m³]
    double volume_correction_factor;    ///< Factor corrección volumétrico
    std::string notes;
};

/**
 * @brief Resultado detallado del cálculo de caudal por personal.
 */
struct PersonnelFlowResult {
    int    num_workers;                 ///< Cantidad de personas
    ActivityLevel activity_level;       ///< Nivel de actividad
    double altitude_masl;               ///< Altitud [msnm]
    double density_correction;          ///< Factor corrección por densidad
    double o2_consumption_lpm;          ///< Consumo O2 por persona [L/min]
    double flow_per_person_base;        ///< Caudal base por persona [m³/min]
    double flow_per_person_corrected;   ///< Caudal corregido [m³/min]
    double q_personnel;                 ///< Caudal total por personal [m³/min]
    double min_velocity_check_mps;      ///< Velocidad resultante [m/s]
    std::string regulation_ref;         ///< Referencia normativa aplicada
};

/**
 * @brief Resultado detallado del cálculo de caudal por equipo diésel.
 */
struct DieselFlowResult {
    std::vector<std::string> equipment_names;
    double hp_factor_base;              ///< Factor base [m³/min/HP]
    double hp_factor_corrected;         ///< Factor corregido por altitud
    double altitude_derate_factor;      ///< Factor de-rate por altitud
    double total_rated_hp;              ///< Sumatoria HP nominal
    double total_effective_hp;          ///< Sumatoria HP×Disp×Ut
    double total_derated_hp;            ///< HP efectivo con de-rate altitud

    // Por contaminante
    double q_for_co_dilution;           ///< Q para diluir CO [m³/min]
    double q_for_nox_dilution;          ///< Q para diluir NOx [m³/min]
    double q_for_pm_dilution;           ///< Q para diluir PM [m³/min]
    double q_diesel;                    ///< Caudal total requerido [m³/min]

    double co_emission_total_g_min;     ///< Emisión total CO [g/min]
    double nox_emission_total_g_min;    ///< Emisión total NOx [g/min]

    std::string regulation_ref;
};

/**
 * @brief Resultado detallado del cálculo de caudal por explosivos.
 */
struct BlastingFlowResult {
    ExplosiveType explosive_type;
    double explosive_kg;                ///< Cantidad de explosivo [kg]
    double co_generated_liters;         ///< CO total generado [L]
    double nox_generated_liters;        ///< NOx total generado [L]
    double total_gas_volume_m3;         ///< Volumen total gases [m³]
    double face_volume_m3;              ///< Volumen de la labor [m³]
    double dilution_time_min;           ///< Tiempo de dilución [min]

    double q_for_co_dilution;           ///< Q para diluir CO [m³/min]
    double q_for_nox_dilution;          ///< Q para diluir NOx [m³/min]
    double q_for_volume_exchange;       ///< Q para recambio volumétrico [m³/min]
    double q_for_min_velocity;          ///< Q para velocidad mínima [m³/min]
    double q_blasting;                  ///< Caudal gobernante [m³/min]

    std::string governing_criterion;    ///< Qué criterio gobierna
    std::string regulation_ref;
};

/**
 * @brief Resultado detallado del cálculo de fugas.
 */
struct LeakageFlowResult {
    DuctType duct_type;
    InstallationQuality quality;
    double duct_length_m;               ///< Longitud del ducto [m]
    double duct_diameter_m;             ///< Diámetro [m]
    int    num_joints;                  ///< Número de juntas
    double base_leakage_factor;         ///< Factor base por tipo/calidad
    double length_leakage_factor;       ///< Factor adicional por longitud
    double joint_leakage_factor;        ///< Factor adicional por juntas
    double total_leakage_factor;        ///< Factor de fuga total
    double base_flow;                   ///< Caudal requerido en el frente [m³/min]
    double q_leakage;                   ///< Caudal perdido en fugas [m³/min]
    double q_at_fan;                    ///< Caudal requerido en ventilador [m³/min]
    std::string notes;
};

/**
 * @brief Resultado del cálculo de dilución de polvo.
 */
struct DustFlowResult {
    double dust_generation_mg_s = 0.0;        ///< Generación de polvo [mg/s]
    double target_concentration = 0.0;        ///< Concentración objetivo [mg/m³]
    double suppression_efficiency = 0.0;      ///< Eficiencia supresión aplicada
    double effective_generation = 0.0;        ///< Generación efectiva post-supresión
    double q_dust = 0.0;                      ///< Caudal requerido [m³/min]
    double resulting_velocity_mps = 0.0;      ///< Velocidad resultante [m/s]
    std::string regulation_ref;
    std::vector<std::string> warnings;  ///< Advertencias de cálculo
};

/**
 * @brief Resultado del cálculo de carga térmica.
 */
struct ThermalFlowResult {
    double heat_from_rock_kw = 0.0;           ///< Calor de la roca [kW]
    double heat_from_equipment_kw = 0.0;      ///< Calor de equipos [kW]
    double heat_from_oxidation_kw = 0.0;      ///< Calor de oxidación mineral [kW]
    double heat_from_autocompression_kw = 0.0;///< Calor por autocompresión [kW]
    double heat_from_other_kw = 0.0;          ///< Otras fuentes [kW]
    double total_heat_load_kw = 0.0;          ///< Carga térmica total [kW]
    double inlet_temp_c = 0.0;                ///< Temperatura entrada aire [°C]
    double target_temp_c = 0.0;               ///< Temperatura objetivo [°C]
    double delta_t_available = 0.0;           ///< ΔT disponible [°C]
    double q_thermal = 0.0;                   ///< Caudal requerido [m³/min]
    double resulting_velocity_mps = 0.0;      ///< Velocidad resultante [m/s]
    std::string regulation_ref;
    std::vector<std::string> warnings;  ///< Advertencias de cálculo
};

/**
 * @brief Resultado consolidado de demanda de ventilación.
 *
 * Estructura de auditoría: contiene el desglose completo del cálculo
 * para trazabilidad según Art. 236 DS 024-2016-EM.
 */
struct VentilationDemandResult {
    ZoneType zone_type;
    RegulatoryStandard standard;

    // Correcciones atmosféricas aplicadas
    std::optional<AtmosphericCorrections> atmospheric;

    // Resultados por factor
    std::optional<PersonnelFlowResult> personnel;
    std::optional<DieselFlowResult>    diesel;
    std::optional<BlastingFlowResult>  blasting;
    std::optional<DustFlowResult>      dust;
    std::optional<ThermalFlowResult>   thermal;
    std::optional<LeakageFlowResult>   leakage;

    // Caudales individuales [m³/min]
    double q_personnel_m3min = 0.0;
    double q_diesel_m3min    = 0.0;
    double q_blasting_m3min  = 0.0;
    double q_dust_m3min      = 0.0;
    double q_thermal_m3min   = 0.0;
    double q_leakage_m3min   = 0.0;

    // Caudal final
    double q_governing_m3min = 0.0;     ///< Caudal gobernante en el frente
    double q_at_fan_m3min    = 0.0;     ///< Caudal requerido en ventilador
    double q_total_m3min     = 0.0;     ///< = q_at_fan (con fugas)
    double q_total_m3s       = 0.0;     ///< Caudal [m³/s]
    double q_total_cfm       = 0.0;     ///< Caudal [CFM]

    // Velocidad resultante
    double face_area_m2      = 0.0;
    double velocity_at_face_mps = 0.0;  ///< Velocidad en el frente [m/s]
    bool   velocity_ok       = true;    ///< ¿Cumple velocidad mínima?

    // Factores de seguridad aplicados
    double safety_factor_applied = 1.0;

    std::string governing_factor;       ///< Qué factor gobierna
    std::string notes;                  ///< Notas adicionales
    std::vector<std::string> warnings;  ///< Advertencias generadas
};

/**
 * @brief Resultado de una estación de aforo (auditable).
 */
struct StationResult {
    std::string station_id;
    double area_m2 = 0.0;
    double velocity_mps = 0.0;
    double velocity_mpm = 0.0;      ///< = velocity_mps × 60 (unidad del Art. 248)
    double q_station_m3min = 0.0;   ///< = area × velocity_mps × 60 (crudo)
    bool   velocity_ok = true;      ///< dentro de [mín efectivo, máx] del Art. 248
    std::string warning;            ///< vacío si velocity_ok
};

/**
 * @brief Cobertura de una zona: requerido vs medido (auditable).
 */
struct ZoneCoverageResult {
    std::string zone_name;
    double q_required_m3min = 0.0;  ///< Requerido (del Governor: ya con fugas + FS)
    double q_measured_m3min = 0.0;  ///< Medido (directo o Σ estaciones, crudo)
    double coverage_ratio = 0.0;    ///< medido/requerido, crudo (diagnóstico)
    double deficit_m3min = 0.0;     ///< safety_ceil(req − med) si hay déficit; 0 si no
    bool   compliant = false;             ///< medido ≥ requerido
    bool   near_deficit_warning = false;  ///< cumple pero cobertura < 1+margen
    bool   overventilated = false;        ///< cobertura > factor
    std::vector<StationResult> stations;  ///< desglose (vacío si medición directa)
    std::optional<VentilationDemandResult> demand;  ///< solo vía analyze_survey
    std::string regulation_ref;
};

/**
 * @brief Balance de cobertura de la mina completa (auditable).
 */
struct MineCoverageResult {
    std::vector<ZoneCoverageResult> zones;
    double q_required_total_m3min = 0.0;
    double q_measured_total_m3min = 0.0;
    double coverage_ratio = 0.0;          ///< global, crudo
    double deficit_total_m3min = 0.0;     ///< safety_ceil(Σreq − Σmed) si positivo
    bool   global_compliant = false;      ///< Σ medido ≥ Σ requerido (Art. 252.f)
    bool   all_zones_compliant = false;   ///< ninguna zona en déficit (Art. 252.g)
    bool   compliant = false;             ///< ambos (criterio estricto)
    std::vector<std::string> warnings;
    std::string regulation_ref;
};

/// Resultado auditable de resistencia de ramal (SIN safety_ceil: R y ΔP crudos —
/// redondearlos falsearía el balance de red; ver spec SP-3a).
struct AirwayResistanceResult {
    std::string airway_id;
    double k_used = 0.0;              ///< k a ρ estándar 1.2
    double k_corrected = 0.0;         ///< k × ρ/1.2
    double air_density_kg_m3 = 0.0;
    double r_friction = 0.0;          ///< [Ns²/m⁸]
    double r_shock = 0.0;
    double r_total = 0.0;
    double q_m3min = 0.0;
    double velocity_mps = 0.0;
    double pressure_drop_pa = 0.0;
    double pressure_drop_mmh2o = 0.0;
    std::string biblio_ref;
    std::vector<std::string> warnings;
};

// ============================================================================
// Structs para dimensionamiento de ducto (Task 3, SP-3a)
// ============================================================================

/**
 * @brief Parámetros para dimensionamiento técnico/económico de ducto.
 */
struct DuctSizingParams {
    double q_m3min = 0.0;                           ///< Caudal requerido [m³/min]
    double length_m = 0.0;                          ///< Longitud del ducto [m]
    AirwayLining duct_lining = AirwayLining::DuctFlexibleSpiral;  ///< Tipo de ducto
    double atkinson_k = 0.0;                        ///< k manual [kg/m³], > 0 si lining=Manual
    std::vector<AirwaySingularity> singularities;   ///< Singularidades de choque
    double max_velocity_mps = 0.0;                  ///< Velocidad máxima [m/s]; 0 ⇒ default 20
    double available_pressure_pa = 0.0;             ///< Presión disponible [Pa]; 0 ⇒ sin restricción
    std::vector<double> diameters_m;                ///< Diámetros a evaluar [m]; vacío ⇒ default comercial
};

/**
 * @brief Parámetros económicos para optimización de ducto (Task 4).
 */
struct EconomicParams {
    double energy_cost_per_kwh = 0.0;                               ///< Costo energía [USD/kWh]
    double duct_cost_per_m_per_m_diam = 0.0;                       ///< Costo lineal [USD/(m·m_diam)]
    double operating_hours = 0.0;                                   ///< Horas operación anuales [h/año]
    double fan_efficiency = 0.65;                                   ///< Eficiencia ventilador (0, 1]
};

/**
 * @brief Resultado de una opción de diámetro comercial.
 */
struct DuctOptionResult {
    double diameter_m = 0.0;            ///< Diámetro evaluado [m]
    double area_m2 = 0.0;               ///< Área de sección [m²]
    double velocity_mps = 0.0;          ///< Velocidad de aire [m/s]
    double r_total = 0.0;               ///< Resistencia total [Ns²/m⁸]
    double pressure_drop_pa = 0.0;      ///< Caída de presión [Pa]
    bool velocity_ok = false;           ///< Cumple v ≤ vmax
    bool pressure_ok = false;           ///< Cumple ΔP ≤ disponible (si aplica)
    double energy_cost = 0.0;           ///< Costo energético anual [USD] (Task 4)
    double capital_cost = 0.0;          ///< Costo capital [USD] (Task 4)
    double total_cost = 0.0;            ///< Costo total [USD] (Task 4)
    std::string rejection_reason;       ///< Razón de rechazo; vacío si viable
};

/**
 * @brief Resultado consolidado del dimensionamiento de ducto.
 */
struct DuctSizingResult {
    double selected_diameter_m = 0.0;       ///< Diámetro elegido [m]
    std::vector<DuctOptionResult> options;  ///< Todas las opciones evaluadas
    bool feasible = false;                  ///< ¿Existe opción viable?
    std::string selection_criterion;        ///< Criterio de selección aplicado
    std::string biblio_ref;                 ///< Referencia bibliográfica
    std::vector<std::string> warnings;      ///< Advertencias (ej: ninguna opción viable)
};

// ============================================================================
// Structs para solver de red de ventilación (SP-3b Hardy Cross)
// ============================================================================

/// Ramal de la red. Resistencia: exactamente UNA fuente (XOR):
/// `airway` (R vía AtkinsonCalculator con la atmósfera dada) o `r_manual`.
struct NetworkBranch {
    std::string branch_id;
    std::string from_node;
    std::string to_node;
    std::optional<AirwayParams> airway;  ///< R calculada (incluye choque)
    double r_manual = 0.0;               ///< [Ns²/m⁸] > 0 si no hay airway
    double fan_pressure_pa = 0.0;        ///< ≥ 0; presión de ventilador en sentido from→to
    double q_initial_m3min = 0.0;        ///< 0 = estimación automática (solo cuerdas)
};

/// Red completa. Convención: la red se modela CERRADA — el nodo "superficie"
/// cierra el circuito admisión/retorno. Un árbol (sin mallas) no tiene
/// solución de circulación y lanza.
struct NetworkDefinition {
    std::vector<NetworkBranch> branches;
};

struct SolverParams {
    double tolerance_m3min = 0.6;   ///< max|ΔQ| de malla para converger (ingenieril)
    int max_iterations = 100;       ///< (ingenieril)
};

struct BranchFlowResult {
    std::string branch_id, from_node, to_node;
    double r_ns2m8 = 0.0;
    double q_m3min = 0.0;           ///< signo: + = from→to
    double pressure_drop_pa = 0.0;  ///< R·Q·|Q| (con signo, crudo)
    double fan_pressure_pa = 0.0;
    double velocity_mps = 0.0;      ///< solo si airway con área
    std::vector<std::string> warnings;  ///< Art. 248 si área conocida
};

struct NetworkSolveResult {
    std::vector<BranchFlowResult> branches;
    bool converged = false;
    int iterations = 0;
    double max_residual_m3min = 0.0;
    int mesh_count = 0;
    int node_count = 0;
    std::vector<std::string> warnings;
    std::string biblio_ref;   ///< McPherson Cap. 7 §7.3.2
};

// ============================================================================
// Structs para ventilador (SP-3c)
// ============================================================================

struct FanCurvePoint {
    double q_m3min = 0.0;      ///< Caudal del punto de catálogo [m³/min]
    double pressure_pa = 0.0;  ///< Presión total del ventilador [Pa] (≥ 0)
};

/// Curva de catálogo del fabricante. Puntos ESTRICTAMENTE crecientes en Q
/// (mínimo 2). Referida a rated_density_kg_m3 (fan laws, ec. 10.28).
struct FanCurve {
    std::string fan_id;
    std::vector<FanCurvePoint> points;
    double rated_density_kg_m3 = 1.2;
};

struct FanOperatingParams {
    double stall_margin = 0.10;     ///< Q_op ≥ Q_pico·(1+margen) — ingenieril
    double under_relaxation = 0.5;  ///< amortiguación del punto fijo (modo red)
    int max_iterations = 50;        ///< iteraciones del punto fijo (modo red)
};

struct FanOperatingResult {
    std::string fan_id;
    double q_m3min = 0.0;             ///< Caudal de operación
    double pressure_pa = 0.0;         ///< Presión de operación (a densidad de sitio)
    double air_density_kg_m3 = 0.0;   ///< ρ de sitio usada (ec. 10.28)
    double density_factor = 0.0;      ///< ρ_sitio / rated_density aplicado a la curva
    bool   in_curve_range = false;    ///< la solución cayó dentro del catálogo
    // Stall (pico a densidad de sitio)
    double q_peak_m3min = 0.0;
    double pressure_peak_pa = 0.0;
    bool   stall_ok = false;          ///< Q_op ≥ Q_pico·(1+margen)
    double stall_margin_actual = 0.0; ///< (Q_op − Q_pico)/Q_pico (crudo, con signo)
    // Modo red
    bool   converged = false;         ///< punto fijo convergió (red); true en modo simple si hay solución
    int    iterations = 0;
    std::optional<NetworkSolveResult> network;  ///< desglose de red en el punto de operación
    std::vector<std::string> warnings;
    std::string biblio_ref;
};

// ============================================================================
// Constantes de conversión y físicas
// ============================================================================

namespace constants {

    // --- Conversiones ---
    inline constexpr double M3MIN_TO_CFM = 35.3147;
    inline constexpr double M3MIN_TO_M3S = 1.0 / 60.0;
    inline constexpr double CFM_TO_M3MIN = 1.0 / 35.3147;
    inline constexpr double HP_TO_KW = 0.7457;
    inline constexpr double KW_TO_HP = 1.341;

    // --- Atmósfera estándar ---
    inline constexpr double SEA_LEVEL_PRESSURE_KPA = 101.325;
    inline constexpr double SEA_LEVEL_TEMP_K = 288.15;       // 15°C
    inline constexpr double SEA_LEVEL_DENSITY_KG_M3 = 1.225;
    inline constexpr double LAPSE_RATE_K_PER_M = 0.0065;
    inline constexpr double GAS_CONSTANT_AIR = 287.05;       // J/(kg·K)
    inline constexpr double GRAVITY_M_S2 = 9.81;
    inline constexpr double O2_FRACTION_AIR = 0.2095;

    // --- Límites de exposición DS 024-2016-EM ---
    inline constexpr double TLV_CO_PPM = 25.0;               // Art. 108
    inline constexpr double TLV_NO2_PPM = 5.0;               // Art. 108
    inline constexpr double TLV_NO_PPM = 25.0;
    inline constexpr double TLV_SO2_PPM = 5.0;
    inline constexpr double TLV_H2S_PPM = 10.0;
    inline constexpr double TLV_DUST_RESPIRABLE_MG_M3 = 3.0; // Art. 111
    inline constexpr double MIN_O2_PERCENT = 19.5;           // Art. 236
    inline constexpr double MAX_EFFECTIVE_TEMP_C = 30.0;     // INGENIERIL:
        // la norma vigente NO fija TE maxima (herencia del derogado DS
        // 055-2010-EM); el criterio normativo real es Art. 252.d (30 m/min,
        // 24-29 C) y Art. 104/Anexo 13 (WBGT) — ver caudal_termico.hpp
    inline constexpr double AIR_CP_KJ_KG_K = 1.005;           // cp aire seco

    // --- Velocidades mínimas DS 024-2016-EM ---
    inline constexpr double MIN_VELOCITY_DEVELOPMENT_MPS = 0.25;  // Art. 236
    inline constexpr double MIN_VELOCITY_RAMP_MPS = 0.30;
    inline constexpr double MIN_VELOCITY_STOPE_MPS = 0.20;

    // --- Consumo de oxígeno por nivel de actividad [L O2/min] ---
    inline constexpr double O2_REST_LPM = 0.3;
    inline constexpr double O2_LIGHT_LPM = 0.5;
    inline constexpr double O2_MODERATE_LPM = 1.0;
    inline constexpr double O2_HEAVY_LPM = 1.5;
    inline constexpr double O2_VERY_HEAVY_LPM = 2.0;

    // --- Gases por tipo de explosivo [L gas/kg explosivo] ---
    // Valores típicos de literatura técnica
    inline constexpr double ANFO_CO_L_PER_KG = 40.0;
    inline constexpr double ANFO_NOX_L_PER_KG = 10.0;
    inline constexpr double EMULSION_CO_L_PER_KG = 30.0;
    inline constexpr double EMULSION_NOX_L_PER_KG = 8.0;
    inline constexpr double DYNAMITE_CO_L_PER_KG = 50.0;
    inline constexpr double DYNAMITE_NOX_L_PER_KG = 15.0;

    // --- Factores de emisión diésel típicos [g/kWh] ---
    // EPA AP-42, NIOSH RI 9324
    inline constexpr double DIESEL_TIER0_CO_G_KWH = 3.5;
    inline constexpr double DIESEL_TIER0_NOX_G_KWH = 14.0;
    inline constexpr double DIESEL_TIER0_PM_G_KWH = 0.8;

    inline constexpr double DIESEL_TIER3_CO_G_KWH = 2.5;
    inline constexpr double DIESEL_TIER3_NOX_G_KWH = 6.0;
    inline constexpr double DIESEL_TIER3_PM_G_KWH = 0.3;

    inline constexpr double DIESEL_TIER4F_CO_G_KWH = 0.5;
    inline constexpr double DIESEL_TIER4F_NOX_G_KWH = 0.4;
    inline constexpr double DIESEL_TIER4F_PM_G_KWH = 0.02;

} // namespace constants

// ============================================================================
// Funciones utilitarias
// ============================================================================

/**
 * @brief Redondeo hacia arriba (ceiling) para seguridad minera.
 *
 * En ventilación de minas NUNCA se redondea hacia abajo.
 */
[[nodiscard]] inline double safety_ceil(double value) noexcept {
    return std::ceil(value);
}

/**
 * @brief Redondeo hacia arriba a un número específico de decimales.
 */
[[nodiscard]] inline double safety_ceil_decimals(double value, int decimals) noexcept {
    double factor = std::pow(10.0, decimals);
    return std::ceil(value * factor) / factor;
}

/**
 * @brief Obtiene el consumo de O2 según nivel de actividad [L/min].
 */
[[nodiscard]] inline double get_o2_consumption(ActivityLevel level) noexcept {
    switch (level) {
        case ActivityLevel::Rest:      return constants::O2_REST_LPM;
        case ActivityLevel::Light:     return constants::O2_LIGHT_LPM;
        case ActivityLevel::Moderate:  return constants::O2_MODERATE_LPM;
        case ActivityLevel::Heavy:     return constants::O2_HEAVY_LPM;
        case ActivityLevel::VeryHeavy: return constants::O2_VERY_HEAVY_LPM;
        default:                       return constants::O2_MODERATE_LPM;
    }
}

/**
 * @brief Obtiene la velocidad mínima requerida según tipo de zona [m/s].
 */
[[nodiscard]] inline double get_min_velocity(ZoneType zone) noexcept {
    switch (zone) {
        case ZoneType::DevelopmentFace: return constants::MIN_VELOCITY_DEVELOPMENT_MPS;
        case ZoneType::Ramp:            return constants::MIN_VELOCITY_RAMP_MPS;
        case ZoneType::Stope:           return constants::MIN_VELOCITY_STOPE_MPS;
        case ZoneType::GeneralMine:     return constants::MIN_VELOCITY_DEVELOPMENT_MPS;
        default:                        return constants::MIN_VELOCITY_DEVELOPMENT_MPS;
    }
}

} // namespace ventpy
