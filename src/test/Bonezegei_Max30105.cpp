/*
  Bonezegei_Max30105.cpp
  Arduino Library for MAX30105 Optical Sensor with Heart Rate & SpO2
*/

#include "Bonezegei_Max30105.h"

Bonezegei_Max30105::Bonezegei_Max30105(uint8_t address) {
    _address = address;
    _wire = &Wire;
    _activeLEDs = 3;
    _red = 0;
    _ir = 0;
    _green = 0;

    _dcRed = 0.0f;
    _dcIR = 0.0f;
    _acRedMax = -99999.0f;
    _acRedMin = 99999.0f;
    _acIRMax = -99999.0f;
    _acIRMin = 99999.0f;

    _lastBeatTime = 0;
    _peakDetected = false;
    _bpm = 0.0f;
    _spo2 = 0.0f;
}

bool Bonezegei_Max30105::begin(TwoWire &wirePort) {
    _wire = &wirePort;
    _wire->begin();

    if (!isConnected()) {
        return false;
    }

    softReset();
    return true;
}

bool Bonezegei_Max30105::isConnected() {
    _wire->beginTransmission(_address);
    return (_wire->endTransmission() == 0);
}

uint8_t Bonezegei_Max30105::readPartID() {
    return readRegister(MAX30105_PART_ID);
}

void Bonezegei_Max30105::softReset() {
    writeRegister(MAX30105_MODE_CONFIG, 0x40);
    delay(100);
}

void Bonezegei_Max30105::setup(uint8_t ledBrightness, uint8_t sampleAverage, uint8_t mode, uint8_t sampleRate, uint8_t pulseWidth, uint8_t adcRange) {
    uint8_t fifoCfg = 0x00;
    if (sampleAverage == 2) fifoCfg |= (1 << 5);
    else if (sampleAverage == 4) fifoCfg |= (2 << 5);
    else if (sampleAverage == 8) fifoCfg |= (3 << 5);
    else if (sampleAverage == 16) fifoCfg |= (4 << 5);
    else if (sampleAverage == 32) fifoCfg |= (5 << 5);
    fifoCfg |= (1 << 4);
    writeRegister(MAX30105_FIFO_CONFIG, fifoCfg);

    writeRegister(MAX30105_MODE_CONFIG, mode & 0x07);

    if (mode == MAX30105_MODE_REDONLY) _activeLEDs = 1;
    else if (mode == MAX30105_MODE_REDIR) _activeLEDs = 2;
    else _activeLEDs = 3;

    uint8_t spo2Cfg = ((adcRange & 0x03) << 5) | ((sampleRate & 0x07) << 2) | (pulseWidth & 0x03);
    writeRegister(MAX30105_SPO2_CONFIG, spo2Cfg);

    writeRegister(MAX30105_LED1_PULSE_AMP, ledBrightness);
    if (_activeLEDs >= 2) writeRegister(MAX30105_LED2_PULSE_AMP, ledBrightness);
    if (_activeLEDs >= 3) writeRegister(MAX30105_LED3_PULSE_AMP, ledBrightness);

    if (mode == MAX30105_MODE_MULTILED) {
        writeRegister(MAX30105_MULTI_LED_MODE1, 0x21);
        writeRegister(MAX30105_MULTI_LED_MODE2, 0x03);
    }

    writeRegister(MAX30105_FIFO_WR_PTR, 0x00);
    writeRegister(MAX30105_OVF_COUNTER, 0x00);
    writeRegister(MAX30105_FIFO_RD_PTR, 0x00);
}

bool Bonezegei_Max30105::check() {
    uint8_t readPtr = readRegister(MAX30105_FIFO_RD_PTR);
    uint8_t writePtr = readRegister(MAX30105_FIFO_WR_PTR);

    if (readPtr == writePtr) {
        return false;
    }

    int numSamples = (int)writePtr - (int)readPtr;
    if (numSamples < 0) numSamples += 32;

    uint8_t bytesToRead = _activeLEDs * 3;
    uint8_t buffer[9];

    for (int i = 0; i < numSamples; i++) {
        readBytes(MAX30105_FIFO_DATA, buffer, bytesToRead);

        if (_activeLEDs >= 1) {
            _red = ((uint32_t)buffer[0] << 16 | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2]) & 0x03FFFF;
        }
        if (_activeLEDs >= 2) {
            _ir = ((uint32_t)buffer[3] << 16 | (uint32_t)buffer[4] << 8 | (uint32_t)buffer[5]) & 0x03FFFF;
        }
        if (_activeLEDs >= 3) {
            _green = ((uint32_t)buffer[6] << 16 | (uint32_t)buffer[7] << 8 | (uint32_t)buffer[8]) & 0x03FFFF;
        }

        processBiometrics();
    }

    return true;
}

