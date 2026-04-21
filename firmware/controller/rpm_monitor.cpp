/**
 * @file rpm_monitor.cpp
 * @brief Implements interrupt-driven fan tachometer sampling.
 */

#include "rpm_monitor.h"

RpmMonitor* RpmMonitor::activeInstance_ = nullptr;

RpmMonitor::RpmMonitor(const RpmMonitorConfig& config)
    : config_(config),
      pulseCountSinceSample_(0),
      lastMeasuredRpm_(0),
      lastSampleMs_(0),
      initialized_(false) {}

bool RpmMonitor::begin() {
  pinMode(config_.tachPin, INPUT_PULLUP);
  pulseCountSinceSample_ = 0;
  lastMeasuredRpm_ = 0;
  lastSampleMs_ = millis();
  activeInstance_ = this;
  attachInterrupt(digitalPinToInterrupt(config_.tachPin), handleInterrupt, FALLING);
  initialized_ = true;
  return true;
}

void RpmMonitor::update(uint32_t nowMs) {
  if (!initialized_) {
    return;
  }

  const uint32_t elapsedMs = nowMs - lastSampleMs_;
  if (elapsedMs < config_.sampleWindowMs) {
    return;
  }

  const uint32_t pulses = takePulseCountSnapshot();
  lastSampleMs_ = nowMs;

  if (elapsedMs == 0 || config_.pulsesPerRevolution == 0) {
    lastMeasuredRpm_ = 0;
    return;
  }

  const uint32_t numerator = pulses * 60000UL;
  const uint32_t denominator =
      (uint32_t)config_.pulsesPerRevolution * elapsedMs;
  lastMeasuredRpm_ = (uint16_t)((numerator + (denominator / 2)) / denominator);
}

uint16_t RpmMonitor::rpm() const {
  return lastMeasuredRpm_;
}

uint32_t RpmMonitor::pulseCount() const {
  return pulseCountSinceSample_;
}

uint32_t RpmMonitor::sampleAgeMs(uint32_t nowMs) const {
  return nowMs - lastSampleMs_;
}

void IRAM_ATTR RpmMonitor::handleInterrupt() {
  if (activeInstance_ != nullptr) {
    activeInstance_->onPulse();
  }
}

void IRAM_ATTR RpmMonitor::onPulse() {
  ++pulseCountSinceSample_;
}

uint32_t RpmMonitor::takePulseCountSnapshot() {
  noInterrupts();
  const uint32_t pulses = pulseCountSinceSample_;
  pulseCountSinceSample_ = 0;
  interrupts();
  return pulses;
}
