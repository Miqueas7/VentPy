/**
 * @file caudal_explosivos.hpp
 * @brief Cálculo robusto de caudal para dilución de gases de voladura (Q_Exp).
 *
 * El cálculo considera múltiples criterios:
 *
 * 1. DILUCIÓN DE CO A TLV (25 ppm):
 *    - CO es el gas más crítico post-voladura
 *    - Q = (CO_total × 24.45) / (28 × 25 × 10⁻⁶ × t)
 *
 * 2. DILUCIÓN DE NOx A TLV (5 ppm):
 *    - Gases nitrosos son muy tóxicos
 *    - Se considera como NO2 para cálculo conservador
 *
 * 3. RECAMBIO VOLUMÉTRICO:
 *    - Renovar el volumen de la labor n veces
 *    - n = factor de recambio (típico 3-5)
 *    - Q = (S × L × n) / t
 *
 * 4. VELOCIDAD MÍNIMA DE BARRIDO:
 *    - Asegurar velocidad ≥ 0.3 m/s en el frente
 *    - Q = S × v × 60
 *
 * 5. TIEMPO DE REINGRESO:
 *    - DS 024: máximo 30 min
 *    - Considera tiempo de medición con detector
 *
 * Referencias normativas (Perú, preset por defecto):
 * - DS 024-2016-EM, Art. 243: 30 min máximo de espera
 * - DS 024-2016-EM, Art. 244: Circuito de ventilación obligatorio
 * - DS 024-2016-EM, Art. 108: TLV CO=25ppm, NO2=5ppm
 *
 * La cita emitida en `regulation_ref` sigue a `config.standard()` a través de
 * `regulation_reference` (normativa.hpp). El DS 132 chileno NO fija tiempo de
 * dilución ni volumen de gases por kg: el reingreso tras la tronadura lo
 * gobiernan sus Arts. 156, 571 y 585, y los 30 min son criterio de ingeniería.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <sstream>

#include "ventpy/atmosphere.hpp"
#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Calculador robusto de caudal para dilución de gases de voladura.
 */
class BlastingFlowCalculator {
public:
    /**
     * @brief Calcula el caudal requerido (versión completa).
     *
     * @param params Parámetros de voladura
     * @param atm Parámetros atmosféricos
     * @param config Configuración normativa
     * @return Resultado detallado con múltiples criterios
     */
    [[nodiscard]] static BlastingFlowResult calculate_full(
        const BlastingParams& params,
        const AtmosphericParams& atm,
        const RegulatoryConfig& config
    ) {
        // --- Validaciones críticas ---
        validation::require_positive(params.explosive_kg,
            "explosive_kg [kg] - Cantidad de explosivo por disparo");
        validation::require_positive(params.dilution_time_min,
            "dilution_time_min [min] - Tiempo de dilucion (CRITICO: division por cero)");
        validation::require_positive(params.face_area_m2,
            "face_area_m2 [m2] - Seccion del frente");
        validation::require_positive(params.face_length_m,
            "face_length_m [m] - Longitud de la labor");

        BlastingFlowResult result;
        result.explosive_type = params.explosive_type;
        result.explosive_kg = params.explosive_kg;
        result.dilution_time_min = params.dilution_time_min;

        // --- Obtener factores de gases según tipo de explosivo ---
        auto [co_per_kg, nox_per_kg] = get_gas_factors(params);

        // --- Calcular gases generados ---
        result.co_generated_liters = params.explosive_kg * co_per_kg;
        result.nox_generated_liters = params.explosive_kg * nox_per_kg;

        // Volumen total de gases (CO + NOx + otros)
        double other_gases = params.explosive_kg * 20.0;  // N2, H2O vapor, etc.
        result.total_gas_volume_m3 = (result.co_generated_liters +
                                       result.nox_generated_liters +
                                       other_gases) / 1000.0;

        // Volumen de la labor
        result.face_volume_m3 = params.face_area_m2 * params.face_length_m;

        // Factor de corrección por altitud
        double volume_correction = AtmosphereCalculator::calculate_volume_correction_factor(
            atm.altitude_masl);

        // --- Criterio 1: Dilución de CO a TLV ---
        result.q_for_co_dilution = calculate_q_for_gas(
            result.co_generated_liters,
            params.target_co_ppm,
            params.dilution_time_min,
            28.01  // PM del CO
        ) * volume_correction;

        // --- Criterio 2: Dilución de NOx a TLV ---
        result.q_for_nox_dilution = calculate_q_for_gas(
            result.nox_generated_liters,
            params.target_nox_ppm,
            params.dilution_time_min,
            46.01  // PM del NO2
        ) * volume_correction;

        // --- Criterio 3: Recambio volumétrico ---
        // Factor de recambio: 3 para labores cortas, 5 para largas
        double exchange_factor = (params.face_length_m > 100) ? 5.0 : 3.0;
        result.q_for_volume_exchange = (result.face_volume_m3 * exchange_factor) /
                                       params.dilution_time_min;

        // --- Criterio 4: Velocidad mínima de barrido ---
        double min_velocity = std::max(params.min_velocity_mps,
                                       constants::MIN_VELOCITY_DEVELOPMENT_MPS);
        result.q_for_min_velocity = params.face_area_m2 * min_velocity * 60.0;

        // --- Tomar el máximo de todos los criterios ---
        result.q_blasting = std::max({
            result.q_for_co_dilution,
            result.q_for_nox_dilution,
            result.q_for_volume_exchange,
            result.q_for_min_velocity
        });

        // Redondeo de seguridad
        result.q_blasting = safety_ceil(result.q_blasting);

        // Identificar criterio gobernante
        result.governing_criterion = identify_governing(
            result.q_for_co_dilution,
            result.q_for_nox_dilution,
            result.q_for_volume_exchange,
            result.q_for_min_velocity
        );

        // Referencia normativa
        result.regulation_ref = build_regulation_ref(params, config, result);

        return result;
    }

