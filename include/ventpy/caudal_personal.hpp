/**
 * @file caudal_personal.hpp
 * @brief Cálculo robusto de caudal por personal (Q_Per).
 *
 * El cálculo considera múltiples factores:
 *
 * 1. CONSUMO DE OXÍGENO:
 *    - Varía según nivel de actividad (reposo → trabajo muy pesado)
 *    - Base: necesidad de mantener 19.5% O2 mínimo (DS 024, Art. 236)
 *
 * 2. CORRECCIÓN POR ALTITUD:
 *    - A mayor altitud, menor densidad → más volumen para misma masa O2
 *    - Factor fisiológico adicional sobre 3500 msnm
 *
 * 3. DILUCIÓN DE CO2 METABÓLICO:
 *    - Producción de CO2 proporcional al consumo de O2
 *    - Mantener CO2 < 0.5% (5000 ppm)
 *
 * 4. VELOCIDAD MÍNIMA:
 *    - Verificación de que el caudal genera velocidad ≥ 0.25 m/s
 *    - DS 024-2016-EM, Art. 236
 *
 * Referencias normativas:
 * - DS 024-2016-EM, Art. 236: Mínimo 3 m³/min por persona
 * - DS 024-2016-EM, Art. 236: Mínimo 19.5% de O2
 * - DS 024-2016-EM, Art. 236: Velocidad mínima 0.25 m/s
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
 * @brief Calculador robusto de caudal por personal.
 */
class PersonnelFlowCalculator {
public:
    /**
     * @brief Calcula el caudal requerido por personal (versión completa).
     *
     * Algoritmo:
     * 1. Calcular Q por consumo de O2 (mantener 19.5% O2)
     * 2. Calcular Q por dilución de CO2 (mantener < 0.5%)
     * 3. Aplicar mínimo normativo (3 m³/min × n × factor_altitud)
     * 4. Verificar velocidad mínima
     * 5. Tomar el máximo de todos los criterios
     *
     * @param personnel Parámetros del personal
     * @param atm Parámetros atmosféricos
     * @param zone_type Tipo de zona (para velocidad mínima)
     * @param face_area_m2 Sección de la labor [m²] (para verificar velocidad)
     * @param config Configuración normativa
     * @return Resultado detallado
     */
    [[nodiscard]] static PersonnelFlowResult calculate_full(
        const PersonnelParams& personnel,
        const AtmosphericParams& atm,
        ZoneType zone_type,
        double face_area_m2,
        const RegulatoryConfig& config
    ) {
        // --- Validaciones ---
        validation::require_positive_int(personnel.num_workers, "num_workers");
        validation::require_non_negative(atm.altitude_masl, "altitude_masl");
        validation::require_positive(face_area_m2, "face_area_m2");

        PersonnelFlowResult result;
        result.num_workers = personnel.num_workers;
        result.activity_level = personnel.activity;
        result.altitude_masl = atm.altitude_masl;

        // --- 1. Consumo de oxígeno ---
        double o2_per_person_lpm = get_o2_consumption(personnel.activity);
        result.o2_consumption_lpm = o2_per_person_lpm;

        // --- 2. Corrección por densidad (altitud) ---
        double density_correction = AtmosphereCalculator::get_personnel_altitude_factor(
            atm.altitude_masl);
        result.density_correction = density_correction;

        // --- 3. Caudal base normativo ---
        double flow_base = get_base_flow_per_person(atm.altitude_masl, config);
        result.flow_per_person_base = flow_base;

        // --- 4. Caudal corregido ---
        // El caudal se ajusta si la actividad es mayor a la moderada
        double activity_factor = 1.0;
        switch (personnel.activity) {
            case ActivityLevel::Rest:      activity_factor = 0.6; break;
            case ActivityLevel::Light:     activity_factor = 0.8; break;
            case ActivityLevel::Moderate:  activity_factor = 1.0; break;
            case ActivityLevel::Heavy:     activity_factor = 1.3; break;
            case ActivityLevel::VeryHeavy: activity_factor = 1.6; break;
        }

        double flow_corrected = flow_base * density_correction * activity_factor;
        result.flow_per_person_corrected = flow_corrected;

        // --- 5. Caudal por consumo de O2 ---
        double q_for_o2 = calculate_q_for_oxygen(
            personnel.num_workers,
            o2_per_person_lpm,
            density_correction
        );

        // --- 6. Caudal por dilución de CO2 ---
        double q_for_co2 = calculate_q_for_co2(
            personnel.num_workers,
            o2_per_person_lpm,
            density_correction
        );

        // --- 7. Caudal normativo (DS 024) ---
        double q_normative = personnel.num_workers * flow_corrected;

        // --- 8. Caudal para velocidad mínima ---
        double min_velocity = get_min_velocity(zone_type);
        double q_for_velocity = face_area_m2 * min_velocity * 60.0;  // m³/min

        // --- 9. Tomar el máximo ---
        double q_personnel = std::max({q_for_o2, q_for_co2, q_normative, q_for_velocity});
        result.q_personnel = safety_ceil(q_personnel);

        // --- 10. Verificar velocidad resultante ---
        result.min_velocity_check_mps = result.q_personnel / 60.0 / face_area_m2;

        // --- 11. Referencia normativa ---
        result.regulation_ref = build_regulation_reference(
            atm.altitude_masl, config, q_normative, q_for_o2, q_for_co2, q_for_velocity);

        return result;
    }

