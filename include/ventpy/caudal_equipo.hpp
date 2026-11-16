/**
 * @file caudal_equipo.hpp
 * @brief Cálculo robusto de caudal por equipos diésel (Q_Eq).
 *
 * El cálculo considera múltiples factores:
 *
 * 1. FACTOR HP NORMATIVO:
 *    - DS 024: mínimo 3 m³/min/HP (Q_Eq = 3 × HP × Dm × Fu)
 *    - Ajustable a 4-5 m³/min/HP según estándar corporativo
 *    - La corrección volumétrica por altitud (criterio de ingeniería) solo
 *      puede subir el factor, nunca bajarlo del valor de la norma
 *
 * 2. DE-RATING POR ALTITUD:
 *    - Motores diésel pierden potencia a mayor altitud
 *    - Turboalimentados: ~1.5%/300m sobre 1000m
 *    - Atmosféricos: ~3%/300m sobre 1000m
 *
 * 3. EMISIONES POR CONTAMINANTE:
 *    - Cálculo de CO, NOx, PM según tier del motor
 *    - Q necesario para diluir cada gas a su límite de 8 h (TWA) de la norma
 *      configurada (`dilution_target_ppm`); el NOx se lleva a NO2
 *    - Considera DPF/DOC si están instalados
 *
 * 4. FACTOR DE SIMULTANEIDAD:
 *    - Ninguna norma implementada lo prevé: con el DS 024, Dm × Fu ya es la
 *      simultaneidad. Por defecto 1,0; un valor menor deja Q_Eq bajo la norma
 *      y el Governor lo advierte
 *
 * Referencias normativas (Perú, preset por defecto):
 * - DS 024-2016-EM, Art. 252 lit. e y Anexo 38 num. 1 lit. d: 3 m³/min/HP
 * - DS 024-2016-EM, Art. 110 + Anexo 15: TWA CO = 25 ppm, NO2 = 3 ppm
 *
 * La cita emitida en `regulation_ref` sigue a `config.standard()` a través de
 * `regulation_reference` (normativa.hpp): bajo el DS 132 chileno el factor por
 * HP efectivo al freno es el Art. 132 (2,83 m³/min/HP).
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <sstream>
#include <vector>

#include "ventpy/atmosphere.hpp"
#include "ventpy/limites_gases.hpp"
#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Flota de equipos diésel para un sector o zona de la mina.
 */
class DieselFleet {
public:
    DieselFleet() = default;

    /**
     * @brief Agrega un equipo diésel a la flota (versión completa).
     */
    void add_equipment(const DieselEquipment& equipment) {
        validate_equipment(equipment);
        equipment_.push_back(std::make_shared<DieselEquipment>(equipment));
    }

    /**
     * @brief Agrega un equipo con parámetros básicos (compatibilidad).
     */
    void add_equipment(const std::string& name, double hp,
                       double availability, double utilization) {
        DieselEquipment eq;
        eq.name = name;
        eq.horsepower = hp;
        eq.availability = availability;
        eq.utilization = utilization;
        eq.emission_tier = EngineEmissionTier::Tier3;  // Default moderno
        add_equipment(eq);
    }

    /**
     * @brief Agrega equipo con tier de emisiones especificado.
     */
    void add_equipment(const std::string& name, double hp,
                       double availability, double utilization,
                       EngineEmissionTier tier, bool has_dpf = false) {
        DieselEquipment eq;
        eq.name = name;
        eq.horsepower = hp;
        eq.availability = availability;
        eq.utilization = utilization;
        eq.emission_tier = tier;
        eq.has_dpf = has_dpf;
        add_equipment(eq);
    }

    [[nodiscard]] const std::vector<std::shared_ptr<DieselEquipment>>&
    equipment() const noexcept {
        return equipment_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return equipment_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return equipment_.empty();
    }

    void clear() noexcept {
        equipment_.clear();
    }

private:
    std::vector<std::shared_ptr<DieselEquipment>> equipment_;

