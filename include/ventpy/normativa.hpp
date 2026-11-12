/**
 * @file normativa.hpp
 * @brief Parámetros normativos para ventilación de minas subterráneas.
 *
 * Encapsula los valores regulatorios del DS 024-2016-EM y DS 023-2017-EM.
 * Los parámetros son configurables (para estándares corporativos más
 * exigentes) pero inmutables durante el ciclo de cálculo.
 *
 * Diseño: Patrón "Configuration Object" — se construye, se valida,
 * y luego se pasa como const& a los calculadores.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <stdexcept>
#include <string>

#include "ventpy/types.hpp"
#include "ventpy/validation.hpp"

namespace ventpy {

/**
 * @brief Configuración normativa inmutable para un cálculo de ventilación.
 *
 * Una vez construida y validada, esta clase NO expone setters.
 * Para cambiar parámetros, se debe construir una nueva instancia.
 * Esto garantiza inmutabilidad durante el cálculo (safety-critical).
 */
class RegulatoryConfig {
public:
    /**
     * @brief Constructor con valores por defecto según DS 024-2016-EM.
     *
     * Todos los valores por defecto corresponden al mínimo normativo peruano.
     * El ingeniero puede sobreescribirlos con estándares corporativos
     * más exigentes al momento de la construcción.
     *
     * Escala de caudal por persona — DS 024-2016-EM, Art. 247 (texto original,
     * no modificado por el DS 023-2017-EM):
     * "hasta 1,500 msnm: 3 m³/min; de 1,500 a 3,000: 4 (+40%);
     *  de 3,000 a 4,000: 5 (+70%); sobre los 4,000: 6 (+100%)".
     * Semántica de borde: '>' estricto — en el umbral exacto rige la banda
     * inferior (lectura "hasta X" inclusiva del texto normativo).
     * (Corrección normativa: versiones previas usaban 3/4/5 con
     * umbrales 3000/4000, una banda corrida respecto del Art. 247.)
     */
    explicit RegulatoryConfig(
        RegulatoryStandard standard           = RegulatoryStandard::DS024_Peru,
        double min_flow_per_person_m3min      = 3.0,     // Art. 247: hasta 1,500 msnm
        double altitude_threshold_1_masl      = 1500.0,
        double flow_per_person_above_t1       = 4.0,     // Art. 247: 1,500–3,000
        double altitude_threshold_2_masl      = 3000.0,
        double flow_per_person_above_t2       = 5.0,     // Art. 247: 3,000–4,000
        double altitude_threshold_3_masl      = 4000.0,
        double flow_per_person_above_t3       = 6.0,     // Art. 247: sobre 4,000
        double diesel_hp_factor_m3min         = 3.0,
        double max_dilution_time_min          = 30.0,
        double default_gas_volume_per_kg_m3   = 0.04,
        double default_leakage_factor         = 0.15
    )
        : standard_(standard)
        , min_flow_per_person_(min_flow_per_person_m3min)
        , altitude_threshold_1_(altitude_threshold_1_masl)
        , flow_above_t1_(flow_per_person_above_t1)
        , altitude_threshold_2_(altitude_threshold_2_masl)
        , flow_above_t2_(flow_per_person_above_t2)
        , altitude_threshold_3_(altitude_threshold_3_masl)
        , flow_above_t3_(flow_per_person_above_t3)
        , diesel_hp_factor_(diesel_hp_factor_m3min)
        , max_dilution_time_(max_dilution_time_min)
        , default_gas_volume_(default_gas_volume_per_kg_m3)
        , default_leakage_factor_(default_leakage_factor)
    {
        validate();
    }

    // ---- Presets normativos (factory) ----

    /// Preset oficial peruano — DS 024-2016-EM / DS 023-2017-EM.
    /// Equivale exactamente a los defaults del constructor.
    [[nodiscard]] static RegulatoryConfig peru() {
        return RegulatoryConfig{};
    }

