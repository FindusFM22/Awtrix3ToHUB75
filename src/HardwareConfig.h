
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// =============================================================================
// HardwareConfig.h — Single source of truth for hardware capabilities.
//
// Each build environment (-DULANZI, -Dawtrix2_upgrade, -DHUB75, ...) enables
// or disables capability macros (HAS_BATTERY, HAS_BUZZER, HAS_DFPLAYER,
// HAS_BUTTONS, DISPLAY_HUB75, DISPLAY_WS2812).
//
// Source files must guard subsystem code with the capability macros, not with
// the build-env macros directly. That keeps later hardware variants additive.
// =============================================================================

#if defined(ULANZI)
  // Ulanzi TC001 stock hardware: WS2812 32x8, 3 buttons, buzzer, battery, no DFPlayer.
  #define DISPLAY_WS2812
  #define HAS_BUTTONS
  #define HAS_BUZZER
  #define HAS_BATTERY
  // HAS_DFPLAYER is NOT defined; runtime flag DFPLAYER_ACTIVE may still gate it.

#elif defined(awtrix2_upgrade)
  // AWTRIX2 upgrade (WEMOS D1 mini32): WS2812, buttons via menu, DFPlayer, no battery.
  #define DISPLAY_WS2812
  #define HAS_BUTTONS
  #define HAS_BUZZER
  #define HAS_DFPLAYER

#elif defined(HUB75)
  // Custom HUB75 build (ESP32 DevKit V1 + HUB75 P2.5 64x32 + BME280 + LDR).
  // Bedroom dashboard: silent, no buttons, no buzzer, no battery, no DFPlayer.
  #define DISPLAY_HUB75
  #define HAS_OUTDOOR_WEATHER  // wttr.in-based outdoor temp/weather-icon fetch
  // No HAS_BUTTONS, HAS_BUZZER, HAS_BATTERY, HAS_DFPLAYER.

#else
  #warning "No hardware variant defined. Defaulting to ULANZI capabilities."
  #define DISPLAY_WS2812
  #define HAS_BUTTONS
  #define HAS_BUZZER
  #define HAS_BATTERY
#endif

#endif // HARDWARE_CONFIG_H
