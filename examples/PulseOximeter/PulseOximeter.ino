#include <Wire.h>
#include <Bonezegei_Max30105.h>

Bonezegei_Max30105 pbx;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Bonezegei MAX30105 - Pulse Oximeter Initialization");

  if (!pbx.begin(Wire)) {
    Serial.println("Sensor connection failed! Check I2C address and wiring.");
    while (1);
  }

  // Setup: brightness = 0x1F, sampleAvg = 4, mode = MULTILED, sampleRate = 0x03, pulseWidth = 0x03, adcRange = 0x02
  pbx.setup(0x1F, 4, MAX30105_MODE_MULTILED, 0x03, 0x03, 0x02);
  Serial.println("Place finger lightly on optical sensor surface...");
}

void loop() {
  if (pbx.check()) {
    if (pbx.isFingerDetected()) {
      float bpm = pbx.getBPM();
      float spo2 = pbx.getSpO2();

      Serial.print("Finger Detected | ");
      Serial.print("Heart Rate: ");
      if (bpm > 0) {
        Serial.print(bpm, 1);
        Serial.print(" BPM");
      } else {
        Serial.print("Calculating...");
      }

      Serial.print("\t| SpO2: ");
      if (spo2 > 0) {
        Serial.print(spo2, 1);
        Serial.println(" %");
      } else {
        Serial.println("Calculating...");
      }
    } else {
      Serial.println("No finger detected on sensor.");
    }
  }

  delay(20);
}