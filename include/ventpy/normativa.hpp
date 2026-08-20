/**
 * @file normativa.hpp
 * @brief Parámetros normativos para ventilación de minas subterráneas.
 *
 * Encapsula los valores regulatorios del DS 024-2016-EM y DS 023-2017-EM.
 * Los parámetros son configurables (para estándares corporativos más
 * exigentes) pero inmutables durante el ciclo de cálculo.
 *
 * Diseño: Patrón "Configuration Object" — se construye, se valida,
 * y luego se pasa como const& a los calculadores.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <stdexcept>
#include <string>

#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Configuración normativa inmutable para un cálculo de ventilación.
 *
 * Una vez construida y validada, esta clase NO expone setters.
 * Para cambiar parámetros, se debe construir una nueva instancia.
 * Esto garantiza inmutabilidad durante el cálculo (safety-critical).
 */
class RegulatoryConfig {
public:
    /**
     * @brief Constructor con valores por defecto según DS 024-2016-EM.
     *
     * Todos los valores por defecto corresponden al mínimo normativo peruano.
     * El ingeniero puede sobreescribirlos con estándares corporativos
     * más exigentes al momento de la construcción.
     *
     * Escala de caudal por persona — DS 024-2016-EM, Art. 247 (texto original,
     * no modificado por el DS 023-2017-EM):
     * "hasta 1,500 msnm: 3 m³/min; de 1,500 a 3,000: 4 (+40%);
     *  de 3,000 a 4,000: 5 (+70%); sobre los 4,000: 6 (+100%)".
     * Semántica de borde: '>' estricto — en el umbral exacto rige la banda
     * inferior (lectura "hasta X" inclusiva del texto normativo).
     * (Corrección normativa: versiones previas usaban 3/4/5 con
     * umbrales 3000/4000, una banda corrida respecto del Art. 247.)
     */
    explicit RegulatoryConfig(
        RegulatoryStandard standard           = RegulatoryStandard::DS024_Peru,
        double min_flow_per_person_m3min      = 3.0,     // Art. 247: hasta 1,500 msnm
        double altitude_threshold_1_masl      = 1500.0,
        double flow_per_person_above_t1       = 4.0,     // Art. 247: 1,500–3,000
        double altitude_threshold_2_masl      = 3000.0,
        double flow_per_person_above_t2       = 5.0,     // Art. 247: 3,000–4,000
        double altitude_threshold_3_masl      = 4000.0,
        double flow_per_person_above_t3       = 6.0,     // Art. 247: sobre 4,000
        double diesel_hp_factor_m3min         = 3.0,
        double max_dilution_time_min          = 30.0,
        double default_gas_volume_per_kg_m3   = 0.04,
        double default_leakage_factor         = 0.15
    )
        : standard_(standard)
        , min_flow_per_person_(min_flow_per_person_m3min)
        , altitude_threshold_1_(altitude_threshold_1_masl)
        , flow_above_t1_(flow_per_person_above_t1)
        , altitude_threshold_2_(altitude_threshold_2_masl)
        , flow_above_t2_(flow_per_person_above_t2)
        , altitude_threshold_3_(altitude_threshold_3_masl)
        , flow_above_t3_(flow_per_person_above_t3)
        , diesel_hp_factor_(diesel_hp_factor_m3min)
        , max_dilution_time_(max_dilution_time_min)
        , default_gas_volume_(default_gas_volume_per_kg_m3)
        , default_leakage_factor_(default_leakage_factor)
    {
        validate();
    }

    // ---- Presets normativos (factory) ----

    /// Preset oficial peruano — DS 024-2016-EM / DS 023-2017-EM.
    /// Equivale exactamente a los defaults del constructor.
    [[nodiscard]] static RegulatoryConfig peru() {
        return RegulatoryConfig{};
    }

