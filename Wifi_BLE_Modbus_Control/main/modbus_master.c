
// =========================
// File: examples/main/modbus_master_example.c
// =========================
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "modbus_master.h"


/* Example for Using library */
/* 
#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    17
#define UART_RX_PIN    16
#define RS485_DE_RE    4   // Connect to DE+RE (tie together). Set to -1 if using plain TTL w/o transceiver

static const char *TAG_EX = "MB_EX";

void app_main(void)
{
    modbus_rtu_master_config_t cfg = {
        .uart_port   = UART_PORT,
        .baudrate    = 9600,
        .tx_pin      = UART_TX_PIN,
        .rx_pin      = UART_RX_PIN,
        .de_re_pin   = RS485_DE_RE,
        .parity      = UART_PARITY_DISABLE,
        .stop_bits   = UART_STOP_BITS_1,
        .rx_buf_size = 256,
        .tx_buf_size = 256,
        .io_timeout  = pdMS_TO_TICKS(50),
    };

    modbus_rtu_master_handle_t h = modbus_rtu_master_init(&cfg);
    if (!h) {
        ESP_LOGE(TAG_EX, "Failed to init Modbus master");
        return;
    }

    uint8_t slave = 1;

    while (1) {
        uint16_t regs[4] = {0};
        esp_err_t err = modbus_read_holding_registers(h, slave, 0x0000, 4, regs, 4);
        if (err == ESP_OK) {
            ESP_LOGI(TAG_EX, "HR[0..3]= %u %u %u %u", regs[0], regs[1], regs[2], regs[3]);
        } else {
            ESP_LOGW(TAG_EX, "Read HR failed: %s", esp_err_to_name(err));
        }

        uint16_t val = 0x1234;
        err = modbus_write_single_register(h, slave, 0x0001, val);
        if (err == ESP_OK) {
            ESP_LOGI(TAG_EX, "Wrote SR addr 1 = 0x%04X", val);
        } else {
            ESP_LOGW(TAG_EX, "Write SR failed: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
*/
/*
// =========================
// File: examples/main/CMakeLists.txt
// =========================
idf_component_register(SRCS "modbus_master_example.c"
                       INCLUDE_DIRS "../.." "../../include"
                       PRIV_REQUIRES driver)

// =========================
// File: CMakeLists.txt (root of this component)
// =========================
cmake_minimum_required(VERSION 3.16)
set(COMPONENTS driver)

# As a component
idf_component_register(SRCS "src/modbus_rtu_master.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver)
*/


// =========================
// File: src/modbus_rtu_master.c
// =========================
#include <string.h>
#include <stdlib.h>
#include "modbus_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "time.h"
#include "esp_timer.h"
#include "string.h"
#include "memory.h"

#define TAG "MODBUS_M"

struct modbus_rtu_master {
    modbus_rtu_master_config_t cfg;
    SemaphoreHandle_t lock;
};

static uint16_t crc16_modbus(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 0; i < 8; i++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001; else crc >>= 1;
        }
    }
    return crc;
}

static inline void rs485_tx_enable(modbus_rtu_master_handle_t h, bool enable)
{
    if (h->cfg.de_re_pin >= 0) {
        gpio_set_level(h->cfg.de_re_pin, enable ? 1 : 0);
    }
}

