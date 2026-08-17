/**
 * @file ducto.hpp
 * @brief Dimensionamiento de ducto de ventilación auxiliar.
 *
 * NOTA DE CAPAS (enmienda 2026-08-17, SP-3a): atkinson.hpp es un header de
 * física base (tablas k/X + resistencia de conducto), no un calculador de
 * caudal de demanda; ducto.hpp lo incluye para no duplicar la física. La
 * cadena sigue siendo acíclica: types → atmosphere → atkinson → ducto.
 *
 * Defaults ingenieriles (gate 2026-08-17, NO normativos): velocidad máxima
 * de ducto 20 m/s; diámetros comerciales {0.30, 0.40, 0.50, 0.60, 0.76,
 * 0.91, 1.07, 1.22} m (12"–48").
 *
 * @copyright 2026 VentPy Project
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/atkinson.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

class DuctSizingCalculator {
public:
    /// Técnico: menor diámetro comercial con v ≤ vmax y ΔP ≤ presión disponible.
    [[nodiscard]] static DuctSizingResult calculate(
        const DuctSizingParams& p, const AtmosphericParams& atm
    ) {
        DuctSizingResult r = evaluate_options(p, atm, nullptr);
        r.selection_criterion = "tecnico: menor diametro comercial viable";
        for (const DuctOptionResult& o : r.options) {
            if (o.rejection_reason.empty()) {
                r.selected_diameter_m = o.diameter_m;
                r.feasible = true;
                break;                        // lista ascendente ⇒ primero = menor
            }
        }
        if (!r.feasible) {
            r.warnings.push_back(
                "Ningun diametro comercial cumple las restricciones "
                "(revisar velocidad maxima, presion disponible o lista de diametros)");
        }
        return r;
    }

private:
    // Task 4 añade calculate_full y el costeo; evaluate_options es compartido.
    [[nodiscard]] static DuctSizingResult evaluate_options(
        const DuctSizingParams& p, const AtmosphericParams& atm,
        const EconomicParams* eco
    ) {
        validation::require_positive(p.q_m3min,  "q_m3min [m3/min] - Caudal del ducto");
        validation::require_positive(p.length_m, "length_m [m] - Longitud del ducto");
        // Tolerancia FP absoluta en fronteras (leccion SP-2)
        constexpr double V_TOL = 1e-9;   // [m/s]

        const double vmax = p.max_velocity_mps > 0.0 ? p.max_velocity_mps : 20.0;
        std::vector<double> diams = p.diameters_m.empty()
            ? std::vector<double>{0.30, 0.40, 0.50, 0.60, 0.76, 0.91, 1.07, 1.22}
            : p.diameters_m;
        for (double d : diams)
            validation::require_positive(d, "diameters_m [m] - Diametro comercial");
        std::sort(diams.begin(), diams.end());

        DuctSizingResult r;
        r.biblio_ref = "Fisica: McPherson (2009) Cap. 5 (Atkinson); defaults "
                       "ingenieriles de gate 2026-08-17 (NO normativos)";
        const double q_m3s = p.q_m3min / 60.0;

        for (double d : diams) {
            DuctOptionResult o;
            o.diameter_m = d;
            o.area_m2 = 3.14159265358979323846 * d * d / 4.0;
            o.velocity_mps = q_m3s / o.area_m2;

            AirwayParams a;
            a.airway_id = "duct";
            a.length_m = p.length_m;
            a.perimeter_m = 3.14159265358979323846 * d;
            a.area_m2 = o.area_m2;
            a.lining = p.duct_lining;
            a.atkinson_k = p.atkinson_k;
            a.singularities = p.singularities;
            o.r_total = AtkinsonCalculator::calculate_resistance(a, atm).r_total;
            o.pressure_drop_pa = o.r_total * q_m3s * q_m3s;

            o.velocity_ok = o.velocity_mps <= vmax + V_TOL;
            o.pressure_ok = p.available_pressure_pa <= 0.0 ||
                            o.pressure_drop_pa <= p.available_pressure_pa;
            if (!o.velocity_ok) {
                std::ostringstream oss;
                oss << "velocidad " << o.velocity_mps << " m/s > maximo " << vmax;
                o.rejection_reason = oss.str();
            } else if (!o.pressure_ok) {
                std::ostringstream oss;
                oss << "caida " << o.pressure_drop_pa << " Pa > disponible "
                    << p.available_pressure_pa;
                o.rejection_reason = oss.str();
            }
            (void)eco;   // Task 4 costea aquí
            r.options.push_back(std::move(o));
        }
        return r;
    }
};

} // namespace ventpy
