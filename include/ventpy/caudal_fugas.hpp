/**
 * @file caudal_fugas.hpp
 * @brief Cálculo robusto de caudal por fugas en ductos de ventilación (Q_Fug).
 *
 * El cálculo considera múltiples factores:
 *
 * 1. TIPO DE DUCTO:
 *    - Flexible tela: mayor fuga (~2-3% por 100m)
 *    - Flexible PVC: fuga moderada (~1-2% por 100m)
 *    - Rígido acero: menor fuga (~0.5-1% por 100m)
 *
 * 2. CALIDAD DE INSTALACIÓN:
 *    - Pobre: 25-35% total
 *    - Promedio: 15-25% total
 *    - Buena: 10-15% total
 *    - Excelente: 5-10% total
 *
 * 3. LONGITUD DEL DUCTO:
 *    - Fugas incrementan con la longitud
 *    - Modelo exponencial o lineal según tipo
 *
 * 4. NÚMERO DE JUNTAS:
 *    - Cada junta aporta ~0.5-1% adicional
 *    - Juntas con abrazaderas vs. amarres
 *
 * 5. PRESIÓN DEL SISTEMA:
 *    - Mayor presión = mayores fugas
 *    - Sistemas de alta presión requieren ductos mejores
 *
 * Referencias normativas:
 * - DS 024-2016-EM, Art. 252: Mangas en buenas condiciones
 * - DS 024-2016-EM, Art. 252: Máximo 15m del frente
 *
 * Modelo: McPherson (Subsurface Ventilation Engineering)
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Calculador robusto de fugas en ductos de ventilación.
 */
class LeakageFlowCalculator {
public:
    /**
     * @brief Calcula fugas con modelo detallado.
     *
     * @param base_flow Caudal requerido en el frente [m³/min]
     * @param duct Parámetros del ducto
     * @param config Configuración normativa
     * @return Resultado detallado
     */
    [[nodiscard]] static LeakageFlowResult calculate_full(
        double base_flow,
        const DuctParams& duct,
        const RegulatoryConfig& config
    ) {
        validation::require_non_negative(base_flow, "base_flow [m3/min]");
        validation::require_positive(duct.duct_diameter_m, "duct_diameter_m");
        validation::require_non_negative(duct.duct_length_m, "duct_length_m");

        LeakageFlowResult result;
        result.duct_type = duct.duct_type;
        result.quality = duct.quality;
        result.duct_length_m = duct.duct_length_m;
        result.duct_diameter_m = duct.duct_diameter_m;
        result.num_joints = static_cast<int>(duct.num_joints);
        result.base_flow = base_flow;

        // --- 1. Factor base por tipo de ducto y calidad ---
        result.base_leakage_factor = get_base_leakage_factor(
            duct.duct_type, duct.quality);

        // --- 2. Factor adicional por longitud ---
        double leakage_per_100m = get_leakage_per_100m(duct);
        result.length_leakage_factor = (duct.duct_length_m / 100.0) * leakage_per_100m;

        // --- 3. Factor adicional por juntas ---
        double leak_per_joint = duct.leakage_per_joint > 0 ?
                                duct.leakage_per_joint : 0.005;  // 0.5% default
        result.joint_leakage_factor = duct.num_joints * leak_per_joint;

        // --- 4. Factor total (limitado a 80% máximo) ---
        result.total_leakage_factor = std::min(
            result.base_leakage_factor +
            result.length_leakage_factor +
            result.joint_leakage_factor,
            0.80  // Límite físico razonable
        );

        // --- 5. Calcular caudales ---
        result.q_leakage = base_flow * result.total_leakage_factor;
        result.q_at_fan = base_flow + result.q_leakage;

        // --- 6. Notas ---
        result.notes = build_notes(result, config);

        return result;
    }

