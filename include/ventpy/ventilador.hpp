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

private:
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
