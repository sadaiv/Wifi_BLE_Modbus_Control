#include "waveshare.h"
#include "string.h"
#include "memory.h"
#include "host/util/util.h"
#include "modbus_master.h"

#include "driver/gpio.h"



QueueHandle_t bleCommandQue;
const int  uxQueueLength = 10;
const int  uxItemSize= 25;

static int g_clear_on_off = 0;

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

const int button_mask[76][3] = {
    /* [0]  */ { 0xFFFF, 0xFFFF, 0 },
    /* [1]  */ { (1<<0) | (1<<1) | (1<<2), (1<<4), 0 },          // R1, R2, R3, R37
    /* [2]  */ { (1<<0) | (1<<1),         (1<<4), 0},          // R1, R2
    /* [3]  */ { (1<<0),                   0 ,0},          // R1
    /* [4]  */ { (1<<1),                   0 ,0},          // R2
    /* [5]  */ { (1<<3) | (1<<4) | (1<<5), 0 ,0},          // R4, R5, R6
    /* [6]  */ { (1<<4) | (1<<5),          0 ,0},          // R5, R6
    /* [7]  */ { (1<<5),                   0 ,0},          // R6
    /* [8]  */ { (1<<6) | (1<<7) | (1<<8), 0 ,0},          // R7, R8, R9
    /* [9]  */ { (1<<7) | (1<<8),          0 ,0},          // R8, R9
    /* [10] */ { (1<<8),                   0 ,0},          // R9
    /* [11] */ { (1<<9)|(1<<10)|(1<<11)|
                 (1<<12)|(1<<13)|(1<<14)|
                 (1<<15)|(1<<16),          0 ,0},          // R10..R17
    /* [12] */ { (1<<9)|(1<<10)|(1<<11),   0 ,0},          // R10, R11, R12
    /* [13] */ { (1<<11),                  0 ,0},          // R12
    /* [14] */ { (1<<12)|(1<<13),          0 ,0},          // R13, R14
    /* [15] */ { (1<<14),                  0 ,0},          // R15
    /* [16] */ { (1<<15),                  0 ,0},          // R16
    /* [17] */ { (1<<16),                  0 ,0},          // R17
    /* [18] */ { (1<<18),           (1<<0)   ,0},            // R19, R33
    /* [19] */ { (1<<19),                  0 ,0},          // R20
    /* [20] */ { (1<<21),           (1<<0)   ,0},            // R22, R33
    /* [21] */ { (1<<22),                  0 ,0},          // R23
    /* [22] */ { (1<<23),                  0 ,0},          // R24
    /* [23] */ { (1<<24),                  0 ,0},          // R25
    /* [24] */ { (1<<25),                  0 ,0},          // R26
    /* [25] */ { (1<<27),           (1<<0)   ,0},            // R28, R33
    /* [26] */ { (1<<28),           (1<<0)   ,0},            // R29, R33
    /* [27] */ { (1<<30),                  0 ,0},          // R31
    /* [28] */ { (1<<22)|(1<<17)|(1<<24)|(1<<29), 0 , 0},  // R23,18,25,30
    /* [29] */ { (1<<18)|(1<<19)|(1<<20)|(1<<21)|
                 (1<<22)|(1<<23)|(1<<24)|(1<<25)|
                 (1<<26),                 0 ,0},          // R19..R27
    /* [30] */ { (1<<27)|(1<<28)|(1<<29)|(1<<30)|(1<<31), 0 ,0}, // R28..R32
    /* [31] */ { 0, (1<<1) , 0 },                            // R34
    /* [32] */ { 0, (1<<17), 0 },  // R50 mix MOCH
    /* [33] */ { 0, (1<<2) , 0 },                            // R35
    /* [34] */ { 0, (1<<3) , 0 },                            // R36
    /* [35] */ { 0, (1<<4) , 0 },                            // R37
    /* [36] */ { 0, (1<<5) , 0 },                            // R38
    /* [37] */ { 0, (1<<6) , 0 },                            // R39
    /* [38] */ { 0, (1<<7) , 0 },                            // R40
    /* [39] */ { 0, (1<<8) , 0 },                            // R41
    /* [40] */ { 0, (1<<9) , 0 },                            // R42
    /* [41] */ { 0, (1<<10), 0 },                            // R43
    /* [42] */ { 0, (1<<11), 0 },                            // R44
    /* [43] */ { 0, (1<<12), 0 },                            // R45
    /* [44] */ { 0, (1<<13), 0 },                            // R46
    /* [45] */ { 0, (1<<14), 0 },                            // R47
    /* [46] */ { 0, (1<<15), 0 },                            // R48
    /* [47] */ { 0, (1<<15)|(1<<14), 0},                    // R47, R48
    /* [48] */ { 0, (1<<22),0},                            // R55
    /* [49] */ { 0, (1<<23),0},                            // R56
    /* [50] */ { 0, (1<<24),0},                            // R57
    /* [51] */ { 0, (1<<25),0},                            // R58
    /* [52] */ { 0, (1<<26),0},                            // R59
    /* [53] */ { 0, (1<<27),0},                            // R60
    /* [54] */ { 0, (1<<28),0},                            // R61
    /* [55] */ { 0, (1<<29),0},                            // R62
    /* [56] */ { 0, (1<<30),0},                            // R63
    /* [57] */ { 0, (1<<22)|(1<<23)|(1<<24)|(1<<25)|
                    (1<<26)|(1<<27)|(1<<28)|(1<<29),0 }, // R55..R63
    /* [58] */ { 0, (1<<16),0},                            // R49
    /* [59] */ { 0, (1<<31),0},                            // R50 autozone
    /* [60] */ { 0, (1<<18),0},                            // R51
    /* [61] */ { 0, (1<<19),0},                            // R52
    /* [62] */ { 0, (1<<20),0},                            // R53
    /* [63] */ { 0, (1<<21),0},                            // R54
    /* [64] */ { 0xFFFFFFFF, 0xFFFFFFFF , (!0x01)},               // all model light
    /* [65] */ { 0, (1<<0) , 0},
    /* [66] */ { (1<<22)|(1<<17)|(1<<24), 0, 0 },
    /* [67] */ { (1<<29), 0 , 0},
    /* [68] */ {  0, 0 , 0x01}, //music on 
    /* [69] */ { 0 , 0 , 0}, //music off 
    /* [70] */ { 0, 0 , 0},  // status keep 
    /* [71] */ { 0, 0 , 0},  // status clear
    /* [72] */ { 0, 0 , 0x02},  // near by R
    /* [73] */ { 0, 0 , 0x04},
    /* [74] */ { 0, 0 , 0x08},

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

    if (button_num > 0 && button_num <= 74) {
        _relay_status.relay_status_on_device[0] = button_mask[button_num][0];
        _relay_status.relay_status_on_device[1] = button_mask[button_num][1];
        _relay_status._relay_on_gateway = button_mask[button_num][2];
    }

    return _relay_status;
}


