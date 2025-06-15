#include "RTClib.h"
#include "Keypad.h"
#include "DHT.h"
#include "Adafruit_SSD1306.h"
#include <util/delay.h>

#define analog_reference 0
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define LDR_PIN A0
#define BUZZER1_PIN 11
#define AC_PIN 12
#define COFFEE_PIN 13
#define LIGHT_PIN 23
#define DHT_PIN 10

#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A',},
  {'4', '5', '6', 'B',},
  {'7', '8', '9', 'C',},
  {'*', '0', '#', 'D',}
};
uint8_t colPins[COLS] = {5, 4, 3, 2};
uint8_t rowPins[ROWS] = {9, 8, 7, 6};


enum MenuState {
  LOCKED_SCREEN,
  MAIN_MENU,
  VIEW_STATUS,
  SET_TIMERS_APPLIANCE_SELECT,
  SET_TIMER_OPTIONS,
  SET_TIMER_INPUT_ON_HH,
  SET_TIMER_INPUT_ON_MM,
  SET_TIMER_INPUT_OFF_HH,
  SET_TIMER_INPUT_OFF_MM,
  MANUAL_CONTROL_SELECT,
  SET_ALARM_OPTIONS,
  SET_WAKE_ALARM_INPUT_HH,
  SET_WAKE_ALARM_INPUT_MM,
  ALARM_IS_SOUNDING
};
MenuState currentMenu = LOCKED_SCREEN;

const String MASTER_PASSCODE = "1234";
String enteredPasscode = "";
const byte PASSCODE_LENGTH = 4;

int currentMainMenuOption = 0;
int currentApplianceSelectOption = 0;
int currentTimerOptionsOption = 0;
int currentManualControlOption = 0;
int currentWakeAlarmMenuOption = 0;

const int AC_APPLIANCE = 0;
const int COFFEE_APPLIANCE = 1;
const int LIGHT_APPLIANCE = 2;
const int NUM_APPLIANCES = 3;
String applianceNames[NUM_APPLIANCES] = {"AC", "Coffee", "Lights"};
bool applianceStates[NUM_APPLIANCES] = {false, false, false};

struct TimerSetting {
  bool enabled = false;
  uint8_t onHour = 0;
  uint8_t onMinute = 0;
  uint8_t offHour = 0;
  uint8_t offMinute = 0;
};
TimerSetting applianceTimers[NUM_APPLIANCES];
int selectedApplianceForTimer = 0;

RTC_DS1307 rtc;
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
DHT dht(DHT_PIN, DHT22);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct AlarmSetting {
  bool enabled = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  bool sounding = false;
};
AlarmSetting wakeUpAlarm;

const unsigned long ALARM_SOUND_PULSE_DURATION = 300;
const unsigned long ALARM_SOUND_PULSE_GAP = 300;
const unsigned long ALARM_PULSE_DURATION_LOOPS = ALARM_SOUND_PULSE_DURATION / 50;
const unsigned long ALARM_PULSE_GAP_LOOPS = ALARM_SOUND_PULSE_GAP / 50;
unsigned long lastAlarmActionLoopIteration = 0;
uint8_t lastAlarmTriggerMinute = 255;
uint8_t lastAlarmTriggerHour = 255;


char input[5];
byte inputIndex = 0;

float temperature = 0.0;
float humidity = 0.0;
int ldrValue = 0;
uint32_t lastSensorReadUnixTime = 0;

unsigned long main_loop_iterations = 0;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void displayLockScreen();
void displayMainMenu();
void displayViewStatus(DateTime now);
void displaySetTimersApplianceSelect();
void displaySetTimerOptions();
void displaySetTimerInput(String prompt);
void displayManualControlSelect();
void displaySetWakeAlarmOptions();
void displaySetWakeAlarmInput(String prompt);
void displayAlarmIsSoundingScreen();