    /**
     * @brief Preset oficial chileno — DS 132, Reglamento de Seguridad Minera.
     *
     * Valores validados contra el texto vigente (LeyChile idNorma=221064,
     * versión 09-abr-2024):
     * - 3,0 m³/min por persona: Art. 138. El DS 132 NO escala este caudal por
     *   altitud (a diferencia del DS 024 peruano), por lo que los escalones se
     *   neutralizan (mismo caudal en todos los tramos).
     * - 2,83 m³/min por HP efectivo al freno: Art. 132 (mínimo cuando el
     *   fabricante no especifica caudal; el caudal por personas siempre se suma).
     * - Tiempo de dilución 30 min: NO regulado por DS 132 (el reingreso
     *   post-tronadura lo gobiernan Arts. 156, 571 y 585) — default ingenieril.
     * - Volumen de gases 0,04 m³/kg: NO regulado por DS 132 — default ingenieril.
     * - Factor de fugas 0,15: NO regulado por DS 132 como factor de ducto (el
     *   15% del Art. 139 es tolerancia del balance general de mina) — default
     *   ingenieril, coincidente en número con dicha tolerancia.
     *
     * Umbrales operacionales diésel del Art. 135 (paralización de equipos):
     * CO 40 ppm, NOx 20 ppm, aldehído fórmico 1,6 ppm — ver limites_gases.hpp.
     */
    [[nodiscard]] static RegulatoryConfig chile() {
        return RegulatoryConfig{
            RegulatoryStandard::DS132_Chile,
            /* min_flow_per_person_m3min */    3.0,    // Art. 138
            /* altitude_threshold_1_masl */    1500.0, // sin efecto (ver Doxygen)
            /* flow_per_person_above_t1 */     3.0,    // Art. 138 (sin escalón)
            /* altitude_threshold_2_masl */    3000.0, // sin efecto
            /* flow_per_person_above_t2 */     3.0,    // Art. 138 (sin escalón)
            /* altitude_threshold_3_masl */    4000.0, // sin efecto
            /* flow_per_person_above_t3 */     3.0,    // Art. 138 (sin escalón)
            /* diesel_hp_factor_m3min */       2.83,   // Art. 132
            /* max_dilution_time_min */        30.0,   // no regulado por DS 132
            /* default_gas_volume_per_kg_m3 */ 0.04,   // no regulado por DS 132
            /* default_leakage_factor */       0.15    // no regulado por DS 132
        };
    }

    /**
     * @brief Preset oficial colombiano — Decreto 1886 de 2015, Reglamento de
     *        Seguridad en las Labores Mineras Subterráneas.
     *
     * Valores validados contra el texto consolidado con el Decreto 944 de
     * 2022, que no modificó los artículos con valores de ventilación:
     * - 3 m³/min por trabajador hasta 1.500 msnm y 6 m³/min de 1.500 msnm en
     *   adelante: Art. 54, nums. 1 y 2. La norma tiene un solo escalón, así
     *   que los tres tramos altos llevan el mismo caudal. Las dos bandas
     *   incluyen el 1.500 en su texto; aquí rige la semántica común de borde
     *   ('>' estricto): a 1.500 msnm exactos aplica la banda inferior.
     * - 6 m³/min por HP: Art. 54, par. 2, num. 1 (CO en el exosto ≤ 0,12 %).
     *   El num. 2 admite 4 m³/min por HP si el CO en el exosto es ≤ 0,08 %;
     *   quien lo acredite construye su propia configuración con ese factor.
     *   Con CO > 0,12 % el decreto no da valor.
     * - Tiempo de dilución 30 min: NO regulado (el Art. 157 fija el reingreso,
     *   no antes de 15 min y con gases dentro de los VLP) — default ingenieril.
     * - Volumen de gases 0,04 m³/kg y factor de fugas 0,15: NO regulados —
     *   defaults ingenieriles.
     */
    [[nodiscard]] static RegulatoryConfig colombia() {
        return RegulatoryConfig{
            RegulatoryStandard::D1886_Colombia,
            /* min_flow_per_person_m3min */    3.0,    // Art. 54, num. 1
            /* altitude_threshold_1_masl */    1500.0, // Art. 54, num. 2
            /* flow_per_person_above_t1 */     6.0,    // Art. 54, num. 2
            /* altitude_threshold_2_masl */    3000.0, // sin efecto (un solo escalón)
            /* flow_per_person_above_t2 */     6.0,
            /* altitude_threshold_3_masl */    4000.0, // sin efecto
            /* flow_per_person_above_t3 */     6.0,
            /* diesel_hp_factor_m3min */       6.0,    // Art. 54, par. 2, num. 1
            /* max_dilution_time_min */        30.0,   // no regulado por el decreto
            /* default_gas_volume_per_kg_m3 */ 0.04,   // no regulado por el decreto
            /* default_leakage_factor */       0.15    // no regulado por el decreto
        };
    }

