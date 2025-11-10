cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(max3421e)

target_sources(app PRIVATE 
src/main.c
tinyusb/src/tusb.c
tinyusb/src/portable/analog/max3421/hcd_max3421.c
tinyusb/src/host/usbh.c
tinyusb/src/host/hub.c
tinyusb/src/common/tusb_fifo.c
tinyusb/src/class/msc/msc_host.c
tinyusb/src/class/cdc/cdc_host.c)

target_include_directories(app PUBLIC 
src
tinyusb/src)
