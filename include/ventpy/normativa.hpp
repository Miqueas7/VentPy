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

#include <algorithm>
#include <optional>
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
     * Los caudales por persona, el factor diésel y el factor de fugas son los
     * mínimos normativos peruanos. El tiempo de dilución (30 min) y el volumen
     * de gases por kg (0,04 m³/kg) no están en la norma: son criterio de
     * ingeniería (el reingreso tras la voladura lo rige el Art. 240). El
     * ingeniero puede sobreescribirlos con estándares corporativos más
     * exigentes al momento de la construcción.
     *
     * Escala de caudal por persona — DS 024-2016-EM, Art. 247 (texto original,
     * no modificado por el DS 023-2017-EM):
     * "hasta 1,500 msnm: 3 m³/min; de 1,500 a 3,000: 4 (+40%);
     *  de 3,000 a 4,000: 5 (+70%); sobre los 4,000: 6 (+100%)".
     * Semántica de borde: el texto pone 1,500 y 3,000 en dos bandas a la vez;
     * en el umbral exacto rige la banda ALTA (caudal mayor), que es la lectura
     * del lado seguro. Lo mismo vale para el escalón colombiano de 1.500 msnm.
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
     *   incluyen el 1.500 en su texto; aquí rige la semántica común de borde:
     *   a 1.500 msnm exactos aplica la banda alta (6 m³/min).
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

    /**
     * @brief Preset chileno para faenas de hasta 5.000 t/mes — DS 132,
     *        Título XV (ámbito del Art. 594).
     *
     * Igual al preset `chile()` salvo el caudal por HP: el Art. 627 exige
     * no menos de 3 m³/min por persona y por cada HP de equipo de combustión
     * interna. Las citas de caudal por persona y por HP nombran el Art. 627.
     */
    [[nodiscard]] static RegulatoryConfig chile_small_mining() {
        RegulatoryConfig c = chile();
        c.diesel_hp_factor_ = 3.0;   // Art. 627
        c.small_mining_ = true;
        return c;
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

    /// Caudal mínimo por persona en la banda baja [m³/min]
    /// (Perú: DS 024-2016-EM, Art. 247, 3 m³/min hasta 1,500 msnm).
    [[nodiscard]] double min_flow_per_person() const noexcept { return min_flow_per_person_; }

    /// Umbral altitud nivel 1 [msnm]
    [[nodiscard]] double altitude_threshold_1() const noexcept { return altitude_threshold_1_; }

    /// Caudal por persona cuando altitud ≥ umbral 1 [m³/min]
    [[nodiscard]] double flow_above_threshold_1() const noexcept { return flow_above_t1_; }

    /// Umbral altitud nivel 2 [msnm]
    [[nodiscard]] double altitude_threshold_2() const noexcept { return altitude_threshold_2_; }

    /// Caudal por persona cuando altitud ≥ umbral 2 [m³/min]
    [[nodiscard]] double flow_above_threshold_2() const noexcept { return flow_above_t2_; }

    /// Umbral altitud nivel 3 [msnm]
    [[nodiscard]] double altitude_threshold_3() const noexcept { return altitude_threshold_3_; }

    /// Caudal por persona cuando altitud ≥ umbral 3 [m³/min] (Art. 247: 6)
    [[nodiscard]] double flow_above_threshold_3() const noexcept { return flow_above_t3_; }

    /// Factor m³/min por HP para equipos diésel (Perú: DS 024-2016-EM,
    /// Art. 252 lit. e y Anexo 38 num. 1 lit. d: Q_Eq = 3 × HP × Dm × Fu).
    [[nodiscard]] double diesel_hp_factor() const noexcept { return diesel_hp_factor_; }

    /// Régimen chileno de pequeña minería (DS 132, Título XV); solo lo activa
    /// `chile_small_mining()`.
    [[nodiscard]] bool small_mining() const noexcept { return small_mining_; }

    /// Tope configurado del tiempo de dilución tras la voladura [min]. Ninguna
    /// de las normas implementadas fija este tiempo: es criterio de ingeniería.
    [[nodiscard]] double max_dilution_time() const noexcept { return max_dilution_time_; }

    /// Volumen de gases por defecto por kg de explosivo [m³/kg] (criterio de
    /// ingeniería: ninguna norma implementada lo fija).
    [[nodiscard]] double default_gas_volume_per_kg() const noexcept { return default_gas_volume_; }

    /// Factor de fugas por defecto (fracción, ej. 0.15 = 15%). En Perú es el
    /// Q_Fu = 15 % de Q_T1 del Anexo 38 (total de mina); en ductos de frente
    /// y en las demás normas es criterio de ingeniería.
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
    bool small_mining_ = false;

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
 * 09-abr-2024; gases, polvo y calor por remisión al DS 594) regulan conjuntos
 * distintos de conceptos. Donde la norma NO fija un valor —tiempo de dilución
 * tras la voladura, volumen de gases por kg de explosivo, factor de fugas de
 * ducto, piso térmico de velocidad— la cita declara ese vacío y atribuye el
 * valor a criterio de ingeniería. Nunca se traslada el artículo de una norma
 * a un cálculo de otra ni se cita un artículo sin verificar.
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
                "DS 024-2016-EM, Art. 247 (3 m3/min por persona hasta 1,500 "
                  "msnm; en el total de mina, Anexo 38 lit. a: Q_Tr = F x N)",
                "DS 132, Art. 138",
                "Decreto 1886 de 2015, Art. 54 nums. 1 y 2 (3 m3/min por trabajador hasta "
                  "1.500 msnm; 6 m3/min de 1.500 msnm en adelante)",
                "NOM-023-STPS-2012, numeral 8.4.4 a) 1)");

        // Solo Peru tiene una clausula de escala aparte (Art. 247). Chile y
        // Mexico no escalan por altitud; el unico escalon colombiano ya va
        // en la cita del caudal por persona (Art. 54 nums. 1 y 2).
        case RegulatoryTopic::PersonnelAltitudeScale:
            return cita("DS 024-2016-EM, Art. 247 escala altitud", "", "", "");

        case RegulatoryTopic::DieselHpFactor:
            return cita(
                "DS 024-2016-EM, Art. 252 lit. e; Anexo 38 num. 1 lit. d "
                  "(Q_Eq = 3 x HP x Dm x Fu)",
                "DS 132, Art. 132 (regimen general; en faenas de hasta 5.000 "
                  "t/mes, Titulo XV, el Art. 627 exige 3 m3/min por HP: usar "
                  "chile_small_mining())",
                "Decreto 1886 de 2015, Art. 54 par. 2 num. 1 (CO en el exosto <= 0.12%; "
                  "el num. 2 admite 4 m3/min por HP si CO <= 0.08%)",
                "NOM-023-STPS-2012, numeral 8.4.4 a) 2)");

        case RegulatoryTopic::BlastingDilution:
            return cita(
                "DS 024-2016-EM, Art. 240 (reingreso solo con gases y polvo bajo "
                  "los limites del Art. 110 / Anexo 15); el DS 024 fija el "
                  "caudal de voladura solo en el total de mina (Anexo 38) - la "
                  "dilucion por contaminante en el frente es criterio de ingenieria",
                "DS 132, Arts. 156, 571 y 585 (reingreso tras tronadura; los "
                  "30 min del Art. 585 rigen solo con guia corriente); "
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
                "max configurado, criterio de ingenieria: el DS 024-2016-EM no "
                  "fija tiempo de dilucion (el reingreso lo rige el Art. 240)",
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
                "DS 594, Art. 66 (via DS 132, Arts. 135 y 144): polvos no "
                  "especificados, fraccion respirable, LPP 2.4 mg/m3; sobre "
                  "1.000 msnm se multiplica por Fa = P/760 (DS 594, Art. 63)",
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
                "LPP de polvos no especificados, fraccion respirable (DS 594, "
                  "Art. 66: 2.4 mg/m3), corregido por Fa = P/760 sobre 1.000 "
                  "msnm (DS 594, Art. 63)",
                "referencia ingenieril (el Decreto 1886 de 2015, Art. 74, "
                  "remite a los VLP de la ACGIH y no fija un limite propio)",
                "VLE-PPT de particulas no especificadas, fraccion respirable "
                  "(NOM-010-STPS-2014, Tabla I.1, fila 609)");

        case RegulatoryTopic::DustSilicaReferral:
            return cita(
                "el DS 024-2016-EM, Anexo 15 (silice cristalina respirable: "
                  "cuarzo, cristobalita y tridimita 0.05 mg/m3)",
                "el DS 594, Art. 66 (silice cristalizada, fraccion respirable: "
                  "cuarzo 0.08 mg/m3; cristobalita y tridimita 0.04 mg/m3; "
                  "correccion Fa del Art. 63)",
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
                  "disponible); el Art. 143 remite las temperaturas al DS 594 "
                  "(Arts. 96 a 98, indice TGBH), que no fija un criterio de "
                  "ventilacion termica",
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
                  "24-29 C aplicado como criterio de ingenieria (el Art. 143 "
                  "remite al DS 594, Arts. 96 a 98, indice TGBH, sin piso de "
                  "velocidad), combinado con balance termico sensible",
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
                  "24-29 C aplicado como criterio de ingenieria (el Art. 143 "
                  "remite al DS 594, Arts. 96 a 98, indice TGBH, sin piso de "
                  "velocidad) - balance termico sensible INFACTIBLE por "
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
                "DS 024-2016-EM: criterio de ingenieria - objetivo de diseño "
                  "no alcanzable por ventilacion (autocompresion agota el ΔT "
                  "disponible)",
                "DS 132: criterio de ingenieria - objetivo de diseño no "
                  "alcanzable por ventilacion (autocompresion agota el ΔT "
                  "disponible)",
                "Decreto 1886 de 2015: criterio de ingenieria - objetivo de "
                  "diseño no alcanzable por ventilacion (autocompresion agota "
                  "el ΔT disponible)",
                "NOM-023-STPS-2012: criterio de ingenieria - objetivo de diseño "
                  "no alcanzable por ventilacion (autocompresion agota el ΔT "
                  "disponible)");

        case RegulatoryTopic::ThermalStressReferral:
            return cita(
                "DS 024-2016-EM, Art. 104 + Anexo 13",
                "DS 132, Art. 143 -> DS 594, Arts. 96 a 98 (indice TGBH)",
                "Decreto 1886 de 2015, Arts. 217 y 218: temperatura efectiva y "
                  "tiempos de permanencia",
                "NOM-015-STPS-2001, indice de temperatura de globo bulbo humedo");

        case RegulatoryTopic::AirVelocityLimits:
            return cita(
                "DS 024-2016-EM, Art. 248",
                "DS 132, Art. 138 inc. 3 (velocidad promedio entre 15 y 150 "
                  "m/min)",
                "Decreto 1886 de 2015, Art. 56 par. 1 (maxima 6 m/s = 360 m/min "
                  "en vias con transito de personal; par. 2: 4.5 m/s = 270 "
                  "m/min en tajos de explotacion de carbon); el decreto fija "
                  "minimas solo para minas de carbon",
                "NOM-023-STPS-2012, numeral 8.4.4 b) (minima 15.24 m/min donde "
                  "opere maquinaria diesel); la norma no fija maxima");

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
                "DS 132, Art. 139 (aforo trimestral de entradas y salidas "
                  "principales y control general semestral de la mina, sin "
                  "perdidas superiores al 15%)",
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
    if (config.small_mining() &&
        (topic == RegulatoryTopic::PersonnelFlow ||
         topic == RegulatoryTopic::DieselHpFactor)) {
        return "DS 132, Art. 627 (Titulo XV, faenas de hasta 5.000 t/mes: "
               "3 m3/min por persona y por cada HP)";
    }
    return regulation_reference(topic, config.standard());
}

