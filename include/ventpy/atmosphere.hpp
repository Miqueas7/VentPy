/**
 * @file atmosphere.hpp
 * @brief Correcciones atmosféricas para cálculos de ventilación.
 *
 * La altitud afecta significativamente:
 * - Densidad del aire (menor a mayor altitud)
 * - Presión parcial de oxígeno
 * - Rendimiento de motores diésel (de-rating)
 * - Capacidad de dilución de gases
 *
 * Modelo: Atmósfera Estándar Internacional (ISA)
 *
 * DS 024-2016-EM: La mayoría de minas peruanas operan entre
 * 3000 y 5000 msnm, donde estos efectos son significativos.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <cmath>
#include <algorithm>

#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Calculador de propiedades atmosféricas y correcciones.
 *
 * Implementa el modelo ISA (International Standard Atmosphere)
 * para altitudes hasta 11,000 m (troposfera).
 */
class AtmosphereCalculator {
public:
    /**
     * @brief Calcula densidad del aire a partir de presión barométrica medida.
     *
     * Útil cuando el usuario dispone de una presión observada y se quiere
     * mantener consistencia entre presión, densidad y factor volumétrico.
     *
     * @param pressure_kpa Presión barométrica [kPa]
     * @param temperature_c Temperatura del aire [°C]
     * @return Densidad del aire [kg/m³]
     */
    [[nodiscard]] static double calculate_density_from_pressure_kpa(
        double pressure_kpa,
        double temperature_c = 15.0
    ) {
        validation::require_positive(pressure_kpa, "barometric_pressure_kpa");

        double pressure_pa = pressure_kpa * 1000.0;
        double temp_k = temperature_c + 273.15;

        return pressure_pa / (constants::GAS_CONSTANT_AIR * temp_k);
    }

    /**
     * @brief Calcula la presión atmosférica a una altitud dada.
     *
     * Fórmula barométrica (ISA):
     * P = P₀ × (1 - L×h/T₀)^(g×M/(R×L))
     *
     * Donde:
     * - P₀ = 101.325 kPa (presión a nivel del mar)
     * - L = 0.0065 K/m (gradiente térmico)
     * - T₀ = 288.15 K (temperatura a nivel del mar)
     * - g = 9.81 m/s²
     * - M = 0.0289644 kg/mol (masa molar del aire)
     * - R = 8.31447 J/(mol·K) (constante gases)
     *
     * Simplificado: P = P₀ × (1 - 0.0000226 × h)^5.256
     *
     * @param altitude_masl Altitud sobre nivel del mar [msnm]
     * @return Presión atmosférica [kPa]
     */
    [[nodiscard]] static double calculate_pressure_kpa(double altitude_masl) {
        validation::require_non_negative(altitude_masl, "altitude_masl");

        // Límite troposfera
        double h = std::min(altitude_masl, 11000.0);

        constexpr double P0 = constants::SEA_LEVEL_PRESSURE_KPA;
        constexpr double T0 = constants::SEA_LEVEL_TEMP_K;
        constexpr double L = constants::LAPSE_RATE_K_PER_M;
        constexpr double g = constants::GRAVITY_M_S2;
        constexpr double M = 0.0289644;  // kg/mol
        constexpr double R = 8.31447;    // J/(mol·K)

        double exponent = (g * M) / (R * L);  // ≈ 5.256
        double pressure = P0 * std::pow(1.0 - (L * h / T0), exponent);

        return pressure;
    }

    /**
     * @brief Calcula la densidad del aire a una altitud dada.
     *
     * ρ = P / (R_air × T)
     *
     * @param altitude_masl Altitud [msnm]
     * @param temperature_c Temperatura del aire [°C]
     * @return Densidad del aire [kg/m³]
     */
    [[nodiscard]] static double calculate_density_kg_m3(
        double altitude_masl,
        double temperature_c = 15.0
    ) {
        return calculate_density_from_pressure_kpa(
            calculate_pressure_kpa(altitude_masl), temperature_c);
    }

    /**
     * @brief Calcula el ratio de densidad respecto al nivel del mar.
     *
     * ρ_ratio = ρ_altitude / ρ_sea_level
     *
     * Este factor se usa para:
     * - Corregir caudales volumétricos
     * - Ajustar capacidad de dilución
     *
     * @param altitude_masl Altitud [msnm]
     * @param temperature_c Temperatura [°C]
     * @return Ratio de densidad [0-1]
     */
    [[nodiscard]] static double calculate_density_ratio(
        double altitude_masl,
        double temperature_c = 15.0
    ) {
        double density = calculate_density_kg_m3(altitude_masl, temperature_c);
        return density / constants::SEA_LEVEL_DENSITY_KG_M3;
    }

    /**
     * @brief Calcula el factor de corrección volumétrico.
     *
     * A mayor altitud, se requiere más volumen de aire para
     * obtener la misma masa de aire (y oxígeno).
     *
     * Factor = 1 / density_ratio = ρ₀ / ρ
     *
     * Ejemplo: A 4000 msnm, factor ≈ 1.5
     * (se necesita 50% más volumen de aire)
     *
     * @param altitude_masl Altitud [msnm]
     * @param temperature_c Temperatura [°C]
     * @return Factor de corrección volumétrico [≥1]
     */
    [[nodiscard]] static double calculate_volume_correction_factor(
        double altitude_masl,
        double temperature_c = 15.0
    ) {
        double ratio = calculate_density_ratio(altitude_masl, temperature_c);
        return (ratio > 0.0) ? (1.0 / ratio) : 1.0;
    }

