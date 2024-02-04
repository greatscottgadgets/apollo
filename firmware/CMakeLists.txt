cmake_minimum_required(VERSION 3.17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# set(CMAKE_C_COMPILER_WORKS 1)

#set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/../lib/tinyusb/hw/bsp/family_support.cmake)

# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT} C CXX ASM)

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Espressif has its own cmake build system
if(FAMILY STREQUAL "espressif")
  return()
endif()

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/console.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_spi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jtag.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jtag_tap.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/button.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/dfu.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/jtag.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/fpga.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/debug_spi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/spi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/led.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/uart.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/usb_descriptors.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}/usb_switch.c
  )

# Example include
target_include_directories(${PROJECT} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/boards/${BOARD}
  )

target_compile_definitions(${PROJECT} PUBLIC
#   CFG_TUSB_OS=OPT_OS_PICO
  _BOARD_REVISION_MAJOR_=${BOARD_REVISION_MAJOR}
  _BOARD_REVISION_MINOR_=${BOARD_REVISION_MINOR}
  )

# Configure compilation flags and libraries for the example... see the corresponding function
# in hw/bsp/FAMILY/family.cmake for details.
# family_configure_device_example(${PROJECT} noos)
family_configure_target(${PROJECT} noos)
target_link_libraries(${PROJECT} PUBLIC pico_stdlib tinyusb_device pico_unique_id pico_fix_rp2040_usb_device_enumeration hardware_spi)