    /**
     * @brief Preset oficial chileno — DS 132, Reglamento de Seguridad Minera.
     *
     * Valores validados contra el texto vigente (LeyChile idNorma=221064,
     * versión 09-abr-2024):
     * - 3,0 m³/min por persona: Art. 138. El DS 132 NO escala este caudal por
     *   altitud (a diferencia del DS 024 peruano), por lo que los escalones se
     *   neutralizan (mismo caudal en todos los tramos).
     * - 2,83 m³/min por HP efectivo al freno: Art. 132 (mínimo cuando el
     *   fabricante no especifica caudal; el caudal por personas siempre se suma).
     * - Tiempo de dilución 30 min: NO regulado por DS 132 (el reingreso
     *   post-tronadura lo gobiernan Arts. 156, 571 y 585) — default ingenieril.
     * - Volumen de gases 0,04 m³/kg: NO regulado por DS 132 — default ingenieril.
     * - Factor de fugas 0,15: NO regulado por DS 132 como factor de ducto (el
     *   15% del Art. 139 es tolerancia del balance general de mina) — default
     *   ingenieril, coincidente en número con dicha tolerancia.
     *
     * Umbrales operacionales diésel del Art. 135 (paralización de equipos):
     * CO 40 ppm, NOx 20 ppm, aldehído fórmico 1,6 ppm — ver limites_gases.hpp.
     */
    [[nodiscard]] static RegulatoryConfig chile() {
        return RegulatoryConfig{
            RegulatoryStandard::DS132_Chile,
            /* min_flow_per_person_m3min */    3.0,    // Art. 138
            /* altitude_threshold_1_masl */    1500.0, // sin efecto (ver Doxygen)
            /* flow_per_person_above_t1 */     3.0,    // Art. 138 (sin escalón)
            /* altitude_threshold_2_masl */    3000.0, // sin efecto
            /* flow_per_person_above_t2 */     3.0,    // Art. 138 (sin escalón)
            /* altitude_threshold_3_masl */    4000.0, // sin efecto
            /* flow_per_person_above_t3 */     3.0,    // Art. 138 (sin escalón)
            /* diesel_hp_factor_m3min */       2.83,   // Art. 132
            /* max_dilution_time_min */        30.0,   // no regulado por DS 132
            /* default_gas_volume_per_kg_m3 */ 0.04,   // no regulado por DS 132
            /* default_leakage_factor */       0.15    // no regulado por DS 132
        };
    }

    /// Construye el preset oficial de la norma indicada.
    /// @throws std::invalid_argument si la norma no tiene preset implementado.
    [[nodiscard]] static RegulatoryConfig for_standard(RegulatoryStandard standard) {
        switch (standard) {
            case RegulatoryStandard::DS024_Peru:  return peru();
            case RegulatoryStandard::DS132_Chile: return chile();
        }
        throw std::invalid_argument(
            "[VentPy] for_standard: unsupported regulatory standard");
    }

    // ---- Getters (const, inmutables) ----

    [[nodiscard]] RegulatoryStandard standard() const noexcept { return standard_; }

    /// DS 024-2016-EM, Art. 236: Mínimo 3 m³/min por persona.
    [[nodiscard]] double min_flow_per_person() const noexcept { return min_flow_per_person_; }

    /// Umbral altitud nivel 1 [msnm]
    [[nodiscard]] double altitude_threshold_1() const noexcept { return altitude_threshold_1_; }

    /// Caudal por persona cuando altitud > umbral 1 [m³/min]
    [[nodiscard]] double flow_above_threshold_1() const noexcept { return flow_above_t1_; }

    /// Umbral altitud nivel 2 [msnm]
    [[nodiscard]] double altitude_threshold_2() const noexcept { return altitude_threshold_2_; }

    /// Caudal por persona cuando altitud > umbral 2 [m³/min]
    [[nodiscard]] double flow_above_threshold_2() const noexcept { return flow_above_t2_; }

    /// Umbral altitud nivel 3 [msnm]
    [[nodiscard]] double altitude_threshold_3() const noexcept { return altitude_threshold_3_; }

