#include "RtcController.hpp"
#include "iostream"
#include "time.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RTC";

namespace espclient {

RtcController::RtcController():
    time_valid(false),
    m_reg_time {
        .reg_addr = 0x00, // Start reading from the seconds register
        .data = {}},
    m_reg_ctrl {
        .reg_addr = 0x0E, // Control register address
        .data = {}}
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_RTC_ADDR,
        .scl_speed_hz = I2C_SPEED,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0
        }
    };

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = static_cast<gpio_num_t>(CONFIG_POTATO_PIN_I2C_SDA), 
        .scl_io_num = static_cast<gpio_num_t>(CONFIG_POTATO_PIN_I2C_SCL),
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0, // Use default transaction queue depth for synchronous transactions
        .flags = {
            .enable_internal_pullup = 1,
            .allow_pd = 0
        }
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &m_bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(m_bus_handle, &dev_config, &m_dev_handle));
};
RtcController::~RtcController() = default;


void RtcController::ReadTime(bool update_rtc) {
    ESP_ERROR_CHECK(i2c_master_transmit_receive(
        m_dev_handle,
        &m_reg_ctrl.reg_addr, 1, // Write the register address we want to read from
        reinterpret_cast<uint8_t*>(&m_reg_ctrl.data), sizeof(m_reg_ctrl.data), // Data to read
        1000 // 1 second timeout
    ));

    if(m_reg_ctrl.data.osf) {
        ESP_LOGW(TAG, "Oscillator stop flag is set. RTC time may be inaccurate.");
        time_valid = false;
    } else {
        time_valid = true; 
    }

    ESP_ERROR_CHECK(i2c_master_transmit_receive(
        m_dev_handle,
        &m_reg_time.reg_addr, 1, // Write the register address we want to read from
        reinterpret_cast<uint8_t*>(&m_reg_time.data), sizeof(m_reg_time.data), // Data to read
        1000 // 1 second timeout
    ));

    int day_of_week = m_reg_time.data.B3.day;
    int year = m_reg_time.data.B6.year + m_reg_time.data.B6.year_ten * 10 + (m_reg_time.data.B5.century ? 1900 : 2000);
    int month = m_reg_time.data.B5.month + m_reg_time.data.B5.month_tens * 10;
    int date = m_reg_time.data.B4.date + m_reg_time.data.B4.date_tens * 10;

    struct tm time_info = {};
    time_info.tm_sec = m_reg_time.data.B0.seconds + m_reg_time.data.B0.seconds_tens * 10;;
    time_info.tm_min = m_reg_time.data.B1.minutes + m_reg_time.data.B1.minutes_tens * 10;; 
    time_info.tm_hour = m_reg_time.data.B2_12h.h12_24 ? 
        (m_reg_time.data.B2_12h.hour + (m_reg_time.data.B2_12h.h_10 * 10) + (m_reg_time.data.B2_12h.h_ampm ? 12 : 0)) : 
        (m_reg_time.data.B2_24h.hour + (m_reg_time.data.B2_24h.hour_ten * 10));

    time_info.tm_mday = date;
    time_info.tm_mon = month - 1; // tm_mon is 0-based
    time_info.tm_year = year - 1900; // tm_year is years since
    time_info.tm_wday = (day_of_week % 7); // tm_wday is 0-based, with Sunday=0

    if(update_rtc) {
        time_t rtc_time = mktime(&time_info);
        struct timeval tv = {
            .tv_sec = rtc_time,
            .tv_usec = 0
        };
        settimeofday(&tv, nullptr); // Set system time to RTC time
     }
    ESP_LOGI(TAG, "Current RTC time: %02d:%02d:%02d", time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    ESP_LOGI(TAG, "Date: %02d/%02d/%d", time_info.tm_mday, (time_info.tm_mon + 1), (time_info.tm_year + 1900));
}



void RtcController::SetTime(int year, int month, int date, int hours, int minutes, int seconds, int day_of_week) {
    m_reg_time.data.B0.seconds = seconds % 10;
    m_reg_time.data.B0.seconds_tens = seconds / 10;
    m_reg_time.data.B1.minutes = minutes % 10;
    m_reg_time.data.B1.minutes_tens = minutes / 10;
    m_reg_time.data.B2_24h.h_2224 = 0; // 24-hour mode
    m_reg_time.data.B2_24h.hour = hours % 10;
    m_reg_time.data.B2_24h.hour_ten = hours / 10;
    m_reg_time.data.B3.day = day_of_week;
    m_reg_time.data.B4.date = date % 10;
    m_reg_time.data.B4.date_tens = date / 10;
    m_reg_time.data.B5.month = month % 10;
    m_reg_time.data.B5.month_tens = month / 10;
    m_reg_time.data.B5.century = (year >= 2000) ? 0 : 1; // Set century bit based on year
    int year_short = year % 100; // Get last two digits of the year
    m_reg_time.data.B6.year = year_short % 10;
    m_reg_time.data.B6.year_ten = year_short / 10;

    i2c_master_transmit(
        m_dev_handle,
        reinterpret_cast<uint8_t*>(&m_reg_time), sizeof(m_reg_time),
        1000 // 1 second timeout
    );
    
    m_reg_ctrl.data.osf = 0; // Clear the Oscillator Stop Flag (OSF) after setting the time
    ESP_ERROR_CHECK(i2c_master_transmit(
        m_dev_handle,
        reinterpret_cast<uint8_t*>(&m_reg_ctrl), sizeof(m_reg_ctrl), 
        1000 // 1 second timeout
    ));
}

}  // namespace espclient