static esp_err_t send_and_recv(modbus_rtu_master_handle_t h, const uint8_t *pdu, int pdu_len, uint8_t *rx, int rx_len, int *out_len)
{
    // Build ADU: [Addr][PDU...][CRC_L][CRC_H]
    uint8_t txbuf[256];
    if (pdu_len + 3 > sizeof(txbuf)) return ESP_ERR_NO_MEM;
    memcpy(txbuf, pdu, pdu_len);
    uint16_t crc = crc16_modbus(txbuf, pdu_len);
    txbuf[pdu_len++] = crc & 0xFF;
    txbuf[pdu_len++] = (crc >> 8) & 0xFF;

    // Critical section per request
    if (xSemaphoreTake(h->lock, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;

    // 3.5 char times silent interval (approx). For simplicity, a small delay before TX
    int char_us = (int)(1000000.0 * (1.0 / (float)h->cfg.baudrate) * 11.0); // 1 char ~ 11 bits
    //ets_delay_us(char_us * 4);

    rs485_tx_enable(h, true);
    uart_flush_input(h->cfg.uart_port);
    uart_write_bytes(h->cfg.uart_port, (const char *)txbuf, pdu_len);
    uart_wait_tx_done(h->cfg.uart_port, pdMS_TO_TICKS(100));
    rs485_tx_enable(h, false);

    // Read response
    int total = 0; int n;
    int64_t start = esp_timer_get_time();
    while (total < rx_len) {
        n = uart_read_bytes(h->cfg.uart_port, rx + total, rx_len - total, h->cfg.io_timeout);
        if (n > 0) {
            total += n;
            // Heuristic: break after inter-char timeout (~ 1.5 char times)
            if (esp_timer_get_time() - start > 20000) { // 20ms without more is likely done for low bauds
                break;
            }
        } else {
            break; // timeout
        }
    }

    xSemaphoreGive(h->lock);

    if (total < 5) return ESP_ERR_TIMEOUT; // addr+fc+len/crc minimal

    // Verify CRC at tail
    uint16_t rcrc = (rx[total-1] << 8) | rx[total-2];
    uint16_t ccrc = crc16_modbus(rx, total-2);
    if (rcrc != ccrc) return ESP_ERR_INVALID_CRC;

    if (out_len) *out_len = total;
    return ESP_OK;
}

modbus_rtu_master_handle_t modbus_rtu_master_init(const modbus_rtu_master_config_t *cfg)
{
    if (!cfg) return NULL;
    modbus_rtu_master_handle_t h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->cfg = *cfg;
    h->lock = xSemaphoreCreateMutex();
    if (!h->lock) { free(h); return NULL; }

    uart_config_t ucfg = {
        .baud_rate = cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = cfg->parity,
        .stop_bits = cfg->stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_driver_install(cfg->uart_port, cfg->rx_buf_size, cfg->tx_buf_size, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(cfg->uart_port, &ucfg));
    ESP_ERROR_CHECK(uart_set_pin(cfg->uart_port, cfg->tx_pin, cfg->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    if (cfg->de_re_pin >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << cfg->de_re_pin,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = 0,
            .pull_down_en = 0,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io));
        rs485_tx_enable(h, false); // receive default
    }

    return h;
}

void modbus_rtu_master_deinit(modbus_rtu_master_handle_t h)
{
    if (!h) return;
    uart_driver_delete(h->cfg.uart_port);
    if (h->lock) vSemaphoreDelete(h->lock);
    free(h);
}

// ---- Helpers to build PDUs ----
static esp_err_t fc01_fc02_read_bits(modbus_rtu_master_handle_t h, uint8_t slave, uint8_t fc, uint16_t addr, uint16_t qty, uint8_t *out, size_t out_sz, size_t *out_len)
{
    uint8_t pdu[8];
    int idx = 0;
    pdu[idx++] = slave;
    pdu[idx++] = fc;
    pdu[idx++] = addr >> 8; pdu[idx++] = addr & 0xFF;
    pdu[idx++] = qty  >> 8; pdu[idx++] = qty  & 0xFF;
    int rlen;
    uint8_t rx[256];
    esp_err_t err = send_and_recv(h, pdu, idx, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rx[0] != slave || rx[1] != fc) return ESP_ERR_INVALID_RESPONSE;
    uint8_t byte_count = rx[2];
    if (byte_count + 5 != rlen) return ESP_ERR_INVALID_SIZE;
    if (out) {
        if (byte_count > out_sz) return ESP_ERR_NO_MEM;
        memcpy(out, &rx[3], byte_count);
        if (out_len) *out_len = byte_count;
    }
    return ESP_OK;
}

esp_err_t modbus_read_coils(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint8_t *coil_bytes, size_t coil_bytes_len, size_t *out_len)
{
    return fc01_fc02_read_bits(h, slave_addr, 0x01, start_addr, quantity, coil_bytes, coil_bytes_len, out_len);
}

esp_err_t modbus_read_discrete_inputs(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint8_t *input_bytes, size_t input_bytes_len, size_t *out_len)
{
    return fc01_fc02_read_bits(h, slave_addr, 0x02, start_addr, quantity, input_bytes, input_bytes_len, out_len);
}

static esp_err_t fc03_fc04_read_regs(modbus_rtu_master_handle_t h, uint8_t slave, uint8_t fc, uint16_t addr, uint16_t qty, uint16_t *regs, size_t regs_len)
{
    uint8_t pdu[8];
    int idx = 0;
    pdu[idx++] = slave;
    pdu[idx++] = fc;
    pdu[idx++] = addr >> 8; pdu[idx++] = addr & 0xFF;
    pdu[idx++] = qty  >> 8; pdu[idx++] = qty  & 0xFF;

    int rlen; uint8_t rx[256];
    esp_err_t err = send_and_recv(h, pdu, idx, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rx[0] != slave || rx[1] != fc) return ESP_ERR_INVALID_RESPONSE;
    uint8_t byte_count = rx[2];
    if (byte_count + 5 != rlen) return ESP_ERR_INVALID_SIZE;
    if (byte_count != qty * 2) return ESP_ERR_INVALID_SIZE;
    if (!regs || regs_len < qty) return ESP_ERR_NO_MEM;
    for (int i = 0; i < qty; i++) {
        regs[i] = (rx[3 + i*2] << 8) | rx[4 + i*2];
    }
    return ESP_OK;
}

esp_err_t modbus_read_holding_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint16_t *registers, size_t regs_len)
{
    return fc03_fc04_read_regs(h, slave_addr, 0x03, start_addr, quantity, registers, regs_len);
}

esp_err_t modbus_read_input_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, uint16_t *registers, size_t regs_len)
{
    return fc03_fc04_read_regs(h, slave_addr, 0x04, start_addr, quantity, registers, regs_len);
}

esp_err_t modbus_write_single_coil(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t addr, bool on)
{
    uint8_t pdu[8]; int idx = 0;
    pdu[idx++] = slave_addr; pdu[idx++] = 0x05;
    pdu[idx++] = addr >> 8; pdu[idx++] = addr & 0xFF;
    uint16_t val = on ? 0xFF00 : 0x0000;
    pdu[idx++] = val >> 8; pdu[idx++] = val & 0xFF;
    int rlen; uint8_t rx[256];
    esp_err_t err = send_and_recv(h, pdu, idx, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rlen < 8 || rx[0] != slave_addr || rx[1] != 0x05) return ESP_ERR_INVALID_RESPONSE;
    return ESP_OK;
}

esp_err_t modbus_write_buffer(modbus_rtu_master_handle_t h, uint8_t* _buffer, uint8_t len, uint8_t* _response_buf)
{
    int rlen =0; uint8_t rx[256];
    uint8_t rx_len;
    esp_err_t err = send_and_recv(h, _buffer, len, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rlen < 8 || rx[0] != _buffer[0] || rx[1] != _buffer[1]) return ESP_ERR_INVALID_RESPONSE;
    memcpy(_response_buf, rx, rlen );
    return ESP_OK;
}
esp_err_t modbus_write_single_register(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t addr, uint16_t value)
{
    uint8_t pdu[8]; int idx = 0;
    pdu[idx++] = slave_addr; pdu[idx++] = 0x06;
    pdu[idx++] = addr >> 8; pdu[idx++] = addr & 0xFF;
    pdu[idx++] = value >> 8; pdu[idx++] = value & 0xFF;

    int rlen; uint8_t rx[256];
    esp_err_t err = send_and_recv(h, pdu, idx, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rlen < 8 || rx[0] != slave_addr || rx[1] != 0x06) return ESP_ERR_INVALID_RESPONSE;
    return ESP_OK;
}

esp_err_t modbus_write_multiple_registers(modbus_rtu_master_handle_t h, uint8_t slave_addr, uint16_t start_addr, uint16_t quantity, const uint16_t *values)
{
    if (quantity == 0 || quantity > 123) return ESP_ERR_INVALID_ARG;
    uint8_t pdu[256]; int idx = 0;
    pdu[idx++] = slave_addr; pdu[idx++] = 0x10;
    pdu[idx++] = start_addr >> 8; pdu[idx++] = start_addr & 0xFF;
    pdu[idx++] = quantity >> 8;   pdu[idx++] = quantity & 0xFF;
    pdu[idx++] = quantity * 2; // byte count
    for (int i = 0; i < quantity; i++) {
        pdu[idx++] = values[i] >> 8;
        pdu[idx++] = values[i] & 0xFF;
    }

    int rlen; uint8_t rx[64];
    esp_err_t err = send_and_recv(h, pdu, idx, rx, sizeof(rx), &rlen);
    if (err != ESP_OK) return err;
    if (rlen < 8 || rx[0] != slave_addr || rx[1] != 0x10) return ESP_ERR_INVALID_RESPONSE;
    return ESP_OK;
}

