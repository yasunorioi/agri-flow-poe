// sensors.h — DIGITEN hall-effect flow meter on G26.
//
// Pulse counting via attachInterrupt(FALLING). One-second sampler in
// sensorsPoll() computes the instantaneous rate; the running counter is
// also exposed as cumulative volume.

#pragma once

#include <Arduino.h>
#include "config.h"

static const int PIN_FLOW = 26;   // Grove signal pin

extern volatile uint32_t g_pulse_count;     // increments in ISR
extern uint32_t          g_cum_pulses;      // since boot (read out of ISR)
extern uint32_t          g_last_window_ms;
extern uint32_t          g_last_window_pulses;
extern float             g_flow_lpm;        // instantaneous L/min

// Defined in main.cpp (forced IRAM placement + linker section).
extern void onFlowPulse();

inline void sensorsBegin() {
  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), onFlowPulse, FALLING);
  g_last_window_ms     = millis();
  g_last_window_pulses = 0;
  Serial.printf("[SENS] flow input GPIO%d (FALLING-edge ISR), ppl=%u\n",
                PIN_FLOW, g_cfg.pulses_per_liter);
}

inline void sensorsPoll() {
  // Snapshot counter atomically (read 32-bit on ESP32 is atomic enough
  // for our pulse cadence; we still cache before subtracting).
  uint32_t current = g_pulse_count;
  uint32_t now     = millis();
  uint32_t dt_ms   = now - g_last_window_ms;
  if (dt_ms < 500) return;            // rate-limit to ~2 Hz updates

  uint32_t dp = current - g_last_window_pulses;
  float hz = (dp * 1000.0f) / dt_ms;
  // pulses / sec ÷ pulses_per_liter × 60 = L/min
  uint16_t ppl = g_cfg.pulses_per_liter;
  g_flow_lpm = (ppl > 0) ? (hz * 60.0f / ppl) : 0.0f;

  g_cum_pulses         = current;
  g_last_window_pulses = current;
  g_last_window_ms     = now;
}

inline float volumeLiters() {
  uint16_t ppl = g_cfg.pulses_per_liter;
  return (ppl > 0) ? (float)g_cum_pulses / (float)ppl : 0.0f;
}

inline void resetVolume() {
  noInterrupts();
  g_pulse_count = 0;
  interrupts();
  g_cum_pulses         = 0;
  g_last_window_pulses = 0;
}
