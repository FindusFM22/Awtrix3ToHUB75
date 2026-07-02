/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 * Copyright (c) 2023 by Stephan Muehl (Blueforcer)
 * Note: This old lib for SSD1306 displays has been extremely
 * modified for AWTRIX 3 and has nothing to do with the original purposes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "MatrixDisplayUi.h"
#include "effects.h"
#include "Globals.h"
#include "effects.h"
#ifdef DISPLAY_HUB75
#include "Overlays.h"  // notifyFlag
#include "Apps.h"      // TimeApp, TempApp, HumApp, OutdoorApp
#include "icons.h"     // built-in fallback icons for grid widgets
#include "Functions.h" // getTextWidth
#include "timer.h"     // timer_localtime, timer_time for the time widget
#include <LittleFS.h>
#endif

GifPlayer gif1;
GifPlayer gif2;

MatrixDisplayUi::MatrixDisplayUi(FastLED_NeoMatrix *matrix)
{
  this->matrix = matrix;
}

void MatrixDisplayUi::init()
{
  this->matrix->begin();
  this->matrix->setTextWrap(false);
  this->matrix->setBrightness(70);
  gif1.setMatrix(this->matrix);
  gif2.setMatrix(this->matrix);
}

void MatrixDisplayUi::setTargetFPS(uint8_t fps)
{
  float oldInterval = this->updateInterval;
  this->updateInterval = ((float)1.0 / (float)fps) * 1000;

  // Calculate new ticksPerApp
  float changeRatio = oldInterval / (float)this->updateInterval;
  // this->ticksPerApp *= changeRatio;
  this->ticksPerTransition *= changeRatio;
}

void MatrixDisplayUi::setBackgroundEffect(int effect)
{
  this->BackgroundEffect = effect;
}

// -/------ Automatic control ------\-

void MatrixDisplayUi::enablesetAutoTransition()
{
  this->setAutoTransition = true;
}
void MatrixDisplayUi::disablesetAutoTransition()
{
  this->setAutoTransition = false;
}
void MatrixDisplayUi::setsetAutoTransitionForwards()
{
  this->state.appTransitionDirection = 1;
  this->lastTransitionDirection = 1;
}
void MatrixDisplayUi::setsetAutoTransitionBackwards()
{
  this->state.appTransitionDirection = -1;
  this->lastTransitionDirection = -1;
}
void MatrixDisplayUi::setTimePerApp(long time)
{
  this->ticksPerApp = time / updateInterval;
}
void MatrixDisplayUi::setTimePerTransition(uint16_t time)
{
  this->ticksPerTransition = (int)((float)time / (float)updateInterval);
}

// -/----- App settings -----\-
void MatrixDisplayUi::setAppAnimation(AnimationDirection dir)
{
  this->appAnimationDirection = dir;
}

void MatrixDisplayUi::setApps(const std::vector<std::pair<String, AppCallback>> &appPairs)
{
  delete[] AppFunctions;
  AppCount = appPairs.size();
  AppFunctions = new AppCallback[AppCount];
  for (size_t i = 0; i < AppCount; ++i)
  {
    AppFunctions[i] = appPairs[i].second;
  }
  this->resetState();
  DisplayManager.sendAppLoop();
  DisplayManager.setAutoTransition(true);
}

// -/----- Overlays ------\-
void MatrixDisplayUi::setOverlays(OverlayCallback *overlayFunctions, uint8_t overlayCount)
{
  this->overlayFunctions = overlayFunctions;
  this->overlayCount = overlayCount;
}

void MatrixDisplayUi::setBackground(BackgroundCallback backgroundFunction)
{
  this->backgroundFunction = backgroundFunction;
}

// -/----- Manual control -----\-
void MatrixDisplayUi::nextApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manualControl = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = 1;
  }
}
void MatrixDisplayUi::previousApp()
{
  if (this->state.appState != IN_TRANSITION)
  {
    this->state.manualControl = true;
    this->state.appState = IN_TRANSITION;
    this->state.ticksSinceLastStateSwitch = 0;
    this->lastTransitionDirection = this->state.appTransitionDirection;
    this->state.appTransitionDirection = -1;
  }
}

bool MatrixDisplayUi::switchToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return false;
  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return false;
  this->state.appState = FIXED;
  this->state.currentApp = app;
  return true;
}

void MatrixDisplayUi::transitionToApp(uint8_t app)
{
  if (app >= this->AppCount)
    return;
  this->state.ticksSinceLastStateSwitch = 0;
  if (app == this->state.currentApp)
    return;
  this->nextAppNumber = app;
  this->lastTransitionDirection = this->state.appTransitionDirection;
  this->state.manualControl = true;
  this->state.appState = IN_TRANSITION;
  this->state.appTransitionDirection = app < this->state.currentApp ? -1 : 1;
}

// -/----- State information -----\-
MatrixDisplayUiState *MatrixDisplayUi::getUiState()
{
  return &this->state;
}

int8_t MatrixDisplayUi::update()
{
  long appStart = millis();
  int8_t timeBudget = this->updateInterval - (appStart - this->state.lastUpdate);
  if (timeBudget <= 0)
  {
    // Implement frame skipping to ensure time budget is kept
    if (this->setAutoTransition && this->state.lastUpdate != 0)
      this->state.ticksSinceLastStateSwitch += ceil(-timeBudget / this->updateInterval);

    this->state.lastUpdate = appStart;
    this->tick();
  }

  return this->updateInterval - (millis() - appStart);
}

