#include "oled_status_display.h"

/**
 * @file oled_status_display.cpp
 * @brief Implements a minimal SSD1306-compatible OLED status renderer.
 */

#include <Wire.h>

#include <string.h>

#include "display_format.h"

namespace {

constexpr size_t kDisplayTextBufferSize = 12;
constexpr size_t kI2cChunkSize = 16;

constexpr uint8_t kGlyphSpace[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
constexpr uint8_t kGlyphBang[5] = {0x00, 0x00, 0x5F, 0x00, 0x00};
constexpr uint8_t kGlyphHyphen[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
constexpr uint8_t kGlyphDot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
constexpr uint8_t kGlyph0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
constexpr uint8_t kGlyph1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
constexpr uint8_t kGlyph2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
constexpr uint8_t kGlyph3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
constexpr uint8_t kGlyph4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
constexpr uint8_t kGlyph5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
constexpr uint8_t kGlyph6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
constexpr uint8_t kGlyph7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
constexpr uint8_t kGlyph8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
constexpr uint8_t kGlyph9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
constexpr uint8_t kGlyphA[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
constexpr uint8_t kGlyphB[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
constexpr uint8_t kGlyphE[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
constexpr uint8_t kGlyphF[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
constexpr uint8_t kGlyphH[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
constexpr uint8_t kGlyphN[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
constexpr uint8_t kGlyphO[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
constexpr uint8_t kGlyphR[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
constexpr uint8_t kGlyphT[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
constexpr uint8_t kGlyphW[5] = {0x7F, 0x20, 0x18, 0x20, 0x7F};

void clearPendingText(char* destination) {
  destination[0] = '\0';
}

}  // namespace

OledStatusDisplay::OledStatusDisplay(const OledStatusDisplayConfig& config)
    : config_(config), address_(0), available_(false), i2cReady_(false) {
  currentState_.mode = OledDisplayMode::kDisabled;
  clearPendingText(currentState_.primaryText);
  clearPendingText(currentState_.secondaryText);
  memset(buffer_, 0, sizeof(buffer_));
}

bool OledStatusDisplay::begin(uint32_t nowMs) {
  (void)nowMs;

  if (!i2cReady_) {
    i2cReady_ = Wire.begin(config_.sdaPin, config_.sclPin, config_.busFrequencyHz);
    if (!i2cReady_) {
      disableDisplay();
      return false;
    }
  }

  if (!detectDisplay()) {
    disableDisplay();
    return false;
  }

  if (!initializeController()) {
    disableDisplay();
    return false;
  }

  available_ = true;
  return true;
}

void OledStatusDisplay::update(uint32_t nowMs,
                               const SensorSnapshot& sensorSnapshot,
                               bool faultSnapshotValid,
                               const FaultPolicySnapshot& policySnapshot) {
  (void)nowMs;
  if (!available_) {
    currentState_.mode = OledDisplayMode::kDisabled;
    clearPendingText(currentState_.primaryText);
    clearPendingText(currentState_.secondaryText);
    return;
  }

  const PendingState pendingState =
      buildPendingState(sensorSnapshot, faultSnapshotValid, policySnapshot);
  if (stateEquals(currentState_, pendingState)) {
    return;
  }

  if (renderState(pendingState)) {
    copyState(&currentState_, pendingState);
  }
}

bool OledStatusDisplay::isAvailable() const {
  return available_;
}

OledDisplayMode OledStatusDisplay::mode() const {
  return currentState_.mode;
}

const char* OledStatusDisplay::modeLabel() const {
  return modeLabelFor(currentState_.mode);
}

const char* OledStatusDisplay::primaryText() const {
  return currentState_.primaryText;
}

const char* OledStatusDisplay::secondaryText() const {
  return currentState_.secondaryText;
}

uint8_t OledStatusDisplay::address() const {
  return available_ ? address_ : 0;
}

const uint8_t* OledStatusDisplay::glyphForChar(char c) {
  switch (c) {
    case ' ':
      return kGlyphSpace;
    case '!':
      return kGlyphBang;
    case '-':
      return kGlyphHyphen;
    case '.':
      return kGlyphDot;
    case '0':
      return kGlyph0;
    case '1':
      return kGlyph1;
    case '2':
      return kGlyph2;
    case '3':
      return kGlyph3;
    case '4':
      return kGlyph4;
    case '5':
      return kGlyph5;
    case '6':
      return kGlyph6;
    case '7':
      return kGlyph7;
    case '8':
      return kGlyph8;
    case '9':
      return kGlyph9;
    case 'A':
      return kGlyphA;
    case 'B':
      return kGlyphB;
    case 'E':
      return kGlyphE;
    case 'F':
      return kGlyphF;
    case 'H':
      return kGlyphH;
    case 'N':
      return kGlyphN;
    case 'O':
      return kGlyphO;
    case 'R':
      return kGlyphR;
    case 'T':
      return kGlyphT;
    case 'W':
      return kGlyphW;
    default:
      return kGlyphSpace;
  }
}

const char* OledStatusDisplay::faultTextForAlarm(AlarmCode alarmCode) {
  switch (alarmCode) {
    case AlarmCode::kWaterSensorFault:
      return "WATER";
    case AlarmCode::kFanFault:
      return "FAN";
    case AlarmCode::kWaterSensorAndFanFault:
      return "BOTH";
    case AlarmCode::kNone:
    default:
      return "";
  }
}

const char* OledStatusDisplay::modeLabelFor(OledDisplayMode mode) {
  switch (mode) {
    case OledDisplayMode::kDisabled:
      return "disabled";
    case OledDisplayMode::kBoot:
      return "boot";
    case OledDisplayMode::kTemperature:
      return "temperature";
    case OledDisplayMode::kFault:
      return "fault";
    default:
      return "unknown";
  }
}

bool OledStatusDisplay::detectDisplay() {
  const uint8_t candidateAddresses[] = {
      config_.primaryAddress,
      config_.secondaryAddress,
  };

  for (size_t i = 0; i < sizeof(candidateAddresses); ++i) {
    Wire.beginTransmission(candidateAddresses[i]);
    if (Wire.endTransmission() == 0) {
      address_ = candidateAddresses[i];
      return true;
    }
  }

  address_ = 0;
  return false;
}

bool OledStatusDisplay::initializeController() {
  const uint8_t initCommands[] = {
      0xAE,  // display off
      0xD5, 0x80,
      0xA8, 0x1F,
      0xD3, 0x00,
      0x40,
      0x8D, 0x14,
      0x20, 0x00,
      0xA1,
      0xC8,
      0xDA, 0x02,
      0x81, 0x8F,
      0xD9, 0xF1,
      0xDB, 0x40,
      0xA4,
      0xA6,
      0x2E,
      0xAF,
  };

  clearBuffer();
  return sendCommands(initCommands, sizeof(initCommands)) && flushBuffer();
}

bool OledStatusDisplay::sendCommand(uint8_t command) {
  Wire.beginTransmission(address_);
  Wire.write(0x00);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

bool OledStatusDisplay::sendCommands(const uint8_t* commands, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (!sendCommand(commands[i])) {
      return false;
    }
  }

  return true;
}

bool OledStatusDisplay::sendData(const uint8_t* data, size_t count) {
  size_t sent = 0;
  while (sent < count) {
    const size_t chunkSize = min(static_cast<size_t>(kI2cChunkSize), count - sent);
    Wire.beginTransmission(address_);
    Wire.write(0x40);
    Wire.write(data + sent, chunkSize);
    if (Wire.endTransmission() != 0) {
      return false;
    }
    sent += chunkSize;
  }

  return true;
}

bool OledStatusDisplay::flushBuffer() {
  const uint8_t setupCommands[] = {
      0x21, 0x00, static_cast<uint8_t>(config_.width - 1),
      0x22, 0x00, static_cast<uint8_t>((config_.height / 8) - 1),
  };

  if (!sendCommands(setupCommands, sizeof(setupCommands))) {
    disableDisplay();
    return false;
  }

  if (!sendData(buffer_, sizeof(buffer_))) {
    disableDisplay();
    return false;
  }

  return true;
}

void OledStatusDisplay::disableDisplay() {
  available_ = false;
  address_ = 0;
  currentState_.mode = OledDisplayMode::kDisabled;
  clearPendingText(currentState_.primaryText);
  clearPendingText(currentState_.secondaryText);
  clearBuffer();
}

void OledStatusDisplay::clearBuffer() {
  memset(buffer_, 0, sizeof(buffer_));
}

void OledStatusDisplay::drawPixel(int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= config_.width || y >= config_.height) {
    return;
  }

  const size_t index = static_cast<size_t>(x) + (static_cast<size_t>(y) / 8u) * config_.width;
  buffer_[index] |= static_cast<uint8_t>(1u << (y & 0x07));
}

void OledStatusDisplay::drawGlyph5x7(int16_t x, int16_t y, char c, uint8_t scale) {
  const uint8_t* glyph = glyphForChar(c);
  for (uint8_t column = 0; column < 5; ++column) {
    const uint8_t columnBits = glyph[column];
    for (uint8_t row = 0; row < 7; ++row) {
      if ((columnBits & (1u << row)) == 0) {
        continue;
      }

      for (uint8_t dx = 0; dx < scale; ++dx) {
        for (uint8_t dy = 0; dy < scale; ++dy) {
          drawPixel(x + column * scale + dx, y + row * scale + dy);
        }
      }
    }
  }
}

int16_t OledStatusDisplay::textWidth(const char* text, uint8_t scale) const {
  if (text == nullptr || text[0] == '\0') {
    return 0;
  }

  const int16_t glyphWidth = 5 * scale;
  const int16_t spacing = scale;
  int16_t width = 0;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    width += glyphWidth;
    if (text[i + 1] != '\0') {
      width += spacing;
    }
  }

  return width;
}

void OledStatusDisplay::drawText(int16_t x, int16_t y, const char* text, uint8_t scale) {
  if (text == nullptr) {
    return;
  }

  const int16_t advance = (5 * scale) + scale;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    drawGlyph5x7(x + static_cast<int16_t>(i) * advance, y, text[i], scale);
  }
}

OledStatusDisplay::PendingState OledStatusDisplay::buildPendingState(
    const SensorSnapshot& sensorSnapshot,
    bool faultSnapshotValid,
    const FaultPolicySnapshot& policySnapshot) const {
  PendingState state = {};
  state.mode = OledDisplayMode::kDisabled;
  clearPendingText(state.primaryText);
  clearPendingText(state.secondaryText);

  if (!available_) {
    return state;
  }

  if (!faultSnapshotValid) {
    state.mode = OledDisplayMode::kBoot;
    strncpy(state.primaryText, "BOOT", sizeof(state.primaryText) - 1);
    return state;
  }

  if (policySnapshot.alarmCode != AlarmCode::kNone) {
    state.mode = OledDisplayMode::kFault;
    strncpy(state.primaryText, "!", sizeof(state.primaryText) - 1);
    strncpy(state.secondaryText,
            faultTextForAlarm(policySnapshot.alarmCode),
            sizeof(state.secondaryText) - 1);
    return state;
  }

  const TrackedSensorSnapshot& waterSensor = sensorSnapshot.trackedSensors[0];
  if (!waterSensor.sampleValid) {
    state.mode = OledDisplayMode::kBoot;
    strncpy(state.primaryText, "BOOT", sizeof(state.primaryText) - 1);
    return state;
  }

  state.mode = OledDisplayMode::kTemperature;
  DisplayFormat::formatTemperatureC(waterSensor.temperatureC,
                                    state.primaryText,
                                    sizeof(state.primaryText));
  return state;
}

bool OledStatusDisplay::renderState(const PendingState& state) {
  clearBuffer();

  switch (state.mode) {
    case OledDisplayMode::kBoot: {
      const uint8_t scale = 3;
      const int16_t x = (config_.width - textWidth(state.primaryText, scale)) / 2;
      const int16_t y = (config_.height - (7 * scale)) / 2;
      drawText(x, y, state.primaryText, scale);
      break;
    }
    case OledDisplayMode::kTemperature: {
      const uint8_t scale = 4;
      const int16_t x = (config_.width - textWidth(state.primaryText, scale)) / 2;
      const int16_t y = (config_.height - (7 * scale)) / 2;
      drawText(x, y, state.primaryText, scale);
      break;
    }
    case OledDisplayMode::kFault: {
      drawText(14, 2, state.primaryText, 4);
      drawText(50, 9, state.secondaryText, 2);
      break;
    }
    case OledDisplayMode::kDisabled:
    default:
      break;
  }

  return flushBuffer();
}

bool OledStatusDisplay::stateEquals(const PendingState& lhs,
                                    const PendingState& rhs) const {
  return lhs.mode == rhs.mode &&
         strcmp(lhs.primaryText, rhs.primaryText) == 0 &&
         strcmp(lhs.secondaryText, rhs.secondaryText) == 0;
}

void OledStatusDisplay::copyState(PendingState* destination,
                                  const PendingState& source) const {
  destination->mode = source.mode;
  strncpy(destination->primaryText, source.primaryText, sizeof(destination->primaryText) - 1);
  destination->primaryText[sizeof(destination->primaryText) - 1] = '\0';
  strncpy(destination->secondaryText, source.secondaryText, sizeof(destination->secondaryText) - 1);
  destination->secondaryText[sizeof(destination->secondaryText) - 1] = '\0';
}
