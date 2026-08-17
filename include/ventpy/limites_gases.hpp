/**
 * @file limites_gases.hpp
 * @brief Tablas de Límites Máximos Permisibles (LMP) de gases por norma.
 *
 * Fuentes (validadas 2026-08-17 contra textos oficiales):
 * - Perú: DS 024-2016-EM, Anexo 15 (obligatorio vía Art. 246, mod. DS 023-2017-EM).
 * - Chile: DS 594 Art. 66 (vía remisión del DS 132 Arts. 135/144) y DS 132.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "ventpy/types.hpp"

namespace ventpy {

/**
 * @brief Tabla LMP de Perú — DS 024-2016-EM, Anexo 15 (vía Art. 246).
 *
 * El Anexo 15 (tomado del DS 015-2005-SA) no fue modificado por el
 * DS 023-2017-EM. El Anexo 15 NO corrige los ppm por altitud; su nota de
 * conversión (mg/m³ = ppm × PM / 24,45) aplica a la conversión de unidades.
 */
inline const std::vector<GasLimit>& gas_limits_peru() {
    static const std::vector<GasLimit> table = {
        {.gas = GasType::CO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 33 (via Art. 246)"},
        {.gas = GasType::CO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 5000.0, .stel = 30000.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 20 (via Art. 246)"},
        {.gas = GasType::NO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 3.0, .stel = 5.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 21 (via Art. 246)"},
        {.gas = GasType::SO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 2.0, .stel = 5.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 7 'Anhidrido Sulfuroso' (via Art. 246)"},
        {.gas = GasType::H2S, .unit = ConcentrationUnit::PPM,
         .twa_8h = 10.0, .stel = 15.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 5 (via Art. 246)"},
        {.gas = GasType::CH4, .unit = ConcentrationUnit::PPM,
         .ceiling = 5000.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 32, columna Techo C "
                           "(= 0.5%; coherente con Art. 259: zona 'gaseada' si CH4 > 0.5%)"},
        {.gas = GasType::NO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "DS 024-2016-EM, Anexo 15, fila 34 (via Art. 246)"},
        {.gas = GasType::O2,  .unit = ConcentrationUnit::PercentVolume,
         .ceiling = 22.5, .floor_min = 19.5,
         .regulation_ref = "DS 024-2016-EM, Art. 246.b (min 19.5%); Anexo 15, fila 36 (max 22.5%)"},
    };
    return table;
}

/**
 * @brief Tabla LMP completa de la norma indicada.
 * @throws std::invalid_argument si la norma no tiene tabla implementada.
 */
inline const std::vector<GasLimit>& gas_limits(RegulatoryStandard standard) {
    switch (standard) {
        case RegulatoryStandard::DS024_Peru:
            return gas_limits_peru();
        default:
            throw std::invalid_argument(
                "[VentPy] gas_limits: no LMP table implemented for this standard");
    }
}

/**
 * @brief LMP de un gas bajo una norma.
 * @throws std::invalid_argument si el gas no está regulado en esa norma
 *         (nunca se retorna un valor por defecto silencioso — safety-critical).
 */
inline const GasLimit& lmp_for(RegulatoryStandard standard, GasType gas) {
    for (const GasLimit& g : gas_limits(standard)) {
        if (g.gas == gas) return g;
    }
    throw std::invalid_argument(
        "[VentPy] lmp_for: gas not regulated under the selected standard");
}

} // namespace ventpy
