/*
  NAME:
  gbj_appled_esp

  DESCRIPTION:
  Library manages an LED, usually BUILT-IN led for signalling purposes.
  - Library accepts different wiring of built-in led on various platforms.

  LICENSE:
  This program is free software; you can redistribute it and/or modify
  it under the terms of the license GNU GPL v3
  http://www.gnu.org/licenses/gpl-3.0.html (related to original code) and MIT
  License (MIT) for added code.

  CREDENTIALS:
  Author: Libor Gabaj
  GitHub: https://github.com/mrkaleArduinoLib/gbj_appled_esp.git
*/
#ifndef GBJ_APPLED_ESP_H
#define GBJ_APPLED_ESP_H

#include <Arduino.h>
#if defined(ESP8266)
  #define USING_TIM_DIV256 true
  #include <ESP8266TimerInterrupt.h>
#elif defined(ESP32)
  #include <ESP32_New_TimerInterrupt.h>
#else
  #error !!! Only ESP8266/ESP32 are supported !!!
#endif

#undef SERIAL_PREFIX
#define SERIAL_PREFIX "gbj_appled_esp"

class gbj_appled_esp
{
public:
#if defined(ESP8266)
  typedef void (*func_t)();
#elif defined(ESP32)
  typedef bool (*func_t)(void *);
#endif

  /*
    Constructor

    DESCRIPTION:
    Constructor creates the class instance object and sets operational
    parameters.

    PARAMETERS:
    pinLed - Number of GPIO pin of the microcontroller managing an led.
      - Data type: non-negative integer
      - Default value: LED_BUILTIN (GPIO depends on platform)
      - Limited range: 0 ~ 255
    reverse - Flag whether the led works in reverse mode, i.e., active low.
      - Data type: boolean
      - Default value: true (preferrably for ESP8266, ESP32)
      - Limited range: true, false
    block - Flag whether the GPIO pin for led is not controlled alltogether. It
    is suitable at ESP8266-01, where builtin led is connected to serial TX pin,
    so that the using led and serial monitor at once is not possible.
      - Data type: boolean
      - Default value: false (preferrably for ESP8266, ESP32)
      - Limited range: true, false

    RETURN: object
  */
  inline gbj_appled_esp(byte pinLed = LED_BUILTIN,
                        bool reverse = true,
                        bool block = false)
  {
    pin_ = pinLed;
    blocked_ = block;
    if (reverse)
    {
      ON = LOW;
      OFF = HIGH;
    }
    else
    {
      ON = HIGH;
      OFF = LOW;
    }
#if defined(ESP8266)
    timer_ = new ESP8266Timer();
#elif defined(ESP32)
    timer_ = new ESP32Timer(0);
#endif
  }

  /*
    Initialization

    DESCRIPTION:
    The method should be called in the SETUP section of a sketch and sets up
    the LED.

    PARAMETERS:
    enabled - Flag whether the LED is enabled in a sketch. Disabled LED is
      ignored entirely.
      - Data type: boolean
      - Default value: true
      - Limited range: true, false

    RETURN: None
  */
#if defined(ESP8266)
  inline void begin(void (*isr)(), bool enabled = true)
#elif defined(ESP32)
  inline void begin(bool (*isr)(void *timer), bool enabled = true)
#endif
  {
    isr_ = isr;
    if (isFree())
    {
      pinMode(pin_, OUTPUT);
    }
    enabled ? enable() : disable();
  }
  inline void block() { blocked_ = true; }
  inline void free() { blocked_ = false; }
  inline void enable()
  {
    enabled_ = true;
    switch (mode_)
    {
      case Modus::MODE_ON:
        on();
        break;

      case Modus::MODE_BLINK:
        blink();
        break;

      case Modus::MODE_HURRY:
        blinkHurry();
        break;

      case Modus::MODE_FAST:
        blinkFast();
        break;

      case Modus::MODE_PATTERN:
        blinkPattern(blinks_);
        break;

      default:
        off();
        break;
    }
  }
  inline void disable()
  {
    enabled_ = false;
    off();
  }
  inline void on()
  {
    if (isEnabled())
    {
      if (isFree())
      {
        if (init_)
        {
          timer_->stopTimer();
        }
        digitalWrite(pin_, ON);
      }
      mode_ = Modus::MODE_ON;
    }
    else
    {
      off();
    }
  }
  inline void off()
  {
    if (isFree())
    {
      if (init_)
      {
        timer_->stopTimer();
      }
      digitalWrite(pin_, OFF);
    }
  }
  inline void toggle()
  {
    if (isEnabled())
    {
      if (isFree())
      {
        digitalWrite(pin_, digitalRead(pin_) ^ 1);
      }
    }
    else
    {
      off();
    }
  }
  inline void blink()
  {
    blinkLed(Timing::PERIOD_NORMAL);
    mode_ = Modus::MODE_BLINK;
  }
  inline void blinkHurry()
  {
    blinkLed(Timing::PERIOD_HURRY);
    mode_ = Modus::MODE_HURRY;
  }
  inline void blinkFast()
  {
    blinkLed(Timing::PERIOD_FAST);
    mode_ = Modus::MODE_FAST;
  }
  inline void blinkPattern(byte blinks = 3)
  {
    blinks_ = constrain(blinks, 2, 255);
    if (!isPatterned())
    {
      blinkPatternRestart();
    }
  }

