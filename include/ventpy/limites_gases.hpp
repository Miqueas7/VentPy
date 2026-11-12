/**
 * @file limites_gases.hpp
 * @brief Tablas de Límites Máximos Permisibles (LMP) de gases por norma.
 *
 * Fuentes (validadas contra los textos oficiales):
 * - Perú: DS 024-2016-EM, Anexo 15 (obligatorio vía Art. 246, mod. DS 023-2017-EM).
 * - Chile: DS 594 Art. 66 (vía remisión del DS 132 Arts. 135/144) y DS 132.
 * - Colombia: Decreto 1886 de 2015, Arts. 38, 39 y 53.
 * - México: NOM-010-STPS-2014, Tabla I.1 (vía remisión de la NOM-023-STPS-2012).
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
 * @brief Tabla LMP de Chile — DS 594 Art. 66 (vía remisión del DS 132
 *        Arts. 135/144) y DS 132 directos.
 *
 * Los límites en ppm NO se corrigen por altitud: el factor Fa = P/760 del
 * DS 594 Art. 63 (> 1.000 msnm) aplica SOLO a límites expresados en mg/m³;
 * la unidad canónica de esta tabla es ppm / % vol (decisión de diseño).
 * Jornadas > 8 h requieren además el factor Fj (DS 594, Art. 62) — fuera del
 * alcance de esta tabla.
 */
inline const std::vector<GasLimit>& gas_limits_chile() {
    static const std::vector<GasLimit> table = {
        {.gas = GasType::CO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 44.0,
         .regulation_ref = "DS 594, Art. 66 (LPP 44 ppm / 48 mg/m3). Umbral operacional "
                           "diesel: DS 132, Art. 135 detiene equipos a 40 ppm"},
        {.gas = GasType::CO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 4375.0, .stel = 30000.0,
         .regulation_ref = "DS 594, Art. 66 'Anhidrido Carbonico' (LPP/LPT)"},
        {.gas = GasType::NO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 2.6, .stel = 5.0,
         .regulation_ref = "DS 594, Art. 66 (LPP/LPT). Umbral operacional diesel: "
                           "DS 132, Art. 135 detiene equipos a NOx 20 ppm"},
        {.gas = GasType::SO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 1.7, .stel = 5.0,
         .regulation_ref = "DS 594, Art. 66 'Anhidrido Sulfuroso' (LPP/LPT)"},
        {.gas = GasType::H2S, .unit = ConcentrationUnit::PPM,
         .twa_8h = 8.8, .stel = 15.0,
         .regulation_ref = "DS 594, Art. 66 'Hidrogeno Sulfurado' (LPP/LPT)"},
        {.gas = GasType::CH4, .unit = ConcentrationUnit::PercentVolume,
         .ceiling = 0.75,
         .regulation_ref = "DS 132, Art. 274: 0.75% en galerias de retorno general "
                           "(valor conservador adoptado); 2% en frentes de arranque"},
        {.gas = GasType::O2,  .unit = ConcentrationUnit::PercentVolume,
         .floor_min = 19.5,
         .regulation_ref = "DS 132, Art. 144 (19.5%, redaccion 'en cuanto a peso' "
                           "documentada como ambigua; interpretacion conservadora). "
                           "DS 594, Art. 58: prohibido < 18% sin EPP"},
    };
    return table;
}

/**
 * @brief Tabla de VLP de Colombia — Decreto 1886 de 2015, Art. 39 (gases),
 *        Art. 38 (oxígeno) y Art. 53 (metano).
 *
 * La tabla del Art. 39 está en ppm y no fija valores techo. El artículo exige
 * además cumplir «como mínimo» los estándares de la ACGIH y actualizar los
 * VLP cada año (par. 1): esta tabla congela los valores del decreto. El SO2
 * solo tiene STEL; el CO, el NO2 y el NO solo tienen TWA. Los VLP no se
 * corrigen por altitud; para jornadas de más de 8 h el par. 2 aplica la
 * corrección de Brief y Scala — fuera del alcance de esta tabla.
 */
