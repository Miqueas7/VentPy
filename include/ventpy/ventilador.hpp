/**
 * @file ventilador.hpp
 * @brief Curva de ventilador, punto de operación y margen de stall.
 *
 * Fuente (NO NORMATIVA, gate 2026-08-18): McPherson (2009), Cap. 10 "Fans" —
 * ec. (10.28) sec. 10.4.1: la presión del ventilador escala con la densidad a
 * igual caudal volumétrico (curvas de catálogo referidas a rated_density,
 * default 1.2 kg/m³); característica de stall de axiales sec. 10.1 (zona
 * inestable a la izquierda del pico de presión).
 *
 * Capa: types → atmosphere → atkinson → red → ventilador.
 * SIN safety_ceil: puntos de equilibrio crudos. Nunca se extrapola la curva
 * en silencio.
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

#include "ventpy/red.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class FanCalculator {
public:
    /**
     * @brief Presión de la curva a densidad de sitio (interpolación lineal).
     * @throws std::invalid_argument fuera del rango del catálogo o curva inválida.
     */
    [[nodiscard]] static double pressure_at(
        const FanCurve& curve, double q_m3min, double air_density_kg_m3
    ) {
        validate_curve(curve);
        validation::require_positive(air_density_kg_m3,
            "air_density_kg_m3 [kg/m3] - Densidad del aire");
        const auto& pts = curve.points;
        if (q_m3min < pts.front().q_m3min || q_m3min > pts.back().q_m3min) {
            std::ostringstream oss;
            oss << "Error de dominio [VentPy]: q_m3min = " << q_m3min
                << " fuera del rango del catalogo [" << pts.front().q_m3min
                << ", " << pts.back().q_m3min << "] del ventilador '"
                << curve.fan_id << "' (no se extrapola).";
            throw std::invalid_argument(oss.str());
        }
        const double factor = air_density_kg_m3 / curve.rated_density_kg_m3;
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            if (q_m3min <= pts[i + 1].q_m3min) {
                const double t = (q_m3min - pts[i].q_m3min) /
                                 (pts[i + 1].q_m3min - pts[i].q_m3min);
                const double p_cat = pts[i].pressure_pa +
                    t * (pts[i + 1].pressure_pa - pts[i].pressure_pa);
                return p_cat * factor;   // McPherson ec. 10.28
            }
        }
        throw std::logic_error("[VentPy] pressure_at: inalcanzable");
    }

    /**
     * @brief Punto de operación contra resistencia de sistema: P(Q) = R·(Q/60)².
     * Bisección sobre el rango del catálogo (precisión interna 1e-6 m³/min).
     */
    [[nodiscard]] static FanOperatingResult operating_point(
        const FanCurve& curve, double r_system_ns2m8,
        const AtmosphericParams& atm, const FanOperatingParams& params = {}
    ) {
        validate_curve(curve);
        validation::require_positive(r_system_ns2m8,
            "r_system_ns2m8 [Ns2/m8] - Resistencia del sistema");
        validate_params(params);

        FanOperatingResult r;
        r.fan_id = curve.fan_id;
        r.air_density_kg_m3 = site_density(atm);
        r.density_factor = r.air_density_kg_m3 / curve.rated_density_kg_m3;
        r.biblio_ref = BIBLIO;

        const double q_lo = curve.points.front().q_m3min;
        const double q_hi = curve.points.back().q_m3min;
        auto f = [&](double q) {
            return pressure_at(curve, q, r.air_density_kg_m3) -
                   r_system_ns2m8 * (q / 60.0) * (q / 60.0);
        };
        const double f_lo = f(q_lo), f_hi = f(q_hi);
        if (!(f_lo > 0.0 && f_hi < 0.0)) {
            r.warnings.push_back(f_lo < 0.0
                ? "Sin interseccion en catalogo: sistema demasiado resistivo "
                  "(el punto de operacion caeria ANTES del primer punto de la curva)"
                : "Sin interseccion en catalogo: sistema poco resistivo "
                  "(el punto de operacion caeria MAS ALLA del ultimo punto)");
            assess_stall(r, curve, params);   // pico informativo igual
            return r;                          // converged=false, in_curve_range=false
        }
        double lo = q_lo, hi = q_hi;
        while (hi - lo > 1e-6) {
            const double mid = 0.5 * (lo + hi);
            (f(mid) > 0.0 ? lo : hi) = mid;
        }
        r.q_m3min = 0.5 * (lo + hi);
        r.pressure_pa = r_system_ns2m8 * (r.q_m3min / 60.0) * (r.q_m3min / 60.0);
        r.in_curve_range = true;
        r.converged = true;
        assess_stall(r, curve, params);
        return r;
    }

