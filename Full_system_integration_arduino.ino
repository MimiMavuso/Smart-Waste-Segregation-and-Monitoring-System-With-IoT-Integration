
#include <CheapStepper.h>
#include <Servo.h>

Servo servo1;

// ================= PIN DEFINITIONS =================
#define IR_PIN 5
#define PROXI_PIN 6
#define BUZZER 12
#define TRIG_PIN 2
#define ECHO_PIN 3
#define LOC_PIN A1

int potPin = A0; // Moisture sensor (rain sensor)

// ================= VARIABLES =================
int fsoil = 0;
int systemLocation = 0;

long duration;
float distance_cm;
float binLevelPercent;

// ⚠️ CHANGE this to your real bin height (cm)
#define BIN_HEIGHT 10.0

// Stepper setup
CheapStepper stepper(8, 9, 10, 11);

// ================= BIN LEVEL FUNCTION =================
float readBinLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return binLevelPercent;
  distance_cm = duration * 0.034 / 2;
  float level = (BIN_HEIGHT - distance_cm) / BIN_HEIGHT * 100.0;
  level = constrain(level, 0, 100);
  return level;
}

// ================= SEND DATA TO ESP32 =================
void sendToESP(String wasteType, float level, int locID) {
  // Format: "WASTE:Wet,LEVEL:45.2,LOC:1"
  String data = "WASTE:" + wasteType + ",LEVEL:" + String(level, 1) + ",LOC:" + String(locID);
  Serial.println(data);
  Serial.print("Sent to ESP: ");
  Serial.println(data);
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);   // Communication with ESP32

  pinMode(PROXI_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LOC_PIN, INPUT_PULLUP);

  servo1.attach(7);
  stepper.setRpm(17);

  // ----- Read system location -----
  if (digitalRead(LOC_PIN) == LOW) {
    systemLocation = 1; // Location A
  } else {
    systemLocation = 2; // Location B
  }

  Serial.print("System Location ID: ");
  Serial.println(systemLocation);

  // Initialize servo
  servo1.write(180);
  delay(1000);
  servo1.write(70);
  delay(1000);

  Serial.println("System Ready...");
}

// ================= LOOP =================
void loop() {
  fsoil = 0;

  int proxiState = digitalRead(PROXI_PIN);
  int irState = digitalRead(IR_PIN);

  // ----- Bin level monitoring (always) -----
  binLevelPercent = readBinLevel();
  Serial.print("Bin Level: ");
  Serial.print(binLevelPercent);
  Serial.println("%");

  if (binLevelPercent > 80) {
    Serial.println("WARNING: Bin almost full!");
    tone(BUZZER, 1500, 500);
  }

  // Send bin level to ESP every loop (or you can add a timer)
  // But to avoid flooding, we'll send only when level changes significantly
  static float lastSentLevel = -1;
  if (abs(binLevelPercent - lastSentLevel) > 2.0) {
    sendToESP("LevelUpdate", binLevelPercent, systemLocation);
    lastSentLevel = binLevelPercent;
  }

  // ================= METAL DETECTION =================
  if (proxiState == HIGH) {
    Serial.println("Metal detected!");
    tone(BUZZER, 1000, 1000);
    stepper.moveDegreesCW(240);
    delay(1500);
    stepper.stop();
    servo1.write(180);
    delay(1000);
    servo1.write(70);
    delay(1000);
    stepper.moveDegreesCCW(240);
    delay(1500);
    stepper.stop();

    // Send metal waste data
    sendToESP("Metal", binLevelPercent, systemLocation);
  }

  // ================= IR WASTE DETECTION =================
  else if (irState == LOW) {
    tone(BUZZER, 1000, 300);
    delay(500);

    // Moisture averaging
    int totalSoil = 0;
    for (int i = 0; i < 3; i++) {
      int soil = analogRead(potPin);
      soil = constrain(soil, 485, 1023);
      totalSoil += map(soil, 485, 1023, 100, 0);
      delay(75);
    }
    fsoil = totalSoil / 3;

    Serial.print("Moisture: ");
    Serial.print(fsoil);
    Serial.println("%");

    // ================= WET WASTE =================
    if (fsoil > 20) {
      Serial.println("Wet Waste Detected");
      tone(BUZZER, 1200, 300);
      stepper.moveDegreesCW(120);
      delay(1000);
      stepper.stop();
      servo1.write(180);
      delay(1000);
      servo1.write(70);
      delay(1000);
      stepper.moveDegreesCCW(120);
      delay(1000);
      stepper.stop();

      sendToESP("Wet", binLevelPercent, systemLocation);
    }
    // ================= DRY WASTE =================
    else {
      Serial.println("Dry Waste Detected");
      tone(BUZZER, 800, 300);
      servo1.write(180);
      delay(1000);
      servo1.write(70);
      delay(1000);

      sendToESP("Dry", binLevelPercent, systemLocation);
    }
  }

  // ================= IDLE =================
  else {
    stepper.stop();
  }

  delay(200);
}
