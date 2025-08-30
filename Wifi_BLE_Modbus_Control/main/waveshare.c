#include "waveshare.h"
#include "string.h"
#include "memory.h"
#include "host/util/util.h"
#include "modbus_master.h"
#include "relay_operations.h"

QueueHandle_t bleCommandQue;
const int  uxQueueLength = 10;
const int  uxItemSize= 25;

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

const int button_mask[70][2] = {
    {0, 0,},
    { (1<<0)  | (1<<1) | (1<<2),    0 },   // R1, R2, R3
    { (1<<0)  | (1<<1),             0 },   // R1, R2
    { (1<<0),                        0 },   // R1
    { (1<<2),                        0 },   // R2
    { (1<<3)  | (1<<4) | (1<<5),    0 },   // R4, R5, R6
    { (1<<4)  | (1<<5),             0 },   // R5, R6
    { (1<<5),                        0 },   // R6
    { (1<<6)  | (1<<7) | (1<<8),    0 },   // R7, R8, R9
    { (1<<7)  | (1<<8),             0 },   // R8, R9
    { (1<<8),                        0 },   // R9
    { (1<<9)  | (1<<10)| (1<<11) | 
      (1<<12) | (1<<13)| (1<<14) | 
      (1<<15) | (1<<16),            0 },   // R10..R17
    { (1<<9)  | (1<<10)| (1<<11),   0 },   // R10, R11, R12
    { (1<<11),                       0 },   // R12
    { (1<<12) | (1<<14),            0 },   // R13, R14
    { (1<<14),                       0 },   // R15
    { (1<<15),                       0 },   // R16
    { (1<<16),                       0 },   // R17
    { (1<<18),              (1<<0) }, // R19, R33
    { (1<<19),                       0 },   // R20
    { (1<<21),              (1<<0) }, // R22, R33
    { (1<<22),                       0 },   // R23
    { (1<<23),                       0 },   // R24
    { (1<<24),                       0 },   // R25
    { (1<<25),                       0 },   // R26
    { (1<<27),              (1<<0) }, // R28, R33
    { (1<<28),              (1<<0) }, // R29, R33
    { (1<<30),                       0 },   // R31
    { (1<<21)|(1<<17)|(1<<24)|(1<<29),0 }, // R23,18,25,30
    { (1<<18)|(1<<19)|(1<<20)|(1<<21)|
      (1<<22)|(1<<23)|(1<<24)|(1<<25)|
      (1<<26),                       0 },   // R19..R27
    { (1<<27)|(1<<28)|(1<<29)|(1<<30)|(1<<31), 0 }, // R28..R32
    { 0, (1<<1) },                           // R34
    { (1<<10)|(1<<11)|(1<<13)|(1<<14)| 
      (1<<15)|(1<<16)|(1<<17)|(1<<18)| 
      (1<<19)|(1<<20)|(1<<21)|(1<<22)| 
      (1<<23)|(1<<24)|(1<<25)|(1<<26), 0 }, // R11..R27 mix
    { 0, (1<<2) },                           // R35
    { 0, (1<<3) },                           // R36
    { 0, (1<<4) },                           // R37
    { 0, (1<<5) },                           // R38
    { 0, (1<<6) },                           // R39
    { 0, (1<<7) },                           // R40
    { 0, (1<<8) },                           // R41
    { 0, (1<<9) },                           // R42
    { 0, (1<<10)},                           // R43
    { 0, (1<<11)},                           // R44
    { 0, (1<<12)},                           // R45
    { 0, (1<<13)},                           // R46
    { 0, (1<<14)},                           // R47
    { 0, (1<<15)},                           // R48
    { 0, (1<<15)|(1<<14)},                   // R47, R48
    { 0, (1<<22)},                           // R55
    { 0, (1<<23)},                           // R56
    { 0, (1<<24)},                           // R57
    { 0, (1<<25)},                           // R58
    { 0, (1<<26)},                           // R59
    { 0, (1<<27)},                           // R60
    { 0, (1<<28)},                           // R61
    { 0, (1<<29)},                           // R62
    { 0, (1<<30)},                           // R63
    { 0, (1<<22)|(1<<23)|(1<<24)|(1<<25)|
          (1<<26)|(1<<27)|(1<<28)|(1<<29)|
          (1<<30)},                         // R55..R63
    { 0, (1<<16)},                           // R49
    { 0, (1<<17)},                           // R50
    { 0, (1<<18)},                           // R51
    { 0, (1<<19)},                           // R52
    { 0, (1<<20)},                           // R53
    { 0, (1<<21)},                           // R54
    {0xFFFFFFFF, 0xFFFFFFFF} // all model light
};