private:
    static constexpr const char* BIBLIO =
        "McPherson (2009), Cap. 10 'Fans': ec. 10.28 (fan laws, densidad); "
        "caracteristica de stall sec. 10.1";

    static void validate_params(const FanOperatingParams& p) {
        validation::require_non_negative(p.stall_margin,
            "stall_margin - Margen de stall en caudal");
        validation::require_in_range(p.under_relaxation, 1e-9, 1.0,
            "under_relaxation");
        validation::require_positive_int(p.max_iterations,
            "max_iterations - Iteraciones del punto fijo");
    }

    /// Pico del catálogo y semántica de stall. El índice del pico no depende
    /// de la densidad (factor común a toda la curva).
    static void assess_stall(FanOperatingResult& r, const FanCurve& curve,
                             const FanOperatingParams& params) {
        size_t peak = 0;
        for (size_t i = 1; i < curve.points.size(); ++i)
            if (curve.points[i].pressure_pa > curve.points[peak].pressure_pa)
                peak = i;
        r.q_peak_m3min = curve.points[peak].q_m3min;
        r.pressure_peak_pa = curve.points[peak].pressure_pa * r.density_factor;

        if (peak == 0) {
            // Curva monótona decreciente: sin zona inestable en catálogo
            r.stall_ok = true;
            if (r.converged) {
                r.stall_margin_actual =
                    (r.q_m3min - r.q_peak_m3min) / r.q_peak_m3min;
            }
            r.warnings.push_back(
                "Curva monotona decreciente en catalogo: sin zona de stall "
                "observable (pico en el primer punto)");
            return;
        }
        if (!r.converged) return;   // sin punto de operación no hay veredicto
        r.stall_margin_actual = (r.q_m3min - r.q_peak_m3min) / r.q_peak_m3min;
        r.stall_ok = r.q_m3min >= r.q_peak_m3min * (1.0 + params.stall_margin);
        if (r.q_m3min <= r.q_peak_m3min) {
            std::ostringstream oss;
            oss << "ZONA DE STALL: el punto de operacion (" << r.q_m3min
                << " m3/min) esta en o a la izquierda del pico ("
                << r.q_peak_m3min << " m3/min) - operacion inestable "
                << "(McPherson Cap. 10, sec. 10.1)";
            r.warnings.push_back(oss.str());
        } else if (!r.stall_ok) {
            std::ostringstream oss;
            oss << "Margen de stall insuficiente: "
                << (r.stall_margin_actual * 100.0) << "% < "
                << (params.stall_margin * 100.0) << "% requerido";
            r.warnings.push_back(oss.str());
        }
    }

    static void validate_curve(const FanCurve& c) {
        if (c.points.size() < 2) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: FanCurve requiere al menos 2 "
                "puntos de catalogo.");
        }
        validation::require_positive(c.rated_density_kg_m3,
            "rated_density_kg_m3 [kg/m3] - Densidad de referencia de la curva");
        for (size_t i = 0; i < c.points.size(); ++i) {
            validation::require_non_negative(c.points[i].pressure_pa,
                "pressure_pa [Pa] - Presion del punto de catalogo");
            validation::require_positive(c.points[i].q_m3min,
                "q_m3min [m3/min] - Caudal del punto de catalogo");
            if (i > 0 && !(c.points[i].q_m3min > c.points[i - 1].q_m3min)) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: los puntos de FanCurve deben "
                    "ser estrictamente crecientes en caudal.");
            }
        }
    }

    /// ρ de sitio — mismo criterio que AtkinsonCalculator (presión dada u
    /// obtenida de la altitud, con temperatura de bulbo seco).
    [[nodiscard]] static double site_density(const AtmosphericParams& atm) {
        const double pressure_kpa = atm.barometric_pressure_kpa > 0.0
            ? atm.barometric_pressure_kpa
            : AtmosphereCalculator::calculate_pressure_kpa(atm.altitude_masl);
        return AtmosphereCalculator::calculate_density_from_pressure_kpa(
            pressure_kpa, atm.dry_bulb_temp_c);
    }
};

} // namespace ventpy
