# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/button
                         $ENV{IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_init
                         $ENV{IDF_PATH}/examples/bluetooth/esp_ble_mesh/common_components/example_nvs)

# Set the build directory
set(BUILD_DIR ${CMAKE_CURRENT_LIST_DIR}/build)

# Set the sdkconfig file
set(SDKCONFIG ${CMAKE_CURRENT_LIST_DIR}/sdkconfig)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(SUPPORTED_TARGETS esp32-s3)
project(onoff_client)