/**
 * @brief Límite de sílice cristalina respirable [mg/m³], o vacío si la norma
 *        no fija un número propio.
 *
 * Perú: 0,05 mg/m³ para cuarzo, cristobalita y tridimita (DS 024-2016-EM,
 * Anexo 15). Chile: cuarzo 0,08 y cristobalita o tridimita 0,04 mg/m³
 * (DS 594, Art. 66), que el llamador corrige por Fa. México: 0,025 mg/m³
 * (NOM-010-STPS-2014, Tabla I.1, fila 668). Colombia: el Decreto 1886 de 2015
 * (Art. 74) remite a la ACGIH sin fijar el valor.
 */
[[nodiscard]] inline std::optional<double> silica_limit_mg_m3(
    RegulatoryStandard standard,
    bool cristobalite_or_tridymite = false
) noexcept {
    switch (standard) {
        case RegulatoryStandard::DS024_Peru:     return 0.05;
        // DS 594 Art. 66: cristobalita y tridimita 0,04, la mitad que el cuarzo.
        case RegulatoryStandard::DS132_Chile:
            return cristobalite_or_tridymite ? 0.04 : 0.08;
        case RegulatoryStandard::D1886_Colombia: return std::nullopt;
        case RegulatoryStandard::NOM023_Mexico:  return 0.025;
    }
    return std::nullopt;
}