  /*
    Processing.

    DESCRIPTION:
    The method should be called in an application sketch loop.
    It processes main functionality and is controlled by the internal timer.

    PARAMETERS: None

    RETURN: none
  */
  inline void run()
  {
    if (isPatterned())
    {
      if (counter_)
      {
        if (isLit())
        {
          counter_--;
        }
        toggle();
      }
      else
      {
        if (halted_)
        {
          blinkPatternRestart();
        }
        else
        {
          digitalWrite(pin_, OFF);
          setPeriod(Timing::PERIOD_NORMAL);
          halted_ = true;
        }
      }
    }
    else
    {
      toggle();
    }
  }

  // Getters
  inline bool isBlocked() { return blocked_; }
  inline bool isFree() { return !isBlocked(); }
  inline bool isEnabled() { return enabled_; }
  inline bool isDisabled() { return !isEnabled(); }
  inline bool isLit() { return isBlocked() ? false : digitalRead(pin_) == ON; }
  inline bool isDim() { return isBlocked() ? false : digitalRead(pin_) == OFF; }
  inline bool isOff() { return isDim() && !isBlinking(); }
  inline bool isOn()
  {
    return isBlocked() ? false : isEnabled() && mode_ == Modus::MODE_ON;
  }
  inline bool isBlinking() { return isBlocked() ? false : isEnabled(); }
  inline bool isPatterned()
  {
    return isBlocked() ? false : isEnabled() && mode_ == Modus::MODE_PATTERN;
  }
  inline String getStatusOn() { return "ON"; }
  inline String getStatusOff() { return "OFF"; }
  inline String getStatus()
  {
    return isEnabled() ? getStatusOn() : getStatusOff();
  }

  // Setters
  inline void setAbility(bool enabled) { enabled ? enable() : disable(); }

private:
  // Blinking periods in microseconds
  enum Timing : unsigned long
  {
    PERIOD_NORMAL = 500 * 1000,
    PERIOD_HURRY = 200 * 1000,
    PERIOD_FAST = 100 * 1000,
  };
  enum Modus
  {
    MODE_OFF,
    MODE_ON,
    MODE_BLINK,
    MODE_HURRY,
    MODE_FAST,
    MODE_PATTERN,
  };
#if defined(ESP8266)
  ESP8266Timer *timer_;
#elif defined(ESP32)
  ESP32Timer *timer_;
#endif
  func_t isr_;
  Modus mode_;
  byte ON, OFF;
  byte pin_, blinks_, counter_;
  bool blocked_, enabled_, halted_, init_;

  inline void setPeriod(unsigned long period)
  {
#if defined(ESP8266)
    timer_->setInterval(period, isr_);
    timer_->restartTimer();
#elif defined(ESP32)
    timer_->setInterval(period, isr_);
    timer_->restartTimer();
    init_ = true;
#endif
  }
  inline void blinkLed(unsigned long period)
  {
    if (isEnabled())
    {
      digitalWrite(pin_, ON);
      setPeriod(period);
      halted_ = false;
    }
    else
    {
      off();
    }
  }
  inline void blinkPatternRestart()
  {
    blinkHurry();
    mode_ = Modus::MODE_PATTERN;
    counter_ = blinks_;
  }
};

#endif