void C_digitalWrite(uint8_t pin, uint8_t val)
{
  uint8_t timer = digitalPinToTimer(pin);
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t *out;

  if (port == NOT_A_PIN){
    return;
  }

  if (timer != NOT_ON_TIMER){
     switch (timer)
     {
       case TIMER1A:   cbi(TCCR1A, COM1A1);    break;
       case TIMER1B:   cbi(TCCR1A, COM1B1);    break;
       case TIMER0A:   cbi(TCCR0A, COM0A1);    break;
       case TIMER0B:   cbi(TCCR0A, COM0B1);    break;
       case TIMER2A:   cbi(TCCR2A, COM2A1);    break;
       case TIMER2B:   cbi(TCCR2A, COM2B1);    break;
     }
  }

  out = portOutputRegister(port);

  uint8_t oldSREG = SREG;
  cli();

  if (val == LOW) {
    *out &= ~bit;
  } else {
    *out |= bit;
  }

  SREG = oldSREG;
}

static void C_turnOffPWM(uint8_t timer)
{
  switch (timer)
  {
    case TIMER1A:   cbi(TCCR1A, COM1A1);    break;
    case TIMER1B:   cbi(TCCR1A, COM1B1);    break;
    case TIMER0A:   cbi(TCCR0A, COM0A1);    break;
    case TIMER0B:   cbi(TCCR0A, COM0B1);    break;
    case TIMER2A:   cbi(TCCR2A, COM2A1);    break;
    case TIMER2B:   cbi(TCCR2A, COM2B1);    break;
  }
}


void C_pinMode(uint8_t pin, uint8_t mode)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t *reg, *out;

  if (port == NOT_A_PIN) return;

  reg = portModeRegister(port);
  out = portOutputRegister(port);

  if (mode == INPUT) {
        uint8_t oldSREG = SREG;
        cli();
        *reg &= ~bit;
        *out &= ~bit;
        SREG = oldSREG;
  } else if (mode == INPUT_PULLUP) {
        uint8_t oldSREG = SREG;
        cli();
        *reg &= ~bit;
        *out |= bit;
        SREG = oldSREG;
  } else {
        uint8_t oldSREG = SREG;
        cli();
        *reg |= bit;
        SREG = oldSREG;
  }
}

int C_analogRead(uint8_t pin)
{
  if (pin >= 14) pin -= 14;

  ADMUX = (analog_reference << 6) | (pin & 0x07);

  sbi(ADCSRA, ADSC);

  while (bit_is_set(ADCSRA, ADSC));

  return ADC;
}

int C_digitalRead(uint8_t pin)
{
        uint8_t timer = digitalPinToTimer(pin);
        uint8_t bit = digitalPinToBitMask(pin);
        uint8_t port = digitalPinToPort(pin);

        if (port == NOT_A_PIN) return LOW;

        if (timer != NOT_ON_TIMER) C_turnOffPWM(timer);

        if (*portInputRegister(port) & bit) return HIGH;
        return LOW;
}

void C_noTone(uint8_t pin) {
    C_digitalWrite(pin, LOW);
}

void C_tone(uint8_t pin, unsigned int frequency) {
    (void)frequency;
    C_digitalWrite(pin, HIGH);
}

void setApplianceState(int applianceIndex, bool state) {
  if (applianceIndex < 0 || applianceIndex >= NUM_APPLIANCES) return;
  applianceStates[applianceIndex] = state;
  int pinToControl = -1;
  switch (applianceIndex) {
    case AC_APPLIANCE:      pinToControl = AC_PIN;      break;
    case COFFEE_APPLIANCE:  pinToControl = COFFEE_PIN;  break;
    case LIGHT_APPLIANCE:   pinToControl = LIGHT_PIN;   break;
  }
  if (pinToControl != -1) {
    C_digitalWrite(pinToControl, state ? HIGH : LOW);
  }
}

