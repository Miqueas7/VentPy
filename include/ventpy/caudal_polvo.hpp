/**
 * @file caudal_polvo.hpp
 * @brief Cálculo de caudal para dilución de polvo respirable (Q_polvo).
 *
 * Normativa: DS 024-2016-EM, Art. 111 (texto original, no modificado por el
 * DS 023-2017-EM): LEO de polvo respirable 3 mg/m³ para jornada de 8 h, con
 * obligación de paralizar la labor si se supera. La sílice NO tiene valor
 * propio en el articulado (remite al Anexo 15 / DS 015-2005-SA): este
 * calculador advierte la remisión, nunca inventa un límite.
 *
 * Modelo: dilución con aire de ingreso limpio (C_in = 0, conservador respecto
 * de aire de ingreso ya cargado NO — documentado: si el ingreso trae polvo,
 * el usuario debe reducir su target). Q = G_efectiva / C_objetivo. El límite
 * en mg/m³ se compara a condiciones del sitio (sin corrección por densidad).
 *
 * @copyright 2026 VentPy Project
 */
#pragma once

#include <cmath>
#include <limits>
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
        (void)config;   // reservado para presets con LEO distinto (multi-norma)
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

        // Calcular caudal: reorganizar para minimizar errores de redondeo
        const double q_m3min_raw = r.effective_generation * 60.0 / p.target_concentration_mg_m3;

        // Aplicar safety_ceil robusto: si está muy cercano a un entero (dentro de
        // error numérico típico), usar ese entero. Si no, aplicar ceil.
        // Esto maneja correctamente casos como 300.0000001 que deberían ser 300.
        const double rounded = std::round(q_m3min_raw);
        constexpr double snap_epsilon = 1e-9;
        if (std::abs(q_m3min_raw - rounded) < snap_epsilon) {
            r.q_dust = (q_m3min_raw >= rounded) ? rounded : rounded + 1.0;
        } else {
            r.q_dust = safety_ceil(q_m3min_raw);
        }
        if (p.face_area_m2 > 0.0) {
            r.resulting_velocity_mps = (r.q_dust / 60.0) / p.face_area_m2;
        }
        r.regulation_ref =
            "DS 024-2016-EM, Art. 111 (LEO polvo respirable 3 mg/m3, jornada "
            "8 h; paralizacion si se supera)";

        if (p.silica_content_percent > 0.0) {
            std::ostringstream oss;
            oss << "Silice presente (" << p.silica_content_percent
                << "%): el LEO especifico de silice se rige por el Anexo 15 / "
                << "DS 015-2005-SA (no cuantificado por este calculador)";
            r.warnings.push_back(oss.str());
        }
        if (p.target_concentration_mg_m3 > 3.0) {
            std::ostringstream oss;
            oss << "Concentracion objetivo (" << p.target_concentration_mg_m3
                << " mg/m3) por ENCIMA del LEO del Art. 111 (3 mg/m3)";
            r.warnings.push_back(oss.str());
        }
        return r;
    }
};

} // namespace ventpy