    /// Caudal por persona cuando altitud > umbral 3 [m³/min] (Art. 247: 6)
    [[nodiscard]] double flow_above_threshold_3() const noexcept { return flow_above_t3_; }

    /// DS 024-2016-EM, Art. 246: Factor HP × m³/min para equipos diésel.
    [[nodiscard]] double diesel_hp_factor() const noexcept { return diesel_hp_factor_; }

    /// DS 024-2016-EM, Art. 243: Tiempo máximo de dilución [min].
    [[nodiscard]] double max_dilution_time() const noexcept { return max_dilution_time_; }

    /// Volumen de gases por defecto por kg de explosivo [m³/kg]
    [[nodiscard]] double default_gas_volume_per_kg() const noexcept { return default_gas_volume_; }

    /// Factor de fugas por defecto (fracción, ej. 0.15 = 15%)
    [[nodiscard]] double default_leakage_factor() const noexcept { return default_leakage_factor_; }

    /// Retorna la referencia normativa como string legible.
    [[nodiscard]] std::string standard_name() const {
        switch (standard_) {
            case RegulatoryStandard::DS024_Peru:
                return "DS 024-2016-EM / DS 023-2017-EM (Peru)";
            case RegulatoryStandard::DS132_Chile:
                return "DS 132 Reglamento de Seguridad Minera (Chile)";
            default:
                return "Unknown Standard";
        }
    }

private:
    RegulatoryStandard standard_;
    double min_flow_per_person_;
    double altitude_threshold_1_;
    double flow_above_t1_;
    double altitude_threshold_2_;
    double flow_above_t2_;
    double altitude_threshold_3_;
    double flow_above_t3_;
    double diesel_hp_factor_;
    double max_dilution_time_;
    double default_gas_volume_;
    double default_leakage_factor_;

    /**
     * @brief Validación de dominio en construcción.
     * @throws std::invalid_argument si algún parámetro es inválido
     */
    void validate() const {
        namespace v = ventpy::validation;
        v::require_positive(min_flow_per_person_,   "min_flow_per_person [m3/min]");
        v::require_non_negative(altitude_threshold_1_, "altitude_threshold_1 [msnm]");
        v::require_non_negative(altitude_threshold_2_, "altitude_threshold_2 [msnm]");
        v::require_non_negative(altitude_threshold_3_, "altitude_threshold_3 [msnm]");
        v::require_positive(flow_above_t1_,         "flow_above_threshold_1 [m3/min]");
        v::require_positive(flow_above_t2_,         "flow_above_threshold_2 [m3/min]");
        v::require_positive(flow_above_t3_,         "flow_above_threshold_3 [m3/min]");
        if (!(altitude_threshold_1_ < altitude_threshold_2_ &&
              altitude_threshold_2_ < altitude_threshold_3_)) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: los umbrales de altitud deben ser "
                "estrictamente crecientes (altitude_threshold_1 < 2 < 3).");
        }
        v::require_positive(diesel_hp_factor_,      "diesel_hp_factor [m3/min/HP]");
        v::require_positive(max_dilution_time_,     "max_dilution_time [min]");
        v::require_positive(default_gas_volume_,    "default_gas_volume_per_kg [m3/kg]");
        v::require_in_range(default_leakage_factor_, 0.0, 1.0,
                            "default_leakage_factor");
    }
};

/**
 * @brief Concepto de ventilación cuyo respaldo legal depende de la norma activa.
 *
 * Cada calculador cita su fuente a través de `regulation_reference`, de modo que
 * la referencia sigue a `RegulatoryConfig::standard()` y no a un literal fijo.
 * Es el único lugar del núcleo donde vive el texto de una cita normativa.
 */
