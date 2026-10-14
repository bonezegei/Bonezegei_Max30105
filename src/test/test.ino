#include <Wire.h>
#include <Bonezegei_Max30105.h>

Bonezegei_Max30105 colorSensor;

// Ambient baseline light levels
uint32_t ambientRed = 0;
uint32_t ambientGreen = 0;

// Surface reflection thresholds (tune based on target object distance ~1cm)
const uint32_t MIN_OBJECT_REFLECT = 1500; // Minimum brightness to confirm object presence
const float COLOR_RATIO_THRESHOLD = 1.35f; // Multiplier threshold to distinguish dominant channel

void calibrateAmbient() {
  Serial.println("Calibrating ambient background light (keep sensor uncovered)...");
  uint32_t sumRed = 0, sumGreen = 0;
  int samples = 30;

  for (int i = 0; i < samples; i++) {
    if (colorSensor.check()) {
      sumRed += colorSensor.getRed();
      sumGreen += colorSensor.getGreen();
      delay(20);
    }
  }

  ambientRed = sumRed / samples;
  ambientGreen = sumGreen / samples;

  Serial.print("Ambient Baseline -> Red: ");
  Serial.print(ambientRed);
  Serial.print(" | Green: ");
  Serial.println(ambientGreen);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("MAX30105 Basic Surface Color Detector");

  if (!colorSensor.begin(Wire)) {
    Serial.println("Sensor not found. Check I2C connections!");
    while (1);
  }

  // Setup: brightness = 0x24 (higher LED output for surface reflection),
  // sampleAverage = 4, mode = MULTILED, sampleRate = 0x03, pulseWidth = 0x03, adcRange = 0x02
  colorSensor.setup(0x24, 4, MAX30105_MODE_MULTILED, 0x03, 0x03, 0x02);

  delay(500);
  calibrateAmbient();
  Serial.println("Place an object 1 cm above the sensor...\n");
}

void loop() {
  if (colorSensor.check()) {
    uint32_t rawRed   = colorSensor.getRed();
    uint32_t rawGreen = colorSensor.getGreen();

    // Subtract ambient baseline to isolate reflected light
    int32_t refRed   = (int32_t)rawRed - (int32_t)ambientRed;
    int32_t refGreen = (int32_t)rawGreen - (int32_t)ambientGreen;

    if (refRed < 0) refRed = 0;
    if (refGreen < 0) refGreen = 0;

    Serial.print("Reflected -> Red: ");
    Serial.print(refRed);
    Serial.print("\tGreen: ");
    Serial.print(refGreen);
    Serial.print("\t| Detected: ");

    // Color Classification Logic
    if (refRed < MIN_OBJECT_REFLECT && refGreen < MIN_OBJECT_REFLECT) {
      Serial.println("Dark / Black / No Object");
    } 
    else if ((float)refRed > ((float)refGreen * COLOR_RATIO_THRESHOLD)) {
      Serial.println("RED Surface");
    } 
    else if ((float)refGreen > ((float)refRed * COLOR_RATIO_THRESHOLD)) {
      Serial.println("GREEN Surface");
    } 
    else {
      Serial.println("YELLOW / WHITE Surface");
    }
  }

  delay(200);
}