#include "RtcController.hpp"
#include "iostream"
#include "time.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "RTC";


namespace espclient {

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

RtcController::RtcController(){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = SDA_GPIO, 
        .scl_io_num = SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 16,
        .flags = {
            .enable_internal_pullup = 1,
            .allow_pd = 0
        }
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &m_bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_RTC_ADDR,
        .scl_speed_hz = I2C_SPEED,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0
        }
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(m_bus_handle, &dev_config, &m_dev_handle));
};

RtcController::~RtcController() = default;


void RtcController::ReadTime(bool update_rtc) {
    uint8_t reg_addr;
    // DS3231_ctrl ctrl_data = {};

    // reg_addr = 0x0E; // Control register address
    // ESP_ERROR_CHECK(i2c_master_transmit_receive(
    //     m_dev_handle,
    //     &reg_addr, 1, // Write the register address we want to read from
    //     reinterpret_cast<uint8_t*>(&ctrl_data), sizeof(ctrl_data), // Data to read
    //     1000 // 1 second timeout
    // ));

    reg_addr = 0x00; // Reset register address to read time data
    ESP_ERROR_CHECK(i2c_master_transmit_receive(
        m_dev_handle,
        &reg_addr, 1, // Write the register address we want to read from
        reinterpret_cast<uint8_t*>(&rtc_data), sizeof(rtc_data), // Data to read
        1000 // 1 second timeout
    ));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "I2C transaction completed");

    int day_of_week = rtc_data.B3.day;
    int year = rtc_data.B6.year + rtc_data.B6.year_ten * 10 + (rtc_data.B5.century ? 1900 : 2000);
    int month = rtc_data.B5.month + rtc_data.B5.month_tens * 10;
    int date = rtc_data.B4.date + rtc_data.B4.date_tens * 10;

    struct tm time_info = {};
    time_info.tm_sec = rtc_data.B0.seconds + rtc_data.B0.seconds_tens * 10;;
    time_info.tm_min = rtc_data.B1.minutes + rtc_data.B1.minutes_tens * 10;; 
    time_info.tm_hour = rtc_data.B2_12h.h12_24 ? 
        (rtc_data.B2_12h.hour + (rtc_data.B2_12h.h_10 * 10) + (rtc_data.B2_12h.h_ampm ? 12 : 0)) : 
        (rtc_data.B2_24h.hour + (rtc_data.B2_24h.hour_ten * 10));

    time_info.tm_mday = date;
    time_info.tm_mon = month - 1; // tm_mon is 0-based
    time_info.tm_year = year - 1900; // tm_year is years since
    time_info.tm_wday = (day_of_week % 7); // tm_wday is 0-based, with Sunday=0

    if(update_rtc) {
        time_t rtc_time = mktime(&time_info);
        struct timespec ts = {
            .tv_sec = rtc_time,
            .tv_nsec = 0
        };
        clock_settime(CLOCK_REALTIME, &ts); // Set system time to RTC time
     }
    ESP_LOGI(TAG, "Current RTC time: %02d:%02d:%02d", time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    ESP_LOGI(TAG, "Date: %02d/%02d/%d", time_info.tm_mday, (time_info.tm_mon + 1), (time_info.tm_year + 1900));
}



void RtcController::SetTime(int year, int month, int date, int hours, int minutes, int seconds, int day_of_week) {
    struct buf{
        uint8_t reg_addr;
        DS3231_data data;
    } buffer = {};
    buffer.reg_addr = 0x00; // Start writing from the seconds register

    buffer.data.B0.seconds = seconds % 10;
    buffer.data.B0.seconds_tens = seconds / 10;
    buffer.data.B1.minutes = minutes % 10;
    buffer.data.B1.minutes_tens = minutes / 10;
    buffer.data.B2_24h.h_2224 = 0; // 24-hour mode
    buffer.data.B2_24h.hour = hours % 10;
    buffer.data.B2_24h.hour_ten = hours / 10;
    buffer.data.B3.day = day_of_week;
    buffer.data.B4.date = date % 10;
    buffer.data.B4.date_tens = date / 10;
    buffer.data.B5.month = month % 10;
    buffer.data.B5.month_tens = month / 10;
    buffer.data.B5.century = (year >= 2000) ? 0 : 1; // Set century bit based on year
    int year_short = year % 100; // Get last two digits of the year
    buffer.data.B6.year = year_short % 10;
    buffer.data.B6.year_ten = year_short / 10;

    i2c_master_transmit(
        m_dev_handle,
        reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer),
        1000 // 1 second timeout
    );
}

}  // namespace espclient