inline const std::vector<GasLimit>& gas_limits_colombia() {
    static const std::vector<GasLimit> table = {
        {.gas = GasType::CO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 (VLP-TWA; sin STEL)"},
        {.gas = GasType::CO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 5000.0, .stel = 30000.0,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 (VLP-TWA y STEL)"},
        {.gas = GasType::NO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 0.2,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 (VLP-TWA; sin STEL)"},
        {.gas = GasType::SO2, .unit = ConcentrationUnit::PPM,
         .stel = 0.25,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 'Anhidrido Sulfuroso' "
                           "(solo STEL; el decreto no fija TWA)"},
        {.gas = GasType::H2S, .unit = ConcentrationUnit::PPM,
         .twa_8h = 1.0, .stel = 5.0,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 'Acido Sulfhidrico' "
                           "(VLP-TWA y STEL)"},
        {.gas = GasType::CH4, .unit = ConcentrationUnit::PercentVolume,
         .ceiling = 1.0,
         .regulation_ref = "Decreto 1886 de 2015, Art. 53: 1.0% en frentes de "
                           "explotacion o avance y en retornos principales "
                           "(suspender trabajos y evacuar; valor mas estricto "
                           "adoptado); 1.5% en retorno de tajos y de frentes de "
                           "preparacion y desarrollo; sobre 2% solo personal de "
                           "salvamento"},
        {.gas = GasType::NO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "Decreto 1886 de 2015, Art. 39 'Oxido Nitrico' "
                           "(VLP-TWA; sin STEL)"},
        {.gas = GasType::O2,  .unit = ConcentrationUnit::PercentVolume,
         .ceiling = 23.5, .floor_min = 19.5,
         .regulation_ref = "Decreto 1886 de 2015, Art. 38 (min 19.5%, max 23.5% "
                           "en volumen)"},
    };
    return table;
}

/**
 * @brief Tabla de VLE de México — NOM-010-STPS-2014, Apéndice I, Tabla I.1.
 *
 * La NOM-023-STPS-2012 no trae tabla de gases: sus numerales 7.4 e) y 7.5 f)
 * remiten a la NOM-010-STPS-1999 «o las que las sustituyan», hoy la
 * NOM-010-STPS-2014. Los valores son VLE-PPT (8 h) y VLE-CT (corto tiempo),
 * para 25 °C y 101,3 kPa; la norma no los corrige por altitud. El SO2 solo
 * tiene VLE-CT. El metano es un límite higiénico: la NOM-023 no fija
 * porcentajes de acción por explosividad (eso es materia de la NOM-032, de
 * minas de carbón, que este preset no cubre).
 *
 * Oxígeno: la NOM-023 no fija el mínimo en labores. El 19,5 % es criterio de
 * ingeniería, y la cita lo declara.
 */
inline const std::vector<GasLimit>& gas_limits_mexico() {
    static const std::vector<GasLimit> table = {
        {.gas = GasType::CO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 552 (VLE-PPT), "
                           "via NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::CO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 5000.0, .stel = 30000.0,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 318 (VLE-PPT y "
                           "VLE-CT), via NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::NO2, .unit = ConcentrationUnit::PPM,
         .twa_8h = 0.2,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 320 (VLE-PPT), "
                           "via NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::SO2, .unit = ConcentrationUnit::PPM,
         .stel = 0.25,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 317 (solo VLE-CT; "
                           "la norma no fija VLE-PPT), via NOM-023-STPS-2012, "
                           "numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::H2S, .unit = ConcentrationUnit::PPM,
         .twa_8h = 1.0, .stel = 5.0,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 683 (VLE-PPT y "
                           "VLE-CT), via NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::CH4, .unit = ConcentrationUnit::PPM,
         .twa_8h = 1000.0,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 500 (VLE-PPT, limite "
                           "higienico = 0.1%); la NOM-023-STPS-2012 no fija "
                           "porcentajes de accion por explosividad"},
        {.gas = GasType::NO,  .unit = ConcentrationUnit::PPM,
         .twa_8h = 25.0,
         .regulation_ref = "NOM-010-STPS-2014, Tabla I.1, fila 599 (VLE-PPT), "
                           "via NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f)"},
        {.gas = GasType::O2,  .unit = ConcentrationUnit::PercentVolume,
         .floor_min = 19.5,
         .regulation_ref = "Criterio de ingenieria (19.5%): la NOM-023-STPS-2012 "
                           "no fija el minimo de oxigeno en labores (solo 18 a 21% "
                           "en refugios, numeral 8.14.4 d); la NOM-010-STPS-2014, "
                           "Tabla I.1 nota (D), da 18% para asfixiantes simples"},
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
        case RegulatoryStandard::DS132_Chile:
            return gas_limits_chile();
        case RegulatoryStandard::D1886_Colombia:
            return gas_limits_colombia();
        case RegulatoryStandard::NOM023_Mexico:
            return gas_limits_mexico();
    }
    throw std::invalid_argument(
        "[VentPy] gas_limits: no LMP table implemented for this standard");
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
