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
     */
    explicit RegulatoryConfig(
        RegulatoryStandard standard           = RegulatoryStandard::DS024_Peru,
        double min_flow_per_person_m3min      = 3.0,
        double altitude_threshold_1_masl      = 3000.0,
        double flow_per_person_above_t1       = 4.0,
        double altitude_threshold_2_masl      = 4000.0,
        double flow_per_person_above_t2       = 5.0,
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
     * versión 09-abr-2024) el 2026-08-17:
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
            /* altitude_threshold_1_masl */    3000.0, // sin efecto (ver Doxygen)
            /* flow_per_person_above_t1 */     3.0,    // Art. 138 (sin escalón)
            /* altitude_threshold_2_masl */    4000.0, // sin efecto
            /* flow_per_person_above_t2 */     3.0,    // Art. 138 (sin escalón)
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
        v::require_positive(flow_above_t1_,         "flow_above_threshold_1 [m3/min]");
        v::require_positive(flow_above_t2_,         "flow_above_threshold_2 [m3/min]");
        v::require_positive(diesel_hp_factor_,      "diesel_hp_factor [m3/min/HP]");
        v::require_positive(max_dilution_time_,     "max_dilution_time [min]");
        v::require_positive(default_gas_volume_,    "default_gas_volume_per_kg [m3/kg]");
        v::require_in_range(default_leakage_factor_, 0.0, 1.0,
                            "default_leakage_factor");
    }
};

} // namespace ventpy
