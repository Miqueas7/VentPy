/**
 * @file cobertura.hpp
 * @brief Análisis de déficit/cobertura: caudal medido vs requerido.
 *
 * Capa superior de la cadena de includes (único header autorizado a incluir
 * governor.hpp). Dos niveles, espejo del patrón calculate/calculate_full:
 *  - compare_zone: puro — requerido ya calculado + medición de campo.
 *  - analyze_survey: orquestador — corre el Governor por zona y agrega.
 *
 * Sustento normativo (gate validado 2026-08-17):
 *  - DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252: evaluaciones integrales
 *    semestrales; lit. f) cobertura de la demanda de la mina; lit. g)
 *    cobertura de las demandas por labor.
 *  - DS 024-2016-EM, Art. 248 (original): velocidad 20–250 m/min; con ANFO
 *    u otros agentes de voladura, mínimo 25 m/min.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <algorithm>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/governor.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Zona del levantamiento: demanda (entrada del Governor) + medición.
 *
 * `input.zone_type` NO puede ser GeneralMine: el total de mina lo calcula
 * el propio análisis (evita doble conteo).
 */
struct ZoneSurvey {
    std::string zone_name;
    VentilationInput input;
    ZoneMeasurement measurement;
};

/**
 * @brief Calculador de déficit/cobertura.
 */
class CoverageCalculator {
public:
    /**
     * @brief Nivel puro: compara un requerido ya calculado contra la medición.
     * @throws std::invalid_argument si la medición no tiene exactamente una
     *         fuente, o ante cualquier dato fuera de dominio.
     */
    [[nodiscard]] static ZoneCoverageResult compare_zone(
        double q_required_m3min,
        const ZoneMeasurement& measurement,
        const CoverageParams& params = {}
    ) {
        validate_params(params);
        validation::require_positive(q_required_m3min,
            "q_required_m3min [m3/min] - Caudal requerido de la zona");

        const bool has_direct = measurement.q_measured_m3min.has_value();
        const bool has_stations = !measurement.stations.empty();
        if (has_direct == has_stations) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: ZoneMeasurement debe tener "
                "exactamente una fuente (q_measured_m3min O stations).");
        }

        ZoneCoverageResult r;
        r.zone_name = measurement.zone_name;
        r.q_required_m3min = q_required_m3min;

        if (has_direct) {
            validation::require_non_negative(*measurement.q_measured_m3min,
                "q_measured_m3min [m3/min] - Caudal medido");
            r.q_measured_m3min = *measurement.q_measured_m3min;
        } else {
            for (const AirflowStation& s : measurement.stations) {
                StationResult sr = evaluate_station(s, params);
                r.q_measured_m3min += sr.q_station_m3min;
                r.stations.push_back(std::move(sr));
            }
        }

        r.coverage_ratio = r.q_measured_m3min / q_required_m3min;
        r.compliant = r.q_measured_m3min >= q_required_m3min;
        r.deficit_m3min = r.compliant
            ? 0.0
            : safety_ceil(q_required_m3min - r.q_measured_m3min);
        r.near_deficit_warning =
            r.compliant && r.coverage_ratio < 1.0 + params.warning_margin;
        r.overventilated = r.coverage_ratio > params.overventilation_factor;
        r.regulation_ref =
            "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252 lit. g - "
            "cobertura de la demanda de aire por labor";
        return r;
    }

private:
    /// Valida umbrales del análisis (frontera).
    static void validate_params(const CoverageParams& p) {
        validation::require_non_negative(p.warning_margin,
            "warning_margin - Margen de advertencia de cobertura");
        if (!(p.overventilation_factor > 1.0)) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: overventilation_factor debe ser > 1.");
        }
        validation::require_positive(p.min_velocity_mpm,
            "min_velocity_mpm [m/min] - Velocidad minima");
        if (!(p.min_velocity_mpm < p.max_velocity_mpm)) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: min_velocity_mpm debe ser menor "
                "que max_velocity_mpm (Art. 248: 20 y 250 m/min).");
        }
    }

    /// Task 2 la implementa; por ahora las mediciones por estaciones no
    /// están soportadas.
    static StationResult evaluate_station(
        const AirflowStation&, const CoverageParams&
    ) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: mediciones por estaciones aun no "
            "soportadas (Task 2).");
    }
};

} // namespace ventpy
