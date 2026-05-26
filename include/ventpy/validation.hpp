/**
 * @file validation.hpp
 * @brief Utilidades de validación de dominio para cálculos safety-critical.
 *
 * Toda entrada numérica debe ser validada antes de usarse en cálculos.
 * Valores negativos, ceros en denominadores y rangos inválidos generan
 * excepciones claras para el ingeniero de ventilación.
 *
 * @copyright 2026 VentPy Project
 */

#pragma once

#include <concepts>
#include <stdexcept>
#include <string>

namespace ventpy::validation {

// ============================================================================
// C++20 Concepts para restricciones de dominio
// ============================================================================

/**
 * @brief Concept: tipo numérico de punto flotante.
 */
template <typename T>
concept FloatingPoint = std::floating_point<T>;

/**
 * @brief Concept: tipo numérico (entero o flotante).
 */
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// ============================================================================
// Funciones de validación
// ============================================================================

/**
 * @brief Valida que un valor sea estrictamente positivo (> 0).
 *
 * Uso: denominadores, potencia HP, cantidades de explosivo, etc.
 *
 * @tparam T Tipo numérico
 * @param value Valor a validar
 * @param param_name Nombre del parámetro (para mensaje de error claro)
 * @throws std::invalid_argument si value <= 0
 */
template <Numeric T>
constexpr void require_positive(T value, const std::string& param_name) {
    if (value <= static_cast<T>(0)) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: El parametro '" + param_name +
            "' debe ser estrictamente positivo (> 0). Valor recibido: " +
            std::to_string(value) +
            ". Verifique los datos de entrada del calculo de ventilacion."
        );
    }
}

/**
 * @brief Valida que un valor sea no negativo (>= 0).
 *
 * Uso: cantidades que pueden ser cero pero nunca negativas.
 *
 * @tparam T Tipo numérico
 * @param value Valor a validar
 * @param param_name Nombre del parámetro
 * @throws std::invalid_argument si value < 0
 */
template <Numeric T>
constexpr void require_non_negative(T value, const std::string& param_name) {
    if (value < static_cast<T>(0)) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: El parametro '" + param_name +
            "' no puede ser negativo. Valor recibido: " +
            std::to_string(value)
        );
    }
}

/**
 * @brief Valida que un valor esté dentro de un rango [min, max].
 *
 * Uso: factores de disponibilidad/utilización [0,1], altitud [0, 6000], etc.
 *
 * @tparam T Tipo numérico
 * @param value Valor a validar
 * @param min_val Límite inferior (inclusivo)
 * @param max_val Límite superior (inclusivo)
 * @param param_name Nombre del parámetro
 * @throws std::invalid_argument si value fuera de rango
 */
template <Numeric T>
constexpr void require_in_range(T value, T min_val, T max_val,
                                const std::string& param_name) {
    if (value < min_val || value > max_val) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: El parametro '" + param_name +
            "' debe estar en el rango [" + std::to_string(min_val) +
            ", " + std::to_string(max_val) +
            "]. Valor recibido: " + std::to_string(value)
        );
    }
}

/**
 * @brief Valida que un entero sea estrictamente positivo.
 *
 * Uso: cantidad de trabajadores.
 *
 * @param value Valor a validar
 * @param param_name Nombre del parámetro
 * @throws std::invalid_argument si value <= 0
 */
inline void require_positive_int(int value, const std::string& param_name) {
    if (value <= 0) {
        throw std::invalid_argument(
            "Error de dominio [VentPy]: El parametro '" + param_name +
            "' debe ser un entero positivo (> 0). Valor recibido: " +
            std::to_string(value)
        );
    }
}

} // namespace ventpy::validation
