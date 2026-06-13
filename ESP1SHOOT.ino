#define STEP_PIN 26
#define DIR_PIN 27
#define EN_PIN 14
#define IR_STEPPER 34
#define IR_SHOOTING 35
#define SOLENOID_PIN 25

#define UART_RX_PIN 16
#define UART_TX_PIN 17
#define UART_BAUD 9600

#define STEPS_PER_REV 1440
#define NUMBER_OF_TURNS 2
#define STEP_DELAY_US 600
#define SOLENOID_ON_MS 100

#define SOLENOID_OFF HIGH
#define SOLENOID_ON LOW

enum Stage { WAIT_SORTING, SHOOTING, LIFTING };
Stage stage = WAIT_SORTING;

String uartBuffer = "";
String usbBuffer = "";

bool motorRunning = false;
bool lastStepperDetect = false;
bool lastShootingDetect = false;
bool shootArmed = true;
bool liftArmed = true;

long stepsRemaining = 0;
unsigned long lastDebugPrint = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(IR_STEPPER, INPUT);
  pinMode(IR_SHOOTING, INPUT);
  pinMode(SOLENOID_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);
  digitalWrite(SOLENOID_PIN, SOLENOID_OFF);
  disableMotor();

  delay(700);

  Serial.println("ESP1 READY: SORTING -> SHOOTING -> LIFTING");
  Serial.println("USB commands: TEST_SHOOT, TEST_LIFT, START_SHOOTING, START_LIFTING");

  Serial2.println("ESP1_READY");
}

void loop() {
  handleUART();
  handleUSBSerial();

  bool shootingDetected = digitalRead(IR_SHOOTING) == LOW;
  bool stepperDetected = digitalRead(IR_STEPPER) == LOW;

  debugPrint(shootingDetected, stepperDetected);

  if (stage == SHOOTING) {
    if (shootingDetected && !lastShootingDetect && shootArmed) {
      shootArmed = false;
      sendLCD("Shooting", "Ball Detected");
      delay(250);
      fireSolenoid();
      sendLCD("Shooting", "Shot Fired");
      delay(700);
      stage = LIFTING;
      liftArmed = true;
      lastStepperDetect = false;
      sendLCD("Lifting", "Waiting for Ball");
      Serial2.println("START_LIFTING");
    }

    if (!shootingDetected) shootArmed = true;

    lastShootingDetect = shootingDetected;
    lastStepperDetect = false;
  }

  else if (stage == LIFTING) {
    if (stepperDetected && !lastStepperDetect && liftArmed && !motorRunning) {
      liftArmed = false;
      Serial2.println("BALL_BEING_LIFTED");
      sendLCD("Lifting in", "Progress");
      startMotor();
    }

    if (!stepperDetected && !motorRunning) liftArmed = true;

    lastStepperDetect = stepperDetected;
    lastShootingDetect = false;
  }

  else {
    lastShootingDetect = false;
    lastStepperDetect = false;
  }

  if (motorRunning) {
    if (stepsRemaining > 0) {
      doStep();
      stepsRemaining--;
    } else {
      disableMotor();
      motorRunning = false;
      Serial2.println("LIFT_DONE");
      Serial2.println("BLINK:Ball Successfully|Lifted");
      blinkLCDCommand("Ball Successfully", "Lifted", 5);
      delay(300);
      stage = WAIT_SORTING;
      Serial2.println("START_SORTING");
    }
    return;
  }
}

void fireSolenoid() {
  digitalWrite(SOLENOID_PIN, SOLENOID_ON);
  delay(SOLENOID_ON_MS);
  digitalWrite(SOLENOID_PIN, SOLENOID_OFF);
  Serial.println("SOLENOID FIRED");
}

void startMotor() {
  digitalWrite(DIR_PIN, HIGH);
  enableMotor();
  delay(5);
  stepsRemaining = (long)NUMBER_OF_TURNS * STEPS_PER_REV;
  motorRunning = true;
  Serial.print("Motor started. Steps = ");
  Serial.println(stepsRemaining);
}

void doStep() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US);
}

void enableMotor() {
  digitalWrite(EN_PIN, LOW);
  Serial.println("TMC ENABLED");
}

void disableMotor() {
  digitalWrite(EN_PIN, HIGH);
  Serial.println("TMC DISABLED");
}

void sendLCD(String line1, String line2) {
  Serial2.print("LCD:");
  Serial2.print(line1);
  Serial2.print("|");
  Serial2.println(line2);
  Serial.print("LCD SENT: ");
  Serial.print(line1);
  Serial.print(" | ");
  Serial.println(line2);
}

void blinkLCDCommand(String line1, String line2, int times) {
  for (int i = 0; i < times; i++) {
    sendLCD(line1, line2);
    delay(220);
    sendLCD("", "");
    delay(220);
  }
  sendLCD(line1, line2);
}

void handleUART() {
  while (Serial2.available()) {
    char ch = Serial2.read();
    if (ch == '\n' || ch == '\r') {
      if (uartBuffer.length() > 0) {
        uartBuffer.trim();
        processCommand(uartBuffer, "TXRX");
        uartBuffer = "";
      }
    } else {
      uartBuffer += ch;
      if (uartBuffer.length() > 90) uartBuffer = "";
    }
  }
}

void handleUSBSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (usbBuffer.length() > 0) {
        usbBuffer.trim();
        processCommand(usbBuffer, "USB");
        usbBuffer = "";
      }
    } else {
      usbBuffer += ch;
      if (usbBuffer.length() > 90) usbBuffer = "";
    }
  }
}

void processCommand(String cmd, String source) {
  cmd.trim();
  cmd.toUpperCase();

  Serial.print(source);
  Serial.print(" CMD: ");
  Serial.println(cmd);

  if (cmd == "START_SHOOTING") {
    stage = SHOOTING;
    motorRunning = false;
    shootArmed = true;
    liftArmed = false;
    lastShootingDetect = false;
    lastStepperDetect = false;
    sendLCD("Shooting", "Waiting for Ball");
  }

  else if (cmd == "START_LIFTING") {
    stage = LIFTING;
    motorRunning = false;
    shootArmed = false;
    liftArmed = true;
    lastShootingDetect = false;
    lastStepperDetect = false;
    sendLCD("Lifting", "Waiting for Ball");
  }

  else if (cmd == "START_SORTING") {
    stage = WAIT_SORTING;
    motorRunning = false;
    shootArmed = false;
    liftArmed = false;
  }

  else if (cmd == "TEST_SHOOT") {
    sendLCD("Shooting", "Test Shot");
    fireSolenoid();
  }

  else if (cmd == "TEST_LIFT") {
    stage = LIFTING;
    sendLCD("Lifting in", "Progress");
    startMotor();
  }
}

void debugPrint(bool shootingDetected, bool stepperDetected) {
  if (millis() - lastDebugPrint >= 1000) {
    lastDebugPrint = millis();

    Serial.print("Stage=");
    if (stage == WAIT_SORTING) Serial.print("WAIT_SORTING");
    else if (stage == SHOOTING) Serial.print("SHOOTING");
    else Serial.print("LIFTING");

    Serial.print(" | ShootIR=");
    Serial.print(digitalRead(IR_SHOOTING));
    Serial.print(" Det=");
    Serial.print(shootingDetected ? "YES" : "NO");

    Serial.print(" | LiftIR=");
    Serial.print(digitalRead(IR_STEPPER));
    Serial.print(" Det=");
    Serial.print(stepperDetected ? "YES" : "NO");

    Serial.print(" | Motor=");
    Serial.println(motorRunning ? "ON" : "OFF");
  }
}