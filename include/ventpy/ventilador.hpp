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

    /**
     * @brief Punto de operación del ventilador DENTRO de una red: punto fijo
     * con sub-relajación sobre NetworkSolver::solve (iteración externa —
     * viable porque cada solve usa presión fija; ver spec SP-3c).
     *
     * El fan_pressure_pa declarado en el ramal se IGNORA (lo gobierna la
     * curva); se advierte si venía > 0. El clamp del caudal al rango del
     * catálogo es solo estabilizador interno: si la ITERACIÓN FINAL queda
     * fuera de rango, in_curve_range = false con advertencia.
     */
    [[nodiscard]] static FanOperatingResult operating_point_in_network(
        const NetworkDefinition& network, const std::string& fan_branch_id,
        const FanCurve& curve, const AtmosphericParams& atm,
        const SolverParams& solver_params = {},
        const FanOperatingParams& params = {}
    ) {
        validate_curve(curve);
        validate_params(params);

        int fan_idx = -1;
        for (size_t i = 0; i < network.branches.size(); ++i)
            if (network.branches[i].branch_id == fan_branch_id)
                fan_idx = static_cast<int>(i);
        if (fan_idx < 0) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: fan_branch_id '" + fan_branch_id +
                "' no existe en la red.");
        }

        FanOperatingResult r;
        r.fan_id = curve.fan_id;
        r.air_density_kg_m3 = site_density(atm);
        r.density_factor = r.air_density_kg_m3 / curve.rated_density_kg_m3;
        r.biblio_ref = BIBLIO;

        NetworkDefinition net = network;
        if (net.branches[fan_idx].fan_pressure_pa > 0.0) {
            r.warnings.push_back(
                "El fan_pressure_pa declarado en el ramal '" + fan_branch_id +
                "' se ignora: lo gobierna la curva del ventilador.");
        }

        const double q_lo = curve.points.front().q_m3min;
        const double q_hi = curve.points.back().q_m3min;
        // Arranque: presión del punto medio del catálogo (a densidad de sitio)
        double p_fan = pressure_at(curve, 0.5 * (q_lo + q_hi),
                                   r.air_density_kg_m3);
        double q_prev = 0.0;
        double q_raw = 0.0;
        double p_used = p_fan;
        NetworkSolveResult last_net;
        for (int it = 1; it <= params.max_iterations; ++it) {
            r.iterations = it;
            net.branches[fan_idx].fan_pressure_pa = p_fan;
            p_used = p_fan;                      // la presión que este solve consume
            last_net = NetworkSolver::solve(net, atm, solver_params);
            if (!last_net.converged) break;   // red no balanceó: abortar auditable
            q_raw = std::abs(last_net.branches[fan_idx].q_m3min);
            const double q_clamped = std::clamp(q_raw, q_lo, q_hi);
            const double p_target =
                pressure_at(curve, q_clamped, r.air_density_kg_m3);
            p_fan += params.under_relaxation * (p_target - p_fan);
            if (it > 1 && std::abs(q_raw - q_prev) <= solver_params.tolerance_m3min) {
                r.converged = true;
                break;
            }
            q_prev = q_raw;
        }
        r.network = last_net;
        r.q_m3min = q_raw;
        // presión del ÚLTIMO solve — trazable con network embebido (auditoría)
        r.pressure_pa = p_used;
        // Signo ANTES del abs, sobre la última red resuelta: si la red fuerza
        // flujo to->from por el ramal del ventilador, las fan laws no aplican
        // (curva definida para flujo from->to) y el punto no es válido.
        if (last_net.converged &&
            last_net.branches[fan_idx].q_m3min < 0.0) {
            r.warnings.push_back(
                "FLUJO INVERTIDO en el ramal del ventilador '" + fan_branch_id +
                "': la red fuerza flujo contra el sentido del ventilador - el punto "
                "de operacion NO es fisicamente valido (curva no aplica a flujo inverso).");
            r.converged = false;
        }
        r.in_curve_range = r.converged && q_raw >= q_lo && q_raw <= q_hi;
        if (!r.converged) {
            r.warnings.push_back(
                "NO CONVERGIO el punto fijo ventilador-red (o la red interna "
                "no balanceo). Resultados NO confiables.");
        } else if (!r.in_curve_range) {
            r.warnings.push_back(
                "El punto de operacion quedo FUERA del rango del catalogo "
                "(se uso clamp estabilizador): revisar seleccion del ventilador.");
        }
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
