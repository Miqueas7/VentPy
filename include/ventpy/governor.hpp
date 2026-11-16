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
 *   Con el DS 024 el total sigue el Anexo 38: Q_T1 = Q_Tr + Q_Ma + Q_Te +
 *   Q_Eq y Q_Fu = 15 % de Q_T1. La voladura no entra si hay equipos diésel
 *   y, si no los hay, rige max(Q_total, Q_Ex = A × V × N).
 *
 * Velocidad en el frente: advierte bajo el mínimo (`get_min_velocity`) y
 * sobre el máximo de la norma (`max_air_velocity_mpm`), sin recortar el
 * caudal requerido.
 *
 * Características robustas:
 * - Correcciones atmosféricas automáticas por altitud
 * - Cálculo de emisiones y dilución por contaminante
 * - Verificación de velocidad mínima
 * - Factor de seguridad configurable
 * - Generación de advertencias automáticas
 *
 * Perú: DS 024-2016-EM, Arts. 246 a 252 y Anexo 38
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/atmosphere.hpp"
#include "ventpy/caudal_equipo.hpp"
#include "ventpy/caudal_explosivos.hpp"
#include "ventpy/caudal_fugas.hpp"
#include "ventpy/caudal_personal.hpp"
#include "ventpy/caudal_polvo.hpp"
#include "ventpy/caudal_termico.hpp"
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
    /// Factor de simultaneidad [0-1]. Ninguna norma implementada lo prevé
    /// (con el DS 024, Q_Eq = 3 × HP × Dm × Fu ya lo incluye): con < 1,0 el
    /// Q_Eq queda bajo la norma y el resultado lo advierte.
    double simultaneity_factor = 1.0;

    // Total de mina con el DS 024 (Anexo 38). Sin estos datos el término
    // vale 0 y el resultado advierte que el total no lo incluye.
    /// T de Q_Ma = T × u: producción [t métricas húmedas por guardia]
    std::optional<double> production_t_per_shift;
    /// Consumo de madera [% de la producción] que fija u (Anexo 38 lit. b)
    std::optional<double> timber_consumption_pct;
    /// N de Q_Te = Vm × A × N: niveles con temperatura seca > 23 °C
    /// (A = `face_area_m2`, área promedio de labor; Vm = 30 m/min)
    std::optional<int> hot_levels;

    // Explosivos (opcional: aplica en frentes ciegos)
    std::optional<BlastingParams> blasting_params;

    // Polvo (opcional)
    std::optional<DustParams> dust_params;
    // Carga térmica (opcional)
    std::optional<ThermalParams> thermal_params;

    // Sistema de ductos (opcional: para cálculo de fugas)
    std::optional<DuctParams> duct_params;

    // Factor de fugas simple (si no se especifican duct_params)
    std::optional<double> leakage_factor;

    // Factor de seguridad adicional [1.0 - 2.0]
    double safety_factor = 1.0;

    /// Mina de carbón. Solo cambia el máximo en tajos con el Decreto 1886
    /// (Art. 56 par. 2: 4,5 m/s); las demás reglas de carbón no están
    /// implementadas y el resultado lo advierte.
    bool coal_mine = false;

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

        // Total de mina con el DS 024: rige el Anexo 38.
        const bool annex38 = input.zone_type == ZoneType::GeneralMine &&
                             config_.standard() == RegulatoryStandard::DS024_Peru;
        const bool has_diesel = input.diesel_fleet.has_value() &&
                                !input.diesel_fleet->empty();
        // ANFO u otros agentes de voladura: velocidad mínima del Art. 248.
        const bool blasting_agents = input.blasting_params.has_value() &&
            is_blasting_agent(input.blasting_params->explosive_type);

        // === 1. Correcciones atmosféricas ===
        result.atmospheric = AtmosphereCalculator::calculate_all(atm);

        // === 2. Caudal por Personal (Q_Per) ===
        if (personnel.num_workers > 0) {
            auto per_result = PersonnelFlowCalculator::calculate_full(
                personnel,
                atm,
                input.zone_type,
                input.face_area_m2,
                config_,
                has_diesel,
                blasting_agents
            );
            result.q_personnel_m3min = per_result.q_personnel;
            result.personnel = per_result;
        }

        // === 3. Caudal por Equipos Diésel (Q_Eq) ===
        if (has_diesel) {
            auto diesel_result = DieselFlowCalculator::calculate_full(
                input.diesel_fleet.value(),
                atm,
                input.simultaneity_factor,
                config_
            );
            result.q_diesel_m3min = diesel_result.q_diesel;
            result.diesel = diesel_result;
            if (input.simultaneity_factor < 1.0) {
                std::ostringstream oss;
                oss << "Factor de simultaneidad " << input.simultaneity_factor
                    << " < 1: Q_Eq queda bajo el minimo de la norma ("
                    << regulation_reference(RegulatoryTopic::DieselHpFactor,
                                            config_)
                    << ", que no preve simultaneidad)";
                result.warnings.push_back(oss.str());
            }
        }

        // === 4. Caudal por Explosivos (Q_Exp) ===
        if (input.blasting_params.has_value()) {
            BlastingParams blast = input.blasting_params.value();
            // Completar parámetros si faltan
            if (blast.face_area_m2 <= 0) blast.face_area_m2 = input.face_area_m2;
            if (blast.face_length_m <= 0) blast.face_length_m = input.face_length_m;

            if (annex38 && has_diesel) {
                result.warnings.push_back(
                    "DS 024-2016-EM, Anexo 38 num. 1: con equipos diesel la "
                    "voladura no entra en el total de mina");
            } else if (annex38) {
                auto blast_result = BlastingFlowCalculator::calculate_annex38(blast);
                result.q_blasting_m3min = blast_result.q_blasting;
                result.blasting = blast_result;
            } else {
                // Dilución por contaminante con corrección de altitud.
                auto blast_result =
                    BlastingFlowCalculator::calculate_full(blast, atm, config_);
                result.q_blasting_m3min = blast_result.q_blasting;
                result.blasting = blast_result;
            }
        }

        // === 4b. Caudal por Polvo (Q_polvo) ===
        if (input.dust_params.has_value()) {
            DustParams dust = input.dust_params.value();
            if (dust.face_area_m2 <= 0) dust.face_area_m2 = input.face_area_m2;
            auto dust_result = DustFlowCalculator::calculate(dust, config_, atm);
            result.q_dust_m3min = dust_result.q_dust;
            for (const auto& w : dust_result.warnings)
                result.warnings.push_back("Q_polvo: " + w);
            result.dust = std::move(dust_result);
        }

        // === 4c. Caudal por Carga Térmica (Q_termico) ===
        if (input.thermal_params.has_value()) {
            ThermalParams thermal = input.thermal_params.value();
            if (thermal.face_area_m2 <= 0) thermal.face_area_m2 = input.face_area_m2;
            auto thermal_result = ThermalFlowCalculator::calculate(thermal, atm, config_);
            result.q_thermal_m3min = thermal_result.q_thermal;
            for (const auto& w : thermal_result.warnings)
                result.warnings.push_back("Q_termico: " + w);
            result.thermal = std::move(thermal_result);
        }

        // === 4d. Madera y temperatura del Anexo 38 (total de mina peruano) ===
        apply_annex38_terms(result, input, annex38);

        // === 5. Lógica de Selección (Governor) ===
        double q_governing = select_governing_flow(result, input.zone_type, annex38);
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

        // Anexo 38 num. 2: sin diésel rige el mayor entre el total con fugas
        // y Q_Ex, que no lleva fugas.
        if (annex38) {
            const double q_ex = result.q_blasting_m3min * input.safety_factor;
            if (q_ex > result.q_total_m3min) {
                result.q_at_fan_m3min = q_ex;
                result.q_total_m3min = safety_ceil(q_ex);
                result.governing_factor = "blasting (Q_Ex, Anexo 38)";
            }
        }

        // === 8. Conversiones ===
        result.q_total_m3s = result.q_total_m3min * constants::M3MIN_TO_M3S;
        result.q_total_cfm = result.q_total_m3min * constants::M3MIN_TO_CFM;

        // === 9. Verificar velocidad en el frente ===
        if (input.face_area_m2 > 0) {
            result.velocity_at_face_mps = result.q_governing_m3min /
                                          60.0 / input.face_area_m2;

            const double min_vel = get_min_velocity(
                input.zone_type, config_.standard(), has_diesel, blasting_agents);
            result.velocity_ok = (result.velocity_at_face_mps >= min_vel);

            if (!result.velocity_ok) {
                // Si la mínima de la norma es la que rige, se cita; si rige
                // el valor por tipo de labor, se declara criterio.
                const auto norm_min = min_air_velocity_mpm(
                    config_.standard(), has_diesel, blasting_agents);
                const bool by_norm = norm_min && *norm_min / 60.0 >= min_vel;
                std::ostringstream oss;
                oss << "Velocidad " << result.velocity_at_face_mps * 60.0
                    << " m/min < minimo " << min_vel * 60.0 << " m/min ("
                    << (by_norm
                            ? regulation_reference(
                                  RegulatoryTopic::AirVelocityLimits, config_)
                            : std::string("criterio de ingenieria por tipo de "
                                          "labor"))
                    << ")";
                result.warnings.push_back(oss.str());
            }

            // Solo advierte: el caudal requerido no se recorta.
            const auto max_mpm = max_air_velocity_mpm(
                config_.standard(),
                input.coal_mine && input.zone_type == ZoneType::Stope);
            const double v_mpm = result.velocity_at_face_mps * 60.0;
            if (input.zone_type != ZoneType::GeneralMine && max_mpm &&
                v_mpm > *max_mpm) {
                result.warnings.push_back(
                    "Velocidad " + std::to_string(std::lround(v_mpm)) +
                    " m/min > maximo " + std::to_string(std::lround(*max_mpm)) +
                    " m/min (" +
                    regulation_reference(RegulatoryTopic::AirVelocityLimits,
                                         config_) +
                    "): ampliar la seccion o repartir el caudal");
            }
        }

        if (input.coal_mine) {
            result.warnings.push_back(
                "Mina de carbon: VentPy solo aplica el maximo de 4,5 m/s en tajos "
                "(Decreto 1886 de 2015, Art. 56 par. 2); metano, minimas de carbon "
                "y demas reglas de carbon no estan implementadas");
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
        ZoneType zone_type,
        bool annex38
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
                // Sumatoria de todos los requerimientos. Con el Anexo 38:
                // Q_Ex no se suma (se compara después con el total), entra
                // Q_Ma, y Q_Te y el balance térmico responden al mismo
                // concepto (aire para la temperatura de confort, Art. 252
                // lit. d), así que rige el mayor de los dos, sin sumarlos.
                // El polvo sigue sumando como criterio de ingeniería.
                if (annex38) {
                    q_governing = q_per + q_eq + result.q_timber_m3min +
                                  std::max(q_thermal, result.q_temperature_m3min) +
                                  q_dust;
                } else {
                    q_governing = q_per + q_eq + q_exp + q_dust + q_thermal;
                }
                result.governing_factor = "summation (mine total)";
                break;
        }

        return q_governing;
    }

    /**
     * @brief Términos Q_Ma y Q_Te del DS 024-2016-EM, Anexo 38 (total de mina).
     *
     * - Q_Ma = T × u (lit. b): T = producción en t métricas húmedas por
     *   guardia; u según el consumo de madera (`timber_production_factor`).
     * - Q_Te = Vm × A × N (lit. c): Vm = 30 m/min (tabla del anexo, labores
     *   con temperatura seca de 24 a 29 °C; la definición de N dice «mayor a
     *   23 °C»), A = área promedio de labor (`face_area_m2`), N = niveles con
     *   temperatura mayor a 23 °C.
     *
     * Sin sus datos el término vale 0 y el resultado advierte que el total no
     * lo incluye. Fuera del total de mina peruano los datos se ignoran con
     * una advertencia.
     */
    void apply_annex38_terms(
        VentilationDemandResult& result,
        const VentilationInput& input,
        bool annex38
    ) const {
        const bool has_production = input.production_t_per_shift.has_value();
        const bool has_timber = input.timber_consumption_pct.has_value();
        if (has_production != has_timber) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: Q_Ma (DS 024-2016-EM, Anexo 38 "
                "lit. b) requiere production_t_per_shift y "
                "timber_consumption_pct juntos.");
        }
        const bool has_hot_levels = input.hot_levels.has_value();

        if (!annex38) {
            if (has_production || has_hot_levels) {
                result.warnings.push_back(
                    "Q_Ma y Q_Te (DS 024-2016-EM, Anexo 38) solo aplican al "
                    "total de mina con el DS 024: se ignoran");
            }
            return;
        }

        if (has_production) {
            validation::require_non_negative(*input.production_t_per_shift,
                "production_t_per_shift [t/guardia] - Produccion (T de Q_Ma)");
            const double u = timber_production_factor(*input.timber_consumption_pct);
            // Tolerancia FP sustractiva (mismo patrón que caudal_polvo.hpp):
            // 100 × 0,6 no debe redondear a 61 por un artefacto binario.
            constexpr double FP_TOL = 1e-9;
            result.q_timber_m3min = std::max(
                0.0, safety_ceil(*input.production_t_per_shift * u - FP_TOL));
        } else {
            result.warnings.push_back(
                "DS 024-2016-EM, Anexo 38 lit. b: el total no incluye Q_Ma "
                "(consumo de madera); faltan production_t_per_shift y "
                "timber_consumption_pct");
        }

        if (has_hot_levels) {
            validation::require_non_negative(
                static_cast<double>(*input.hot_levels),
                "hot_levels - Niveles con temperatura > 23 C (N de Q_Te)");
            validation::require_positive(input.face_area_m2,
                "face_area_m2 [m2] - Area promedio de labor (A de Q_Te)");
            constexpr double VM_MPM = 30.0;  // Anexo 38 lit. c; Art. 252 lit. d
            result.q_temperature_m3min =
                safety_ceil(VM_MPM * input.face_area_m2 * *input.hot_levels);
        } else {
            result.warnings.push_back(
                "DS 024-2016-EM, Anexo 38 lit. c: el total no incluye Q_Te "
                "(temperatura en labores); falta hot_levels");
        }
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
