/**
 * @file test_validation.cpp
 * @brief Tests unitarios para validación de dominio.
 */

#include <gtest/gtest.h>
#include <stdexcept>

#include "ventpy/validation.hpp"

namespace v = ventpy::validation;

// ============================================================================
// require_positive
// ============================================================================

TEST(Validation, RequirePositive_AcceptsPositiveDouble) {
    EXPECT_NO_THROW(v::require_positive(1.0, "test_param"));
    EXPECT_NO_THROW(v::require_positive(0.001, "test_param"));
    EXPECT_NO_THROW(v::require_positive(99999.0, "test_param"));
}

TEST(Validation, RequirePositive_RejectsZero) {
    EXPECT_THROW(v::require_positive(0.0, "test_param"), std::invalid_argument);
}

TEST(Validation, RequirePositive_RejectsNegative) {
    EXPECT_THROW(v::require_positive(-1.0, "test_param"), std::invalid_argument);
    EXPECT_THROW(v::require_positive(-0.001, "test_param"), std::invalid_argument);
}

TEST(Validation, RequirePositive_ErrorMessageContainsParamName) {
    try {
        v::require_positive(-5.0, "horsepower_HP");
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("horsepower_HP"), std::string::npos);
        EXPECT_NE(msg.find("VentPy"), std::string::npos);
    }
}

// ============================================================================
// require_non_negative
// ============================================================================

TEST(Validation, RequireNonNegative_AcceptsZero) {
    EXPECT_NO_THROW(v::require_non_negative(0.0, "test"));
}

TEST(Validation, RequireNonNegative_AcceptsPositive) {
    EXPECT_NO_THROW(v::require_non_negative(100.0, "test"));
}

TEST(Validation, RequireNonNegative_RejectsNegative) {
    EXPECT_THROW(v::require_non_negative(-0.01, "altitude"),
                 std::invalid_argument);
}

// ============================================================================
// require_in_range
// ============================================================================

TEST(Validation, RequireInRange_AcceptsWithinBounds) {
    EXPECT_NO_THROW(v::require_in_range(0.5, 0.0, 1.0, "factor"));
    EXPECT_NO_THROW(v::require_in_range(0.0, 0.0, 1.0, "factor"));
    EXPECT_NO_THROW(v::require_in_range(1.0, 0.0, 1.0, "factor"));
}

TEST(Validation, RequireInRange_RejectsBelowMin) {
    EXPECT_THROW(v::require_in_range(-0.1, 0.0, 1.0, "factor"),
                 std::invalid_argument);
}

TEST(Validation, RequireInRange_RejectsAboveMax) {
    EXPECT_THROW(v::require_in_range(1.1, 0.0, 1.0, "factor"),
                 std::invalid_argument);
}

// ============================================================================
// require_positive_int
// ============================================================================

TEST(Validation, RequirePositiveInt_AcceptsPositive) {
    EXPECT_NO_THROW(v::require_positive_int(1, "workers"));
    EXPECT_NO_THROW(v::require_positive_int(100, "workers"));
}

TEST(Validation, RequirePositiveInt_RejectsZero) {
    EXPECT_THROW(v::require_positive_int(0, "workers"), std::invalid_argument);
}

TEST(Validation, RequirePositiveInt_RejectsNegative) {
    EXPECT_THROW(v::require_positive_int(-5, "workers"), std::invalid_argument);
}