    /**
     * @brief Preset oficial mexicano — NOM-023-STPS-2012 (minas subterráneas
     *        que no son de carbón).
     *
     * Valores validados contra el texto del DOF (11-oct-2012), vigente sin
     * modificaciones:
     * - 1,50 m³/min por trabajador: numeral 8.4.4 a) 1). La norma NO escala
     *   por altitud, por lo que los escalones se neutralizan.
     * - 2,13 m³/min por caballo de fuerza diésel: numeral 8.4.4 a) 2). La
     *   norma no precisa si la potencia es nominal o al freno.
     * - Tiempo de dilución 30 min: NO regulado (el numeral 8.6.9 g) fija el
     *   reconocimiento del área después de 30 min y sin humo) — default
     *   ingenieril, coincidente en número con ese plazo.
     * - Volumen de gases 0,04 m³/kg y factor de fugas 0,15: NO regulados —
     *   defaults ingenieriles.
     *
     * La NOM-023 exceptúa las minas de carbón (numeral 2.2 b), que se rigen
     * por la NOM-032-STPS-2008: este preset no las cubre.
     */
    [[nodiscard]] static RegulatoryConfig mexico() {
        return RegulatoryConfig{
            RegulatoryStandard::NOM023_Mexico,
            /* min_flow_per_person_m3min */    1.5,    // 8.4.4 a) 1)
            /* altitude_threshold_1_masl */    1500.0, // sin efecto (ver Doxygen)
            /* flow_per_person_above_t1 */     1.5,
            /* altitude_threshold_2_masl */    3000.0, // sin efecto
            /* flow_per_person_above_t2 */     1.5,
            /* altitude_threshold_3_masl */    4000.0, // sin efecto
            /* flow_per_person_above_t3 */     1.5,
            /* diesel_hp_factor_m3min */       2.13,   // 8.4.4 a) 2)
            /* max_dilution_time_min */        30.0,   // no regulado por la norma
            /* default_gas_volume_per_kg_m3 */ 0.04,   // no regulado por la norma
            /* default_leakage_factor */       0.15    // no regulado por la norma
        };
    }

    /// Construye el preset oficial de la norma indicada.
    /// @throws std::invalid_argument si la norma no tiene preset implementado.
    [[nodiscard]] static RegulatoryConfig for_standard(RegulatoryStandard standard) {
        switch (standard) {
            case RegulatoryStandard::DS024_Peru:     return peru();
            case RegulatoryStandard::DS132_Chile:    return chile();
            case RegulatoryStandard::D1886_Colombia: return colombia();
            case RegulatoryStandard::NOM023_Mexico:  return mexico();
        }
        throw std::invalid_argument(
            "[VentPy] for_standard: unsupported regulatory standard");
    }

    // ---- Getters (const, inmutables) ----

    [[nodiscard]] RegulatoryStandard standard() const noexcept { return standard_; }

    /// DS 024-2016-EM, Art. 236: Mínimo 3 m³/min por persona.
    [[nodiscard]] double min_flow_per_person() const noexcept { return min_flow_per_person_; }

    /// Umbral altitud nivel 1 [msnm]
    [[nodiscard]] double altitude_threshold_1() const noexcept { return altitude_threshold_1_; }

    /// Caudal por persona cuando altitud > umbral 1 [m³/min]
    [[nodiscard]] double flow_above_threshold_1() const noexcept { return flow_above_t1_; }