void setup() {
  rtc.begin();
  dht.begin();
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  display.clearDisplay();
  display.display();

  C_pinMode(LDR_PIN, INPUT);
  C_pinMode(BUZZER1_PIN, OUTPUT);
  C_pinMode(AC_PIN, OUTPUT);
  C_pinMode(COFFEE_PIN, OUTPUT);
  C_pinMode(LIGHT_PIN, OUTPUT);

  C_digitalWrite(AC_PIN, LOW);
  C_digitalWrite(COFFEE_PIN, LOW);
  C_digitalWrite(LIGHT_PIN, LOW);
  C_noTone(BUZZER1_PIN);

  currentMenu = LOCKED_SCREEN;
}


void loop() {
  main_loop_iterations++;

  DateTime now = rtc.now();
  char key = keypad.getKey();

  if (now.unixtime() - lastSensorReadUnixTime >= 2) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    ldrValue = C_analogRead(LDR_PIN);
    lastSensorReadUnixTime = now.unixtime();
  }

  checkTimers(now);
  checkWakeUpAlarm(now);
  manageAlarmSound();

  if (key) {
    if (currentMenu == ALARM_IS_SOUNDING) {
      handleAlarmSoundingInput(key);
    } else {
      handleKeyInput(key, now);
    }
  }

  updateDisplay(now);
  _delay_ms(50);
}

void handleKeyInput(char key, DateTime now) {
  switch (currentMenu) {
    case LOCKED_SCREEN:
      handleLockScreenInput(key);
      break;
    case MAIN_MENU:
      handleMainMenuInput(key);
      break;
    case VIEW_STATUS:
      if (key == 'D') currentMenu = MAIN_MENU;
      break;
    case SET_TIMERS_APPLIANCE_SELECT:
      handleSetTimersApplianceSelectInput(key);
      break;
    case SET_TIMER_OPTIONS:
      handleSetTimerOptionsInput(key);
      break;
    case SET_TIMER_INPUT_ON_HH:
    case SET_TIMER_INPUT_ON_MM:
    case SET_TIMER_INPUT_OFF_HH:
    case SET_TIMER_INPUT_OFF_MM:
      handleTimerTimeInput(key);
      break;
    case MANUAL_CONTROL_SELECT:
      handleManualControlInput(key);
      break;
    case SET_ALARM_OPTIONS:
      handleSetWakeAlarmOptionsInput(key);
      break;
    case SET_WAKE_ALARM_INPUT_HH:
    case SET_WAKE_ALARM_INPUT_MM:
      handleWakeAlarmTimeInput(key);
      break;
    case ALARM_IS_SOUNDING:
      break;
  }
}

void handleLockScreenInput(char key) {
  if (isdigit(key)) {
    if (enteredPasscode.length() < PASSCODE_LENGTH) {
      enteredPasscode += key;
    }
  } else if (key == 'D') {
    if (enteredPasscode.length() > 0) {
      enteredPasscode.remove(enteredPasscode.length() - 1);
    }
  } else if (key == 'C') {
    if (enteredPasscode == MASTER_PASSCODE) {
      currentMenu = MAIN_MENU;
      enteredPasscode = "";
    } else {
      enteredPasscode = "";
    }
  }
}

void handleMainMenuInput(char key) {
  String menuItems[] = {"View Status", "Set Timers", "Set Alarm", "Manual Control"};
  int numItems = sizeof(menuItems) / sizeof(String);

  if (key == 'A') {
    currentMainMenuOption = (currentMainMenuOption - 1 + numItems) % numItems;
  } else if (key == 'B') {
    currentMainMenuOption = (currentMainMenuOption + 1) % numItems;
  } else if (key == 'C') {
    switch (currentMainMenuOption) {
      case 0: currentMenu = VIEW_STATUS; break;
      case 1: currentMenu = SET_TIMERS_APPLIANCE_SELECT; currentApplianceSelectOption = 0; break;
      case 2: currentMenu = SET_ALARM_OPTIONS; currentWakeAlarmMenuOption = 0; break;
      case 3: currentMenu = MANUAL_CONTROL_SELECT; currentManualControlOption = 0; break;
    }
  } else if (key == 'D') {
    currentMenu = LOCKED_SCREEN;
    enteredPasscode = "";
  }
}

