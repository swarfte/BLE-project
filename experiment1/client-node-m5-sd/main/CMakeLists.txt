set(COMPONENT_SRCS "ili9340.c" "main.c"
                   "board.c"
                   "ili9340"
                   "fontx.c"
                   "pngle.c")

set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()

spiffs_create_partition_image(storage ../font FLASH_IN_PROJECT)