enum class RegulatoryTopic {
    PersonnelFlow,              ///< Caudal mínimo por persona
    PersonnelAltitudeScale,     ///< Escalón de caudal por altitud (vacío si la norma no escala)
    DieselHpFactor,             ///< Caudal por HP de equipo diésel
    BlastingDilution,           ///< Dilución de gases de voladura
    BlastingDilutionTimeLimit,  ///< Etiqueta del tope de tiempo de dilución
    DustRespirableLimit,        ///< Límite de polvo respirable
    DustLimitBasis,             ///< Base del umbral de 3 mg/m³ en advertencias
    DustSilicaReferral,         ///< Remisión del límite específico de sílice
    ThermalBalance,             ///< Balance térmico sensible, sin piso de velocidad
    ThermalWithVelocityFloor,   ///< Balance combinado con el piso de velocidad térmico
    ThermalInfeasibleWithFloor, ///< Balance infactible, con piso de velocidad exigible
    ThermalInfeasibleNoFloor,   ///< Balance infactible, sin piso de velocidad aplicable
    ThermalStressReferral,      ///< Remisión a evaluación de estrés térmico (WBGT)
    AirVelocityLimits,          ///< Límites de velocidad del aire en labores
    CoverageZone,               ///< Cobertura de la demanda de aire por labor
    CoverageMine                ///< Balance de cobertura de la mina completa
};

/**
 * @brief Referencia normativa de un concepto bajo la norma indicada.
 *
 * Perú (DS 024-2016-EM / DS 023-2017-EM) y Chile (DS 132, Reglamento de
 * Seguridad Minera, texto vigente LeyChile idNorma=221064, versión
 * 09-abr-2024) regulan conjuntos distintos de conceptos. Donde el DS 132 NO
 * fija un valor —tiempo de dilución tras tronadura, volumen de gases por kg de
 * explosivo, factor de fugas de ducto— o donde esta librería no tiene una
 * referencia chilena verificada —polvo respirable, carga térmica, límites de
 * velocidad del aire, régimen de cobertura— la cita declara ese vacío y
 * atribuye el valor a criterio de ingeniería. Nunca se traslada un artículo
 * peruano a un cálculo chileno ni se cita un artículo sin verificar.
 *
 * @throws std::invalid_argument si la norma o el concepto no están soportados.
 */