    /**
     * @brief Calcula la presión parcial de oxígeno.
     *
     * P_O2 = P_atm × 0.2095
     *
     * DS 024-2016-EM, Art. 236: Mínimo 19.5% de O2.
     * A mayor altitud, aunque el porcentaje es el mismo,
     * la presión parcial (y por tanto la disponibilidad
     * fisiológica) es menor.
     *
     * @param altitude_masl Altitud [msnm]
     * @return Presión parcial de O2 [kPa]
     */
    [[nodiscard]] static double calculate_o2_partial_pressure_kpa(
        double altitude_masl
    ) {
        double pressure = calculate_pressure_kpa(altitude_masl);
        return pressure * constants::O2_FRACTION_AIR;
    }

    /**
     * @brief Calcula el factor de de-rating para motores diésel.
     *
     * Los motores diésel naturalmente aspirados pierden aproximadamente
     * 3% de potencia por cada 300 m de altitud sobre 1000 m.
     *
     * Los motores turboalimentados tienen mejor comportamiento,
     * pero aún pierden aproximadamente 1.5-2% por cada 300 m.
     *
     * Fórmula simplificada (motor NA):
     * Derate = 1.0 - 0.03 × ((h - 1000) / 300)  para h > 1000
     *
     * @param altitude_masl Altitud [msnm]
     * @param is_turbocharged ¿Motor turboalimentado?
     * @return Factor de de-rating [0-1]
     */
    [[nodiscard]] static double calculate_diesel_derate_factor(
        double altitude_masl,
        bool is_turbocharged = true
    ) {
        if (altitude_masl <= 1000.0) {
            return 1.0;  // Sin pérdida bajo 1000 m
        }

        double altitude_above_1000 = altitude_masl - 1000.0;

        // Factor de pérdida por cada 300 m
        double loss_per_300m = is_turbocharged ? 0.015 : 0.03;

        double total_loss = loss_per_300m * (altitude_above_1000 / 300.0);

        // Limitar pérdida máxima al 50%
        total_loss = std::min(total_loss, 0.50);

        return 1.0 - total_loss;
    }

    /**
     * @brief Calcula temperatura del aire considerando gradiente.
     *
     * T = T₀ - L × h
     *
     * @param altitude_masl Altitud [msnm]
     * @param surface_temp_c Temperatura en superficie [°C]
     * @return Temperatura estimada [°C]
     */
    [[nodiscard]] static double calculate_temperature_at_altitude(
        double altitude_masl,
        double surface_temp_c = 15.0
    ) {
        // Gradiente térmico estándar: -6.5°C por 1000 m
        constexpr double lapse_rate_c_per_m = 0.0065;
        return surface_temp_c - (lapse_rate_c_per_m * altitude_masl);
    }

    /**
     * @brief Calcula todas las correcciones atmosféricas.
     *
     * Método conveniente que retorna todos los valores calculados
     * en una estructura para auditoría.
     *
     * @param params Parámetros atmosféricos
     * @return Estructura con todas las correcciones
     */
    [[nodiscard]] static AtmosphericCorrections calculate_all(
        const AtmosphericParams& params
    ) {
        validation::require_non_negative(params.altitude_masl, "altitude_masl");

        AtmosphericCorrections result;
        result.altitude_masl = params.altitude_masl;

        // Presión: usar la proporcionada o calcular
        if (params.barometric_pressure_kpa > 0.0) {
            result.pressure_kpa = params.barometric_pressure_kpa;
        } else {
            result.pressure_kpa = calculate_pressure_kpa(params.altitude_masl);
        }

        // Densidad y correcciones
        result.air_density_kg_m3 = calculate_density_from_pressure_kpa(
            result.pressure_kpa, params.dry_bulb_temp_c);

        result.density_ratio = result.air_density_kg_m3 /
                               constants::SEA_LEVEL_DENSITY_KG_M3;

        result.volume_correction_factor = 1.0 / result.density_ratio;

        result.oxygen_partial_pressure_kpa =
            result.pressure_kpa * constants::O2_FRACTION_AIR;

        // Nota informativa
        if (params.altitude_masl > 4000.0) {
            result.notes = "ALTITUD EXTREMA: Considerar efectos fisiologicos "
                           "adicionales y posible necesidad de oxigeno suplementario.";
        } else if (params.altitude_masl > 3000.0) {
            result.notes = "Altitud elevada: Correcciones significativas aplicadas.";
        } else {
            result.notes = "Altitud moderada.";
        }

        return result;
    }

    /**
     * @brief Factor de ajuste para cálculo de caudal por personal.
     *
     * Considera que a mayor altitud:
     * 1. Se necesita más volumen para la misma masa de O2
     * 2. El consumo metabólico puede aumentar ligeramente
     *
     * @param altitude_masl Altitud [msnm]
     * @return Factor multiplicador para Q_personnel
     */
    [[nodiscard]] static double get_personnel_altitude_factor(
        double altitude_masl
    ) {
        // Factor volumétrico base
        double volume_factor = calculate_volume_correction_factor(altitude_masl);

        // Factor adicional por estrés fisiológico a gran altitud
        double physiological_factor = 1.0;
        if (altitude_masl > 4500.0) {
            physiological_factor = 1.10;  // +10% adicional
        } else if (altitude_masl > 3500.0) {
            physiological_factor = 1.05;  // +5% adicional
        }

        return volume_factor * physiological_factor;
    }
};

} // namespace ventpy