    /// Umbral altitud nivel 2 [msnm]
    [[nodiscard]] double altitude_threshold_2() const noexcept { return altitude_threshold_2_; }

    /// Caudal por persona cuando altitud > umbral 2 [m³/min]
    [[nodiscard]] double flow_above_threshold_2() const noexcept { return flow_above_t2_; }

    /// Umbral altitud nivel 3 [msnm]
    [[nodiscard]] double altitude_threshold_3() const noexcept { return altitude_threshold_3_; }

    /// Caudal por persona cuando altitud > umbral 3 [m³/min] (Art. 247: 6)
    [[nodiscard]] double flow_above_threshold_3() const noexcept { return flow_above_t3_; }

    /// DS 024-2016-EM, Art. 246: Factor HP × m³/min para equipos diésel.
    [[nodiscard]] double diesel_hp_factor() const noexcept { return diesel_hp_factor_; }

    /// DS 024-2016-EM, Art. 243: Tiempo máximo de dilución [min].
    [[nodiscard]] double max_dilution_time() const noexcept { return max_dilution_time_; }

    /// Volumen de gases por defecto por kg de explosivo [m³/kg]
    [[nodiscard]] double default_gas_volume_per_kg() const noexcept { return default_gas_volume_; }

    /// Factor de fugas por defecto (fracción, ej. 0.15 = 15%)
    [[nodiscard]] double default_leakage_factor() const noexcept { return default_leakage_factor_; }

    /// Retorna la referencia normativa como string legible.
    [[nodiscard]] std::string standard_name() const {
        switch (standard_) {
            case RegulatoryStandard::DS024_Peru:
                return "DS 024-2016-EM / DS 023-2017-EM (Peru)";
            case RegulatoryStandard::DS132_Chile:
                return "DS 132 Reglamento de Seguridad Minera (Chile)";
            case RegulatoryStandard::D1886_Colombia:
                return "Decreto 1886 de 2015 (Colombia)";
            case RegulatoryStandard::NOM023_Mexico:
                return "NOM-023-STPS-2012 (Mexico)";
            default:
                return "Unknown Standard";
        }
    }

private:
    RegulatoryStandard standard_;
    double min_flow_per_person_;
    double altitude_threshold_1_;
    double flow_above_t1_;
    double altitude_threshold_2_;
    double flow_above_t2_;
    double altitude_threshold_3_;
    double flow_above_t3_;
    double diesel_hp_factor_;
    double max_dilution_time_;
    double default_gas_volume_;
    double default_leakage_factor_;

    /**
     * @brief Validación de dominio en construcción.
     * @throws std::invalid_argument si algún parámetro es inválido
     */
    void validate() const {
        namespace v = ventpy::validation;
        v::require_positive(min_flow_per_person_,   "min_flow_per_person [m3/min]");
        v::require_non_negative(altitude_threshold_1_, "altitude_threshold_1 [msnm]");
        v::require_non_negative(altitude_threshold_2_, "altitude_threshold_2 [msnm]");
        v::require_non_negative(altitude_threshold_3_, "altitude_threshold_3 [msnm]");
        v::require_positive(flow_above_t1_,         "flow_above_threshold_1 [m3/min]");
        v::require_positive(flow_above_t2_,         "flow_above_threshold_2 [m3/min]");
        v::require_positive(flow_above_t3_,         "flow_above_threshold_3 [m3/min]");
        if (!(altitude_threshold_1_ < altitude_threshold_2_ &&
              altitude_threshold_2_ < altitude_threshold_3_)) {
            throw std::invalid_argument(
                "Error de dominio [VentPy]: los umbrales de altitud deben ser "
                "estrictamente crecientes (altitude_threshold_1 < 2 < 3).");
        }
        v::require_positive(diesel_hp_factor_,      "diesel_hp_factor [m3/min/HP]");
        v::require_positive(max_dilution_time_,     "max_dilution_time [min]");
        v::require_positive(default_gas_volume_,    "default_gas_volume_per_kg [m3/kg]");
        v::require_in_range(default_leakage_factor_, 0.0, 1.0,
                            "default_leakage_factor");
    }
};

