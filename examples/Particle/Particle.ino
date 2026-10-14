#include <Wire.h>
#include <Bonezegei_Max30105.h>

Bonezegei_Max30105 particleSensor;

// Calibration baseline and threshold variables
uint32_t baselineRed = 0;
uint32_t baselineIR = 0;
uint32_t baselineGreen = 0;

const int CALIBRATION_SAMPLES = 50;
const uint32_t SMOKE_THRESHOLD = 400;  // Delta rise for smoke detection
const uint32_t DUST_THRESHOLD  = 600;  // Delta rise for dust detection

void calibrateBaseline() {
  Serial.println("Calibrating ambient baseline (keep sensor clear)...");
  
  uint32_t sumRed = 0, sumIR = 0, sumGreen = 0;
  int count = 0;

  while (count < CALIBRATION_SAMPLES) {
    if (particleSensor.check()) {
      sumRed   += particleSensor.getRed();
      sumIR    += particleSensor.getIR();
      sumGreen += particleSensor.getGreen();
      count++;
      delay(20);
    }
  }

  baselineRed   = sumRed / CALIBRATION_SAMPLES;
  baselineIR    = sumIR / CALIBRATION_SAMPLES;
  baselineGreen = sumGreen / CALIBRATION_SAMPLES;

  Serial.print("Baselines -> Red: "); Serial.print(baselineRed);
  Serial.print(" | IR: "); Serial.print(baselineIR);
  Serial.print(" | Green: "); Serial.println(baselineGreen);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("MAX30105 Particle & Air Quality Monitor");

  if (!particleSensor.begin(Wire)) {
    Serial.println("Sensor error! Check wiring.");
    while (1);
  }

  // Setup: ledBrightness = 0x24 (higher LED drive for particle bounce),
  // sampleAverage = 4, mode = MULTILED, sampleRate = 0x03, pulseWidth = 0x03, adcRange = 0x02
  particleSensor.setup(0x24, 4, MAX30105_MODE_MULTILED, 0x03, 0x03, 0x02);

  delay(500);
  calibrateBaseline();
  Serial.println("Monitoring air quality...\n");
}

void loop() {
  if (particleSensor.check()) {
    uint32_t red   = particleSensor.getRed();
    uint32_t ir    = particleSensor.getIR();
    uint32_t green = particleSensor.getGreen();

    // Calculate scattering delta above ambient baseline
    int32_t deltaRed   = (int32_t)red - (int32_t)baselineRed;
    int32_t deltaIR    = (int32_t)ir - (int32_t)baselineIR;
    int32_t deltaGreen = (int32_t)green - (int32_t)baselineGreen;

    // Prevent negative deltas due to minor ambient noise
    if (deltaRed < 0)   deltaRed = 0;
    if (deltaIR < 0)    deltaIR = 0;
    if (deltaGreen < 0) deltaGreen = 0;

    Serial.print("Scatter Delta -> G: ");
    Serial.print(deltaGreen);
    Serial.print("\tR: ");
    Serial.print(deltaRed);
    Serial.print("\tIR: ");
    Serial.print(deltaIR);
    Serial.print("\t| Status: ");

    // Particle Classification Logic
    if (deltaGreen > SMOKE_THRESHOLD && deltaGreen > deltaIR) {
      Serial.println("[WARNING] Fine Particles / Smoke Detected!");
    } else if (deltaIR > DUST_THRESHOLD || deltaRed > DUST_THRESHOLD) {
      Serial.println("[NOTICE] Larger Particles / Dust Detected!");
    } else {
      Serial.println("Air Clean");
    }
  }

  delay(100);
}