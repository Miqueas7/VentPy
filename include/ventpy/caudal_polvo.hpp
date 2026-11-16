/**
 * @file caudal_polvo.hpp
 * @brief Cálculo de caudal para dilución de polvo respirable (Q_polvo).
 *
 * Límite de polvo respirable según la norma configurada (se usa cuando
 * `DustParams::target_concentration_mg_m3` vale 0):
 * - Perú: DS 024-2016-EM, Art. 111 (texto original, no modificado por el
 *   DS 023-2017-EM): LEO 3 mg/m³ para jornada de 8 h, con obligación de
 *   paralizar la labor si se supera.
 * - Chile: DS 594, Art. 66 (por remisión del DS 132, Arts. 135 y 144): polvos
 *   no especificados, fracción respirable, LPP 2,4 mg/m³. Sobre 1.000 msnm se
 *   multiplica por Fa = P/760, con P en mmHg y Fa con dos decimales (Art. 63).
 * - México: NOM-010-STPS-2014, Tabla I.1, fila 609: 3 mg/m³ (fracción
 *   respirable), por remisión de la NOM-023-STPS-2012.
 * - Colombia: el Decreto 1886, Art. 74, remite a los VLP de la ACGIH sin
 *   valor propio; 3 mg/m³ es criterio de ingeniería.
 *
 * La sílice tiene límite propio en cada norma: este calculador advierte la
 * remisión con su cita, nunca lo cuantifica.
 *
 * Modelo: dilución con aire de ingreso limpio (C_in = 0, conservador respecto
 * de aire de ingreso ya cargado NO — documentado: si el ingreso trae polvo,
 * el usuario debe reducir su target). Q = G_efectiva / C_objetivo. El límite
 * en mg/m³ se compara a condiciones del sitio (sin corrección por densidad).
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
#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class DustFlowCalculator {
public:
    /**
     * @brief Límite de polvo respirable de la norma configurada [mg/m³].
     *
     * Con Chile aplica el factor Fa del DS 594, Art. 63 sobre 1.000 msnm. La
     * presión local es `barometric_pressure_kpa` si se dio, o la ISA de la
     * altitud.
     */
    [[nodiscard]] static double respirable_limit_mg_m3(
        const RegulatoryConfig& config, const AtmosphericParams& atm = {}
    ) {
        if (config.standard() != RegulatoryStandard::DS132_Chile) return 3.0;
        constexpr double CHILE_LPP_MG_M3 = 2.4;  // DS 594, Art. 66
        if (!(atm.altitude_masl > 1000.0)) return CHILE_LPP_MG_M3;
        const double p_kpa = atm.barometric_pressure_kpa > 0.0
            ? atm.barometric_pressure_kpa
            : AtmosphereCalculator::calculate_pressure_kpa(atm.altitude_masl);
        // Fa = P[mmHg]/760 = P[kPa]/101,325. El Art. 63 fija dos decimales,
        // subiendo el segundo si el tercero es 5 o más.
        const double fa_raw = p_kpa / constants::SEA_LEVEL_PRESSURE_KPA;
        const double fa = std::floor(fa_raw * 100.0 + 0.5) / 100.0;
        return CHILE_LPP_MG_M3 * fa;
    }

    [[nodiscard]] static DustFlowResult calculate(
        const DustParams& p, const RegulatoryConfig& config,
        const AtmosphericParams& atm = {}
    ) {
        validation::require_non_negative(p.dust_generation_rate_mg_s,
            "dust_generation_rate_mg_s [mg/s] - Generacion de polvo");
        validation::require_non_negative(p.target_concentration_mg_m3,
            "target_concentration_mg_m3 [mg/m3] - Concentracion objetivo "
            "(0 = limite de la norma)");
        validation::require_in_range(p.suppression_efficiency, 0.0, 0.999999,
            "suppression_efficiency");
        validation::require_in_range(p.silica_content_percent, 0.0, 100.0,
            "silica_content_percent [%]");
        validation::require_non_negative(p.face_area_m2,
            "face_area_m2 [m2] - Seccion de la labor");

        const double limit = respirable_limit_mg_m3(config, atm);
        // Con sílice declarada, el polvo respirable tampoco puede superar
        // límite de sílice / fracción de sílice. Con Chile el límite de sílice
        // lleva el mismo Fa que el de polvo (limit = 2,4 x Fa).
        std::optional<double> silica_target;
        if (p.silica_content_percent > 0.0) {
            if (const auto s = silica_limit_mg_m3(
                    config.standard(), p.silica_cristobalite_or_tridymite)) {
                const double fa = config.standard() == RegulatoryStandard::DS132_Chile
                    ? limit / 2.4 : 1.0;
                silica_target = *s * fa / (p.silica_content_percent / 100.0);
            }
        }
        const bool silica_governs = silica_target && *silica_target < limit;
        const double target = p.target_concentration_mg_m3 > 0.0
            ? p.target_concentration_mg_m3
            : (silica_governs ? *silica_target : limit);

        DustFlowResult r;
        r.dust_generation_mg_s = p.dust_generation_rate_mg_s;
        r.target_concentration = target;
        r.suppression_efficiency =
            p.water_suppression ? p.suppression_efficiency : 0.0;
        r.effective_generation = p.dust_generation_rate_mg_s *
            (p.water_suppression ? (1.0 - p.suppression_efficiency) : 1.0);

        // Calcular caudal
        const double q_m3s = r.effective_generation / target;

        // Tolerancia FP sustractiva antes del ceil (mismo patron que
        // V_TOL/P_TOL en ducto.hpp/cobertura.hpp): absorbe artefactos de
        // punto flotante (ej. 50×(1−0.7)/3×60 = 300.00000000000006) sin
        // usar round y sin poder JAMÁS sobre-reportar: ceil(x−eps) ≤
        // ceil(x); lo máximo que "resta" es < 1e-9 m³/min, por debajo de
        // precisión instrumental.
        constexpr double FP_TOL = 1e-9;
        // Clamp cosmetico: evita -0.0 cuando la generacion es 0 (safety_ceil
        // (-FP_TOL) puede devolver -0.0).
        r.q_dust = std::max(0.0, safety_ceil(q_m3s * 60.0 - FP_TOL));
        if (p.face_area_m2 > 0.0) {
            r.resulting_velocity_mps = (r.q_dust / 60.0) / p.face_area_m2;
        }
        r.regulation_ref = (silica_governs && p.target_concentration_mg_m3 <= 0.0)
            ? "Objetivo por silice segun " +
                  regulation_reference(RegulatoryTopic::DustSilicaReferral, config)
            : regulation_reference(RegulatoryTopic::DustRespirableLimit, config);

        if (silica_target && target > *silica_target) {
            std::ostringstream oss;
            oss << "Concentracion objetivo (" << target
                << " mg/m3) por ENCIMA del limite por silice (" << *silica_target
                << " mg/m3) - "
                << regulation_reference(RegulatoryTopic::DustSilicaReferral, config);
            r.warnings.push_back(oss.str());
        } else if (p.silica_content_percent > 0.0 && !silica_target) {
            std::ostringstream oss;
            oss << "Silice presente (" << p.silica_content_percent
                << "%): el limite especifico de silice se rige por "
                << regulation_reference(
                       RegulatoryTopic::DustSilicaReferral, config)
                << " (no cuantificado por este calculador)";
            r.warnings.push_back(oss.str());
        }
        if (target > limit) {
            std::ostringstream oss;
            oss << "Concentracion objetivo (" << target
                << " mg/m3) por ENCIMA de " << limit << " mg/m3 - "
                << regulation_reference(RegulatoryTopic::DustLimitBasis, config);
            r.warnings.push_back(oss.str());
        }
        return r;
    }
};

} // namespace ventpy