void handleSetTimersApplianceSelectInput(char key) {
  int numItems = NUM_APPLIANCES;
  if (key == 'A') {
    currentApplianceSelectOption = (currentApplianceSelectOption - 1 + numItems) % numItems;
  } else if (key == 'B') {
    currentApplianceSelectOption = (currentApplianceSelectOption + 1) % numItems;
  } else if (key == 'C') {
    selectedApplianceForTimer = currentApplianceSelectOption;
    currentMenu = SET_TIMER_OPTIONS;
    currentTimerOptionsOption = 0;
  } else if (key == 'D') {
    currentMenu = MAIN_MENU;
  }
}

void handleSetTimerOptionsInput(char key) {
  String options[] = {"Set ON Time", "Set OFF Time", "Enable Timer"};
  int numItems = sizeof(options) / sizeof(String);

  if (key == 'A') {
    currentTimerOptionsOption = (currentTimerOptionsOption - 1 + numItems) % numItems;
  } else if (key == 'B') {
    currentTimerOptionsOption = (currentTimerOptionsOption + 1) % numItems;
  } else if (key == 'C') {
    input[0] = '\0';
    inputIndex = 0;
    switch (currentTimerOptionsOption) {
      case 0: currentMenu = SET_TIMER_INPUT_ON_HH; break;
      case 1: currentMenu = SET_TIMER_INPUT_OFF_HH; break;
      case 2:
        applianceTimers[selectedApplianceForTimer].enabled = !applianceTimers[selectedApplianceForTimer].enabled;
        currentMenu = SET_TIMER_OPTIONS;
        break;
    }
  } else if (key == 'D') {
    currentMenu = SET_TIMERS_APPLIANCE_SELECT;
  }
}

void handleTimerTimeInput(char key) {
  bool isHourInput = (currentMenu == SET_TIMER_INPUT_ON_HH || currentMenu == SET_TIMER_INPUT_OFF_HH);
  bool isMinuteInput = (currentMenu == SET_TIMER_INPUT_ON_MM || currentMenu == SET_TIMER_INPUT_OFF_MM);

  if (isdigit(key)) {
    if (inputIndex < 2) {
      input[inputIndex++] = key;
      input[inputIndex] = '\0';
    }
  } else if (key == 'D') {
    if (inputIndex > 0) {
      inputIndex--;
      input[inputIndex] = '\0';
    } else {
      if (isHourInput) {
        currentMenu = SET_TIMER_OPTIONS;
      } else if (isMinuteInput) {
        if (currentMenu == SET_TIMER_INPUT_ON_MM) currentMenu = SET_TIMER_INPUT_ON_HH;
        else if (currentMenu == SET_TIMER_INPUT_OFF_MM) currentMenu = SET_TIMER_INPUT_OFF_HH;
      }
    }
  } else if (key == 'C') {
    if (inputIndex == 2) {
      int val = atoi(input);
      bool validationSuccessful = false;

      if (isHourInput) {
        if (val >= 0 && val <= 23) {
          if (currentMenu == SET_TIMER_INPUT_ON_HH) {
            applianceTimers[selectedApplianceForTimer].onHour = val;
            currentMenu = SET_TIMER_INPUT_ON_MM;
          } else {
            applianceTimers[selectedApplianceForTimer].offHour = val;
            currentMenu = SET_TIMER_INPUT_OFF_MM;
          }
          validationSuccessful = true;
        }
      } else if (isMinuteInput) {
        if (val >= 0 && val <= 59) {
          if (currentMenu == SET_TIMER_INPUT_ON_MM) {
            applianceTimers[selectedApplianceForTimer].onMinute = val;
          } else {
            applianceTimers[selectedApplianceForTimer].offMinute = val;
          }
          validationSuccessful = true;
          currentMenu = SET_TIMER_OPTIONS;
        }
      }
      if (!isHourInput || !validationSuccessful || currentMenu == SET_TIMER_OPTIONS) {
         input[0] = '\0';
         inputIndex = 0;
      } else if (isHourInput && validationSuccessful) {
         input[0] = '\0';
         inputIndex = 0;
      }

    } else {
      input[0] = '\0';
      inputIndex = 0;
    }
  }
}

