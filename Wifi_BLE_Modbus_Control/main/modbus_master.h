// =========================
// File: include/modbus_rtu_master.h
// =========================
#ifndef MODBUS_RTU_MASTER_H
#define MODBUS_RTU_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Configuration structure ----
typedef struct {
    uart_port_t uart_port;     // e.g., UART_NUM_1
    int baudrate;              // e.g., 9600..921600
    int tx_pin;                // GPIO for UART TX
    int rx_pin;                // GPIO for UART RX
    int de_re_pin;             // GPIO to control DE/RE (RS485 transceiver). Set to -1 if not used
    uart_parity_t parity;      // UART_PARITY_DISABLE / UART_PARITY_EVEN / UART_PARITY_ODD
    uart_stop_bits_t stop_bits;// UART_STOP_BITS_1 / UART_STOP_BITS_2
    int rx_buf_size;           // e.g., 256 or 512
    int tx_buf_size;           // e.g., 256 or 512
    TickType_t io_timeout;     // UART read timeout (ticks). e.g., pdMS_TO_TICKS(100)
} modbus_rtu_master_config_t;

// ---- Opaque handle ----
typedef struct modbus_rtu_master *modbus_rtu_master_handle_t;

// Initialize master. Returns handle or NULL on error.
modbus_rtu_master_handle_t modbus_rtu_master_init(const modbus_rtu_master_config_t *cfg);

// Deinit and free resources
void modbus_rtu_master_deinit(modbus_rtu_master_handle_t handle);

// Basic Modbus function codes
esp_err_t modbus_read_coils(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint8_t *coil_bytes, size_t coil_bytes_len, size_t *out_len);

esp_err_t modbus_read_discrete_inputs(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint8_t *input_bytes, size_t input_bytes_len, size_t *out_len);

esp_err_t modbus_read_holding_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint16_t *registers, size_t regs_len);

esp_err_t modbus_read_input_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint16_t *registers, size_t regs_len);

esp_err_t modbus_write_single_coil(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t addr, bool on);

esp_err_t modbus_write_single_register(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t addr, uint16_t value);

esp_err_t modbus_write_buffer(modbus_rtu_master_handle_t h, uint8_t* _buffer, uint8_t len, uint8_t* _response_buf);


esp_err_t modbus_write_multiple_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, const uint16_t *values);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_RTU_MASTER_H

