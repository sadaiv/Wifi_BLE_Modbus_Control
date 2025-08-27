#include "waveshare.h"
#include "string.h"
#include "memory.h"
#include "host/util/util.h"
#include "modbus_master.h"

QueueHandle_t bleCommandQue;
const int  uxQueueLength = 10;
const int  uxItemSize= 25;

modbus_rtu_master_config_t cfg = {
        .uart_port   = UART_PORT,
        .baudrate    = 115200,
        .tx_pin      = UART_TX_PIN,
        .rx_pin      = UART_RX_PIN,
        .de_re_pin   = RS485_DE_RE,
        .parity      = UART_PARITY_DISABLE,
        .stop_bits   = UART_STOP_BITS_1,
        .rx_buf_size = 256,
        .tx_buf_size = 256,
        .io_timeout  = pdMS_TO_TICKS(50),
    };


uint8_t get_device_add();

void modbus_task(void *pvParameters)
{
    modbus_rtu_master_handle_t modbus_handle =  modbus_rtu_master_init(&cfg);
    bleCommandQue = xQueueCreate(uxQueueLength, uxItemSize);

    while (1)
    {
        char rxBuffer[25] = {0};
        device_add_t _status = {0};
        uint8_t _is_command_recvd =0;
        if( xQueueReceive(bleCommandQue, &(rxBuffer), (TickType_t)500))
        {   
            for (int i = 0; command_table[i].cmd != NULL; i++)
            {
                if (strcmp(command_table[i].cmd, rxBuffer) == 0) 
                {
                    _status = command_table[i].func(); // Call function via pointer
                    _is_command_recvd = 1;
                    break;
                }

            }
            if (_is_command_recvd == 0)
            {
                continue;
            }
           // device_add_t _status = button_operation_1();
            for (int i = 0; i <2 ; i++)
            {
                if(_status._device_addresses[i] != 0)
                {
                    uint8_t _buf[11] = {0};
                    uint8_t _res_buf[255];
                    _buf[0] = _status._device_addresses[i];
                    _buf[1] = 0x0f;
                    _buf[2] = 0x00; _buf[3] =0x00; _buf[4] = 0x04;
                    _buf[5] = 0x00; _buf[6]= 0x20;
                    _buf[7] = (uint8_t) (( _status.relay_status_on_device[i] ) & 0xFF) ;
                    _buf[8] = (uint8_t) (( _status.relay_status_on_device[i] >> 8  ) & 0xFF);
                    _buf[9] = (uint8_t) (( _status.relay_status_on_device[i] >> 16 ) & 0xFF);
                    _buf[10] =(uint8_t) (( _status.relay_status_on_device[i] >> 24)  & 0xFF);
                    modbus_write_buffer(modbus_handle,_buf ,11, _res_buf);
                    vTaskDelay(100);
                }           
            }
        // if (_status._relay_on_gateway)
        }         
    }
}


// typedef struct 
// {
//     uint8_t _has_multiple_devices;
//     uint8_t _is_modbus_device;
//     uint8_t _device_addresses[NUM_OF_MODBUS_DEVICES];
//     uint8_t _relay_on_gateway;
//     uint32_t relay_status_on_device[NUM_OF_MODBUS_DEVICES];
// }device_add_t;

command_t command_table[70]= 
{
    {"Button_1",  button_operation_1  },
    {"Button_2",  button_operation_2  },
    {"Button_3",  button_operation_3  }
    /*
    {"Button_4",  button_operation_4  },
    {"Button_5",  button_operation_5  },
    {"Button_6",  button_operation_6  },
    {"Button_7",  button_operation_7  },
    {"Button_8",  button_operation_8  },
    {"Button_9",  button_operation_9  },
    {"Button_10", button_operation_10 },
    {"Button_11", button_operation_11 },
    {"Button_12", button_operation_12 },
    {"Button_13", button_operation_13 },
    {"Button_14", button_operation_14 },
    {"Button_15", button_operation_15 },
    {"Button_16", button_operation_16 },
    {"Button_17", button_operation_17 },
    {"Button_18", button_operation_18 },
    {"Button_19", button_operation_19 },
    {"Button_20", button_operation_20 },
    {"Button_21", button_operation_21 },
    {"Button_22", button_operation_22 },
    {"Button_23", button_operation_23 },
    {"Button_24", button_operation_24 },
    {"Button_25", button_operation_25 },
    {"Button_26", button_operation_26 },
    {"Button_27", button_operation_27 },
    {"Button_28", button_operation_28 },
    {"Button_29", button_operation_29 },
    {"Button_30", button_operation_30 },
    {"Button_31", button_operation_31 },
    {"Button_32", button_operation_32 },
    {"Button_33", button_operation_33 },
    {"Button_34", button_operation_34 },
    {"Button_35", button_operation_35 },
    {"Button_36", button_operation_36 },
    {"Button_37", button_operation_37 },
    {"Button_38", button_operation_38 },
    {"Button_39", button_operation_39 },
    {"Button_40", button_operation_40 },
    {"Button_41", button_operation_41 },
    {"Button_42", button_operation_42 },
    {"Button_43", button_operation_43 },
    {"Button_44", button_operation_44 },
    {"Button_45", button_operation_45 },
    {"Button_46", button_operation_46 },
    {"Button_47", button_operation_47 },
    {"Button_48", button_operation_48 },
    {"Button_49", button_operation_49 },
    {"Button_50", button_operation_50 },
    {"Button_51", button_operation_51 },
    {"Button_52", button_operation_52 },
    {"Button_53", button_operation_53 },
    {"Button_54", button_operation_54 },
    {"Button_55", button_operation_55 },
    {"Button_56", button_operation_56 },
    {"Button_57", button_operation_57 },
    {"Button_58", button_operation_58 },
    {"Button_59", button_operation_59 },
    {"Button_60", button_operation_60 },
    {"Button_61", button_operation_61 },
    {"Button_62", button_operation_62 },
    {"Button_63", button_operation_63 },
    {"Button_64", button_operation_64 },
    {"Button_65", button_operation_65 },
    {"Button_66", button_operation_66 },
    {"Button_67", button_operation_67 },
    {"Button_68", button_operation_68 },
    {"Button_69", button_operation_69 },
    {"Button_70", button_operation_70 }*/
};


device_add_t button_operation_1()
{
    
    device_add_t _relay_status;
    uint32_t _relay_to_operate = 0;
    
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

    _relay_to_operate = _relay_to_operate  | (1 << 0);
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}


device_add_t button_operation_2()
{
    
    device_add_t _relay_status;
    uint32_t _relay_to_operate = 0;
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_TWO;

    _relay_to_operate = _relay_to_operate  | (1 << 2);
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}


device_add_t button_operation_3()
{
    
    device_add_t _relay_status;
    uint32_t _relay_to_operate = 0;
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

    _relay_to_operate = _relay_to_operate  | (1 << 2) | (1 << 1);
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}