void handleManualControlInput(char key) {
  int numItems = NUM_APPLIANCES;
  if (key == 'A') {
    currentManualControlOption = (currentManualControlOption - 1 + numItems) % numItems;
  } else if (key == 'B') {
    currentManualControlOption = (currentManualControlOption + 1) % numItems;
  } else if (key == 'C') {
    setApplianceState(currentManualControlOption, !applianceStates[currentManualControlOption]);
    currentMenu = MANUAL_CONTROL_SELECT;
  } else if (key == 'D') {
    currentMenu = MAIN_MENU;
  }
}

void handleSetWakeAlarmOptionsInput(char key) {
  String options[] = {"Set Time", "Enable Alarm"};
  int numItems = sizeof(options) / sizeof(String);

  if (key == 'A') {
    currentWakeAlarmMenuOption = (currentWakeAlarmMenuOption - 1 + numItems) % numItems;
  } else if (key == 'B') {
    currentWakeAlarmMenuOption = (currentWakeAlarmMenuOption + 1) % numItems;
  } else if (key == 'C') {
    input[0] = '\0';
    inputIndex = 0;
    switch (currentWakeAlarmMenuOption) {
      case 0:
        currentMenu = SET_WAKE_ALARM_INPUT_HH;
        break;
      case 1:
        wakeUpAlarm.enabled = !wakeUpAlarm.enabled;
        currentMenu = SET_ALARM_OPTIONS;
        break;
    }
  } else if (key == 'D') {
    currentMenu = MAIN_MENU;
  }
}

void handleWakeAlarmTimeInput(char key) {
  bool isHourInput = (currentMenu == SET_WAKE_ALARM_INPUT_HH);
  bool isMinuteInput = (currentMenu == SET_WAKE_ALARM_INPUT_MM);

  if (isdigit(key)) {
    if (inputIndex < 2) {
      input[inputIndex++] = key;
      input[inputIndex] = '\0';
    }
  } else if (key == 'D') {
    if (inputIndex > 0) {
      inputIndex--;
      input[inputIndex] = '\0';
    } else {
      if (isHourInput) {
        currentMenu = SET_ALARM_OPTIONS;
      } else if (isMinuteInput) {
        currentMenu = SET_WAKE_ALARM_INPUT_HH;
      }
    }
  } else if (key == 'C') {
    if (inputIndex == 2) {
      int val = atoi(input);
      bool validationSuccessful = false;

      if (isHourInput) {
        if (val >= 0 && val <= 23) {
          wakeUpAlarm.hour = val;
          currentMenu = SET_WAKE_ALARM_INPUT_MM;
          validationSuccessful = true;
        }
      } else if (isMinuteInput) {
        if (val >= 0 && val <= 59) {
          wakeUpAlarm.minute = val;
          validationSuccessful = true;
          currentMenu = SET_ALARM_OPTIONS;
        }
      }

      if (!isHourInput || !validationSuccessful || currentMenu == SET_ALARM_OPTIONS) {
         input[0] = '\0';
         inputIndex = 0;
      } else if (isHourInput && validationSuccessful) {
         input[0] = '\0';
         inputIndex = 0;
      }

    } else {
      input[0] = '\0';
      inputIndex = 0;
    }
  }
}