    static void validate_equipment(const DieselEquipment& eq) {
        validation::require_positive(eq.horsepower,
            "horsepower [HP] del equipo '" + eq.name + "'");
        validation::require_in_range(eq.availability, 0.0, 1.0,
            "availability del equipo '" + eq.name + "'");
        validation::require_in_range(eq.utilization, 0.0, 1.0,
            "utilization del equipo '" + eq.name + "'");
    }
};

/**
 * @brief Calculador robusto de caudal por equipos diésel.
 */
class DieselFlowCalculator {
public:
    /**
     * @brief Calcula el caudal requerido (versión completa con emisiones).
     *
     * @param fleet Flota de equipos diésel
     * @param atm Parámetros atmosféricos (para de-rating)
     * @param simultaneity_factor Factor de simultaneidad [0-1]; 1,0 respeta la
     *        norma (un valor menor deja Q_Eq bajo el mínimo legal)
     * @param config Configuración normativa
     * @return Resultado detallado con desglose por contaminante
     */
    [[nodiscard]] static DieselFlowResult calculate_full(
        const DieselFleet& fleet,
        const AtmosphericParams& atm,
        double simultaneity_factor,
        const RegulatoryConfig& config
    ) {
        validation::require_in_range(simultaneity_factor, 0.0, 1.0,
            "simultaneity_factor");

        DieselFlowResult result;
        result.hp_factor_base = config.diesel_hp_factor();
        result.total_rated_hp = 0.0;
        result.total_effective_hp = 0.0;
        result.total_derated_hp = 0.0;
        result.co_emission_total_g_min = 0.0;
        result.nox_emission_total_g_min = 0.0;

        if (fleet.empty()) {
            result.q_diesel = 0.0;
            result.regulation_ref = "Sin equipos diesel";
            return result;
        }

        // Factor de de-rating por altitud
        double derate = AtmosphereCalculator::calculate_diesel_derate_factor(
            atm.altitude_masl, true);  // Asumir turbo por defecto
        result.altitude_derate_factor = derate;

        // Factor de corrección volumétrica (más aire necesario a altitud).
        // Al nivel del mar el ISA da 0,99999: el factor de la norma no se
        // reduce nunca, la corrección solo lo sube.
        double volume_correction = AtmosphereCalculator::calculate_volume_correction_factor(
            atm.altitude_masl);
        result.hp_factor_corrected =
            result.hp_factor_base * std::max(1.0, volume_correction);
        result.target_co_ppm = dilution_target_ppm(config.standard(), GasType::CO);
        result.target_nox_ppm = dilution_target_ppm(config.standard(), GasType::NO2);

        // Procesar cada equipo
        for (const auto& eq_ptr : fleet.equipment()) {
            const auto& eq = *eq_ptr;
            result.equipment_names.push_back(eq.name);

            // HP nominal
            result.total_rated_hp += eq.horsepower;

            // HP efectivo (con disponibilidad y utilización)
            double hp_eff = eq.horsepower * eq.availability * eq.utilization;
            result.total_effective_hp += hp_eff;

            // HP con de-rating por altitud
            double hp_derated = hp_eff * derate;
            result.total_derated_hp += hp_derated;

            // Calcular emisiones de este equipo
            auto [co_g_min, nox_g_min, pm_g_min] = calculate_emissions(eq, hp_derated);
            result.co_emission_total_g_min += co_g_min;
            result.nox_emission_total_g_min += nox_g_min;
        }

        // Aplicar factor de simultaneidad
        result.total_derated_hp *= simultaneity_factor;
        result.co_emission_total_g_min *= simultaneity_factor;
        result.nox_emission_total_g_min *= simultaneity_factor;

        // --- Calcular Q por diferentes criterios ---

        // 1. Q por factor HP normativo (Perú: Anexo 38, Q_Eq = 3 × HP × Dm × Fu)
        double q_hp_method = result.total_effective_hp * result.hp_factor_corrected *
                             simultaneity_factor;

        // 2. Q para diluir CO a su TWA de la norma
        result.q_for_co_dilution = calculate_q_for_gas_dilution(
            result.co_emission_total_g_min,
            result.target_co_ppm,
            28.01  // Peso molecular CO
        ) * volume_correction;

        // 3. Q para diluir NOx (como NO2) a su TWA de la norma
        result.q_for_nox_dilution = calculate_q_for_gas_dilution(
            result.nox_emission_total_g_min,
            result.target_nox_ppm,
            46.01  // Peso molecular NO2
        ) * volume_correction;

        // 4. Q para PM (aproximación: usar factor HP con PM reducido)
        result.q_for_pm_dilution = q_hp_method * 0.5;  // PM menos crítico que gases

        // Tomar el máximo
        result.q_diesel = std::max({
            q_hp_method,
            result.q_for_co_dilution,
            result.q_for_nox_dilution
        });

        // Redondeo de seguridad
        result.q_diesel = safety_ceil(result.q_diesel);

        // Referencia normativa
        result.regulation_ref = build_regulation_ref(
            config, atm.altitude_masl, q_hp_method,
            result.q_for_co_dilution, result.q_for_nox_dilution);

        return result;
    }