void MatrixDisplayUi::tick()
{
  this->state.ticksSinceLastStateSwitch++;

  if (this->AppCount > 0)
  {
    switch (this->state.appState)
    {
    case IN_TRANSITION:
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerTransition)
      {
        this->state.appState = FIXED;
        this->state.currentApp = getnextAppNumber();
        this->state.ticksSinceLastStateSwitch = 0;
        this->nextAppNumber = -1;
      }
      break;
    case FIXED:
      // Revert manualControl
      if (this->state.manualControl)
      {
        this->state.appTransitionDirection = 1;
        this->state.manualControl = false;
      }
      if (this->state.ticksSinceLastStateSwitch >= this->ticksPerApp)
      {
        if (this->setAutoTransition)
        {
          this->state.appState = IN_TRANSITION;
        }
        this->state.ticksSinceLastStateSwitch = 0;
      }
      break;
    }
  }

  this->matrix->clear();

  if (BackgroundEffect > -1)
  {
    callEffect(this->matrix, 0, 0, BackgroundEffect);
  }

#ifdef DISPLAY_HUB75
  // Grid layout: four native apps rendered simultaneously in 32x16 quadrants.
  // Suppressed while a notification is active — NotifyOverlay renders full
  // panel via drawOverlays() below. The classic drawApp() rotation path is
  // never taken on HUB75 builds; setAutoTransition is disabled at boot so
  // the internal state machine stays in FIXED forever.
  if (!notifyFlag)
    this->drawGrid();
#else
  if (this->AppCount > 0)
    this->drawApp();
#endif
  this->drawOverlays();
  this->drawIndicators();
  if (GLOBAL_OVERLAY > 0)
  {
    EffectOverlay(matrix, 0, 0, GLOBAL_OVERLAY);
  }
  DisplayManager.gammaCorrection();
#ifdef DISPLAY_HUB75
  DisplayManager.blitToPanel();
#else
  this->matrix->show();
#endif
}

void MatrixDisplayUi::drawIndicators()
{
  uint32_t drawColor;

  // Indicator 1
  if (indicator1State)
  {
    if (indicator1Blink)
    {
      if (millis() % (2 * indicator1Blink) < indicator1Blink)
      {
        drawColor = indicator1Color;
      }
      else
      {
        drawColor = 0; // Schwarz
      }
    }
    else if (indicator1Fade)
    {
      drawColor = fadeColor(indicator1Color, indicator1Fade);
    }
    else
    {
      drawColor = indicator1Color;
    }
    matrix->drawPixel(31, 0, drawColor);
    matrix->drawPixel(30, 0, drawColor);
    matrix->drawPixel(31, 1, drawColor);
  }

  // Indicator 2
  if (indicator2State)
  {
    if (indicator2Blink)
    {
      if (millis() % (2 * indicator2Blink) < indicator2Blink)
      {
        drawColor = indicator2Color;
      }
      else
      {
        drawColor = 0; // Schwarz
      }
    }
    else if (indicator2Fade)
    {
      drawColor = fadeColor(indicator2Color, indicator2Fade);
    }
    else
    {
      drawColor = indicator2Color;
    }
    matrix->drawPixel(31, 3, drawColor);
    matrix->drawPixel(31, 4, drawColor);
  }

  // Indicator 3
  if (indicator3State)
  {
    if (indicator3Blink)
    {
      if (millis() % (2 * indicator3Blink) < indicator3Blink)
      {
        drawColor = indicator3Color;
      }
      else
      {
        drawColor = 0; // Schwarz
      }
    }
    else if (indicator3Fade)
    {
      drawColor = fadeColor(indicator3Color, indicator3Fade);
    }
    else
    {
      drawColor = indicator3Color;
    }
    matrix->drawPixel(31, 7, drawColor);
    matrix->drawPixel(31, 6, drawColor);
    matrix->drawPixel(30, 7, drawColor);
  }
}

uint32_t MatrixDisplayUi::fadeColor(uint32_t color, uint32_t interval)
{
  float phase = (sin(2 * PI * millis() / float(interval)) + 1) * 0.5;
  uint8_t r = ((color >> 16) & 0xFF) * phase;
  uint8_t g = ((color >> 8) & 0xFF) * phase;
  uint8_t b = (color & 0xFF) * phase;
  return (r << 16) | (g << 8) | b;
}

uint8_t currentTransition;
bool gotNewTransition = true;
TransitionType getRandomTransition()
{
  // RANDOM is now index 0, so we add 1 to the result to ensure it's never selected
  return static_cast<TransitionType>((rand() % (CROSSFADE)) + 1);
}

bool swapped = false;