void handleAlarmSoundingInput(char key) {
  if (key) {
    wakeUpAlarm.sounding = false;
    C_noTone(BUZZER1_PIN);
    currentMenu = MAIN_MENU;
    lastAlarmTriggerMinute = 255;
    lastAlarmTriggerHour = 255;
  }
}


void checkTimers(DateTime now) {
  for (int i = 0; i < NUM_APPLIANCES; i++) {
    if (applianceTimers[i].enabled) {
      bool onTimeMatch = (now.hour() == applianceTimers[i].onHour && now.minute() == applianceTimers[i].onMinute);
      if (onTimeMatch && now.second() == 0 && !applianceStates[i]) {
        setApplianceState(i, true);
      }

      bool offTimeMatch = (now.hour() == applianceTimers[i].offHour && now.minute() == applianceTimers[i].offMinute);
      if (offTimeMatch && now.second() == 0 && applianceStates[i]) {
        setApplianceState(i, false);
      }
    }
  }
}

void checkWakeUpAlarm(DateTime now) {
  if (wakeUpAlarm.enabled && !wakeUpAlarm.sounding &&
      now.hour() == wakeUpAlarm.hour &&
      now.minute() == wakeUpAlarm.minute &&
      now.second() == 0) {

    if (now.hour() != lastAlarmTriggerHour || now.minute() != lastAlarmTriggerMinute) {
      wakeUpAlarm.sounding = true;

      C_digitalWrite(BUZZER1_PIN, HIGH);
      lastAlarmActionLoopIteration = main_loop_iterations;

      currentMenu = ALARM_IS_SOUNDING;

      lastAlarmTriggerHour = now.hour();
      lastAlarmTriggerMinute = now.minute();
    }
  }

  if ((now.hour() > wakeUpAlarm.hour || (now.hour() == wakeUpAlarm.hour && now.minute() > wakeUpAlarm.minute)) &&
      (lastAlarmTriggerHour == wakeUpAlarm.hour && lastAlarmTriggerMinute == wakeUpAlarm.minute)) {
    lastAlarmTriggerHour = 255;
    lastAlarmTriggerMinute = 255;
  }
}

void manageAlarmSound() {
  if (!wakeUpAlarm.sounding) {
    if (C_digitalRead(BUZZER1_PIN) == HIGH) {
        C_noTone(BUZZER1_PIN);
    }
    return;
  }

  if (C_digitalRead(BUZZER1_PIN) == LOW) {
    if (main_loop_iterations - lastAlarmActionLoopIteration >= ALARM_PULSE_GAP_LOOPS) {
      C_digitalWrite(BUZZER1_PIN, HIGH);
      lastAlarmActionLoopIteration = main_loop_iterations;
    }
  } else {
    if (main_loop_iterations - lastAlarmActionLoopIteration >= ALARM_PULSE_DURATION_LOOPS) {
      C_noTone(BUZZER1_PIN);
      lastAlarmActionLoopIteration = main_loop_iterations;
    }
  }
}


void updateDisplay(DateTime now) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  switch (currentMenu) {
    case LOCKED_SCREEN:           displayLockScreen(); break;
    case MAIN_MENU:               displayMainMenu(); break;
    case VIEW_STATUS:             displayViewStatus(now); break;
    case SET_TIMERS_APPLIANCE_SELECT: displaySetTimersApplianceSelect(); break;
    case SET_TIMER_OPTIONS:       displaySetTimerOptions(); break;
    case SET_TIMER_INPUT_ON_HH:   displaySetTimerInput("ON Hour (00-23):"); break;
    case SET_TIMER_INPUT_ON_MM:   displaySetTimerInput("ON Min (00-59):"); break;
    case SET_TIMER_INPUT_OFF_HH:  displaySetTimerInput("OFF Hour (00-23):"); break;
    case SET_TIMER_INPUT_OFF_MM:  displaySetTimerInput("OFF Min (00-59):"); break;
    case MANUAL_CONTROL_SELECT:   displayManualControlSelect(); break;
    case SET_ALARM_OPTIONS:       displaySetAlarmOptions(); break;
    case SET_WAKE_ALARM_INPUT_HH: displaySetWakeAlarmInput("Alarm HH (00-23):"); break;
    case SET_WAKE_ALARM_INPUT_MM: displaySetWakeAlarmInput("Alarm MM (00-59):"); break;
    case ALARM_IS_SOUNDING:       displayAlarmIsSoundingScreen(); break;
  }
  display.display();
}

