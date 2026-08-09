#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Axp2101"

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
}

int Axp2101::GetBatteryCurrentDirection() {
    uint8_t value = 0;
    if (!TryReadReg(0x01, value)) {
        return -1; // I2C failed; caller treats -1 as "unknown"
    }
    return (value & 0b01100000) >> 5;
}

bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;
}

bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;
}

bool Axp2101::IsChargingDone() {
    uint8_t value = 0;
    if (!TryReadReg(0x01, value)) {
        return false;
    }
    return (value & 0b00000111) == 0b00000100;
}

int Axp2101::GetBatteryLevel() {
    uint8_t value = 0;
    if (!TryReadReg(0xA4, value)) {
        return -1; // out-of-range; boards reject it and keep previous icon
    }
    return value;
}

float Axp2101::GetTemperature() {
    uint8_t value = 0;
    if (!TryReadReg(0xA5, value)) {
        return -1.0f;
    }
    return value;
}

void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    value = value | 0x01;
    WriteReg(0x10, value);
}
