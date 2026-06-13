#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

#define SENSOR_SERVO_PIN 27

#define UART_RX_PIN 16
#define UART_TX_PIN 17
#define UART_BAUD 9600

#define SERVO_CENTER 90
#define BLUE_ANGLE 170
#define RED_ANGLE 10

#define MIN_CLEAR 250
#define RED_RATIO_MIN 0.45
#define BLUE_RATIO_MIN 0.35

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

Servo sensorServo;

enum SortState { SENSOR_SCREEN, COLOR_BLINK, LAUNCH_COUNTDOWN, WAIT_ESP1 };
SortState state = SENSOR_SCREEN;

String uartBuffer = "";
String lcdLine1 = "";
String lcdLine2 = "";
String detectedColor = "";

unsigned long colorBlinkStart = 0;
unsigned long colorBlinkTimer = 0;
bool blinkOn = true;

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if (!tcs.begin()) {
    lcdPrint("SENSOR ERROR", "Check TCS34725");
    while (1) {
      handleUART();
      delay(20);
    }
  }

  sensorServo.attach(SENSOR_SERVO_PIN, 500, 2400);
  sensorServo.write(SERVO_CENTER);

  delay(700);

  Serial.println("ESP2 SORTING READY");
  Serial2.println("ESP2_READY");

  state = SENSOR_SCREEN;
  showSensorScreen();
}

void loop() {
  handleUART();

  if (state == SENSOR_SCREEN) {
    showSensorScreen();
    readColorSensor();
  }

  else if (state == COLOR_BLINK) {
    blinkDetectedColor();
  }

  else if (state == LAUNCH_COUNTDOWN) {
    launchCountdown();
  }

  else if (state == WAIT_ESP1) {
  }
}

void handleUART() {
  while (Serial2.available()) {
    char ch = Serial2.read();

    if (ch == '\n' || ch == '\r') {
      if (uartBuffer.length() > 0) {
        uartBuffer.trim();
        processUART(uartBuffer);
        uartBuffer = "";
      }
    } else {
      uartBuffer += ch;
      if (uartBuffer.length() > 100) uartBuffer = "";
    }
  }
}

void processUART(String cmd) {
  cmd.trim();

  Serial.print("ESP1 -> ");
  Serial.println(cmd);

  if (cmd.startsWith("LCD:")) {
    String payload = cmd.substring(4);
    int sep = payload.indexOf('|');

    if (sep >= 0) {
      lcdPrint(payload.substring(0, sep), payload.substring(sep + 1));
    }
  }

  else if (cmd.startsWith("BLINK:")) {
    String payload = cmd.substring(6);
    int sep = payload.indexOf('|');

    if (sep >= 0) {
      blinkMessage(payload.substring(0, sep), payload.substring(sep + 1), 2500);
    } else {
      blinkMessage(payload, "", 2500);
    }
  }

  else if (cmd == "START_SORTING") {
    sensorServo.write(SERVO_CENTER);
    state = SENSOR_SCREEN;
    showSensorScreen();
  }

  else if (cmd == "START_LIFTING") {
    lcdPrint("Lifting", "Waiting for Ball");
  }

  else if (cmd == "BALL_BEING_LIFTED") {
    lcdPrint("Lifting in", "Progress");
  }
}

void showSensorScreen() {
  lcdPrint("     Sensor     ", "Blue        Red ");
}

void showLaunchCountdown(int secondsLeft) {
  lcdPrint("Ball Launching", "in " + String(secondsLeft));
}

void lcdPrint(String line1, String line2) {
  line1 = fit16(line1);
  line2 = fit16(line2);

  if (line1 != lcdLine1) {
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcdLine1 = line1;
  }

  if (line2 != lcdLine2) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
    lcdLine2 = line2;
  }

  Serial.print("LCD:");
  Serial.print(line1);
  Serial.print("|");
  Serial.println(line2);
}

String fit16(String s) {
  if (s.length() > 16) s = s.substring(0, 16);
  while (s.length() < 16) s += " ";
  return s;
}

void blinkMessage(String line1, String line2, unsigned long durationMs) {
  unsigned long start = millis();
  bool show = true;

  while (millis() - start < durationMs) {
    handleUART();

    if (show) {
      lcdPrint(line1, line2);
    } else {
      lcdPrint("", "");
    }

    show = !show;
    delay(220);
  }
}

void readColorSensor() {
  static unsigned long lastRead = 0;

  if (millis() - lastRead < 120) return;
  lastRead = millis();

  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  if (c < MIN_CLEAR) return;

  float redRatio = (float)r / (float)c;
  float blueRatio = (float)b / (float)c;

  bool isRed = redRatio > RED_RATIO_MIN && r > b && r > g;
  bool isBlue = blueRatio > BLUE_RATIO_MIN && b > r;

  if (isRed) {
    detectedColor = "RED";
    sensorServo.write(RED_ANGLE);
    Serial.println("Detected RED");
    Serial2.println("COLOR:RED");

    colorBlinkStart = millis();
    colorBlinkTimer = 0;
    blinkOn = true;
    state = COLOR_BLINK;
  }

  else if (isBlue) {
    detectedColor = "BLUE";
    sensorServo.write(BLUE_ANGLE);
    Serial.println("Detected BLUE");
    Serial2.println("COLOR:BLUE");

    colorBlinkStart = millis();
    colorBlinkTimer = 0;
    blinkOn = true;
    state = COLOR_BLINK;
  }
}

void blinkDetectedColor() {
  if (millis() - colorBlinkTimer >= 120) {
    colorBlinkTimer = millis();
    blinkOn = !blinkOn;

    if (detectedColor == "BLUE") {
      if (blinkOn) {
        lcdPrint("     Sensor     ", "Blue        Red ");
      } else {
        lcdPrint("     Sensor     ", "            Red ");
      }
    }

    else if (detectedColor == "RED") {
      if (blinkOn) {
        lcdPrint("     Sensor     ", "Blue        Red ");
      } else {
        lcdPrint("     Sensor     ", "Blue            ");
      }
    }
  }

  if (millis() - colorBlinkStart >= 1800) {
    state = LAUNCH_COUNTDOWN;
  }
}

void launchCountdown() {
  for (int i = 5; i >= 1; i--) {
    showLaunchCountdown(i);
    delayWithUART(1000);
  }

  lcdPrint("Launching", detectedColor);
  delayWithUART(700);

  sensorServo.write(SERVO_CENTER);
  delayWithUART(400);

  if (detectedColor == "BLUE") {
    Serial2.println("CHOSEN:BLUE");
  } else if (detectedColor == "RED") {
    Serial2.println("CHOSEN:RED");
  }

  Serial2.println("START_SHOOTING");
  state = WAIT_ESP1;
}

void delayWithUART(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    handleUART();
    delay(5);
  }
}