/**
 * @brief Velocidad máxima del aire en labores con personal [m/min], o vacío
 *        si la norma no la fija (la cita está en `AirVelocityLimits`).
 *
 * Perú: 250 m/min (DS 024-2016-EM, Art. 248). Chile: 150 m/min como promedio
 * (DS 132, Art. 138 inc. 3). Colombia: 6 m/s = 360 m/min en vías con tránsito
 * de personal (Decreto 1886 de 2015, Art. 56 par. 1) y 4,5 m/s = 270 m/min en
 * tajos de explotación de carbón (par. 2, con `coal_stope`). México: la
 * NOM-023-STPS-2012 no fija máxima.
 */
[[nodiscard]] inline std::optional<double> max_air_velocity_mpm(
    RegulatoryStandard standard,
    bool coal_stope = false
) noexcept {
    switch (standard) {
        case RegulatoryStandard::DS024_Peru:     return 250.0;
        case RegulatoryStandard::DS132_Chile:    return 150.0;
        case RegulatoryStandard::D1886_Colombia: return coal_stope ? 270.0 : 360.0;
        case RegulatoryStandard::NOM023_Mexico:  return std::nullopt;
    }
    return std::nullopt;
}

/**
 * @brief Velocidad mínima del aire que fija la norma [m/min], o vacío si la
 *        norma no la fija en esas condiciones (la cita está en
 *        `AirVelocityLimits`).
 *
 * - Perú: 20 m/min en las labores de explotación, desarrollo y preparación;
 *   25 m/min cuando se emplea ANFO u otros agentes de voladura
 *   (DS 024-2016-EM, Art. 248).
 * - Chile: 15 m/min como promedio (DS 132, Art. 138 inc. 3).
 * - Colombia: vacío. El Decreto 1886 de 2015 (Art. 56) fija mínimas solo para
 *   minas de carbón.
 * - México: 15,24 m/min donde opere maquinaria diésel (NOM-023-STPS-2012,
 *   numeral 8.4.4 b); vacío sin diésel.
 *
 * @param diesel           Opera maquinaria diésel en la labor.
 * @param blasting_agents  Se emplea ANFO u otros agentes de voladura.
 */