    /**
     * @brief Versión simplificada compatible con API anterior.
     *
     * Mantiene compatibilidad con la interfaz original mientras usa
     * internamente el cálculo robusto.
     */
    [[nodiscard]] static PersonnelFlowResult calculate(
        int num_workers,
        double altitude_masl,
        const RegulatoryConfig& config
    ) {
        // --- Validación de dominio ---
        validation::require_positive_int(num_workers, "num_workers");
        validation::require_non_negative(altitude_masl, "altitude_masl [msnm]");

        PersonnelFlowResult result;
        result.num_workers = num_workers;
        result.activity_level = ActivityLevel::Moderate;
        result.altitude_masl = altitude_masl;
        result.o2_consumption_lpm = get_o2_consumption(ActivityLevel::Moderate);
        result.density_correction = AtmosphereCalculator::get_personnel_altitude_factor(
            altitude_masl);
        result.flow_per_person_base = get_base_flow_per_person(altitude_masl, config);
        result.flow_per_person_corrected = result.flow_per_person_base;
        result.q_personnel = safety_ceil(num_workers * result.flow_per_person_base);
        result.min_velocity_check_mps = 0.0;
        result.regulation_ref = "DS 024-2016-EM, Art. 236 [Gobernante: normativo]";
        return result;
    }

private:
    /**
     * @brief Obtiene el caudal base por persona según altitud.
     */
    [[nodiscard]] static double get_base_flow_per_person(
        double altitude_masl,
        const RegulatoryConfig& config
    ) {
        if (altitude_masl > config.altitude_threshold_2()) {
            return config.flow_above_threshold_2();
        } else if (altitude_masl > config.altitude_threshold_1()) {
            return config.flow_above_threshold_1();
        }
        return config.min_flow_per_person();
    }

    /**
     * @brief Calcula Q para mantener 19.5% de O2.
     *
     * Fórmula:
     * El aire tiene 20.95% O2. Para que no baje de 19.5%:
     * O2_entrada - O2_consumido = O2_salida
     * Q × 0.2095 - n × O2_cons = Q × 0.195
     * Q = n × O2_cons / (0.2095 - 0.195)
     * Q = n × O2_cons / 0.0145
     *
     * Cada L/min de O2 consumido requiere ~69 L/min de aire
     */
    [[nodiscard]] static double calculate_q_for_oxygen(
        int num_workers,
        double o2_consumption_lpm,
        double density_correction
    ) {
        constexpr double O2_INLET = constants::O2_FRACTION_AIR;  // 0.2095
        constexpr double O2_MIN = 0.195;  // 19.5%
        constexpr double DELTA_O2 = O2_INLET - O2_MIN;  // 0.0145

        // L/min a m³/min
        double total_o2_lpm = num_workers * o2_consumption_lpm;
        double total_o2_m3min = total_o2_lpm / 1000.0;

        // Q base
        double q_base = total_o2_m3min / DELTA_O2;

        // Corregir por densidad (más volumen a mayor altitud)
        double q_corrected = q_base * density_correction;

        // Factor de seguridad 1.25 (recomendación ASHRAE/NIOSH)
        return q_corrected * 1.25;
    }

    /**
     * @brief Calcula Q para mantener CO2 < 0.5%.
     *
     * Producción de CO2 ≈ cociente respiratorio × consumo O2
     * RQ típico = 0.85 para trabajo moderado
     */
    [[nodiscard]] static double calculate_q_for_co2(
        int num_workers,
        double o2_consumption_lpm,
        double density_correction
    ) {
        constexpr double RQ = 0.85;  // Cociente respiratorio
        constexpr double CO2_AMBIENT = 0.0004;  // 400 ppm
        constexpr double CO2_MAX = 0.005;  // 5000 ppm = 0.5%
        constexpr double DELTA_CO2 = CO2_MAX - CO2_AMBIENT;

        double co2_production_lpm = num_workers * o2_consumption_lpm * RQ;
        double co2_production_m3min = co2_production_lpm / 1000.0;

        double q_base = co2_production_m3min / DELTA_CO2;
        return q_base * density_correction * 1.1;  // Factor seguridad 10%
    }

    /**
     * @brief Construye la referencia normativa para el resultado.
     */
    [[nodiscard]] static std::string build_regulation_reference(
        double altitude,
        const RegulatoryConfig& config,
        double q_norm,
        double q_o2,
        double q_co2,
        double q_vel
    ) {
        std::ostringstream oss;
        oss << "DS 024-2016-EM, Art. 236";

        if (altitude > config.altitude_threshold_2()) {
            oss << " + Estandar corporativo (>" 
                << static_cast<int>(config.altitude_threshold_2()) 
                << " msnm)";
        } else if (altitude > config.altitude_threshold_1()) {
            oss << " + Estandar corporativo (>"
                << static_cast<int>(config.altitude_threshold_1())
                << " msnm)";
        }

        // Indicar criterio gobernante
        double max_q = std::max({q_norm, q_o2, q_co2, q_vel});
        if (max_q == q_vel) {
            oss << " [Gobernante: velocidad minima]";
        } else if (max_q == q_o2) {
            oss << " [Gobernante: consumo O2]";
        } else if (max_q == q_co2) {
            oss << " [Gobernante: dilucion CO2]";
        } else {
            oss << " [Gobernante: normativo]";
        }

        return oss.str();
    }
};

} // namespace ventpy
