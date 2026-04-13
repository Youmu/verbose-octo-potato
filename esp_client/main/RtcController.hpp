#pragma once
#include "driver/i2c_master.h"

namespace espclient {

struct DS3231_data{
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

class RtcController {
private:
    static constexpr uint8_t DS3231_RTC_ADDR = 0x68; // I2C address of the RTC  module
    static constexpr i2c_port_num_t I2C_PORT_NUM = I2C_NUM_0; // I2C port number
    static constexpr gpio_num_t SDA_GPIO = GPIO_NUM_18;
    static constexpr gpio_num_t SCL_GPIO = GPIO_NUM_17;
    static constexpr uint32_t I2C_SPEED = 100000; 
    
    i2c_master_bus_handle_t m_bus_handle;
    i2c_master_dev_handle_t m_dev_handle;

    DS3231_data rtc_data = {};
public:
    RtcController();
    ~RtcController();

    void ReadTime(bool update_rtc = false);
    void SetTime(int year, int month, int date, int hours, int minutes, int seconds, int day_of_week);
};

}  // namespace espclient
