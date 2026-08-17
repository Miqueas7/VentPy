/**
 * @file atkinson.hpp
 * @brief Resistencia de ramal: ecuación de Atkinson + pérdidas por choque.
 *
 * Fuente de las tablas (NO NORMATIVA — bibliografía de ingeniería, gate
 * 2026-08-17): McPherson, M.J., "Subsurface Ventilation Engineering",
 * ed. 2009 (SRK): Cap. 5, Tabla 5.1 (p. 5-6) para k; Apéndice A5 (p. 5-26
 * a 5-38) para choque. k tabulado a densidad estándar 1.2 kg/m³
 * (corrección ρ/1.2, ec. 5.9). Conversión imperial: k[kg/m³] × 5.39e-7 =
 * k[lb·min²/ft⁴]. Ductos: valores de ducto NUEVO — McPherson recomienda
 * añadir ~20% por desgaste (VentPy NO lo aplica en silencio).
 *
 * @copyright 2026 VentPy Project
 */
#pragma once

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/atmosphere.hpp"
#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/// Tabla k de Atkinson — McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6.
inline const std::vector<FrictionFactorEntry>& atkinson_friction_factors() {
    static const std::vector<FrictionFactorEntry> table = {
        {AirwayLining::SmoothLined, 0.004,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Smooth concrete lined' (rectangular)"},
        {AirwayLining::Shotcrete, 0.0055,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Shotcrete' (rectangular)"},
        {AirwayLining::UnlinedMinorIrreg, 0.009,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Unlined with minor irregularities only'"},
        {AirwayLining::UnlinedTypical, 0.012,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Unlined, typical conditions no major irregularities'"},
        {AirwayLining::UnlinedRough, 0.016,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Unlined, rough or irregular conditions'"},
        {AirwayLining::ArchedDriftBolted, 0.010,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: Metal mines 'Arch-shaped level drifts, rock bolts and mesh'"},
        {AirwayLining::ArchedRampBolted, 0.014,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: Metal mines 'Arch-shaped ramps, rock bolts and mesh'"},
        {AirwayLining::TimberedCribbed, 0.14,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: Coal mines 'Cribbed entries 0.05 to 0.14' - "
         "extremo conservador del rango (decision de gate 2026-08-17)"},
        {AirwayLining::DuctFabricCollapsible, 0.0037,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Collapsible fabric ducting (forcing systems only)' - "
         "ducto nuevo; anadir ~20% por desgaste (nota 3 de la tabla)"},
        {AirwayLining::DuctFlexibleSpiral, 0.011,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Flexible ducting with fully stretched spiral "
         "spring reinforcement' - ducto nuevo; anadir ~20% por desgaste (nota 3)"},
        {AirwayLining::DuctFiberglass, 0.0024,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Fibreglass' - ducto nuevo (nota 3)"},
        {AirwayLining::DuctSteelSpiral, 0.0021,
         "McPherson (2009), Cap. 5, Tabla 5.1, p. 5-6: 'Spiral wound galvanized steel' - ducto nuevo (nota 3)"},
    };
    return table;
}

/// k de la tabla para un tipo de labor. @throws si Manual (k lo pone el usuario).
inline double friction_factor_for(AirwayLining lining) {
    if (lining == AirwayLining::Manual) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: AirwayLining::Manual requiere "
            "atkinson_k > 0 provisto por el usuario (no hay valor de tabla).");
    }
    for (const auto& e : atkinson_friction_factors()) {
        if (e.lining == lining) return e.k;
    }
    throw std::invalid_argument(
        "[VentPy] friction_factor_for: lining sin entrada de tabla");
}

/**
 * @brief Resuelve el factor de choque X de una singularidad.
 *
 * - Exit = 1.0 / Entrance = 0.5: límites exactos de las fórmulas de
 *   expansión/contracción brusca (McPherson A5.2, p. 5-28) con A→∞.
 * - Expansion: X = (1 − area_ratio)² con area_ratio = A1/A2 (A5.2.a).
 * - Contraction: X = 0.5·(1 − area_ratio)² con area_ratio = A2/A1 (A5.2.b).
 * - Bend90/Bend45/Junction: MANUAL-ONLY v1 (gate 2026-08-17): McPherson solo
 *   publica gráficos (Figs. A5.1–A5.3; corrección por ángulo Xθ = X90·k en
 *   A5.3) y la fórmula de junction (A5.3) exige velocidades de ramales que
 *   solo existirán con la red (SP-3b). Requieren shock_factor_x > 0.
 * @throws std::invalid_argument si falta el dato requerido.
 */
inline double resolve_shock_factor(const AirwaySingularity& s) {
    switch (s.type) {
        case SingularityType::Exit:     return 1.0;
        case SingularityType::Entrance: return 0.5;
        case SingularityType::Expansion:
        case SingularityType::Contraction: {
            if (!(s.area_ratio > 0.0 && s.area_ratio < 1.0)) {
                throw std::invalid_argument(
                    "Error de dominio [VentPy]: area_ratio (0 < r < 1) es "
                    "obligatorio para Expansion/Contraction (McPherson A5.2).");
            }
            const double d = 1.0 - s.area_ratio;
            return s.type == SingularityType::Expansion ? d * d : 0.5 * d * d;
        }
        case SingularityType::Bend90:
        case SingularityType::Bend45:
        case SingularityType::Junction:
        case SingularityType::Manual: {
            validation::require_positive(s.shock_factor_x,
                "shock_factor_x - Factor de choque manual (Bend/Junction/Manual)");
            return s.shock_factor_x;
        }
    }
    throw std::invalid_argument("[VentPy] resolve_shock_factor: tipo desconocido");
}

} // namespace ventpy