void modbus_task(void *pvParameters)
{
    modbus_rtu_master_handle_t modbus_handle =  modbus_rtu_master_init(&cfg);
    bleCommandQue = xQueueCreate(uxQueueLength, uxItemSize);
     device_add_t _prev_status = {0};

    while (1)
    {
        char rxBuffer[25] = {0};
        device_add_t _status = {0};
       // uint8_t _is_command_recvd =0;
        int _command_id=255;
                
        if( xQueueReceive(bleCommandQue, &(rxBuffer), (TickType_t)500))
        {   
            if ( sscanf(rxBuffer, "Button_%d", &_command_id) != 1)
            continue;
            if ( (_command_id >= 68) && (_command_id <= 71))
            {
                switch (_command_id)
                {
                    case 68: // music on 
                     // add GPIO enable here 
                    gpio_set_level(RELAY_1, 1); // Set to HIGH
                    break;
                    
                    case 69: // music off 
                    gpio_set_level(RELAY_1, 0); // Set to HIGH //  add GPIO disable off here
                    break;

                    case 70: //enable clear on command 
                    g_clear_on_off = 1;                        
                    break;

                    case 71: //disble clear on command
                    g_clear_on_off = 0;
                    break;
                }
            }
            else 
            {
                _status = button_operation_num(_command_id);
                
                // device_add_t _status = button_operation_1();
                for (int i = 0; i < 2; i++)
                {
                    if (_status._device_addresses[i] != 0)
                    {
                        uint8_t _buf[11] = {0};
                        uint8_t _res_buf[255];
                        _buf[0] = _status._device_addresses[i];
                        _buf[1] = 0x0f;
                        _buf[2] = 0x00;
                        _buf[3] = 0x00;
                        _buf[4] = 0x00;
                        _buf[5] = 0x20;
                        _buf[6] = 0x04;
                        if (g_clear_on_off)
                        {
                           _status.relay_status_on_device[i] |= _prev_status.relay_status_on_device[i];
                           _status._relay_on_gateway |= _prev_status._relay_on_gateway; 
                        }
                        _buf[7] = (uint8_t)((_status.relay_status_on_device[i]) & 0xFF);
                        _buf[8] = (uint8_t)((_status.relay_status_on_device[i] >> 8) & 0xFF);
                        _buf[9] = (uint8_t)((_status.relay_status_on_device[i] >> 16) & 0xFF);
                        _buf[10] = (uint8_t)((_status.relay_status_on_device[i] >> 24) & 0xFF);
                       
                        modbus_write_buffer(modbus_handle, _buf, 11, _res_buf);
                        for( int i =1; i <6; i++)
                        {
                            int level = (_status._relay_on_gateway >> i) & 0x01;
                            // Your GPIOs are 1–6, so map i(0–5) → GPIO(i+1)
                          
                        }
                        vTaskDelay(10);
                    }
                }
                ESP_LOGI( "TAG", "Button status =%x", _status._relay_on_gateway);
              gpio_set_level(RELAY_2, (_status._relay_on_gateway & 0x02) ? 1 : 0);
              gpio_set_level(RELAY_3, (_status._relay_on_gateway & 0x04) ? 1 : 0);
                    gpio_set_level(RELAY_4, (_status._relay_on_gateway & 0x08) ? 1 : 0);
                _prev_status = _status;
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


// device_add_t button_operation_1()
// {
    
//     device_add_t _relay_status ={0};
//     uint32_t _relay_to_operate = 0;
    
//     memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

//     _relay_status._has_multiple_devices=0;
//     _relay_status._is_modbus_device =0;
//     _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

//     _relay_to_operate = _relay_to_operate  | BUTTON_1;
//     _relay_status.relay_status_on_device[0]= _relay_to_operate;

//     return _relay_status;
// }


// device_add_t button_operation_2()
// {
    
//     device_add_t _relay_status;
//     uint32_t _relay_to_operate = 0;
//     memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

//     _relay_status._has_multiple_devices=0;
//     _relay_status._is_modbus_device =0;
//     _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_TWO;

//     _relay_to_operate = _relay_to_operate  | BUTTON_2;
//     _relay_status.relay_status_on_device[0]= _relay_to_operate;

//     return _relay_status;
// }


// device_add_t button_operation_3()
// {
    
//     device_add_t _relay_status;
//     uint32_t _relay_to_operate = 0;
//     memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

//     _relay_status._has_multiple_devices=0;
//     _relay_status._is_modbus_device =0;
//     _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

//     _relay_to_operate = _relay_to_operate  | BUTTON_3;
//     _relay_status.relay_status_on_device[0]= _relay_to_operate;

//     return _relay_status;
// }

// device_add_t button_operation_4()
// {
    
//     device_add_t _relay_status;
//     uint32_t _relay_to_operate = 0;
//     memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

//     _relay_status._has_multiple_devices=0;
//     _relay_status._is_modbus_device =0;
//     _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

//     _relay_to_operate = _relay_to_operate  | BUTTON_4;
//     _relay_status.relay_status_on_device[0]= _relay_to_operate;

//     return _relay_status;
// }

// device_add_t button_operation_5()
// {
    
//     device_add_t _relay_status;
//     uint32_t _relay_to_operate = 0;
//     memset((uint8_t *)&_relay_status, 0, sizeof(device_add_t));

//     _relay_status._has_multiple_devices=0;
//     _relay_status._is_modbus_device =0;
//     _relay_status._device_addresses[0] = MODBUS_ADD_DEVICE_ONE;

//     _relay_to_operate = _relay_to_operate  | BUTTON_5;
//     _relay_status.relay_status_on_device[0]= _relay_to_operate;

//     return _relay_status;
// }