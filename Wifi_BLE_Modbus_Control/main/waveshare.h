#ifndef WAVESHARE_MODBUS_H
#define WAVESHARE_MODBUS_H

#include "modbus_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define NUM_OF_MODBUS_DEVICES 2
#define MODBUS_ADD_DEVICE_ONE 0x01
#define MODBUS_ADD_DEVICE_TWO 0x02

#define UART_PORT      UART_NUM_2
#define UART_TX_PIN    17
#define UART_RX_PIN    16
#define RS485_DE_RE    4   // Connect to DE+RE (tie together). Set to -1 if using plain TTL w/o transceiver

extern QueueHandle_t bleCommandQue;


#define RELAY_1 GPIO_NUM_1 
#define RELAY_2 GPIO_NUM_2 
#define RELAY_3 GPIO_NUM_41 
#define RELAY_4 GPIO_NUM_42 
#define RELAY_5 GPIO_NUM_45 
#define RELAY_6 GPIO_NUM_46 



typedef struct 
{
    uint8_t _has_multiple_devices;
    uint8_t _is_modbus_device;
    uint8_t _device_addresses[NUM_OF_MODBUS_DEVICES];
    uint8_t _relay_on_gateway;
    uint32_t relay_status_on_device[NUM_OF_MODBUS_DEVICES];
}device_add_t;

typedef device_add_t (* func_ptr_t)(void);

typedef struct 
{
    uint8_t count;
    uint8_t add[10];
}modbus_device_st;

typedef struct {
    const char *cmd;
    func_ptr_t func;
} command_t;

void modbus_task(void *pvParameters);

device_add_t button_operation_1();
device_add_t button_operation_2();
device_add_t button_operation_3();

extern command_t command_table[70];

// typedef 
// {
//     RELAY_TURN_OFF =0x0000,
//     RELAY_TURN_ON =0xFF00,    
//     RELAY_TURN_TOGGLE = 0x5500;
// }




void wsh_operate_single_relay(uint8_t _device_add, uint8_t _relay_num, uint8_t _status);

void wsh_operate_multiple_relay(uint8_t _device_add, uint16_t  _masked_relay_num, uint8_t _status);



#endif