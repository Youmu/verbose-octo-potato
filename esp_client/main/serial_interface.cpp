#include "serial_interface.hpp"

#include <cctype>
#include <cstdio>
#include <vector>

#include "esp_log.h"

static const char *TAG = "UART_IF";

namespace espclient{
constexpr int bufferSize = 2048;
constexpr TickType_t kReadTimeoutTicks = pdMS_TO_TICKS(100);

SerialInterface::SerialInterface(int uartDevNum, int boudrate)
    : uart_port_(static_cast<uart_port_t>(uartDevNum)),
      baudrate_(boudrate),
      started_(false),
      running_(false),
      stop_requested_(false)
{ 
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = boudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {0,0}
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(uart_port_, bufferSize * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(uart_port_, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_port_, CONFIG_POTATO_PIN_UART_TX, CONFIG_POTATO_PIN_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    buffer = (uint8_t *) malloc(bufferSize);
}

SerialInterface::~SerialInterface()
{
    Stop();
    uart_driver_delete(uart_port_);
}

void SerialInterface::Start()
{
    if (started_) {
        return;
    }
    stop_requested_ = false;
    BaseType_t created = xTaskCreate(
        [](void* pSelf){
            auto self = reinterpret_cast<SerialInterface*>(pSelf);
            self->DoReadLoop();
        },
        "serial_rx",
        4096,
        this,
        5,
        &task_handle_);

    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create serial reader task");
        task_handle_ = nullptr;
        return;
    }
    started_ = true;
    ESP_LOGI(TAG, "SerialInterface started");
}

void SerialInterface::Stop()
{
    if (!started_) {
        return;
    }

    stop_requested_ = true;
    while (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    started_ = false;
}

void SerialInterface::SetOnReceive(std::function<void(const std::string)> onReceive)
{
    on_receive_ = onReceive;
}

void SerialInterface::SendMessage(const std::string &msg) const
{
    std::string with_cr = msg;
    with_cr.push_back('\r');
    with_cr.push_back('\n');
    uart_write_bytes(
        uart_port_,
        with_cr.c_str(),
        static_cast<size_t>(with_cr.size()));
}

void SerialInterface::DoReadLoop()
{
    
    ESP_LOGI(TAG, "DoReadLoop ENTER.");
    std::vector<uint8_t> rx_buffer(128);
    std::string line;

    while (!stop_requested_) {
        int len = uart_read_bytes(
            uart_port_,
            rx_buffer.data(),
            rx_buffer.size(),
            kReadTimeoutTicks);
        if (len <= 0) {
            continue;
        }

        for (int i = 0; i < len; ++i) {
            char ch = static_cast<char>(rx_buffer[i]);
            if (ch == '\r' || ch == '\n') {
                if (!line.empty() && on_receive_ != nullptr) {
                    on_receive_(line);
                }
                line.clear();
            } else {
                line.push_back(ch);
            }
        }
    }
    ESP_LOGI(TAG, "Stopped...");

    TaskHandle_t task_to_delete = task_handle_;
    task_handle_ = nullptr;
    vTaskDelete(task_to_delete);
}

}