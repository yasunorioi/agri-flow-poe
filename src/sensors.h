// sensors.h — two DIGITEN hall-effect flow meters on the Grove port.
//
// Same-model dual irrigation lines: ch0 signal on G26, ch1 on G32 (both
// exposed on the M5 ATOM Grove connector, sharing 5V/GND). Pulse counting
// via attachInterrupt(FALLING) per channel; a shared ~2 Hz sampler computes
// the instantaneous rate, and the running counters give cumulative volume.

#pragma once

#include <Arduino.h>
#include "config.h"

static const int NUM_FLOW       = 2;
static const int PIN_FLOW[NUM_FLOW] = {26, 32};   // Grove signal pins (ch0, ch1)

extern volatile uint32_t g_pulse_count[NUM_FLOW];        // increments in ISRs
extern uint32_t          g_cum_pulses[NUM_FLOW];         // since boot (read out of ISR)
extern uint32_t          g_last_window_ms;               // shared sample window
extern uint32_t          g_last_window_pulses[NUM_FLOW];
extern float             g_flow_lpm[NUM_FLOW];           // instantaneous L/min per channel

// Defined in main.cpp (forced IRAM placement + linker section).
extern void onFlowPulse0();
extern void onFlowPulse1();

inline void sensorsBegin() {
  for (int i = 0; i < NUM_FLOW; i++) {
    pinMode(PIN_FLOW[i], INPUT_PULLUP);
    g_last_window_pulses[i] = 0;
  }
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW[0]), onFlowPulse0, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW[1]), onFlowPulse1, FALLING);
  g_last_window_ms = millis();
  Serial.printf("[SENS] flow ch0=GPIO%d ch1=GPIO%d (FALLING-edge ISR), ppl=%u\n",
                PIN_FLOW[0], PIN_FLOW[1], g_cfg.pulses_per_liter);
}

inline void sensorsPoll() {
  // Both channels share one sample window — same cadence, simpler timing.
  uint32_t now   = millis();
  uint32_t dt_ms = now - g_last_window_ms;
  if (dt_ms < 500) return;            // rate-limit to ~2 Hz updates

  uint16_t ppl = g_cfg.pulses_per_liter;
  for (int i = 0; i < NUM_FLOW; i++) {
    uint32_t current = g_pulse_count[i];   // 32-bit read is atomic enough here
    uint32_t dp      = current - g_last_window_pulses[i];
    float    hz      = (dp * 1000.0f) / dt_ms;
    // pulses / sec ÷ pulses_per_liter × 60 = L/min
    g_flow_lpm[i]          = (ppl > 0) ? (hz * 60.0f / ppl) : 0.0f;
    g_cum_pulses[i]        = current;
    g_last_window_pulses[i] = current;
  }
  g_last_window_ms = now;
}

inline float volumeLiters(int ch) {
  uint16_t ppl = g_cfg.pulses_per_liter;
  return (ppl > 0) ? (float)g_cum_pulses[ch] / (float)ppl : 0.0f;
}

inline void resetVolume() {
  noInterrupts();
  for (int i = 0; i < NUM_FLOW; i++) g_pulse_count[i] = 0;
  interrupts();
  for (int i = 0; i < NUM_FLOW; i++) {
    g_cum_pulses[i]         = 0;
    g_last_window_pulses[i] = 0;
  }
}