void MatrixDisplayUi::drawApp()
{
  switch (this->state.appState)
  {
  case IN_TRANSITION:
  {
    swapped = false;
    gotNewTransition = false;
    if (currentTransition == SLIDE)
    {
      slideTransition();
    }
    else if (currentTransition == FADE)
    {
      fadeTransition();
    }
    else if (currentTransition == ZOOM)
    {
      zoomTransition();
    }
    else if (currentTransition == ROTATE)
    {
      rotateTransition();
    }
    else if (currentTransition == PIXELATE)
    {
      pixelateTransition();
    }
    else if (currentTransition == CURTAIN)
    {
      curtainTransition();
    }
    else if (currentTransition == RIPPLE)
    {
      rippleTransition();
    }
    else if (currentTransition == BLINK)
    {
      blinkTransition();
    }
    else if (currentTransition == RELOAD)
    {
      reloadTransition();
    }
    else if (currentTransition == CROSSFADE)
    {
      crossfadeTransition();
    }
    break;
  }
  case FIXED:
    if (TRANS_EFFECT == RANDOM)
    {
      if (gotNewTransition == false)
      {
        currentTransition = getRandomTransition(); // Wähle einen neuen zufälligen Übergang aus, wenn TRANS_EFFECT auf RANDOM gesetzt ist
        gotNewTransition = true;
      }
    }
    else
    {
      currentTransition = TRANS_EFFECT; // Wenn TRANS_EFFECT nicht RANDOM ist, setzen Sie currentTransition auf TRANS_EFFECT
    }

    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
    swapped = true;
    break;
  }
}

bool MatrixDisplayUi::isCurrentAppValid()
{
  for (size_t i = 0; i < AppCount; ++i)
  {
    if (AppFunctions[i] == AppFunctions[this->state.currentApp])
    {
      return true;
    }
  }
  return false;
}

void MatrixDisplayUi::resetState()
{
  if (!isCurrentAppValid())
  {
    this->state.lastUpdate = 0;
    this->state.ticksSinceLastStateSwitch = 0;
    this->state.appState = FIXED;
    this->state.currentApp = 0;
  }
}

void MatrixDisplayUi::forceResetState()
{
  this->state.lastUpdate = 0;
  this->state.ticksSinceLastStateSwitch = 0;
  this->state.appState = FIXED;
  this->state.currentApp = 0;
}

void MatrixDisplayUi::drawOverlays()
{
  for (uint8_t i = 0; i < this->overlayCount; i++)
  {
    (this->overlayFunctions[i])(this->matrix, &this->state, &gif2);
  }
}

void MatrixDisplayUi::drawBackground()
{
  this->backgroundFunction(this->matrix);
}

uint8_t MatrixDisplayUi::getnextAppNumber()
{
  if (this->nextAppNumber != -1)
    return this->nextAppNumber;
  return (this->state.currentApp + this->AppCount + this->state.appTransitionDirection) % this->AppCount;
}

void MatrixDisplayUi::setIndicator1Color(uint32_t color)
{
  this->indicator1Color = color;
}

void MatrixDisplayUi::setIndicator1State(bool state)
{
  this->indicator1State = state;
}

void MatrixDisplayUi::setIndicator1Blink(int blink)
{
  this->indicator1Blink = blink;
}

void MatrixDisplayUi::setIndicator1Fade(int fade)
{
  this->indicator1Fade = fade;
}

void MatrixDisplayUi::setIndicator2Color(uint32_t color)
{
  this->indicator2Color = color;
}

void MatrixDisplayUi::setIndicator2State(bool state)
{
  this->indicator2State = state;
}

void MatrixDisplayUi::setIndicator2Blink(int blink)
{
  this->indicator2Blink = blink;
}

void MatrixDisplayUi::setIndicator2Fade(int fade)
{
  this->indicator2Fade = fade;
}

void MatrixDisplayUi::setIndicator3Color(uint32_t color)
{
  this->indicator3Color = color;
}

void MatrixDisplayUi::setIndicator3State(bool state)
{
  this->indicator3State = state;
}

void MatrixDisplayUi::setIndicator3Blink(int blink)
{
  this->indicator3Blink = blink;
}

void MatrixDisplayUi::setIndicator3Fade(int fade)
{
  this->indicator3Fade = fade;
}

// ------------------ TRANSITIONS -------------------
float distance(int x1, int y1, int x2, int y2)
{
  return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

void rotate(int &x, int &y, float angle)
{
  // Move the point to the origin
  x -= 16;
  y -= 4;

  // Perform the rotation
  int newX = x * cos(angle) - y * sin(angle);
  int newY = x * sin(angle) + y * cos(angle);

  // Move the point back to the actual origin
  x = newX + 16;
  y = newY + 4;
}

void MatrixDisplayUi::fadeTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int fadeValue;
  if (progress < 0.5)
  {
    fadeValue = pow(progress * 2, 2) * 255; // Fading out the old app (progress from 0 to 0.5)
  }
  else
  {
    fadeValue = pow((1.0 - progress) * 2, 2) * 255; // Fading in the new app (progress from 0.5 to 1.0)
  }
  this->matrix->clear(); // Clear the matrix
  // If fading out the old app
  if (progress < 0.5)
  {
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
  }
  else
  {
    // Otherwise fading in the new app
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);
  }

  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      CRGB color = DisplayManager.getLeds()[this->matrix->XY(i, j)];
      color.fadeToBlackBy(fadeValue);
      DisplayManager.getLeds()[this->matrix->XY(i, j)] = color;
    }
  }
}

void MatrixDisplayUi::slideTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int16_t x, y, x1, y1;
  switch (this->appAnimationDirection)
  {
  case SLIDE_UP:
    x = 0;
    y = -8 * progress;
    x1 = 0;
    y1 = y + 8;
    break;
  case SLIDE_DOWN:
    x = 0;
    y = 8 * progress;
    x1 = 0;
    y1 = y - 8;
    break;
  }
  // Invert animation if direction is reversed.
  int8_t dir = this->state.appTransitionDirection >= 0 ? 1 : -1;
  x *= dir;
  y *= dir;
  x1 *= dir;
  y1 *= dir;
  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, x, y, &gif1);
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, x1, y1, &gif2);
}

