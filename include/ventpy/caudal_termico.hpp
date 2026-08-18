/**
 * @file caudal_termico.hpp
 * @brief Cálculo de caudal por carga térmica (Q_termico).
 *
 * Normativa: DS 024-2016-EM, Art. 252.d (mod. DS 023-2017-EM): velocidad
 * mínima de 30 m/min cuando la temperatura seca de la labor está entre 24 y
 * 29°C — único criterio de ventilación térmica citable de la norma vigente.
 * Art. 104 + Anexo 13: remisión a evaluación de estrés térmico (WBGT) cuando
 * la temperatura del aire lo amerite (> 29°C aquí; v1 NO calcula WBGT).
 *
 * `target_effective_temp_c` (default 28°C) y `constants::MAX_EFFECTIVE_TEMP_C`
 * (30°C) son objetivos INGENIERILES heredados del derogado DS 055-2010-EM —
 * la norma vigente no fija una temperatura efectiva máxima (gate G4, SP-4).
 *
 * Modelo:
 * - Autocompresión: el aire se calienta al descender
 *   (`inlet_temp_bottom = dry_bulb + auto_compression × profundidad/100`).
 * - VRT (temperatura virgen de roca) en profundidad: puramente informativo;
 *   si supera `target + 10°C` se advierte recomendar estudio geotérmico.
 *   `heat_from_rock_kw` v1 = 0 (requiere modelo de transferencia roca-aire —
 *   extensión fuera de alcance; el usuario puede aproximarlo sumándolo a
 *   `heat_from_equipment_kw`/`heat_from_oxidation_kw`).
 * - Carga total = equipos + oxidación (+0 de roca v1). La autocompresión NO
 *   es una fuente de calor en kW: reduce el ΔT disponible (es un cambio de
 *   temperatura del aire de ingreso, no una carga generada en la labor).
 * - ΔT disponible = objetivo − temperatura de entrada. Si ≤ 0.5°C (piso
 *   documentado): infactible por autocompresión ⇒ q_thermal = 0 y
 *   advertencia FUERTE (se requiere refrigeración mecánica) — nunca un
 *   caudal absurdo por división cercana a cero.
 * - Balance sensible: Q = carga_total / (ρ_sitio × cp × ΔT), con
 *   `cp = constants::AIR_CP_KJ_KG_K` y ρ del sitio (mismo ternario
 *   presión/altitud + bulbo seco que atkinson.hpp/ventilador.hpp).
 * - Criterio Art. 252.d: si la temperatura de entrada ∈ [24, 29]°C y hay
 *   sección de labor, se exige además 30 m/min (`face_area × 0.5 × 60`); el
 *   caudal final es el máximo entre balance y este piso. La cita del
 *   Art. 252.d aparece en `regulation_ref` siempre que la temperatura esté en
 *   ese rango, gobierne o no el resultado (documenta la aplicabilidad real
 *   del criterio, no solo cuándo domina numéricamente).
 * - Si la temperatura de entrada > 29°C: advertencia de remisión al Art. 104
 *   + Anexo 13 (estrés térmico WBGT; v1 no lo calcula).
 *
 * @copyright 2026 VentPy Project
 */
