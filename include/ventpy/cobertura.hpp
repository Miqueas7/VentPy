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
 *
 * El zone_name del ZoneSurvey es el que manda en el informe; el de
 * measurement.zone_name se ignora en la ruta analyze_survey.
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

    /**
     * @brief Orquestador: corre el Governor por zona y agrega el balance.
     *
     * Cumplimiento estricto (safety-first): `compliant` exige cobertura
     * global (Art. 252.f) Y todas las zonas cubiertas (Art. 252.g).
     *
     * @throws std::invalid_argument si el levantamiento está vacío, hay
     *         nombres de zona duplicados o alguna zona es GeneralMine.
     */
    [[nodiscard]] static MineCoverageResult analyze_survey(
        const std::vector<ZoneSurvey>& zones,
        const RegulatoryConfig& config,
        const CoverageParams& params = {}
    ) {
        validate_params(params);
        if (zones.empty()) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: analyze_survey requiere al menos "
                "una zona (levantamiento vacio).");
        }
        std::set<std::string> names;
        for (const ZoneSurvey& z : zones) {
            if (z.input.zone_type == ZoneType::GeneralMine) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: ZoneType::GeneralMine no es "
                    "valido dentro del levantamiento (el total de mina lo "
                    "calcula el analisis; evita doble conteo).");
            }
            if (!names.insert(z.zone_name).second) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: nombre de zona duplicado en "
                    "el levantamiento: '" + z.zone_name + "'.");
            }
        }

        const VentilationGovernor governor{config};
        MineCoverageResult r;

        for (const ZoneSurvey& z : zones) {
            VentilationDemandResult demand = governor.calculateTotalDemand(z.input);
            if (demand.q_total_m3min <= 0.0) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: la zona '" + z.zone_name +
                    "' no genera requerimiento de ventilacion (q_total = 0) - revisar "
                    "su VentilationInput (trabajadores/flota/voladura).");
            }
            ZoneCoverageResult zr =
                compare_zone(demand.q_total_m3min, z.measurement, params);
            zr.zone_name = z.zone_name;
            zr.demand = demand;

            r.q_required_total_m3min += zr.q_required_m3min;
            r.q_measured_total_m3min += zr.q_measured_m3min;

            if (!zr.compliant) {
                std::ostringstream oss;
                oss << "Zona '" << z.zone_name << "': DEFICIT de "
                    << zr.deficit_m3min << " m3/min (cobertura "
                    << (zr.coverage_ratio * 100.0) << "%) - Art. 252 lit. g";
                r.warnings.push_back(oss.str());
            } else if (zr.near_deficit_warning) {
                std::ostringstream oss;
                oss << "Zona '" << z.zone_name << "': cobertura justa ("
                    << (zr.coverage_ratio * 100.0)
                    << "%) por debajo del margen ingenieril";
                r.warnings.push_back(oss.str());
            }
            if (zr.overventilated) {
                std::ostringstream oss;
                oss << "Zona '" << z.zone_name << "': sobre-ventilada ("
                    << (zr.coverage_ratio * 100.0)
                    << "%) - posible desperdicio de energia";
                r.warnings.push_back(oss.str());
            }
            for (const StationResult& sr : zr.stations) {
                if (!sr.velocity_ok) {
                    r.warnings.push_back("Zona '" + z.zone_name + "', " + sr.warning);
                }
            }
            r.zones.push_back(std::move(zr));
        }

        r.coverage_ratio = r.q_measured_total_m3min / r.q_required_total_m3min;
        r.global_compliant =
            r.q_measured_total_m3min >= r.q_required_total_m3min;
        r.all_zones_compliant = true;
        for (const ZoneCoverageResult& zr : r.zones) {
            if (!zr.compliant) { r.all_zones_compliant = false; break; }
        }
        r.compliant = r.global_compliant && r.all_zones_compliant;
        r.deficit_total_m3min = r.global_compliant
            ? 0.0
            : safety_ceil(r.q_required_total_m3min - r.q_measured_total_m3min);
        r.regulation_ref =
            "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252: evaluacion "
            "integral semestral; lit. f (cobertura de mina) y lit. g "
            "(cobertura por labor)";
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

    /// Evalúa una estación: caudal Q = A × v × 60 y velocidad vs Art. 248.
    /// Con ANFO el mínimo efectivo es max(min_velocity_mpm, 25) — Art. 248.
    [[nodiscard]] static StationResult evaluate_station(
        const AirflowStation& s,
        const CoverageParams& params
    ) {
        validation::require_positive(s.area_m2,
            "area_m2 [m2] - Seccion de la estacion de aforo");
        validation::require_non_negative(s.velocity_mps,
            "velocity_mps [m/s] - Velocidad medida en la estacion");

        StationResult r;
        r.station_id = s.station_id;
        r.area_m2 = s.area_m2;
        r.velocity_mps = s.velocity_mps;
        r.velocity_mpm = s.velocity_mps * 60.0;
        r.q_station_m3min = s.area_m2 * s.velocity_mps * 60.0;

        const double min_effective = params.anfo_or_blasting_agents
            ? std::max(params.min_velocity_mpm, 25.0)
            : params.min_velocity_mpm;

        r.velocity_ok = (r.velocity_mpm >= min_effective) &&
                        (r.velocity_mpm <= params.max_velocity_mpm);
        if (!r.velocity_ok) {
            std::ostringstream oss;
            oss << "Estacion '" << s.station_id << "': velocidad "
                << r.velocity_mpm << " m/min fuera de rango ["
                << min_effective << ", " << params.max_velocity_mpm
                << "] (DS 024-2016-EM, Art. 248";
            if (params.anfo_or_blasting_agents && r.velocity_mpm < min_effective) {
                oss << "; minimo 25 m/min con ANFO u otros agentes de voladura";
            }
            oss << ")";
            r.warning = oss.str();
        }
        return r;
    }
};

} // namespace ventpy