    /**
     * @brief Versión simplificada (compatibilidad con API anterior).
     */
    [[nodiscard]] static BlastingFlowResult calculate(
        const BlastingParams& params,
        const RegulatoryConfig& config
    ) {
        validation::require_positive(params.explosive_kg,
            "explosive_kg [kg] - Cantidad de explosivo por disparo");
        validation::require_positive(params.dilution_time_min,
            "dilution_time_min [min] - Tiempo de dilucion");
        // 0 = sentinel "usar factores del tipo de explosivo"; negativo es dato inválido
        validation::require_non_negative(params.gas_volume_per_kg,
            "gas_volume_per_kg [m3/kg] - Volumen de gases por kg (0 = usar tipo)");

        BlastingFlowResult result;
        result.explosive_type = params.explosive_type;
        result.explosive_kg = params.explosive_kg;
        result.dilution_time_min = params.dilution_time_min;

        double gas_volume_per_kg = params.gas_volume_per_kg;
        if (gas_volume_per_kg <= 0.0) {
            auto [co, nox] = get_gas_factors(params);
            gas_volume_per_kg = (co + nox) / 1000.0;
        }

        result.co_generated_liters = 0.0;
        result.nox_generated_liters = 0.0;
        result.total_gas_volume_m3 = params.explosive_kg * gas_volume_per_kg;
        result.face_volume_m3 = params.face_area_m2 * params.face_length_m;
        result.q_for_co_dilution = 0.0;
        result.q_for_nox_dilution = 0.0;
        result.q_for_volume_exchange =
            result.total_gas_volume_m3 / params.dilution_time_min;
        result.q_for_min_velocity = 0.0;
        result.q_blasting = safety_ceil(result.q_for_volume_exchange);
        result.governing_criterion = "volumen total";
        // Misma referencia auditable que calculate_full: incluye la ADVERTENCIA
        // si el tiempo de dilución excede el máximo normativo (Art. 243).
        result.regulation_ref = build_regulation_ref(params, config, result);
        return result;
    }

private:
    /**
     * @brief Obtiene factores de gases según tipo de explosivo.
     *
     * @return Tupla [CO L/kg, NOx L/kg]
     */
    [[nodiscard]] static std::tuple<double, double>
    get_gas_factors(const BlastingParams& params) {
        // Si están especificados manualmente, usarlos
        if (params.co_per_kg_liters > 0 && params.nox_per_kg_liters > 0) {
            return {params.co_per_kg_liters, params.nox_per_kg_liters};
        }

        // Usar valores por tipo de explosivo
        switch (params.explosive_type) {
            case ExplosiveType::ANFO:
                return {constants::ANFO_CO_L_PER_KG, constants::ANFO_NOX_L_PER_KG};
            case ExplosiveType::Emulsion:
                return {constants::EMULSION_CO_L_PER_KG, constants::EMULSION_NOX_L_PER_KG};
            case ExplosiveType::Dynamite:
                return {constants::DYNAMITE_CO_L_PER_KG, constants::DYNAMITE_NOX_L_PER_KG};
            case ExplosiveType::WaterGel:
                return {35.0, 8.0};  // Valores típicos hidrogel
            case ExplosiveType::ElectronicDet:
                return {5.0, 2.0};   // Solo detonadores
            default:
                return {constants::ANFO_CO_L_PER_KG, constants::ANFO_NOX_L_PER_KG};
        }
    }

