set(srcs "main.c")

idf_component_register(SRCS  "main.c"
        "modbus_master.c"
        "waveshare.c"
        "wifi_server.c"
        "../src/otadrive_esp.c"
        "../src/http.c"
        "../src/conf.c"
        "../src/misc.c"
        INCLUDE_DIRS "../include")