void MatrixDisplayUi::curtainTransition()
{
  CRGB *leds = DisplayManager.getLeds();
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int curtainWidth = (int)(16 * progress); // 16 ist die Hälfte der Matrix-Breite

  if (this->state.ticksSinceLastStateSwitch == 1 || this->state.ticksSinceLastStateSwitch == 0)
  {
    // Kopieren Sie die aktuelle App-Ansicht in ledsCopy
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
    for (int i = 0; i < 32; i++)
    {
      for (int j = 0; j < 8; j++)
      {
        ledsCopy[i + j * 32] = leds[this->matrix->XY(i, j)];
      }
    }
  }
  // Zeichnen Sie die neue App-Ansicht
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);

  // Anwenden des Vorhang-Effekts basierend auf dem Fortschritt
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      if ((i < (16 - curtainWidth)) || (i >= (16 + curtainWidth)))
      {
        leds[this->matrix->XY(i, j)] = ledsCopy[i + j * 32];
      }
    }
  }
}

void MatrixDisplayUi::zoomTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  float scale = 1.0;
  // If zooming out the old app
  if (progress < 0.5)
  {
    scale = 1 - progress * 2; // scale will change from 1.0 to 0.0
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
  }
  else
  {
    // Otherwise zooming in the new app
    scale = (progress - 0.5) * 2; // scale will change from 0.0 to 1.0
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);
  }

  // Copy the data to the temporary array ledsCopy
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      ledsCopy[i + j * 32] = DisplayManager.getLeds()[this->matrix->XY(i, j)];
    }
  }

  // Scale the data and copy back to the matrix
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      int iScaled = 16 + (i - 16) * scale;
      int jScaled = 4 + (j - 4) * scale;

      if (iScaled < 0)
        iScaled = 0;
      if (iScaled >= 32)
        iScaled = 31;
      if (jScaled < 0)
        jScaled = 0;
      if (jScaled >= 8)
        jScaled = 7;
      DisplayManager.getLeds()[this->matrix->XY(i, j)] = ledsCopy[iScaled + jScaled * 32];
    }
  }
}

void MatrixDisplayUi::rotateTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  float angle = progress * 2 * PI; // Rotate 360 degrees over the transition

  // Determine which app to draw
  if (progress < 0.5)
  {
    // Rotate out the old app (progress from 0 to 0.5)
    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
  }
  else
  {
    // Rotate in the new app (progress from 0.5 to 1.0)
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);
  }

  // Copy the data to the temporary array ledsCopy
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      ledsCopy[i + j * 32] = DisplayManager.getLeds()[this->matrix->XY(i, j)];
    }
  }

  // Rotate the data and copy back to the matrix
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      int iRotated = i;
      int jRotated = j;
      rotate(iRotated, jRotated, angle);

      if (iRotated < 0)
        iRotated = 0;
      if (iRotated >= 32)
        iRotated = 31;
      if (jRotated < 0)
        jRotated = 0;
      if (jRotated >= 8)
        jRotated = 7;

      DisplayManager.getLeds()[this->matrix->XY(i, j)] = ledsCopy[iRotated + jRotated * 32];
    }
  }
}

void MatrixDisplayUi::pixelateTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  // Draw the old app and copy to ledsCopy
  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      ledsCopy[i + j * 32] = DisplayManager.getLeds()[this->matrix->XY(i, j)];
    }
  }

  // Clear the screen and draw the new app
  this->matrix->clear();
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);

  // Apply the random pixel swap transition effect
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      // If the random number is greater than the progress, display the pixel from the old app
      if (random(255) > progress * 255)
      {
        DisplayManager.getLeds()[this->matrix->XY(i, j)] = ledsCopy[i + j * 32];
      }
      // Otherwise, keep the pixel from the new app
    }
  }
}

void MatrixDisplayUi::rippleTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  // Draw the old app and copy to ledsCopy
  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      ledsCopy[i + j * 32] = DisplayManager.getLeds()[this->matrix->XY(i, j)];
    }
  }

  // Clear the screen and draw the new app
  this->matrix->clear();
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);

  // Apply the checkerboard transition effect
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      // If the sum of i and j is an even number and the progress is less than 0.5, display the pixel from the old app
      if ((i + j) % 2 == 0 && progress < 0.5)
      {
        DisplayManager.getLeds()[this->matrix->XY(i, j)] = ledsCopy[i + j * 32];
      }
      // If the sum of i and j is an odd number and the progress is more than 0.5, display the pixel from the old app
      else if ((i + j) % 2 != 0 && progress >= 0.5)
      {
        DisplayManager.getLeds()[this->matrix->XY(i, j)] = ledsCopy[i + j * 32];
      }
      // Otherwise, keep the pixel from the new app
    }
  }
}

void MatrixDisplayUi::blinkTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  // Number of blinks during the transition
  int blinks = 3;

  // Calculate the current blink state (on or off) by considering the number of blinks and the progress
  bool blinkState = (int)(progress * blinks) % 2 == 0;

  // Depending on the blinkState and the progress, draw the old or the new app
  if (blinkState)
  {
    // If blinkState is true, draw the old app if progress is less than 0.5, otherwise draw the new app
    if (progress < 0.5)
    {
      (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);
    }
    else
    {
      (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);
    }
  }
  else
  {
    // If blinkState is false, clear the matrix (display off)
    this->matrix->clear();
  }
}