[[nodiscard]] inline std::string regulation_reference(
    RegulatoryTopic topic,
    RegulatoryStandard standard
) {
    const bool peru = (standard == RegulatoryStandard::DS024_Peru);
    if (!peru && standard != RegulatoryStandard::DS132_Chile) {
        throw std::invalid_argument(
            "[VentPy] regulation_reference: norma sin referencias definidas");
    }

    switch (topic) {
        case RegulatoryTopic::PersonnelFlow:
            return peru
                ? "DS 024-2016-EM, Art. 236"
                : "DS 132, Art. 138";

        // El DS 132 fija 3,0 m3/min por persona sin escalon por altitud
        // (Art. 138), de modo que no hay clausula de escala que citar.
        case RegulatoryTopic::PersonnelAltitudeScale:
            return peru ? "Art. 247 escala altitud" : "";

        case RegulatoryTopic::DieselHpFactor:
            return peru
                ? "DS 024-2016-EM, Art. 246"
                : "DS 132, Art. 132";

        case RegulatoryTopic::BlastingDilution:
            return peru
                ? "DS 024-2016-EM, Art. 243-244"
                : "DS 132, Arts. 156, 571 y 585 (reingreso tras tronadura); "
                  "el DS 132 no fija tiempo de dilucion ni volumen de gases "
                  "por kg de explosivo - criterio de ingenieria";

        case RegulatoryTopic::BlastingDilutionTimeLimit:
            return peru
                ? "max normativo, Art. 243"
                : "max configurado, criterio de ingenieria: el DS 132 no fija "
                  "tiempo de dilucion";

        case RegulatoryTopic::DustRespirableLimit:
            return peru
                ? "DS 024-2016-EM, Art. 111 (LEO polvo respirable 3 mg/m3, "
                  "jornada 8 h; paralizacion si se supera)"
                : "DS 132: sin limite de polvo respirable verificado para "
                  "Chile en esta libreria; la concentracion objetivo es "
                  "criterio de ingenieria del usuario";

        case RegulatoryTopic::DustLimitBasis:
            return peru
                ? "LEO del Art. 111 (DS 024-2016-EM)"
                : "referencia ingenieril (el DS 132 no fija un limite de polvo "
                  "respirable verificado en esta libreria)";

        case RegulatoryTopic::DustSilicaReferral:
            return peru
                ? "el Anexo 15 / DS 015-2005-SA"
                : "la normativa de higiene ocupacional chilena aplicable (no "
                  "identificada en esta libreria)";

        case RegulatoryTopic::ThermalBalance:
            return peru
                ? "DS 024-2016-EM: balance termico sensible (criterio "
                  "ingenieril; carga = equipos + oxidacion, autocompresion "
                  "reduce el DT disponible)"
                : "DS 132: balance termico sensible (criterio ingenieril; "
                  "carga = equipos + oxidacion, autocompresion reduce el DT "
                  "disponible); el DS 132 no fija un criterio de ventilacion "
                  "termica identificado en esta libreria";

        case RegulatoryTopic::ThermalWithVelocityFloor:
            return peru
                ? "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min con "
                  "temperatura seca 24-29 C) combinado con balance termico "
                  "sensible (criterio ingenieril)"
                : "DS 132: piso de velocidad de 30 m/min con temperatura seca "
                  "24-29 C aplicado como criterio de ingenieria (el DS 132 no "
                  "fija un criterio de ventilacion termica identificado en "
                  "esta libreria), combinado con balance termico sensible";

        case RegulatoryTopic::ThermalInfeasibleWithFloor:
            return peru
                ? "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min "
                  "con temperatura seca 24-29 C) — balance termico "
                  "sensible INFACTIBLE por autocompresion, se requiere "
                  "refrigeracion mecanica adicional para alcanzar el "
                  "objetivo termico"
                : "DS 132: piso de velocidad de 30 m/min con temperatura seca "
                  "24-29 C aplicado como criterio de ingenieria (el DS 132 no "
                  "fija un criterio de ventilacion termica identificado en "
                  "esta libreria) - balance termico sensible INFACTIBLE por "
                  "autocompresion, se requiere refrigeracion mecanica "
                  "adicional para alcanzar el objetivo termico";

        case RegulatoryTopic::ThermalInfeasibleNoFloor:
            return peru
                ? "DS 024-2016-EM: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)"
                : "DS 132: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)";

        case RegulatoryTopic::ThermalStressReferral:
            return peru
                ? "Art. 104 + Anexo 13"
                : "referencia chilena no identificada en esta libreria";

        case RegulatoryTopic::AirVelocityLimits:
            return peru
                ? "DS 024-2016-EM, Art. 248"
                : "limites configurados en CoverageParams; el DS 132 no fija "
                  "limites de velocidad del aire identificados en esta "
                  "libreria";

        case RegulatoryTopic::CoverageZone:
            return peru
                ? "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252 lit. g - "
                  "cobertura de la demanda de aire por labor"
                : "DS 132: cobertura de la demanda de aire por labor - "
                  "criterio de ingenieria; el DS 132 no fija un regimen de "
                  "evaluacion de cobertura identificado en esta libreria";

        case RegulatoryTopic::CoverageMine:
            return peru
                ? "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252: evaluacion "
                  "integral semestral; lit. f (cobertura de mina) y lit. g "
                  "(cobertura por labor)"
                : "DS 132: balance de cobertura de la mina - criterio de "
                  "ingenieria; el DS 132 no fija una evaluacion integral de "
                  "cobertura identificada en esta libreria";
    }

    throw std::invalid_argument(
        "[VentPy] regulation_reference: concepto no soportado");
}

/// Sobrecarga por configuración: la cita sigue a `config.standard()`.
[[nodiscard]] inline std::string regulation_reference(
    RegulatoryTopic topic,
    const RegulatoryConfig& config
) {
    return regulation_reference(topic, config.standard());
}

} // namespace ventpy