/**
 * @brief Concepto de ventilación cuyo respaldo legal depende de la norma activa.
 *
 * Cada calculador cita su fuente a través de `regulation_reference`, de modo que
 * la referencia sigue a `RegulatoryConfig::standard()` y no a un literal fijo.
 * Es el único lugar del núcleo donde vive el texto de una cita normativa.
 */
enum class RegulatoryTopic {
    PersonnelFlow,              ///< Caudal mínimo por persona
    PersonnelAltitudeScale,     ///< Escalón de caudal por altitud (vacío si la norma no escala)
    DieselHpFactor,             ///< Caudal por HP de equipo diésel
    BlastingDilution,           ///< Dilución de gases de voladura
    BlastingDilutionTimeLimit,  ///< Etiqueta del tope de tiempo de dilución
    DustRespirableLimit,        ///< Límite de polvo respirable
    DustLimitBasis,             ///< Base del umbral de 3 mg/m³ en advertencias
    DustSilicaReferral,         ///< Remisión del límite específico de sílice
    ThermalBalance,             ///< Balance térmico sensible, sin piso de velocidad
    ThermalWithVelocityFloor,   ///< Balance combinado con el piso de velocidad térmico
    ThermalInfeasibleWithFloor, ///< Balance infactible, con piso de velocidad exigible
    ThermalInfeasibleNoFloor,   ///< Balance infactible, sin piso de velocidad aplicable
    ThermalStressReferral,      ///< Remisión a evaluación de estrés térmico (WBGT)
    AirVelocityLimits,          ///< Límites de velocidad del aire en labores
    CoverageZone,               ///< Cobertura de la demanda de aire por labor
    CoverageMine                ///< Balance de cobertura de la mina completa
};

/**
 * @brief Referencia normativa de un concepto bajo la norma indicada.
 *
 * Perú (DS 024-2016-EM / DS 023-2017-EM) y Chile (DS 132, Reglamento de
 * Seguridad Minera, texto vigente LeyChile idNorma=221064, versión
 * 09-abr-2024) regulan conjuntos distintos de conceptos. Donde el DS 132 NO
 * fija un valor —tiempo de dilución tras tronadura, volumen de gases por kg de
 * explosivo, factor de fugas de ducto— o donde esta librería no tiene una
 * referencia chilena verificada —polvo respirable, carga térmica, límites de
 * velocidad del aire, régimen de cobertura— la cita declara ese vacío y
 * atribuye el valor a criterio de ingeniería. Nunca se traslada un artículo
 * peruano a un cálculo chileno ni se cita un artículo sin verificar.
 *
 * La misma regla rige para Colombia (Decreto 1886 de 2015, consolidado con el
 * Decreto 944 de 2022) y México (NOM-023-STPS-2012, que remite a la
 * NOM-010-STPS-2014 y a la NOM-015-STPS-2001): cada cita nombra el artículo o
 * numeral leído en el texto vigente, o declara que la norma no fija el valor.
 *
 * @throws std::invalid_argument si la norma o el concepto no están soportados.
 */