void MatrixDisplayUi::reloadTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;
  int visiblePixel;

  if (progress < 0.5)
  {

    (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);

    // Calculating pixel to be visible based on progress
    visiblePixel = 32 * (1.0 - (progress * 2));
    if (visiblePixel < 0)
      visiblePixel = 0;

    for (int i = visiblePixel; i < 32; i++)
    {
      for (int j = 0; j < 8; j++)
      {
        // Turning the pixels off to create a fly out effect
        DisplayManager.getLeds()[this->matrix->XY(i, j)] = CRGB::Black;
      }
    }
  }
  else
  {
    // Draw the new app and let the pixels fly in
    (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);

    // Calculating pixel to be visible based on progress
    visiblePixel = 32 * ((progress - 0.5) * 2);
    if (visiblePixel > 32)
      visiblePixel = 32;

    for (int i = visiblePixel; i < 32; i++)
    {
      for (int j = 0; j < 8; j++)
      {
        // Turning the pixels off to create a fly in effect
        DisplayManager.getLeds()[this->matrix->XY(i, j)] = CRGB::Black;
      }
    }
  }
}

void MatrixDisplayUi::crossfadeTransition()
{
  float progress = (float)this->state.ticksSinceLastStateSwitch / (float)this->ticksPerTransition;

  // Draw the old app
  (this->AppFunctions[this->state.currentApp])(this->matrix, &this->state, 0, 0, &gif1);

  // Copy the old app data to ledsCopy array
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      ledsCopy[i + j * 32] = DisplayManager.getLeds()[this->matrix->XY(i, j)];
    }
  }

  // Clear the matrix before drawing the new app
  this->matrix->fillScreen(0);

  // Draw the new app
  (this->AppFunctions[this->getnextAppNumber()])(this->matrix, &this->state, 0, 0, &gif2);

  // Linearly interpolate between old and new pixel colors based on the progress
  for (int i = 0; i < 32; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      CRGB pixelOld = ledsCopy[i + j * 32];
      CRGB pixelNew = DisplayManager.getLeds()[this->matrix->XY(i, j)];
      DisplayManager.getLeds()[this->matrix->XY(i, j)] = pixelOld.lerp8(pixelNew, progress * 255);
    }
  }
}

#ifdef DISPLAY_HUB75
// Four native apps rendered concurrently in 32x16 quadrants of the 64x32
// panel. Slot layout:
//   TL (0, 4)   → Time         (uses the classic TimeApp callback)
//   TR (32, 4)  → Temperature  (grid widget: icon + padded 2-digit °C)
//   BL (0, 20)  → Humidity     (grid widget: icon + padded 2-digit %)
//   BR (32, 20) → Outdoor      (grid widget: dynamic weather icon + °C)
//
// TimeApp is called directly because its internal `printText(centered=true)`
// already handles slot centering when x is 0 or 32 (see printText's
// slotBase logic in DisplayManager.cpp). The other three widgets are
// grid-native reimplementations rather than reuse of TempApp/HumApp/
// OutdoorApp because those apps render left-aligned and can't easily
// centre an icon+text pair inside a 32-wide slot without changing the
// Ulanzi rotation behaviour.
//
// The Y offsets 4 / 20 (rather than 0 / 16) place the text baseline at
// y=10 / y=26 — the vertical centre of each 32x16 quadrant. Icons at the
// same slot Y stay in the upper half of the quadrant.
//
// GifPlayer sharing: gif1 for TL/BL, gif2 for TR/BR. TimeApp uses gif1 for
// its optional background GIF; OutdoorApp uses whichever we hand it.
// Splitting across the two instances avoids the file-name cache in
// GifPlayer::playGif thrashing between two active animations per frame.

// Format a signed integer as a 2-digit label. Non-negatives get a leading
// "0" for single digits ("07" instead of "7"); negatives that are single-
// digit swap the leading zero for the minus sign ("-3" instead of "-03").
// Result always fits in the caller's buffer of size >= 4 (sign + 2 digits + NUL).
static void gridFormatInt2(int value, char *out)
{
    if (value < 0)
    {
        int abs = -value;
        if (abs < 10)
            snprintf(out, 4, "-%d", abs);       // "-3"
        else
            snprintf(out, 4, "%d", value);       // "-27" — sign in the number
    }
    else
    {
        snprintf(out, 4, "%02d", value);         // "07", "23"
    }
}

// Time widget: HH:MM only, centred in the 32-wide slot. No calendar box,
// no weekday bar — keeps the TL quadrant visually balanced with the other
// three widgets. Colon blinks once per second when TIME_FORMAT uses a
// space separator (mirrors TimeApp's behaviour for %H %M).
static void gridWidgetTime(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                           int16_t x, int16_t y, GifPlayer *gifPlayer)
{
    (void)state; (void)gifPlayer;
    // Force HH:MM regardless of TIME_FORMAT/TIME_MODE — the classic app has
    // %I/%p variants and seconds, but at 5-char width they wouldn't fit
    // neatly in 32px anyway.
    const bool blink = (TIME_FORMAT.length() >= 3 && TIME_FORMAT[2] == ' ');
    const char *fmt = (blink && (timer_time() % 2)) ? "%H %M" : "%H:%M";
    char buf[8];
    strftime(buf, sizeof(buf), fmt, timer_localtime());

    if (TIME_COLOR > 0)
        DisplayManager.setTextColor(TIME_COLOR);
    else
        DisplayManager.resetTextColor();

    const uint16_t textW = (uint16_t)getTextWidth(buf, 0);
    const int ox = x + (32 - textW) / 2;
    DisplayManager.setCursor(ox, y + 6);
    DisplayManager.matrixPrint(buf);
}

