/**
 * @file caudal_polvo.hpp
 * @brief Cálculo de caudal para dilución de polvo respirable (Q_polvo).
 *
 * Normativa (Perú, preset por defecto): DS 024-2016-EM, Art. 111 (texto
 * original, no modificado por el DS 023-2017-EM): LEO de polvo respirable
 * 3 mg/m³ para jornada de 8 h, con obligación de paralizar la labor si se
 * supera. La sílice NO tiene valor propio en el articulado (remite al Anexo 15
 * / DS 015-2005-SA): este calculador advierte la remisión, nunca inventa un
 * límite.
 *
 * La cita emitida en `regulation_ref` sigue a `config.standard()` a través de
 * `regulation_reference` (normativa.hpp). Bajo el DS 132 chileno esta librería
 * no tiene un límite de polvo respirable verificado: la cita lo declara y
 * atribuye la concentración objetivo a criterio de ingeniería del usuario, en
 * vez de trasladar el Art. 111 peruano. El umbral de 3 mg/m³ de las
 * advertencias es, bajo esa norma, una referencia ingenieril.
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class DustFlowCalculator {
public:
    [[nodiscard]] static DustFlowResult calculate(
        const DustParams& p, const RegulatoryConfig& config
    ) {
        validation::require_non_negative(p.dust_generation_rate_mg_s,
            "dust_generation_rate_mg_s [mg/s] - Generacion de polvo");
        validation::require_positive(p.target_concentration_mg_m3,
            "target_concentration_mg_m3 [mg/m3] - Concentracion objetivo");
        validation::require_in_range(p.suppression_efficiency, 0.0, 0.999999,
            "suppression_efficiency");
        validation::require_in_range(p.silica_content_percent, 0.0, 100.0,
            "silica_content_percent [%]");
        validation::require_non_negative(p.face_area_m2,
            "face_area_m2 [m2] - Seccion de la labor");

        DustFlowResult r;
        r.dust_generation_mg_s = p.dust_generation_rate_mg_s;
        r.target_concentration = p.target_concentration_mg_m3;
        r.suppression_efficiency =
            p.water_suppression ? p.suppression_efficiency : 0.0;
        r.effective_generation = p.dust_generation_rate_mg_s *
            (p.water_suppression ? (1.0 - p.suppression_efficiency) : 1.0);

        // Calcular caudal
        const double q_m3s = r.effective_generation / p.target_concentration_mg_m3;

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
        r.regulation_ref =
            regulation_reference(RegulatoryTopic::DustRespirableLimit, config);

        if (p.silica_content_percent > 0.0) {
            std::ostringstream oss;
            oss << "Silice presente (" << p.silica_content_percent
                << "%): el limite especifico de silice se rige por "
                << regulation_reference(
                       RegulatoryTopic::DustSilicaReferral, config)
                << " (no cuantificado por este calculador)";
            r.warnings.push_back(oss.str());
        }
        if (p.target_concentration_mg_m3 > 3.0) {
            std::ostringstream oss;
            oss << "Concentracion objetivo (" << p.target_concentration_mg_m3
                << " mg/m3) por ENCIMA de 3 mg/m3 - "
                << regulation_reference(RegulatoryTopic::DustLimitBasis, config);
            r.warnings.push_back(oss.str());
        }
        return r;
    }
};

} // namespace ventpy
