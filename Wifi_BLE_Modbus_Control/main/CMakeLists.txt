set(srcs "main.c")

idf_component_register(SRCS "waveshare.c" "modbus_master.c" "${srcs}"
                       INCLUDE_DIRS ".")
