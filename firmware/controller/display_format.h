#pragma once

/**
 * @file display_format.h
 * @brief Shared helpers for rounded operator-facing value formatting.
 */

#include <Arduino.h>
#include <stdio.h>

namespace DisplayFormat {

constexpr size_t kTemperatureBufferSize = 16;

/**
 * @brief Formats a temperature-like float with one decimal place.
 *
 * Formatting happens only at display boundaries so control logic can keep the
 * full internal floating-point precision.
 *
 * @param value Temperature or delta value in degrees Celsius.
 * @param buffer Destination character buffer.
 * @param bufferSize Size of the destination buffer in bytes.
 */
inline void formatTemperatureC(float value, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%.1f", value);
}

/**
 * @brief Prints a temperature-like float with one decimal place.
 *
 * @param out Destination stream.
 * @param value Temperature or delta value in degrees Celsius.
 */
inline void printTemperatureC(Stream& out, float value) {
  char buffer[kTemperatureBufferSize] = {};
  formatTemperatureC(value, buffer, sizeof(buffer));
  out.print(buffer);
}

}  // namespace DisplayFormat