void displayLockScreen() {
  display.setCursor(25, 0);
  display.setTextSize(2);
  display.println(F("MENU"));
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(F("Enter Passcode:"));
  display.setCursor(30, 38);
  display.setTextSize(2);
  String stars = "";
  for (int i = 0; i < enteredPasscode.length(); i++) stars += "*";
  display.print(stars);
  for (int i = enteredPasscode.length(); i < PASSCODE_LENGTH; i++) {
      display.print(F("_"));
  }

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("C=Enter D=Delete"));
}

void displayMainMenu() {
  display.setCursor(20, 0);
  display.setTextSize(2);
  display.print(F("MENU"));
  display.setTextSize(1);

  String menuItems[] = {"View Status", "Set Timers", "Set Alarm", "Manual Control"};
  int numItems = sizeof(menuItems) / sizeof(String);

  for (int i = 0; i < numItems; i++) {
    display.setCursor(0, 18 + i * 10);
    if (i == currentMainMenuOption) display.print(F("> "));
    else display.print(F("  "));
    display.println(menuItems[i]);
  }
    display.setCursor(0, 56);
    display.print(F("A/B:<--> C:Sel D:Lock"));
}

void displayViewStatus(DateTime now) {
  display.setCursor(0, 0);
  char buf[25];
  sprintf(buf, "%s %02d:%02d", daysOfTheWeek[now.dayOfTheWeek()], now.hour(), now.minute());
  display.println(buf);

  display.setCursor(0, 10);
  sprintf(buf, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  display.println(buf);

  display.setCursor(0, 22);
  display.print(F("Temp: "));
  if (isnan(temperature)) display.print(F("N/A")); else display.print(temperature, 1);
  display.print((char)247); display.print(F("C"));

  display.setCursor(0, 32);
  display.print(F("Humi: "));
  if (isnan(humidity)) display.print(F("N/A")); else display.print(humidity, 1);
  display.print(F("%"));

  display.setCursor(75, 22);
  display.print(F("LDR: ")); display.print(ldrValue);

  display.setCursor(0, 44);
  display.print(F("AC:")); display.print(applianceStates[AC_APPLIANCE] ? F("ON ") : F("OFF"));
  display.print(F(" CF:")); display.print(applianceStates[COFFEE_APPLIANCE] ? F("ON ") : F("OFF"));

  display.setCursor(0, 54);
  display.print(F("LT:")); display.print(applianceStates[LIGHT_APPLIANCE] ? F("ON ") : F("OFF"));
  display.print(F(" (D:Back)"));
}

void displaySetTimersApplianceSelect() {
  display.setCursor(5, 0);
  display.setTextSize(1);
  display.println(F("Set Timer For:"));
  for (int i = 0; i < NUM_APPLIANCES; i++) {
    display.setCursor(0, 15 + i * 10);
    if (i == currentApplianceSelectOption) display.print(F("> "));
    else display.print(F("  "));
    display.println(applianceNames[i]);
  }
  display.setCursor(0, 56);
  display.print(F("A/B:<--> C:Sel D:Back"));
}

void displaySetTimerOptions() {
  display.setCursor(0, 0);
  display.print(F("Timer: ")); display.println(applianceNames[selectedApplianceForTimer]);

  TimerSetting currentTimer = applianceTimers[selectedApplianceForTimer];
  char buf[20];

  String options[] = {"ON Time", "OFF Time", "Enabled"};
  String values[3];
  sprintf(buf, "%02d:%02d", currentTimer.onHour, currentTimer.onMinute);
  values[0] = String(buf);
  sprintf(buf, "%02d:%02d", currentTimer.offHour, currentTimer.offMinute);
  values[1] = String(buf);
  values[2] = currentTimer.enabled ? F("YES") : F("NO");


  for (int i = 0; i < 3; i++) {
    display.setCursor(0, 18 + i * 11);
    if (i == currentTimerOptionsOption) display.print(F("> "));
    else display.print(F("  "));
    display.print(options[i]);

    int valCursor = 75;
    if (options[i].length() > 8 ) valCursor = 85;
    display.setCursor(valCursor, 18 + i * 11);
    display.print(values[i]);
  }
  display.setCursor(0, 56);
  display.print(F("A/B:<--> C:Sel D:Back"));
}

void displaySetTimerInput(String prompt) {
  display.setCursor(0, 0);
  display.println(applianceNames[selectedApplianceForTimer] + F(" Timer"));
  display.setCursor(0, 12);
  display.println(prompt);

  display.setTextSize(2);
  display.setCursor(30, 30);
  display.print(input);
  for(int i=inputIndex; i < 2; i++) display.print(F("_"));

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("0-9 C:Ent D:Del/Back"));
}