    /**
     * @brief Calcula Q para diluir un gas a su TLV en tiempo t.
     *
     * Fórmula:
     * La concentración final después de dilución:
     * C = V_gas / (Q × t)
     *
     * Para alcanzar C = TLV:
     * Q = V_gas / (TLV × t)
     *
     * Donde V_gas está en litros y TLV en fracción (ppm / 10^6)
     *
     * @param gas_liters Volumen del gas generado [L]
     * @param tlv_ppm Límite de exposición [ppm]
     * @param time_min Tiempo de dilución [min]
     * @param molecular_weight Peso molecular (no usado en esta simplificación)
     * @return Caudal requerido [m³/min]
     */
    [[nodiscard]] static double calculate_q_for_gas(
        double gas_liters,
        double tlv_ppm,
        double time_min,
        [[maybe_unused]] double molecular_weight
    ) {
        if (gas_liters <= 0 || tlv_ppm <= 0 || time_min <= 0) return 0.0;

        // Q = V_gas / (TLV × t)
        // V_gas en L, queremos Q en m³/min
        // TLV en ppm = partes por millón = fracción × 10^6
        double tlv_fraction = tlv_ppm / 1e6;
        double q_lpm = gas_liters / (tlv_fraction * time_min);
        double q_m3min = q_lpm / 1000.0;

        // Factor de seguridad del 20%
        return q_m3min * 1.2;
    }

    /**
     * @brief Identifica cuál criterio gobierna el cálculo.
     */
    [[nodiscard]] static std::string identify_governing(
        double q_co, double q_nox, double q_vol, double q_vel
    ) {
        double max_q = std::max({q_co, q_nox, q_vol, q_vel});

        if (max_q == q_co) return "dilucion CO";
        if (max_q == q_nox) return "dilucion NOx";
        if (max_q == q_vol) return "recambio volumetrico";
        return "velocidad minima";
    }

    /**
     * @brief Construye referencia normativa detallada.
     */
    [[nodiscard]] static std::string build_regulation_ref(
        const BlastingParams& params,
        const RegulatoryConfig& config,
        const BlastingFlowResult& result
    ) {
        std::ostringstream oss;
        oss << regulation_reference(RegulatoryTopic::BlastingDilution, config);

        // Advertencia si el tiempo excede el tope aplicable. El tope es
        // normativo bajo el DS 024 y meramente configurado bajo el DS 132,
        // que no fija tiempo de dilución: la etiqueta lo distingue.
        if (params.dilution_time_min > config.max_dilution_time()) {
            oss << " [ADVERTENCIA: t=" << params.dilution_time_min
                << "min > " << config.max_dilution_time() << "min ("
                << regulation_reference(
                       RegulatoryTopic::BlastingDilutionTimeLimit, config)
                << ")]";
        }

        oss << " [Gobernante: " << result.governing_criterion << "]";

        return oss.str();
    }
};

} // namespace ventpy