#pragma once

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/atmosphere.hpp"
#include "ventpy/normativa.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class ThermalFlowCalculator {
public:
    [[nodiscard]] static ThermalFlowResult calculate(
        const ThermalParams& p, const AtmosphericParams& atm,
        const RegulatoryConfig& config
    ) {
        (void)config;   // reservado para presets con criterio distinto (multi-norma)
        validation::require_non_negative(p.heat_from_equipment_kw,
            "heat_from_equipment_kw [kW] - Calor de equipos");
        validation::require_non_negative(p.heat_from_oxidation_kw,
            "heat_from_oxidation_kw [kW] - Calor de oxidacion mineral");
        validation::require_non_negative(p.depth_below_surface_m,
            "depth_below_surface_m [m] - Profundidad");
        validation::require_non_negative(p.geothermal_gradient_c_per_100m,
            "geothermal_gradient_c_per_100m [C/100m] - Gradiente geotermico");
        validation::require_non_negative(p.auto_compression_c_per_100m,
            "auto_compression_c_per_100m [C/100m] - Autocompresion");
        validation::require_positive(p.target_effective_temp_c,
            "target_effective_temp_c [C] - Temperatura objetivo");
        validation::require_non_negative(p.face_area_m2,
            "face_area_m2 [m2] - Seccion de la labor");

        ThermalFlowResult r;

        // Autocompresión: temperatura de entrada al fondo de la labor.
        const double inlet_temp_bottom = atm.dry_bulb_temp_c +
            p.auto_compression_c_per_100m * p.depth_below_surface_m / 100.0;
        r.inlet_temp_c = inlet_temp_bottom;
        r.target_temp_c = p.target_effective_temp_c;
        r.heat_from_autocompression_kw = 0.0;  // reduce ΔT, no es carga en kW
        r.heat_from_rock_kw = 0.0;             // v1: sin modelo roca-aire
        r.heat_from_other_kw = 0.0;
        r.heat_from_equipment_kw = p.heat_from_equipment_kw;
        r.total_heat_load_kw = p.heat_from_equipment_kw + p.heat_from_oxidation_kw +
            r.heat_from_rock_kw;

        r.delta_t_available = p.target_effective_temp_c - inlet_temp_bottom;

        // VRT en profundidad: informativo (heat_from_rock_kw permanece 0 en v1).
        const double vrt_at_depth = p.virgin_rock_temp_c +
            p.geothermal_gradient_c_per_100m * p.depth_below_surface_m / 100.0;
        if (vrt_at_depth > p.target_effective_temp_c + 10.0) {
            std::ostringstream oss;
            oss << "Temperatura virgen de roca proyectada a la profundidad ("
                << vrt_at_depth << " C) supera el objetivo + 10 C ("
                << (p.target_effective_temp_c + 10.0)
                << " C): se recomienda estudio geotermico detallado";
            r.warnings.push_back(oss.str());
        }

        // Tolerancia FP sustractiva antes del ceil (patrón SP-2/Task-1:
        // absorbe artefactos de punto flotante sin usar round y sin poder
        // JAMÁS sobre-reportar: ceil(x−eps) ≤ ceil(x); lo máximo que "resta"
        // es < 1e-9 m³/min, por debajo de precisión instrumental).
        constexpr double FP_TOL = 1e-9;

        if (r.delta_t_available <= 0.5) {
            // Piso documentado: infactible por autocompresión — nunca un
            // caudal absurdo por división cercana a cero. Pero el piso legal
            // de velocidad del Art. 252.d (30 m/min con temperatura seca en
            // [24,29] C) NUNCA se descarta: si aplica, sigue siendo exigible
            // aunque el balance térmico sensible sea infactible.
            const bool in_252d_range_infeasible =
                inlet_temp_bottom >= 24.0 && inlet_temp_bottom <= 29.0;
            if (in_252d_range_infeasible && p.face_area_m2 > 0.0) {
                r.q_thermal = std::max(0.0,
                    safety_ceil(p.face_area_m2 * 0.5 * 60.0 - FP_TOL));
                r.regulation_ref =
                    "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min "
                    "con temperatura seca 24-29 C) — balance termico "
                    "sensible INFACTIBLE por autocompresion, se requiere "
                    "refrigeracion mecanica adicional para alcanzar el "
                    "objetivo termico";
            } else {
                r.q_thermal = 0.0;
                r.regulation_ref =
                    "DS 024-2016-EM: objetivo de diseño no alcanzable por "
                    "ventilacion (autocompresion agota el ΔT disponible)";
            }
            std::ostringstream oss;
            oss << "INFACTIBLE por autocompresion: temperatura de entrada ("
                << inlet_temp_bottom << " C) deja un ΔT disponible ("
                << r.delta_t_available
                << " C) demasiado bajo (<= 0.5 C) — se requiere "
                   "refrigeracion mecanica; la ventilacion sola no puede "
                   "alcanzar el objetivo de " << p.target_effective_temp_c
                << " C";
            r.warnings.push_back(oss.str());

            // I2: la remisión al Art. 104 + Anexo 13 (estrés térmico WBGT)
            // aplica por temperatura de entrada, independiente de si el
            // balance térmico es factible o no.
            if (inlet_temp_bottom > 29.0) {
                std::ostringstream oss2;
                oss2 << "Temperatura de entrada (" << inlet_temp_bottom
                    << " C) supera 29 C: remitir a evaluacion de estres "
                       "termico WBGT (Art. 104 + Anexo 13; no calculado por "
                       "este calculador)";
                r.warnings.push_back(oss2.str());
            }
            return r;
        }

        // ρ de sitio: mismo ternario presión/altitud + dry_bulb que
        // atkinson.hpp/ventilador.hpp.
        const double pressure_kpa = atm.barometric_pressure_kpa > 0.0
            ? atm.barometric_pressure_kpa
            : AtmosphereCalculator::calculate_pressure_kpa(atm.altitude_masl);
        const double site_density_kg_m3 =
            AtmosphereCalculator::calculate_density_from_pressure_kpa(
                pressure_kpa, atm.dry_bulb_temp_c);

        // Balance sensible: Q [m³/s] = carga_total / (ρ × cp × ΔT).
        const double q_balance_m3s = r.total_heat_load_kw /
            (site_density_kg_m3 * constants::AIR_CP_KJ_KG_K * r.delta_t_available);
        const double q_balance = safety_ceil(q_balance_m3s * 60.0 - FP_TOL);

        const bool in_252d_range = inlet_temp_bottom >= 24.0 &&
            inlet_temp_bottom <= 29.0;

        double q_thermal = q_balance;
        std::string regulation_ref =
            "DS 024-2016-EM: balance termico sensible (criterio ingenieril; "
            "carga = equipos + oxidacion, autocompresion reduce el DT "
            "disponible)";

        if (in_252d_range && p.face_area_m2 > 0.0) {
            const double q_for_velocity_raw = p.face_area_m2 * 0.5 * 60.0;
            const double q_for_velocity = safety_ceil(q_for_velocity_raw - FP_TOL);
            q_thermal = std::max(q_balance, q_for_velocity);
            regulation_ref =
                "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min con "
                "temperatura seca 24-29 C) combinado con balance termico "
                "sensible (criterio ingenieril)";
        }

        // Clamp cosmetico: evita -0.0 cuando la carga/generacion es 0
        // (safety_ceil(-FP_TOL) puede devolver -0.0).
        r.q_thermal = std::max(0.0, q_thermal);
        r.regulation_ref = regulation_ref;
        if (p.face_area_m2 > 0.0) {
            r.resulting_velocity_mps = (r.q_thermal / 60.0) / p.face_area_m2;
        }

        if (inlet_temp_bottom > 29.0) {
            std::ostringstream oss;
            oss << "Temperatura de entrada (" << inlet_temp_bottom
                << " C) supera 29 C: remitir a evaluacion de estres termico "
                   "WBGT (Art. 104 + Anexo 13; no calculado por este "
                   "calculador)";
            r.warnings.push_back(oss.str());
        }

        return r;
    }
};

} // namespace ventpy
