set(srcs "main.c")

idf_component_register(SRCS "waveshare.c" "modbus_master.c" "wifi_server.c" "${srcs}"
                       INCLUDE_DIRS "." )
