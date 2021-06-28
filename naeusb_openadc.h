#pragma once
#include "naeusb.h"

#define REQ_MEMREAD_BULK 0x10
#define REQ_MEMWRITE_BULK 0x11
#define REQ_MEMREAD_CTRL 0x12
#define REQ_MEMWRITE_CTRL 0x13

#define REQ_FPGA_STATUS 0x15
#define REQ_FPGA_PROGRAM 0x16

#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21

#define REQ_FPGA_RESET 0x25

void openadc_register_handlers(void);