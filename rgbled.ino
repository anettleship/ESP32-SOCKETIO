void setupPixels() {

  hue[USERLED] = 0;
  hue[REMOTELED] = 0;
  saturation[USERLED] = 0;
  saturation[REMOTELED] = 0;
  value[USERLED] = 0;
  value[REMOTELED] = 0;

  FastLED.addLeds<WS2812, WS2812PIN, RGB>(leds, NUMPIXELS);
  fill_solid(leds, NUMPIXELS, CRGB(0, 0, 0));
  FastLED.show();
  delay(1000);

  blinkRGB();

}

void rgbLedHandler() {
  unsigned long millisCheck = millis();
  if (millisCheck - prevPixelMillis > PIXELUPDATETIME) {
    if (isSelectingColour == true) {
      cycleHue(USERLED);
    }
    prevPixelMillis = millisCheck;
    if (ledChanged[USERLED]) {
      ledChanged[USERLED] = false;
      saturation[USERLED] = 255;
      value[USERLED] = 255;
      leds[USERLED] = CHSV(hue[USERLED], saturation[USERLED], value[USERLED]);
      FastLED.show();
    }
    if (ledChanged[REMOTELED]) {
      ledChanged[REMOTELED] = false;
      saturation[REMOTELED] = 255;
      //  value[REMOTELED] = 255;
      leds[REMOTELED] = CHSV(hue[REMOTELED], saturation[REMOTELED], value[REMOTELED]);
      FastLED.show();
    }
    //long fade on remote led
    longFadeHandler();
  }
  //updating every 5 seconds to make sure the leds dont lose their colours
  if (millisCheck - prevlongPixelMillis > PIXELUPDATETIMELONG) {
    prevlongPixelMillis = millisCheck;
    FastLED.show();
  }
  //short fade on remote led receive or user led trigger
  fadeRGBHandler();
}

void cycleHue(int led) {
  if (hue[led] < 255) {
    hue[led]++;
  } else {
    hue[led] = 0;
  }
  ledChanged[led] = true;
}

uint16_t getUserHue() {
  return hue[USERLED];
}


void blinkRGB() {
  leds[0] = CHSV(1, 255, 255);
  leds[1] = CHSV(127, 255, 255);
  FastLED.show();
  delay(100);
  leds[0] = CHSV(1, 0, 0);
  leds[1] = CHSV(127, 0, 0);
  FastLED.show();
}

void fadeRGB(int led) {
  if (readyToFadeRGB[led] == false) {
    readyToFadeRGB[led] = true;
  }
}

void fadeRGBHandler() {
  for (byte i = 0; i < NUMPIXELS; i++) {
    if (readyToFadeRGB[i] == true && isFadingRGB[i] == false) {
      ledChanged[i] = true;
      isFadingRGB[i] = true;
      fadeTimeRGB[i] = millis();
      value[i] = 255;
    }
    if (millis() - fadeTimeRGB[i] > RGBFADEMILLIS && isFadingRGB[i] == true) {
      fadeTimeRGB[i] = millis();
      if (value[i] > RGBLEDPWMSTART) {
        value[i]--;
        leds[i] = CHSV(hue[i], saturation[i], value[i]);
        FastLED.show();
      } else {
        isFadingRGB[i] = false;
        readyToFadeRGB[i] = false;
      }
    }
  }
}

void startLongFade() {
  isLongFade = true;
  longFadeMinutes = LONGFADEMINUTESMAX;
  prevLongFadeVal = 0;
}

void longFadeHandler() {
  if (isLongFade) {
    if (millis() - prevLongFadeMillis > LONGFADECHECKMILLIS) {
      prevLongFadeMillis = millis();
      longFadeMinutes--;
      unsigned long currLongFadeVal = longFadeMinutes / (LONGFADEMINUTESMAX / RGBLEDPWMSTART);
      if (currLongFadeVal != prevLongFadeVal) {
        prevLongFadeVal = currLongFadeVal;
        currLongFadeVal = currLongFadeVal - 1;
        value[REMOTELED] = (byte)currLongFadeVal;
        ledChanged[REMOTELED] = true;
        Serial.println(value[REMOTELED]);
      }
      if (longFadeMinutes <= 0) {
        isLongFade = false;
      }
    }
  }
}