void displayManualControlSelect() {
  display.setCursor(5, 0);
  display.setTextSize(1);
  display.println(F("Manual Control:"));
  for (int i = 0; i < NUM_APPLIANCES; i++) {
    display.setCursor(0, 15 + i * 11);
    if (i == currentManualControlOption) display.print(F("> "));
    else display.print(F("  "));
    display.print(applianceNames[i]);
    display.setCursor(80, 15 + i * 11);
    display.print(applianceStates[i] ? F("(ON)") : F("(OFF)"));
  }
  display.setCursor(0, 56);
  display.print(F("A/B:<--> C:Toggle D:Bk"));
}

void displaySetAlarmOptions() {
  display.setCursor(5, 0);
  display.setTextSize(1);
  display.println(F("Wake-Up Alarm:"));

  String options[] = {"Set Time", "Enabled"};
  String values[2];
  char buf[10];
  sprintf(buf, "%02d:%02d", wakeUpAlarm.hour, wakeUpAlarm.minute);
  values[0] = String(buf);
  values[1] = wakeUpAlarm.enabled ? F("YES") : F("NO");

  for (int i = 0; i < 2; i++) {
    display.setCursor(0, 20 + i * 12);
    if (i == currentWakeAlarmMenuOption) display.print(F("> "));
    else display.print(F("  "));
    display.print(options[i]);
    display.setCursor(75, 20 + i * 12);
    if (i == 0) display.print(values[0]);
    else if (i == 1) display.print(values[1]);
  }
  display.setCursor(0, 56);
  display.print(F("A/B:<--> C:Sel D:Back"));
}

void displaySetWakeAlarmInput(String prompt) {
  display.setCursor(0, 0);
  display.println(F("Set Wake-Up Alarm"));
  display.setCursor(0, 12);
  display.println(prompt);

  display.setTextSize(2);
  display.setCursor(30, 30);
  display.print(input);
  for(int i = inputIndex; i < 2; i++) display.print(F("_"));

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("0-9 C:Ent D:Del/Back"));
}

void displayAlarmIsSoundingScreen() {
  display.setTextSize(2);
  display.setCursor(15, 10);
  display.println(F("ALARM!"));
  display.setTextSize(1);
  display.setCursor(10, 35);
  display.println(F("Wake Up! Wake Up!"));
  display.setCursor(15, 50);
  display.println(F("Press any key..."));
}