[[nodiscard]] inline std::string regulation_reference(
    RegulatoryTopic topic,
    RegulatoryStandard standard
) {
    // Una cita por norma, siempre en este orden: Perú, Chile, Colombia, México.
    const auto cita = [standard](const char* peru, const char* chile,
                                 const char* colombia,
                                 const char* mexico) -> std::string {
        switch (standard) {
            case RegulatoryStandard::DS024_Peru:     return peru;
            case RegulatoryStandard::DS132_Chile:    return chile;
            case RegulatoryStandard::D1886_Colombia: return colombia;
            case RegulatoryStandard::NOM023_Mexico:  return mexico;
        }
        throw std::invalid_argument(
            "[VentPy] regulation_reference: norma sin referencias definidas");
    };

    switch (topic) {
        case RegulatoryTopic::PersonnelFlow:
            return cita(
                "DS 024-2016-EM, Art. 236",
                "DS 132, Art. 138",
                "Decreto 1886 de 2015, Art. 54 nums. 1 y 2 (3 m3/min por trabajador hasta "
                  "1.500 msnm; 6 m3/min de 1.500 msnm en adelante)",
                "NOM-023-STPS-2012, numeral 8.4.4 a) 1)");

        // Solo Peru tiene una clausula de escala aparte (Art. 247). Chile y
        // Mexico no escalan por altitud; el unico escalon colombiano ya va
        // en la cita del caudal por persona (Art. 54 nums. 1 y 2).
        case RegulatoryTopic::PersonnelAltitudeScale:
            return cita("Art. 247 escala altitud", "", "", "");

        case RegulatoryTopic::DieselHpFactor:
            return cita(
                "DS 024-2016-EM, Art. 246",
                "DS 132, Art. 132",
                "Decreto 1886 de 2015, Art. 54 par. 2 num. 1 (CO en el exosto <= 0.12%; "
                  "el num. 2 admite 4 m3/min por HP si CO <= 0.08%)",
                "NOM-023-STPS-2012, numeral 8.4.4 a) 2)");

        case RegulatoryTopic::BlastingDilution:
            return cita(
                "DS 024-2016-EM, Art. 243-244",
                "DS 132, Arts. 156, 571 y 585 (reingreso tras tronadura); "
                  "el DS 132 no fija tiempo de dilucion ni volumen de gases "
                  "por kg de explosivo - criterio de ingenieria",
                "Decreto 1886 de 2015, Art. 157 (reingreso no antes de 15 min y con "
                  "gases dentro de los VLP); el decreto no fija tiempo de dilucion "
                  "ni volumen de gases por kg de explosivo - criterio de ingenieria",
                "NOM-023-STPS-2012, numeral 8.6.9 g) (reconocimiento despues de 30 "
                  "min de la disparada y sin humo); la norma no fija tiempo de "
                  "dilucion ni volumen de gases por kg de explosivo - criterio de "
                  "ingenieria");

        case RegulatoryTopic::BlastingDilutionTimeLimit:
            return cita(
                "max normativo, Art. 243",
                "max configurado, criterio de ingenieria: el DS 132 no fija "
                  "tiempo de dilucion",
                "max configurado, criterio de ingenieria: el Decreto 1886 de "
                  "2015 no fija tiempo de dilucion",
                "max configurado, criterio de ingenieria: la "
                  "NOM-023-STPS-2012 no fija tiempo de dilucion");

        case RegulatoryTopic::DustRespirableLimit:
            return cita(
                "DS 024-2016-EM, Art. 111 (LEO polvo respirable 3 mg/m3, "
                  "jornada 8 h; paralizacion si se supera)",
                "DS 132: sin limite de polvo respirable verificado para "
                  "Chile en esta libreria; la concentracion objetivo es "
                  "criterio de ingenieria del usuario",
                "Decreto 1886 de 2015, Art. 74: remite a los VLP de la ACGIH y no fija "
                  "un limite propio de polvo respirable; la concentracion objetivo "
                  "es criterio de ingenieria del usuario",
                "NOM-023-STPS-2012, numerales 7.4 e) y 7.5 f): remite a la "
                  "NOM-010-STPS-2014 (Tabla I.1, fila 609: particulas no "
                  "especificadas, 3 mg/m3 fraccion respirable); la concentracion "
                  "objetivo es criterio de ingenieria del usuario");

        case RegulatoryTopic::DustLimitBasis:
            return cita(
                "LEO del Art. 111 (DS 024-2016-EM)",
                "referencia ingenieril (el DS 132 no fija un limite de polvo "
                  "respirable verificado en esta libreria)",
                "referencia ingenieril (el Decreto 1886 de 2015, Art. 74, "
                  "remite a los VLP de la ACGIH y no fija un limite propio)",
                "VLE-PPT de particulas no especificadas, fraccion respirable "
                  "(NOM-010-STPS-2014, Tabla I.1, fila 609)");

        case RegulatoryTopic::DustSilicaReferral:
            return cita(
                "el Anexo 15 / DS 015-2005-SA",
                "la normativa de higiene ocupacional chilena aplicable (no "
                  "identificada en esta libreria)",
                "los VLP de la ACGIH (Decreto 1886 de 2015, Art. 74)",
                "la NOM-010-STPS-2014, Tabla I.1, fila 668 (silice cristalina "
                  "respirable: 0.025 mg/m3)");

        case RegulatoryTopic::ThermalBalance:
            return cita(
                "DS 024-2016-EM: balance termico sensible (criterio "
                  "ingenieril; carga = equipos + oxidacion, autocompresion "
                  "reduce el DT disponible)",
                "DS 132: balance termico sensible (criterio ingenieril; "
                  "carga = equipos + oxidacion, autocompresion reduce el DT "
                  "disponible); el DS 132 no fija un criterio de ventilacion "
                  "termica identificado en esta libreria",
                "Decreto 1886 de 2015: balance termico sensible (criterio "
                  "ingenieril; carga = equipos + oxidacion, autocompresion reduce "
                  "el DT disponible); el decreto regula el calor por temperatura "
                  "efectiva y tiempos de permanencia (Arts. 217 y 218), no por "
                  "caudal",
                "NOM-023-STPS-2012: balance termico sensible (criterio "
                  "ingenieril; carga = equipos + oxidacion, autocompresion reduce "
                  "el DT disponible); la norma remite a la NOM-015-STPS-2001 "
                  "(indice TGBH) y no fija un criterio de ventilacion termica");

        case RegulatoryTopic::ThermalWithVelocityFloor:
            return cita(
                "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min con "
                  "temperatura seca 24-29 C) combinado con balance termico "
                  "sensible (criterio ingenieril)",
                "DS 132: piso de velocidad de 30 m/min con temperatura seca "
                  "24-29 C aplicado como criterio de ingenieria (el DS 132 no "
                  "fija un criterio de ventilacion termica identificado en "
                  "esta libreria), combinado con balance termico sensible",
                "Decreto 1886 de 2015: piso de velocidad de 30 m/min con "
                  "temperatura seca 24-29 C aplicado como criterio de ingenieria "
                  "(el decreto regula el calor por temperatura efectiva, Arts. 217 "
                  "y 218, y no fija un piso de velocidad termico), combinado con "
                  "balance termico sensible",
                "NOM-023-STPS-2012: piso de velocidad de 30 m/min con "
                  "temperatura seca 24-29 C aplicado como criterio de ingenieria "
                  "(la norma remite a la NOM-015-STPS-2001, indice TGBH, y no fija "
                  "un piso de velocidad termico), combinado con balance termico "
                  "sensible");

        case RegulatoryTopic::ThermalInfeasibleWithFloor:
            return cita(
                "DS 024-2016-EM, Art. 252.d (velocidad minima 30 m/min "
                  "con temperatura seca 24-29 C) — balance termico "
                  "sensible INFACTIBLE por autocompresion, se requiere "
                  "refrigeracion mecanica adicional para alcanzar el "
                  "objetivo termico",
                "DS 132: piso de velocidad de 30 m/min con temperatura seca "
                  "24-29 C aplicado como criterio de ingenieria (el DS 132 no "
                  "fija un criterio de ventilacion termica identificado en "
                  "esta libreria) - balance termico sensible INFACTIBLE por "
                  "autocompresion, se requiere refrigeracion mecanica "
                  "adicional para alcanzar el objetivo termico",
                "Decreto 1886 de 2015: piso de velocidad de 30 m/min con "
                  "temperatura seca 24-29 C aplicado como criterio de ingenieria "
                  "(el decreto regula el calor por temperatura efectiva, Arts. 217 "
                  "y 218, y no fija un piso de velocidad termico) - balance termico "
                  "sensible INFACTIBLE por autocompresion, se requiere "
                  "refrigeracion mecanica adicional para alcanzar el objetivo "
                  "termico",
                "NOM-023-STPS-2012: piso de velocidad de 30 m/min con "
                  "temperatura seca 24-29 C aplicado como criterio de ingenieria "
                  "(la norma remite a la NOM-015-STPS-2001, indice TGBH, y no fija "
                  "un piso de velocidad termico) - balance termico sensible "
                  "INFACTIBLE por autocompresion, se requiere refrigeracion "
                  "mecanica adicional para alcanzar el objetivo termico");

        case RegulatoryTopic::ThermalInfeasibleNoFloor:
            return cita(
                "DS 024-2016-EM: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)",
                "DS 132: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)",
                "Decreto 1886 de 2015: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)",
                "NOM-023-STPS-2012: objetivo de diseño no alcanzable por "
                  "ventilacion (autocompresion agota el ΔT disponible)");

        case RegulatoryTopic::ThermalStressReferral:
            return cita(
                "Art. 104 + Anexo 13",
                "referencia chilena no identificada en esta libreria",
                "Decreto 1886 de 2015, Arts. 217 y 218: temperatura efectiva y "
                  "tiempos de permanencia",
                "NOM-015-STPS-2001, indice de temperatura de globo bulbo humedo");

        case RegulatoryTopic::AirVelocityLimits:
            return cita(
                "DS 024-2016-EM, Art. 248",
                "limites configurados en CoverageParams; el DS 132 no fija "
                  "limites de velocidad del aire identificados en esta "
                  "libreria",
                "limites configurados en CoverageParams; el Decreto 1886 de "
                  "2015, Art. 56, fija maxima de 6 m/s (360 m/min) en vias con "
                  "transito de personal y minimas solo para minas de carbon",
                "limites configurados en CoverageParams; la "
                  "NOM-023-STPS-2012, numeral 8.4.4 b), solo fija minima de 15.24 "
                  "m/min donde opere maquinaria diesel y no fija maxima");

        case RegulatoryTopic::CoverageZone:
            return cita(
                "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252 lit. g - "
                  "cobertura de la demanda de aire por labor",
                "DS 132: cobertura de la demanda de aire por labor - "
                  "criterio de ingenieria; el DS 132 no fija un regimen de "
                  "evaluacion de cobertura identificado en esta libreria",
                "Decreto 1886 de 2015: cobertura de la demanda de aire por labor - "
                  "criterio de ingenieria; el decreto exige aforo semanal de "
                  "caudales (Art. 57) y no fija un regimen de evaluacion de "
                  "cobertura",
                "NOM-023-STPS-2012: cobertura de la demanda de aire por labor - "
                  "criterio de ingenieria; la norma exige un plan de ventilacion "
                  "con los volumenes de aire requeridos (numerales 8.4.1 y 8.4.2) "
                  "y no fija un regimen de evaluacion de cobertura");

        case RegulatoryTopic::CoverageMine:
            return cita(
                "DS 024-2016-EM (mod. DS 023-2017-EM), Art. 252: evaluacion "
                  "integral semestral; lit. f (cobertura de mina) y lit. g "
                  "(cobertura por labor)",
                "DS 132: balance de cobertura de la mina - criterio de "
                  "ingenieria; el DS 132 no fija una evaluacion integral de "
                  "cobertura identificada en esta libreria",
                "Decreto 1886 de 2015: balance de cobertura de la mina - criterio "
                  "de ingenieria; el decreto exige aforo semanal de caudales (Art. "
                  "57) y no fija una evaluacion integral de cobertura",
                "NOM-023-STPS-2012: balance de cobertura de la mina - criterio de "
                  "ingenieria; la norma exige un plan de ventilacion con los "
                  "volumenes de aire requeridos (numerales 8.4.1 y 8.4.2) y no "
                  "fija una evaluacion integral de cobertura");
    }

    throw std::invalid_argument(
        "[VentPy] regulation_reference: concepto no soportado");
}

/// Sobrecarga por configuración: la cita sigue a `config.standard()`.
[[nodiscard]] inline std::string regulation_reference(
    RegulatoryTopic topic,
    const RegulatoryConfig& config
) {
    return regulation_reference(topic, config.standard());
}

} // namespace ventpy