uint8_t get_device_add();

device_add_t button_operation_num(uint8_t button_num)
{
    device_add_t _relay_status;
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices = 1;   // two devices
    _relay_status._is_modbus_device     = 1;
    _relay_status._device_addresses[0]  = MODBUS_ADD_DEVICE_ONE;
    _relay_status._device_addresses[1]  = MODBUS_ADD_DEVICE_TWO;

    if (button_num > 0 && button_num <= 70) {
        _relay_status.relay_status_on_device[0] = button_mask[button_num][0];
        _relay_status.relay_status_on_device[1] = button_mask[button_num][1];
    }

    return _relay_status;
}


void modbus_task(void *pvParameters)
{
    modbus_rtu_master_handle_t modbus_handle =  modbus_rtu_master_init(&cfg);
    bleCommandQue = xQueueCreate(uxQueueLength, uxItemSize);

    while (1)
    {
        char rxBuffer[25] = {0};
        device_add_t _status = {0};
        uint8_t _is_command_recvd =0;
        int _command_id=255;
                
        if( xQueueReceive(bleCommandQue, &(rxBuffer), (TickType_t)500))
        {   
            if ( sscanf(rxBuffer, "Button_%d", &_command_id) != 1)
            continue;

            _status = button_operation_num(_command_id);
           // device_add_t _status = button_operation_1();
            for (int i = 0; i <2 ; i++)
            {
                if(_status._device_addresses[i] != 0)
                {
                    uint8_t _buf[11] = {0};
                    uint8_t _res_buf[255];
                    _buf[0] = _status._device_addresses[i];
                    _buf[1] = 0x0f;
                    _buf[2] = 0x00; _buf[3] =0x00; _buf[4] = 0x00;
                    _buf[5] = 0x20; _buf[6]= 0x04;
                    _buf[7] = (uint8_t) (( _status.relay_status_on_device[i] ) & 0xFF) ;
                    _buf[8] = (uint8_t) (( _status.relay_status_on_device[i] >> 8  ) & 0xFF);
                    _buf[9] = (uint8_t) (( _status.relay_status_on_device[i] >> 16 ) & 0xFF);
                    _buf[10] =(uint8_t) (( _status.relay_status_on_device[i] >> 24)  & 0xFF);
                    modbus_write_buffer(modbus_handle,_buf ,11, _res_buf);
                    vTaskDelay(10);
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
    
    device_add_t _relay_status ={0};
    uint32_t _relay_to_operate = 0;
    
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

    _relay_to_operate = _relay_to_operate  | BUTTON_1;
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

    _relay_to_operate = _relay_to_operate  | BUTTON_2;
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

    _relay_to_operate = _relay_to_operate  | BUTTON_3;
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}

device_add_t button_operation_4()
{
    
    device_add_t _relay_status;
    uint32_t _relay_to_operate = 0;
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

    _relay_to_operate = _relay_to_operate  | BUTTON_4;
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}

device_add_t button_operation_5()
{
    
    device_add_t _relay_status;
    uint32_t _relay_to_operate = 0;
    memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

    _relay_status._has_multiple_devices=0;
    _relay_status._is_modbus_device =0;
    _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

    _relay_to_operate = _relay_to_operate  | BUTTON_5;
    _relay_status.relay_status_on_device[0]= _relay_to_operate;

    return _relay_status;
}