// Draw an 8x8 icon + text label horizontally centred inside a 32x16 slot.
// The icon+text pair together occupies (8 + gap + textWidth). The x offset
// is (32 - contentWidth) / 2, so the whole widget sits centred regardless
// of text length. Icon is drawn at slot-y-relative row 0..7 (top of the
// quadrant); text baseline lands at slotY + 6 (same as the classic apps).
// Font case defaults to 0 so getTextWidth matches the actual matrixPrint.
static void gridDrawIconText(FastLED_NeoMatrix *matrix,
                             const uint16_t *icon8x8,
                             int16_t slotX, int16_t slotY,
                             const char *text, uint32_t color)
{
    const uint16_t textW = (uint16_t)getTextWidth(text, 0);
    const int gap = 2;
    const int contentW = 8 + gap + textW;
    const int ox = slotX + (32 - contentW) / 2;
    matrix->drawRGBBitmap(ox, slotY, (uint16_t *)icon8x8, 8, 8);
    if (color > 0)
        DisplayManager.setTextColor(color);
    else
        DisplayManager.resetTextColor();
    DisplayManager.setCursor(ox + 8 + gap, slotY + 6);
    DisplayManager.matrixPrint(text);
}

// Load an outdoor weather icon from LittleFS for the current OUTDOOR_ICON
// slug, falling back to icon_234 (sun) when missing. Static cache handle
// keeps GifPlayer decode state across frames so animation doesn't restart
// (same pattern as OutdoorApp's caching in Apps.cpp).
static const char *gridOutdoorIconId(const String &slug)
{
    if (slug == "clear")        return "11201";
    if (slug == "partlycloudy") return "876";
    if (slug == "cloudy")       return "12246";
    if (slug == "rain")         return "72";
    if (slug == "sleet")        return "160";
    if (slug == "snow")         return "4702";
    if (slug == "storm")        return "11428";
    if (slug == "fog")          return "12196";
    return nullptr;
}

// Draw an outdoor weather icon into an 8x8 area at (x, y). Uses LittleFS
// icons; falls back to icon_234 if none matched. Caches file handle across
// calls so the GIF animation doesn't reset each frame.
static void gridDrawOutdoorIcon(FastLED_NeoMatrix *matrix, GifPlayer *gifPlayer,
                                int16_t x, int16_t y)
{
    static fs::File cachedIcon;
    static String cachedSlug = "";
    const char *iconId = gridOutdoorIconId(OUTDOOR_ICON);
    if (iconId != nullptr)
    {
        const String base = String("/ICONS/") + iconId;
        if (cachedSlug != OUTDOOR_ICON || !cachedIcon)
        {
            if (cachedIcon) cachedIcon.close();
            cachedSlug = OUTDOOR_ICON;
            if (LittleFS.exists(base + ".jpg"))
                cachedIcon = LittleFS.open(base + ".jpg");
            else if (LittleFS.exists(base + ".gif"))
                cachedIcon = LittleFS.open(base + ".gif");
        }
        if (cachedIcon)
        {
            const String name = cachedIcon.name();
            if (name.endsWith(".jpg"))
            {
                DisplayManager.drawJPG(x, y, cachedIcon);
                cachedIcon.seek(0);
            }
            else
            {
                gifPlayer->minFrameDelay = 500;
                gifPlayer->playGif(x, y, &cachedIcon);
            }
            return;
        }
    }
    matrix->drawRGBBitmap(x, y, (uint16_t *)icon_234, 8, 8);
}

// Same as gridDrawIconText but the icon-draw function is passed in so a
// widget with a dynamic icon (like Outdoor's weather-based GIF) can reuse
// the centering math without duplicating it.
static void gridDrawWidget(FastLED_NeoMatrix *matrix,
                           GifPlayer *gifPlayer,
                           int16_t slotX, int16_t slotY,
                           const char *text, uint32_t color,
                           void (*drawIcon)(FastLED_NeoMatrix *, GifPlayer *, int16_t, int16_t))
{
    const uint16_t textW = (uint16_t)getTextWidth(text, 0);
    const int gap = 2;
    const int contentW = 8 + gap + textW;
    const int ox = slotX + (32 - contentW) / 2;
    drawIcon(matrix, gifPlayer, ox, slotY);
    if (color > 0)
        DisplayManager.setTextColor(color);
    else
        DisplayManager.resetTextColor();
    DisplayManager.setCursor(ox + 8 + gap, slotY + 6);
    DisplayManager.matrixPrint(text);
}