    /**
     * @brief Versión simplificada (compatibilidad con API anterior).
     */
    [[nodiscard]] static DieselFlowResult calculate(
        const DieselFleet& fleet,
        const RegulatoryConfig& config
    ) {
        DieselFlowResult result;
        result.hp_factor_base = config.diesel_hp_factor();
        result.hp_factor_corrected = result.hp_factor_base;
        result.altitude_derate_factor = 1.0;
        result.total_rated_hp = 0.0;
        result.total_effective_hp = 0.0;
        result.total_derated_hp = 0.0;
        result.q_for_co_dilution = 0.0;
        result.q_for_nox_dilution = 0.0;
        result.q_for_pm_dilution = 0.0;
        result.co_emission_total_g_min = 0.0;
        result.nox_emission_total_g_min = 0.0;

        for (const auto& eq_ptr : fleet.equipment()) {
            const auto& eq = *eq_ptr;
            result.equipment_names.push_back(eq.name);
            result.total_rated_hp += eq.horsepower;

            double hp_eff = eq.horsepower * eq.availability * eq.utilization;
            result.total_effective_hp += hp_eff;
        }

        result.total_derated_hp = result.total_effective_hp;
        result.q_diesel = safety_ceil(result.total_effective_hp * result.hp_factor_base);
        result.regulation_ref =
            regulation_reference(RegulatoryTopic::DieselHpFactor, config) +
            " [Gobernante: factor HP]";
        return result;
    }

private:
    /**
     * @brief Obtiene factores de emisión según tier del motor.
     *
     * Retorna [CO, NOx, PM] en g/kWh
     */
    [[nodiscard]] static std::tuple<double, double, double>
    get_emission_factors(EngineEmissionTier tier) {
        switch (tier) {
            case EngineEmissionTier::Tier0_Unregulated:
                return {constants::DIESEL_TIER0_CO_G_KWH,
                        constants::DIESEL_TIER0_NOX_G_KWH,
                        constants::DIESEL_TIER0_PM_G_KWH};
            case EngineEmissionTier::Tier4_Final:
                return {constants::DIESEL_TIER4F_CO_G_KWH,
                        constants::DIESEL_TIER4F_NOX_G_KWH,
                        constants::DIESEL_TIER4F_PM_G_KWH};
            default:  // Tier 2, 3, 4i
                return {constants::DIESEL_TIER3_CO_G_KWH,
                        constants::DIESEL_TIER3_NOX_G_KWH,
                        constants::DIESEL_TIER3_PM_G_KWH};
        }
    }