[[nodiscard]] inline std::optional<double> min_air_velocity_mpm(
    RegulatoryStandard standard,
    bool diesel = false,
    bool blasting_agents = false
) noexcept {
    switch (standard) {
        case RegulatoryStandard::DS024_Peru:
            return blasting_agents ? 25.0 : 20.0;
        case RegulatoryStandard::DS132_Chile:    return 15.0;
        case RegulatoryStandard::D1886_Colombia: return std::nullopt;
        case RegulatoryStandard::NOM023_Mexico:
            return diesel ? std::optional<double>{15.24} : std::nullopt;
    }
    return std::nullopt;
}

/**
 * @brief Indica si el explosivo cuenta como «ANFO u otros agentes de
 *        voladura» del DS 024-2016-EM, Art. 248.
 *
 * La dinamita es explosivo y no agente de voladura. Los tipos sin clasificar
 * (`Custom`, `ElectronicDet`) se tratan como agentes, la lectura más exigente.
 */
[[nodiscard]] inline bool is_blasting_agent(ExplosiveType type) noexcept {
    return type != ExplosiveType::Dynamite;
}

/**
 * @brief Velocidad mínima para dimensionar una labor [m/s].
 *
 * El mayor entre el valor de ingeniería por tipo de labor
 * (`constants::MIN_VELOCITY_*`) y la mínima de la norma
 * (`min_air_velocity_mpm`): nunca queda por debajo de la norma. Donde la
 * norma no fija mínima (Colombia fuera del carbón; México sin diésel) rige
 * solo el valor de ingeniería.
 *
 * Sin norma explícita rige el preset peruano, igual que el constructor por
 * defecto de `RegulatoryConfig`.
 */
