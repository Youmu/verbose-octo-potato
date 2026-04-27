#pragma once
#include "driver/i2c_master.h"

namespace espclient {

struct DS3231_time{
    struct {
        uint8_t seconds : 4;
        uint8_t seconds_tens : 3;
        uint8_t reserved : 1;
    } B0;
    struct {
        uint8_t minutes : 4;
        uint8_t minutes_tens : 3;
        uint8_t reserved : 1;
    } B1;
    union{
        struct {
            uint8_t hour : 4; // Hour units
            uint8_t hour_ten : 2; // 10-hour bits 
            uint8_t h_2224 : 1; // 10-hour bit
            uint8_t reserved: 1;
        } B2_24h;
        struct {
            uint8_t hour : 4; // Hour units
            uint8_t h_10 : 1; // 10-hour bit
            uint8_t h_ampm: 1; // AM/PM bit (0 for AM, 1 for PM)
            uint8_t h12_24 : 1; // 0 for 24-hour mode, 1 for 12-hour mode
            uint8_t reserved : 1;
        } B2_12h;
    };
    struct {
        uint8_t day : 3; // Day of the week (1-7)
        uint8_t reserved : 5;
    } B3;
    struct {
        uint8_t date : 4; // Date of the month (1-31)
        uint8_t date_tens : 2; // 10-date bit
        uint8_t reserved : 2;
    } B4;
    struct {
        uint8_t month : 3; // Month (1-9)
        uint8_t month_tens : 1; // 10-month bit
        uint8_t reserved : 2;
        uint8_t century : 1; // Century bit (0 for 20xx, 1 for 19xx)
    } B5;
    struct {
        uint8_t year : 4; // Year (0-9)
        uint8_t year_ten : 4; // 10-year bit
    } B6;
};

struct DS3231_ctrl{
    // Control Register (0x0E)
    uint16_t a1ie : 1; // Alarm 1 interrupt enable
    uint16_t a2ie : 1; // Alarm 2 interrupt enable
    uint16_t intcn : 1; // Interrupt control (0 for square wave output, 1 for interrupt output)
    uint16_t rs2 : 1; // Square wave rate select bit 2
    uint16_t rs1 : 1; // Square wave rate select bit 1
    uint16_t bbsqw : 1; // Battery-backed square wave enable (0 to enable, 1 to disable)
    uint16_t conv : 1; // Convert temperature (write 1 to start conversion)
    uint16_t en_osc : 1; // Enable oscillator (0 to enable, 1 to disable)

    // Status Register (0x0F)
    uint16_t a1f : 1; // Alarm 1 flag
    uint16_t a2f : 1; // Alarm 2 flag
    uint16_t bsy : 1; // Busy flag (1 when the device is updating time or temperature)
    uint16_t en32kHz : 1; // Enable 32kHz output (0 to enable, 1 to disable)
    uint16_t reserved : 3;
    uint16_t osf : 1; // Oscillator stop flag (1 if the oscillator has stopped)
};

struct DS3231_temp{
    uint8_t temp_lsb; // Temperature LSB (0.25°C resolution)
    uint8_t temp_msb; // Temperature MSB (integer part of temperature)
};


class RtcController {
private:
    static constexpr uint8_t DS3231_RTC_ADDR = 0x68; // I2C address of the RTC  module
    static constexpr i2c_port_num_t I2C_PORT_NUM = I2C_NUM_0; // I2C port number
    static constexpr uint32_t I2C_SPEED = 100000; 
    
    i2c_master_bus_handle_t m_bus_handle;
    i2c_master_dev_handle_t m_dev_handle;

    bool time_valid = false; // Flag to indicate if the time read from RTC is valid
#pragma pack(push, 1)
    struct {
        uint8_t reg_addr;
        DS3231_time data;
    } m_reg_time;

    struct {
        uint8_t reg_addr;
        DS3231_ctrl data;
    } m_reg_ctrl;
#pragma pack(pop)

static_assert(sizeof(RtcController::m_reg_time) == sizeof(DS3231_time) + 1, "m_reg_time struct must be packed without padding");
static_assert(sizeof(RtcController::m_reg_ctrl) == sizeof(DS3231_ctrl) + 1, "m_reg_ctrl struct must be packed without padding");

public:
    RtcController();
    ~RtcController();

    void ReadTime(bool update_rtc = false);
    void SetTime(int year, int month, int date, int hours, int minutes, int seconds, int day_of_week);
    bool IsTimeValid() const { return time_valid; }
};

}  // namespace espclient
