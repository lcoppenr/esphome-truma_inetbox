#ifdef USE_ESP32_FRAMEWORK_ESP_IDF

#include "LinBusListener.h"
#include "esphome/core/log.h"

//#ifdef CUSTOM_ESPHOME_UART
//#include "esphome/components/uart/truma_uart_component_esp_idf.h"
//#define ESPHOME_UART uart::truma_IDFUARTComponent
//#else
#include "esphome/components/uart/uart_component_esp_idf.h"
#define ESPHOME_UART uart::IDFUARTComponent
//#endif  // CUSTOM_ESPHOME_UART

#include <driver/uart.h>

namespace esphome {
namespace truma_inetbox {

static const char *const TAG = "truma_inetbox.LinBusListener";

void LinBusListener::setup_framework() {
  auto *uart_comp = static_cast<ESPHOME_UART *>(this->parent_);
  uart_port_t uart_num = static_cast<uart_port_t>(uart_comp->get_hw_serial_number());
  this->uart_num_ = uart_num;

  // Wake the driver reader per byte — the reader task blocks in
  // uart_read_bytes(), so this is what bounds our answer latency.
  esp_err_t err = uart_set_rx_full_threshold(uart_num, 1);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_rx_full_threshold failed: %s", esp_err_to_name(err));
  }

  err = uart_set_rx_timeout(uart_num, 2);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_rx_timeout failed: %s", esp_err_to_name(err));
  }

  // PATCH #6: dedicated reader task (see LinBusListener.h). Priority above
  // the ESPHome main loop so LIN answers preempt housekeeping; pinned to
  // core 1 to stay clear of the WiFi stack on core 0. 8 KB stack: the task
  // runs the frame parser, the app-layer answer builder, and ESP_LOG calls.
  BaseType_t ok = xTaskCreatePinnedToCore(LinBusListener::read_task_trampoline, "truma_lin", 8192, this,
                                          /* priority */ 12, &this->read_task_handle_, /* core */ 1);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create LIN reader task — falling back to nothing; bus will be deaf!");
  } else {
    ESP_LOGCONFIG(TAG, "LIN reader task started (core 1, prio 12)");
  }
}

}  // namespace truma_inetbox
}  // namespace esphome

#undef ESPHOME_UART

#endif  // USE_ESP32_FRAMEWORK_ESP_IDF