// Temperature widget: 8x8 sun icon + "07°C" / "23°C" / "-3°C" / "-27°C".
static void gridWidgetTemp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                           int16_t x, int16_t y, GifPlayer *gifPlayer)
{
    (void)state; (void)gifPlayer;
    char num[4];
    if (IS_CELSIUS)
    {
        gridFormatInt2((int)CURRENT_TEMP, num);
    }
    else
    {
        int f = (int)((CURRENT_TEMP * 9.0 / 5.0) + 32.0);
        gridFormatInt2(f, num);
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%s%s", num, utf8ascii(IS_CELSIUS ? "°C" : "°F").c_str());
    gridDrawIconText(matrix, icon_234, x, y, buf, TEMP_COLOR);
}

// Humidity widget: 8x8 droplet icon + "07%" / "42%".
static void gridWidgetHum(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                          int16_t x, int16_t y, GifPlayer *gifPlayer)
{
    (void)state; (void)gifPlayer;
    char num[4];
    gridFormatInt2((int)CURRENT_HUM, num);
    char buf[6];
    snprintf(buf, sizeof(buf), "%s%%", num);
    gridDrawIconText(matrix, icon_2075, x, y, buf, HUM_COLOR);
}

// Outdoor widget: dynamic weather icon (from wttr.in slug) + padded temp.
// Draws nothing while OUTDOOR_TEMP_VALID is false so a stale 0°C never
// appears at boot; same behaviour as OutdoorApp.
static void gridWidgetOutdoor(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                              int16_t x, int16_t y, GifPlayer *gifPlayer)
{
    (void)state;
    if (!OUTDOOR_TEMP_VALID)
        return;
    char num[4];
    if (IS_CELSIUS)
    {
        gridFormatInt2((int)OUTDOOR_TEMP, num);
    }
    else
    {
        int f = (int)((OUTDOOR_TEMP * 9.0 / 5.0) + 32.0);
        gridFormatInt2(f, num);
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%s%s", num, utf8ascii(IS_CELSIUS ? "°C" : "°F").c_str());
    gridDrawWidget(matrix, gifPlayer, x, y, buf, TEMP_COLOR, gridDrawOutdoorIcon);
}

// UV widget: sun icon + UV index number (0..11+). Uses icon_234 (sun) as
// the icon since there is no dedicated UV icon. Shows "UV?" while
// OUTDOOR_UV is -1 (fresh boot before first fetch, or a wttr.in response
// without a UV field). Color scales with the index: green ≤2, yellow 3-5,
// orange 6-7, red 8-10, purple ≥11 — matches the WHO UV-index colour code.
static void gridWidgetUV(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                         int16_t x, int16_t y, GifPlayer *gifPlayer)
{
    (void)state; (void)gifPlayer;
    char buf[8];
    uint32_t color;
    if (OUTDOOR_UV < 0)
    {
        snprintf(buf, sizeof(buf), "UV?");
        color = 0x808080;  // grey while unknown
    }
    else
    {
        snprintf(buf, sizeof(buf), "UV%d", OUTDOOR_UV);
        if      (OUTDOOR_UV <= 2)  color = 0x00C800;  // green
        else if (OUTDOOR_UV <= 5)  color = 0xFFC800;  // yellow
        else if (OUTDOOR_UV <= 7)  color = 0xFF8000;  // orange
        else if (OUTDOOR_UV <= 10) color = 0xFF0000;  // red
        else                       color = 0xA000FF;  // purple (extreme)
    }
    gridDrawIconText(matrix, icon_234, x, y, buf, color);
}

struct GridSlot
{
  int16_t x, y;
  AppCallback callback;
  GifPlayer *gif;
};
static GridSlot gridSlots[4];
static bool gridInitialised = false;

// Fullscreen "big clock" page: shows the date at the top (WD DD.MM) and a
// large HH:MM below it, using Adafruit_GFX's built-in font at size=2 so each
// glyph is 12x16. The 5-char "HH:MM" is 60 px wide → fits 64 px with 2 px
// margin. Uses setTextSize on the matrix directly (bypasses the custom
// pixel-fonts used elsewhere) so the glyphs actually scale.
static void gridPageFullscreenClock(FastLED_NeoMatrix *matrix, GifPlayer *gifPlayer)
{
  (void)gifPlayer;
  matrix->fillScreen(0);

  // --- Date row (small, top) ---
  char date[16];
  struct tm *now = timer_localtime();
  const char *weekdays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  snprintf(date, sizeof(date), "%s %02d.%02d",
           weekdays[now->tm_wday], now->tm_mday, now->tm_mon + 1);
  const uint16_t dateW = (uint16_t)getTextWidth(date, 0);
  const int dateX = (64 - (int)dateW) / 2;
  if (DATE_COLOR > 0)
    DisplayManager.setTextColor(DATE_COLOR);
  else
    DisplayManager.resetTextColor();
  DisplayManager.setCursor(dateX, 6);
  DisplayManager.matrixPrint(date);

  // --- Time row (large, centred vertically in the lower half) ---
  const char *fmt = "%H:%M";
  if (TIME_FORMAT.length() >= 3 && TIME_FORMAT[2] == ' ')
    fmt = (timer_time() % 2) ? "%H %M" : "%H:%M";
  char clock[8];
  strftime(clock, sizeof(clock), fmt, timer_localtime());

  if (TIME_COLOR > 0)
    matrix->setTextColor(TIME_COLOR);
  else
    matrix->setTextColor(0xFFFF);  // 16-bit white (matrix uses 565)
  matrix->setTextSize(2);
  // "HH:MM" at size 2 = 5 * 6 * 2 - 1 * 2 = 58 px wide, 14 px tall.
  // Center: x = (64-58)/2 = 3; y aligned so text baseline sits at row 24.
  const int clockX = (64 - 58) / 2;
  const int clockY = 14;
  matrix->setCursor(clockX, clockY);
  matrix->print(clock);
  matrix->setTextSize(1);
}

// Page rotator: alternate between fullscreen clock and 2x2 grid every 5 s.
// A 500 ms roll-down transition runs at the end of each page window:
// the incoming page enters from the top and pushes the outgoing page
// downwards off the panel.
enum GridPage { PAGE_CLOCK = 0, PAGE_GRID = 1 };
static constexpr uint32_t PAGE_DURATION_MS = 5000;
static constexpr uint32_t TRANSITION_MS    = 500;

// Renders a single page into leds[] via the matrix wrapper. matrix->clear()
// is called first so the page starts on a blank canvas. Kept as a free
// function (not a MatrixDisplayUi method) so it can be called twice in a
// row during transitions without inheriting extra state.
static void renderPage(GridPage page, FastLED_NeoMatrix *matrix,
                       MatrixDisplayUiState *state)
{
  matrix->clear();
  if (page == PAGE_CLOCK)
  {
    gridPageFullscreenClock(matrix, &gif1);
  }
  else
  {
    for (int i = 0; i < 4; i++)
    {
      const GridSlot &s = gridSlots[i];
      s.callback(matrix, state, s.x, s.y, s.gif);
    }
  }
}

// Called from DisplayManager_::setup() once the app callbacks are known.
// Kept here rather than in DisplayManager.cpp so all grid layout constants
// live in one place.
void MatrixDisplayUi_initGrid()
{
  // Slot layout for the "grid" page:
  //   TL: UV index   (the fullscreen page shows the clock — no need for
  //                   a second clock in the corner)
  //   TR: indoor temperature
  //   BL: humidity
  //   BR: outdoor temperature (dynamic weather icon)
  gridSlots[0] = {0,  4,  gridWidgetUV,      &gif1};
  gridSlots[1] = {32, 4,  gridWidgetTemp,    &gif2};
  gridSlots[2] = {0,  20, gridWidgetHum,     &gif1};
  gridSlots[3] = {32, 20, gridWidgetOutdoor, &gif2};
  gridInitialised = true;
}

void MatrixDisplayUi::drawGrid()
{
  if (!gridInitialised)
    return;

  // Timing: which page owns this instant, and how deep are we into it?
  const uint32_t t = millis();
  const uint32_t cyc = t / PAGE_DURATION_MS;
  const uint32_t posInPage = t % PAGE_DURATION_MS;
  const GridPage nowPage = (cyc % 2 == 0) ? PAGE_CLOCK : PAGE_GRID;

  // Transition runs during the LAST `TRANSITION_MS` of each page window:
  // the incoming page (nextPage) slides in from the top and pushes the
  // outgoing page (nowPage) downwards. Once we cross the page boundary the
  // roles swap and rendering falls back to plain single-page mode.
  const bool inTransition = (posInPage >= PAGE_DURATION_MS - TRANSITION_MS);
  if (!inTransition)
  {
    // Steady state — render the current page directly into leds[].
    renderPage(nowPage, this->matrix, &this->state);
    CURRENT_APP = (nowPage == PAGE_CLOCK) ? "Clock" : "Grid";
    currentCustomApp = "";
    return;
  }

  // Snapshot the outgoing page into a static buffer, then render the
  // incoming page into leds[]. Both buffers are static so we don't pay
  // heap allocation every transition frame.
  //
  // 2 * 64*32 * sizeof(CRGB) = 12 KB static — the panel is 64x32 on HUB75
  // (MATRIX_WIDTH * MATRIX_HEIGHT = 2048). Guarded by DISPLAY_HUB75 already
  // via the surrounding #ifdef so no waste on Ulanzi.
  static CRGB pageOutBuf[64 * 32];
  static CRGB pageInBuf[64 * 32];
  CRGB *leds = DisplayManager.getLeds();

  renderPage(nowPage, this->matrix, &this->state);
  memcpy(pageOutBuf, leds, sizeof(pageOutBuf));

  const GridPage nextPage = (nowPage == PAGE_CLOCK) ? PAGE_GRID : PAGE_CLOCK;
  renderPage(nextPage, this->matrix, &this->state);
  memcpy(pageInBuf, leds, sizeof(pageInBuf));

  // Compute how many rows of the incoming page are visible from the top.
  // Progress 0..1 mapped to 0..32 rows.
  const uint32_t transElapsed = posInPage - (PAGE_DURATION_MS - TRANSITION_MS);
  const int offset = (int)(((uint32_t)32 * transElapsed) / TRANSITION_MS);
  const int clampedOffset = offset > 32 ? 32 : offset;

  // Compose: leds[y*W + x] = pageInBuf[(32 - offset + y) * W + x]  if y < offset
  //                        = pageOutBuf[(y - offset) * W + x]      otherwise
  // Meaning: the incoming page enters from the top (its bottom rows are the
  // first visible), and the outgoing page slides downwards until it falls
  // off the bottom edge.
  constexpr int W = 64;
  constexpr int H = 32;
  for (int y = 0; y < H; y++)
  {
    if (y < clampedOffset)
    {
      const int srcY = (H - clampedOffset) + y;
      memcpy(&leds[y * W], &pageInBuf[srcY * W], W * sizeof(CRGB));
    }
    else
    {
      const int srcY = y - clampedOffset;
      memcpy(&leds[y * W], &pageOutBuf[srcY * W], W * sizeof(CRGB));
    }
  }

  CURRENT_APP = (nextPage == PAGE_CLOCK) ? "Clock" : "Grid";
  currentCustomApp = "";
}
#endif