void Bonezegei_Max30105::processBiometrics() {
    // Check if finger is present
    if (!isFingerDetected()) {
        _bpm = 0.0f;
        _spo2 = 0.0f;
        _dcRed = 0.0f;
        _dcIR = 0.0f;
        return;
    }

    // Exponential moving average filter for DC estimation
    if (_dcRed == 0.0f) _dcRed = (float)_red;
    else _dcRed = 0.95f * _dcRed + 0.05f * (float)_red;

    if (_dcIR == 0.0f) _dcIR = (float)_ir;
    else _dcIR = 0.95f * _dcIR + 0.05f * (float)_ir;

    // Isolate AC components
    float acRed = (float)_red - _dcRed;
    float acIR  = (float)_ir  - _dcIR;

    // Track peak-to-peak AC amplitudes
    if (acRed > _acRedMax) _acRedMax = acRed;
    if (acRed < _acRedMin) _acRedMin = acRed;
    if (acIR  > _acIRMax)  _acIRMax  = acIR;
    if (acIR  < _acIRMin)  _acIRMin  = acIR;

    // Dynamic peak detection on IR channel
    unsigned long now = millis();
    if (acIR > 30.0f && !_peakDetected && (now - _lastBeatTime > 300)) { // Max ~200 BPM
        _peakDetected = true;
        unsigned long duration = now - _lastBeatTime;
        _lastBeatTime = now;

        if (duration > 300 && duration < 2000) { // Valid 30-200 BPM window
            float currentBPM = 60000.0f / (float)duration;
            _bpm = (_bpm == 0.0f) ? currentBPM : (0.7f * _bpm + 0.3f * currentBPM);

            // Compute ratio of ratios (R)
            float redAmp = _acRedMax - _acRedMin;
            float irAmp  = _acIRMax  - _acIRMin;

            if (_dcRed > 0 && _dcIR > 0 && irAmp > 0) {
                float rRatio = (redAmp / _dcRed) / (irAmp / _dcIR);
                float calculatedSpO2 = 104.0f - (17.0f * rRatio);

                if (calculatedSpO2 > 100.0f) calculatedSpO2 = 100.0f;
                if (calculatedSpO2 < 70.0f)  calculatedSpO2 = 70.0f;

                _spo2 = (_spo2 == 0.0f) ? calculatedSpO2 : (0.8f * _spo2 + 0.2f * calculatedSpO2);
            }

            // Reset cycle peak trackers
            _acRedMax = -99999.0f; _acRedMin = 99999.0f;
            _acIRMax  = -99999.0f; _acIRMin  = 99999.0f;
        }
    } else if (acIR < 0.0f) {
        _peakDetected = false;
    }
}

bool Bonezegei_Max30105::isFingerDetected() {
    return (_ir > 20000);
}

uint32_t Bonezegei_Max30105::getRed() { return _red; }
uint32_t Bonezegei_Max30105::getIR() { return _ir; }
uint32_t Bonezegei_Max30105::getGreen() { return _green; }
float Bonezegei_Max30105::getBPM() { return _bpm; }
float Bonezegei_Max30105::getSpO2() { return _spo2; }

float Bonezegei_Max30105::readTemperature() {
    writeRegister(MAX30105_DIE_TEMP_CONFIG, 0x01);

    for (int i = 0; i < 100; i++) {
        if ((readRegister(MAX30105_DIE_TEMP_CONFIG) & 0x01) == 0) break;
        delay(1);
    }

    int8_t tempInt = (int8_t)readRegister(MAX30105_DIE_TEMP_INT);
    uint8_t tempFrac = readRegister(MAX30105_DIE_TEMP_FRAC);

    return (float)tempInt + ((float)tempFrac * 0.0625f);
}

void Bonezegei_Max30105::writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

uint8_t Bonezegei_Max30105::readRegister(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, (uint8_t)1);
    if (_wire->available()) {
        return _wire->read();
    }
    return 0;
}

void Bonezegei_Max30105::readBytes(uint8_t reg, uint8_t *buffer, uint8_t length) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, length);
    for (uint8_t i = 0; i < length; i++) {
        if (_wire->available()) {
            buffer[i] = _wire->read();
        }
    }
}