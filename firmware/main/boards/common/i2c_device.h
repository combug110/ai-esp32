#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

#include <driver/i2c_master.h>

class I2cDevice {
public:
    I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    // True when the most recent I2C transaction completed successfully.
    // Runtime polling code should check this after Try* calls.
    bool LastTransactionOk() const { return last_transaction_ok_; }

protected:
    i2c_master_dev_handle_t i2c_device_;

    void WriteReg(uint8_t reg, uint8_t value);
    uint8_t ReadReg(uint8_t reg);
    void ReadRegs(uint8_t reg, uint8_t* buffer, size_t length);

    // Non-fatal variants for runtime polling: on bus failure they log an
    // error and return false instead of aborting, so a transient I2C glitch
    // does not reboot the device. 'value' is left untouched when false.
    bool TryReadReg(uint8_t reg, uint8_t& value);
    bool TryWriteReg(uint8_t reg, uint8_t value);

private:
    bool last_transaction_ok_ = true;
};

#endif // I2C_DEVICE_H