[[nodiscard]] inline double get_min_velocity(
    ZoneType zone,
    RegulatoryStandard standard = RegulatoryStandard::DS024_Peru,
    bool diesel = false,
    bool blasting_agents = false
) noexcept {
    double engineering = constants::MIN_VELOCITY_DEVELOPMENT_MPS;
    switch (zone) {
        case ZoneType::DevelopmentFace:
        case ZoneType::GeneralMine:
            engineering = constants::MIN_VELOCITY_DEVELOPMENT_MPS; break;
        case ZoneType::Ramp:  engineering = constants::MIN_VELOCITY_RAMP_MPS; break;
        case ZoneType::Stope: engineering = constants::MIN_VELOCITY_STOPE_MPS; break;
    }
    const auto normative = min_air_velocity_mpm(standard, diesel, blasting_agents);
    return normative ? std::max(engineering, *normative / 60.0) : engineering;
}

/**
 * @brief Factor de producción u del DS 024-2016-EM, Anexo 38 lit. b
 *        (Q_Ma = T × u) según el consumo de madera [m³/min por t/guardia].
 *
 * Tabla del Anexo 38 y del Art. 252 lit. d, 2.º párrafo: < 20 % → 0;
 * 20 a 40 % → 0,60; 41 a 70 % → 1,00; > 70 % → 1,25. El num. 2 del Anexo 38
 * escribe «> 71» en la última fila, pero el num. 1 y el Art. 252 dicen «> 70»
 * («mayor al 70 %»): se usa > 70. Entre 40 y 41 % la tabla no dice nada; ese
 * tramo toma 1,00, el valor mayor.
 *
 * @throws std::invalid_argument si el porcentaje está fuera de [0, 100].
 */
[[nodiscard]] inline double timber_production_factor(double timber_pct) {
    validation::require_in_range(timber_pct, 0.0, 100.0,
        "timber_consumption_pct [%] - Consumo de madera");
    if (timber_pct < 20.0) return 0.0;
    if (timber_pct <= 40.0) return 0.60;
    if (timber_pct <= 70.0) return 1.00;
    return 1.25;
}

} // namespace ventpy
