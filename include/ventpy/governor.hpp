/**
 * @file governor.hpp
 * @brief Motor robusto de cálculo de demanda de ventilación.
 *
 * El "Governor" implementa la lógica de negocio central:
 *
 * Para un FRENTE DE DESARROLLO (Development Face):
 *   Q_total = max(Q_Per, Q_Eq, Q_Exp, Q_Dust, Q_Thermal) + Fugas
 *   El caudal gobernante es el MAYOR de todos los factores.
 *
 * Para la MINA TOTAL (General Mine):
 *   Q_total = Σ(Q_zonas) con sumatoria inteligente de todos los sectores.
 *
 * Características robustas:
 * - Correcciones atmosféricas automáticas por altitud
 * - Cálculo de emisiones y dilución por contaminante
 * - Verificación de velocidad mínima
 * - Factor de seguridad configurable
 * - Generación de advertencias automáticas
 *
 * DS 024-2016-EM, Art. 236-252
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ventpy/atmosphere.hpp"
#include "ventpy/caudal_equipo.hpp"
#include "ventpy/caudal_explosivos.hpp"
#include "ventpy/caudal_fugas.hpp"
#include "ventpy/caudal_personal.hpp"
#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"

namespace ventpy {

/**
 * @brief Parámetros de entrada completos para el cálculo.
 */
struct VentilationInput {
    ZoneType zone_type = ZoneType::DevelopmentFace;

    // Parámetros atmosféricos (requerido para cálculos precisos)
    AtmosphericParams atmospheric;

    // Geometría de la labor
    double face_area_m2 = 12.0;     ///< Sección de la labor [m²]
    double face_length_m = 100.0;   ///< Longitud de la labor [m]

    // Personal (opcional)
    std::optional<PersonnelParams> personnel;

    // Equipos diésel (opcional)
    std::optional<DieselFleet> diesel_fleet;
    double simultaneity_factor = 0.85;  ///< Factor de simultaneidad

    // Explosivos (opcional: aplica en frentes ciegos)
    std::optional<BlastingParams> blasting_params;

    // Sistema de ductos (opcional: para cálculo de fugas)
    std::optional<DuctParams> duct_params;

    // Factor de fugas simple (si no se especifican duct_params)
    std::optional<double> leakage_factor;

    // Factor de seguridad adicional [1.0 - 2.0]
    double safety_factor = 1.0;

    // Notas del ingeniero
    std::string notes;

    // === Compatibilidad con API anterior ===
    std::optional<int>    num_workers;      ///< Deprecated: usar personnel
    std::optional<double> altitude_masl;    ///< Deprecated: usar atmospheric
};

/**
 * @brief Motor robusto de cálculo de demanda de ventilación.
 */
class VentilationGovernor {
public:
    /**
     * @brief Constructor con configuración normativa.
     */
    explicit VentilationGovernor(const RegulatoryConfig& config)
        : config_(config) {}

    /**
     * @brief Calcula la demanda total de ventilación (método principal).
     *
     * @param input Parámetros de entrada completos
     * @return Resultado detallado con desglose para auditoría
     */
    [[nodiscard]] VentilationDemandResult calculateTotalDemand(
        const VentilationInput& input
    ) const {
        VentilationDemandResult result;
        result.zone_type = input.zone_type;
        result.standard = config_.standard();
        result.notes = input.notes;
        result.face_area_m2 = input.face_area_m2;
        result.safety_factor_applied = input.safety_factor;

        // Resolver parámetros con compatibilidad hacia atrás
        AtmosphericParams atm = resolve_atmospheric(input);
        PersonnelParams personnel = resolve_personnel(input);

        // === 1. Correcciones atmosféricas ===
        result.atmospheric = AtmosphereCalculator::calculate_all(atm);

        // === 2. Caudal por Personal (Q_Per) ===
        if (personnel.num_workers > 0) {
            auto per_result = PersonnelFlowCalculator::calculate_full(
                personnel,
                atm,
                input.zone_type,
                input.face_area_m2,
                config_
            );
            result.q_personnel_m3min = per_result.q_personnel;
            result.personnel = per_result;
        }

        // === 3. Caudal por Equipos Diésel (Q_Eq) ===
        if (input.diesel_fleet.has_value() && !input.diesel_fleet->empty()) {
            auto diesel_result = DieselFlowCalculator::calculate_full(
                input.diesel_fleet.value(),
                atm,
                input.simultaneity_factor,
                config_
            );
            result.q_diesel_m3min = diesel_result.q_diesel;
            result.diesel = diesel_result;
        }

        // === 4. Caudal por Explosivos (Q_Exp) ===
        if (input.blasting_params.has_value()) {
            BlastingParams blast = input.blasting_params.value();
            // Completar parámetros si faltan
            if (blast.face_area_m2 <= 0) blast.face_area_m2 = input.face_area_m2;
            if (blast.face_length_m <= 0) blast.face_length_m = input.face_length_m;

            auto blast_result = BlastingFlowCalculator::calculate_full(
                blast, atm, config_);
            result.q_blasting_m3min = blast_result.q_blasting;
            result.blasting = blast_result;
        }

        // === 5. Lógica de Selección (Governor) ===
        double q_governing = select_governing_flow(result, input.zone_type);
        result.q_governing_m3min = q_governing;

        // === 6. Aplicar factor de seguridad ===
        if (input.safety_factor > 1.0) {
            q_governing *= input.safety_factor;
            result.warnings.push_back(
                "Factor de seguridad " + std::to_string(input.safety_factor) + " aplicado");
        }

        // === 7. Caudal por Fugas (Q_Fug) ===
        if (q_governing > 0.0) {
            LeakageFlowResult leak_result;

            if (input.duct_params.has_value()) {
                leak_result = LeakageFlowCalculator::calculate_full(
                    q_governing, input.duct_params.value(), config_);
            } else {
                leak_result = LeakageFlowCalculator::calculate(
                    q_governing, input.leakage_factor, config_);
            }

            result.q_leakage_m3min = leak_result.q_leakage;
            result.q_at_fan_m3min = leak_result.q_at_fan;
            result.leakage = leak_result;

            // Caudal total = caudal en ventilador
            result.q_total_m3min = safety_ceil(leak_result.q_at_fan);
        }

        // === 8. Conversiones ===
        result.q_total_m3s = result.q_total_m3min * constants::M3MIN_TO_M3S;
        result.q_total_cfm = result.q_total_m3min * constants::M3MIN_TO_CFM;

        // === 9. Verificar velocidad en el frente ===
        if (input.face_area_m2 > 0) {
            result.velocity_at_face_mps = result.q_governing_m3min /
                                          60.0 / input.face_area_m2;

            double min_vel = get_min_velocity(input.zone_type);
            result.velocity_ok = (result.velocity_at_face_mps >= min_vel);

            if (!result.velocity_ok) {
                result.warnings.push_back(
                    "Velocidad " + std::to_string(result.velocity_at_face_mps) +
                    " m/s < minimo " + std::to_string(min_vel) + " m/s");
            }
        }

        // === 10. Generar advertencias adicionales ===
        generate_warnings(result, atm);

        return result;
    }

