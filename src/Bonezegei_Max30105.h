/*
  Bonezegei_Max30105.h
  Arduino Library for MAX30105 Optical Sensor with Heart Rate & SpO2
*/

#ifndef BONEZEGEI_MAX30105_H
#define BONEZEGEI_MAX30105_H

#include <Arduino.h>
#include <Wire.h>

#define MAX30105_ADDRESS          0x57

// Registers
#define MAX30105_INTR_STATUS_1    0x00
#define MAX30105_INTR_STATUS_2    0x01
#define MAX30105_INTR_ENABLE_1    0x02
#define MAX30105_INTR_ENABLE_2    0x03
#define MAX30105_FIFO_WR_PTR      0x04
#define MAX30105_OVF_COUNTER      0x05
#define MAX30105_FIFO_RD_PTR      0x06
#define MAX30105_FIFO_DATA        0x07
#define MAX30105_FIFO_CONFIG      0x08
#define MAX30105_MODE_CONFIG      0x09
#define MAX30105_SPO2_CONFIG      0x0A
#define MAX30105_LED1_PULSE_AMP   0x0C // Red
#define MAX30105_LED2_PULSE_AMP   0x0D // IR
#define MAX30105_LED3_PULSE_AMP   0x0E // Green
#define MAX30105_MULTI_LED_MODE1  0x11
#define MAX30105_MULTI_LED_MODE2  0x12
#define MAX30105_DIE_TEMP_INT     0x1F
#define MAX30105_DIE_TEMP_FRAC    0x20
#define MAX30105_DIE_TEMP_CONFIG  0x21
#define MAX30105_PART_ID          0xFF

// LED Operating Modes
#define MAX30105_MODE_REDONLY     0x02
#define MAX30105_MODE_REDIR       0x03
#define MAX30105_MODE_MULTILED    0x07

class Bonezegei_Max30105 {
public:
    Bonezegei_Max30105(uint8_t address = MAX30105_ADDRESS);

    bool begin(TwoWire &wirePort = Wire);
    bool isConnected();
    uint8_t readPartID();
    void softReset();

    void setup(uint8_t ledBrightness = 0x1F, 
               uint8_t sampleAverage = 4, 
               uint8_t mode = MAX30105_MODE_MULTILED, 
               uint8_t sampleRate = 0x03, 
               uint8_t pulseWidth = 0x03, 
               uint8_t adcRange = 0x02);

    bool check();
    uint32_t getRed();
    uint32_t getIR();
    uint32_t getGreen();
    float readTemperature();

    // Pulse & Oximeter Methods
    bool isFingerDetected();
    float getBPM();
    float getSpO2();

private:
    TwoWire *_wire;
    uint8_t _address;
    uint8_t _activeLEDs;

    uint32_t _red;
    uint32_t _ir;
    uint32_t _green;

    // DSP & Biometric Processing State
    float _dcRed;
    float _dcIR;
    float _acRedMax;
    float _acRedMin;
    float _acIRMax;
    float _acIRMin;

    unsigned long _lastBeatTime;
    bool _peakDetected;
    float _bpm;
    float _spo2;

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    void readBytes(uint8_t reg, uint8_t *buffer, uint8_t length);
    void processBiometrics();
};

#endif // BONEZEGEI_MAX30105_H