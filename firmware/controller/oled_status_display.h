#pragma once

/**
 * @file oled_status_display.h
 * @brief Optional local OLED rendering for boot, temperature, and fault state.
 */

#include <Arduino.h>

#include "fault_policy.h"
#include "sensor_manager.h"

/**
 * @brief Visible OLED states exposed for diagnostics and rendering decisions.
 */
enum class OledDisplayMode : uint8_t {
  kDisabled,    ///< No compatible OLED is available or the display path is disabled.
  kBoot,        ///< The controller is still in its short startup display state.
  kTemperature, ///< The display shows the current water temperature.
  kFault,       ///< The display shows the fault indicator and short fault text.
};

/**
 * @brief Configuration for the optional OLED display path.
 */
struct OledStatusDisplayConfig {
  uint8_t sdaPin;            ///< GPIO used for the I2C SDA line.
  uint8_t sclPin;            ///< GPIO used for the I2C SCL line.
  uint8_t width;             ///< OLED width in pixels.
  uint8_t height;            ///< OLED height in pixels.
  uint8_t primaryAddress;    ///< Preferred I2C address to probe first.
  uint8_t secondaryAddress;  ///< Fallback I2C address to probe second.
  uint32_t busFrequencyHz;   ///< I2C bus speed in hertz.
};

/**
 * @brief Optional OLED renderer with fail-open behavior.
 *
 * The display is intentionally non-critical. If the OLED is missing, cannot be
 * initialized, or later fails during I2C writes, the class disables itself and
 * leaves the rest of the controller unaffected.
 */
class OledStatusDisplay {
 public:
  explicit OledStatusDisplay(
      const OledStatusDisplayConfig& config = {
          32,
          27,
          128,
          32,
          0x3C,
          0x3D,
          400000,
      });

  /**
   * @brief Probes and initializes a compatible OLED if one is present.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @return True when a display was detected and initialized.
   */
  bool begin(uint32_t nowMs);

  /**
   * @brief Updates the visible OLED state from current controller snapshots.
   *
   * @param nowMs Current monotonic timestamp in milliseconds.
   * @param sensorSnapshot Latest sensor state.
   * @param faultSnapshotValid True when policy/fault snapshots are valid.
   * @param policySnapshot Latest fault policy state.
   */
  void update(uint32_t nowMs,
              const SensorSnapshot& sensorSnapshot,
              bool faultSnapshotValid,
              const FaultPolicySnapshot& policySnapshot);

  /**
   * @brief Indicates whether a compatible OLED is currently available.
   *
   * @return True when the display path is active.
   */
  bool isAvailable() const;

  /**
   * @brief Returns the last rendered or selected display mode.
   *
   * @return Current mode label.
   */
  OledDisplayMode mode() const;

  /**
   * @brief Returns a short diagnostic label for the current mode.
   *
   * @return Null-terminated label string.
   */
  const char* modeLabel() const;

  /**
   * @brief Returns the primary visible text used for the current mode.
   *
   * @return Null-terminated text buffer.
   */
  const char* primaryText() const;

  /**
   * @brief Returns the secondary visible text used for the current mode.
   *
   * @return Null-terminated text buffer.
   */
  const char* secondaryText() const;

  /**
   * @brief Returns the detected I2C address when the display is available.
   *
   * @return Active I2C address, or 0 when unavailable.
   */
  uint8_t address() const;

 private:
  struct PendingState {
    OledDisplayMode mode;
    char primaryText[12];
    char secondaryText[12];
  };

  static const uint8_t* glyphForChar(char c);
  static const char* faultTextForAlarm(AlarmCode alarmCode);
  static const char* modeLabelFor(OledDisplayMode mode);

  bool detectDisplay();
  bool initializeController();
  bool sendCommand(uint8_t command);
  bool sendCommands(const uint8_t* commands, size_t count);
  bool sendData(const uint8_t* data, size_t count);
  bool flushBuffer();
  void disableDisplay();

  void clearBuffer();
  void drawPixel(int16_t x, int16_t y);
  void drawGlyph5x7(int16_t x, int16_t y, char c, uint8_t scale);
  int16_t textWidth(const char* text, uint8_t scale) const;
  void drawText(int16_t x, int16_t y, const char* text, uint8_t scale);

  PendingState buildPendingState(const SensorSnapshot& sensorSnapshot,
                                 bool faultSnapshotValid,
                                 const FaultPolicySnapshot& policySnapshot) const;
  bool renderState(const PendingState& state);
  bool stateEquals(const PendingState& lhs, const PendingState& rhs) const;
  void copyState(PendingState* destination, const PendingState& source) const;

  OledStatusDisplayConfig config_;
  uint8_t address_;
  bool available_;
  bool i2cReady_;
  PendingState currentState_;
  uint8_t buffer_[128 * 32 / 8];
};