    /**
     * @brief Acceso a la configuración normativa.
     */
    [[nodiscard]] const RegulatoryConfig& config() const noexcept {
        return config_;
    }

private:
    const RegulatoryConfig config_;

    /**
     * @brief Resuelve parámetros atmosféricos con compatibilidad.
     */
    [[nodiscard]] static AtmosphericParams resolve_atmospheric(
        const VentilationInput& input
    ) {
        AtmosphericParams atm = input.atmospheric;

        // Compatibilidad: si se usó altitude_masl directamente
        if (input.altitude_masl.has_value() && atm.altitude_masl == 0.0) {
            atm.altitude_masl = input.altitude_masl.value();
        }

        return atm;
    }

    /**
     * @brief Resuelve parámetros de personal con compatibilidad.
     */
    [[nodiscard]] static PersonnelParams resolve_personnel(
        const VentilationInput& input
    ) {
        if (input.personnel.has_value()) {
            return input.personnel.value();
        }

        // Compatibilidad: si se usó num_workers directamente
        PersonnelParams personnel;
        if (input.num_workers.has_value()) {
            personnel.num_workers = input.num_workers.value();
            personnel.activity = ActivityLevel::Moderate;
        }
        return personnel;
    }

    /**
     * @brief Selecciona el caudal gobernante según tipo de zona.
     */
    [[nodiscard]] double select_governing_flow(
        VentilationDemandResult& result,
        ZoneType zone_type
    ) const {
        double q_per = result.q_personnel_m3min;
        double q_eq = result.q_diesel_m3min;
        double q_exp = result.q_blasting_m3min;
        double q_dust = result.q_dust_m3min;
        double q_thermal = result.q_thermal_m3min;

        double q_governing = 0.0;

        switch (zone_type) {
            case ZoneType::DevelopmentFace:
            case ZoneType::Stope:
            case ZoneType::Ramp: {
                // El MAYOR de todos los factores
                q_governing = std::max({q_per, q_eq, q_exp, q_dust, q_thermal});

                // Identificar cuál gobierna
                if (q_governing == q_per && q_per > 0) {
                    result.governing_factor = "personnel (Q_Per)";
                } else if (q_governing == q_eq && q_eq > 0) {
                    result.governing_factor = "diesel (Q_Eq)";
                } else if (q_governing == q_exp && q_exp > 0) {
                    result.governing_factor = "blasting (Q_Exp)";
                } else if (q_governing == q_dust && q_dust > 0) {
                    result.governing_factor = "dust (Q_Dust)";
                } else if (q_governing == q_thermal && q_thermal > 0) {
                    result.governing_factor = "thermal (Q_Thermal)";
                } else {
                    result.governing_factor = "none";
                }
                break;
            }

            case ZoneType::GeneralMine:
                // Sumatoria de todos los requerimientos
                q_governing = q_per + q_eq + q_exp + q_dust + q_thermal;
                result.governing_factor = "summation (mine total)";
                break;
        }

        return q_governing;
    }

    /**
     * @brief Genera advertencias basadas en los resultados.
     */
    void generate_warnings(
        VentilationDemandResult& result,
        const AtmosphericParams& atm
    ) const {
        // Advertencia por altitud extrema
        if (atm.altitude_masl > 4500.0) {
            result.warnings.push_back(
                "ALTITUD EXTREMA (>" + std::to_string(static_cast<int>(atm.altitude_masl)) +
                " msnm): Verificar condiciones de trabajo");
        }

        // Advertencia si no hay caudal calculado
        if (result.q_total_m3min <= 0.0) {
            result.warnings.push_back(
                "Sin requerimiento de ventilacion calculado. "
                "Verificar parametros de entrada.");
        }

        // Advertencia por fugas elevadas
        if (result.leakage.has_value() &&
            result.leakage->total_leakage_factor > 0.30) {
            result.warnings.push_back(
                "Fugas elevadas (" +
                std::to_string(static_cast<int>(result.leakage->total_leakage_factor * 100)) +
                "%): Revisar sistema de ductos");
        }
    }
};

} // namespace ventpy