    /**
     * @brief Calcula emisiones de un equipo [g/min].
     *
     * @param eq Equipo
     * @param hp_derated HP efectivo con de-rating
     * @return Tupla [CO, NOx, PM] en g/min
     */
    [[nodiscard]] static std::tuple<double, double, double>
    calculate_emissions(const DieselEquipment& eq, double hp_derated) {
        auto [co_gkwh, nox_gkwh, pm_gkwh] = get_emission_factors(eq.emission_tier);

        // Usar factores custom si están especificados
        if (eq.co_emission_factor > 0) co_gkwh = eq.co_emission_factor;
        if (eq.nox_emission_factor > 0) nox_gkwh = eq.nox_emission_factor;
        if (eq.pm_emission_factor > 0) pm_gkwh = eq.pm_emission_factor;

        // Reducción por DPF/DOC
        if (eq.has_dpf) {
            pm_gkwh *= 0.03;   // DPF reduce PM en ~97%
        }
        if (eq.has_doc) {
            co_gkwh *= 0.20;   // DOC reduce CO en ~80%
        }

        // kW = HP × 0.7457
        double kw = hp_derated * constants::HP_TO_KW;

        // g/min = g/kWh × kW / 60
        double co_g_min = co_gkwh * kw / 60.0;
        double nox_g_min = nox_gkwh * kw / 60.0;
        double pm_g_min = pm_gkwh * kw / 60.0;

        return {co_g_min, nox_g_min, pm_g_min};
    }

    /**
     * @brief Calcula Q para diluir un gas a su TLV.
     *
     * Q = (masa_emitida × 24.45) / (PM × TLV × 10^-6)
     *
     * Donde:
     * - masa_emitida: g/min
     * - 24.45: volumen molar a 25°C [L/mol]
     * - PM: peso molecular [g/mol]
     * - TLV: límite en ppm
     *
     * Resultado en m³/min
     *
     * @param mass_rate Tasa de emisión [g/min]
     * @param tlv_ppm Límite de exposición [ppm]
     * @param molecular_weight Peso molecular [g/mol]
     * @return Caudal requerido [m³/min]
     */
    [[nodiscard]] static double calculate_q_for_gas_dilution(
        double mass_rate,
        double tlv_ppm,
        double molecular_weight
    ) {
        if (mass_rate <= 0 || tlv_ppm <= 0) return 0.0;

        constexpr double MOLAR_VOLUME = 24.45;  // L/mol @ 25°C, 1 atm

        // Convertir tasa de masa a tasa volumétrica del gas
        double moles_per_min = mass_rate / molecular_weight;
        double gas_volume_lpm = moles_per_min * MOLAR_VOLUME;

        // Q para diluir a TLV
        // ppm = (vol_gas / vol_aire) × 10^6
        // vol_aire = vol_gas × 10^6 / ppm
        double air_volume_lpm = gas_volume_lpm * 1e6 / tlv_ppm;

        // L/min a m³/min
        return air_volume_lpm / 1000.0;
    }

    /**
     * @brief Construye referencia normativa detallada.
     */
    [[nodiscard]] static std::string build_regulation_ref(
        const RegulatoryConfig& config,
        double altitude,
        double q_hp,
        double q_co,
        double q_nox
    ) {
        std::ostringstream oss;
        oss << regulation_reference(RegulatoryTopic::DieselHpFactor, config)
            << " (Factor: " << config.diesel_hp_factor() << " m3/min/HP)";

        if (altitude > 1000.0) {
            oss << " + correccion por altitud a " << static_cast<int>(altitude)
                << " msnm (criterio de ingenieria)";
        }

        // Indicar criterio gobernante
        double max_q = std::max({q_hp, q_co, q_nox});
        if (max_q == q_co) {
            oss << " [Gobernante: dilucion CO]";
        } else if (max_q == q_nox) {
            oss << " [Gobernante: dilucion NOx]";
        } else {
            oss << " [Gobernante: factor HP]";
        }

        return oss.str();
    }
};

} // namespace ventpy