    /**
     * @brief Versión simplificada con factor único (compatibilidad).
     */
    [[nodiscard]] static LeakageFlowResult calculate(
        double base_flow,
        std::optional<double> leakage_factor,
        const RegulatoryConfig& config
    ) {
        validation::require_non_negative(base_flow, "base_flow [m3/min]");

        double factor = leakage_factor.value_or(config.default_leakage_factor());
        validation::require_in_range(factor, 0.0, 1.0, "leakage_factor");

        LeakageFlowResult result;
        result.duct_type = DuctType::FlexiblePVC;
        result.quality = InstallationQuality::Average;
        result.duct_length_m = 0.0;
        result.duct_diameter_m = 0.6;
        result.num_joints = 0;
        result.base_flow = base_flow;

        result.base_leakage_factor = factor;
        result.length_leakage_factor = 0.0;
        result.joint_leakage_factor = 0.0;
        result.total_leakage_factor = factor;

        result.q_leakage = base_flow * factor;
        result.q_at_fan = base_flow + result.q_leakage;
        result.notes = "Factor de fugas simplificado";

        return result;
    }

    /**
     * @brief Sobrecarga que usa el factor por defecto.
     */
    [[nodiscard]] static LeakageFlowResult calculate(
        double base_flow,
        const RegulatoryConfig& config
    ) {
        return calculate(base_flow, std::nullopt, config);
    }

private:
    /**
     * @brief Obtiene factor base según tipo de ducto y calidad.
     *
     * Valores típicos de literatura (McPherson, Hartman).
     */
    [[nodiscard]] static double get_base_leakage_factor(
        DuctType type,
        InstallationQuality quality
    ) {
        // Matriz de factores base [tipo][calidad]
        // Valores en fracción (0.15 = 15%)

        double base = 0.0;

        // Factor por tipo de ducto
        switch (type) {
            case DuctType::FlexibleFabric:   base = 0.08; break;
            case DuctType::FlexiblePVC:      base = 0.05; break;
            case DuctType::RigidFiberglass:  base = 0.03; break;
            case DuctType::RigidSteel:       base = 0.02; break;
            case DuctType::SpiralSteel:      base = 0.015; break;
        }

        // Multiplicador por calidad de instalación
        double quality_mult = 1.0;
        switch (quality) {
            case InstallationQuality::Poor:      quality_mult = 2.5; break;
            case InstallationQuality::Average:   quality_mult = 1.5; break;
            case InstallationQuality::Good:      quality_mult = 1.0; break;
            case InstallationQuality::Excellent: quality_mult = 0.6; break;
        }

        return base * quality_mult;
    }

    /**
     * @brief Obtiene tasa de fuga por 100m de ducto.
     */
    [[nodiscard]] static double get_leakage_per_100m(const DuctParams& duct) {
        // Si está especificado, usarlo
        if (duct.leakage_per_100m > 0) {
            return duct.leakage_per_100m;
        }

        // Valores típicos por tipo de ducto
        switch (duct.duct_type) {
            case DuctType::FlexibleFabric:   return 0.025;  // 2.5% por 100m
            case DuctType::FlexiblePVC:      return 0.015;  // 1.5% por 100m
            case DuctType::RigidFiberglass:  return 0.010;  // 1.0% por 100m
            case DuctType::RigidSteel:       return 0.005;  // 0.5% por 100m
            case DuctType::SpiralSteel:      return 0.003;  // 0.3% por 100m
            default:                         return 0.015;
        }
    }

    /**
     * @brief Construye notas informativas.
     */
    [[nodiscard]] static std::string build_notes(
        const LeakageFlowResult& result,
        [[maybe_unused]] const RegulatoryConfig& config
    ) {
        std::ostringstream oss;

        if (result.total_leakage_factor > 0.30) {
            oss << "ADVERTENCIA: Fugas elevadas (>"
                << static_cast<int>(result.total_leakage_factor * 100)
                << "%). Revisar estado del sistema de ductos. ";
        }

        if (result.total_leakage_factor >= 0.80) {
            oss << "CRITICO: Sistema de ductos muy deteriorado. ";
        }

        oss << "Q_ventilador = " << safety_ceil(result.q_at_fan) << " m3/min";

        return oss.str();
    }
};

} // namespace ventpy
