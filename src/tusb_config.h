#define CFG_TUSB_MCU OPT_MCU_NRF5X
#define CFG_TUSB_OS OPT_OS_ZEPHYR
#define CFG_TUH_ENABLED 1
#define CFG_TUH_MAX3421 1
//#define CFG_TUH_MSC 1
#define CFG_TUH_CDC 2
#define BOARD_TUH_RHPORT 0
#define CFG_TUSB_DEBUG 3

// Set Line Control state on enumeration/mounted:
// DTR ( bit 0), RTS (bit 1)
#define CFG_TUH_CDC_LINE_CONTROL_ON_ENUM (CDC_CONTROL_LINE_STATE_DTR | CDC_CONTROL_LINE_STATE_RTS)

// Set Line Coding on enumeration/mounted, value for cdc_line_coding_t
// bit rate = 115200, 1 stop bit, no parity, 8 bit data width
#define CFG_TUH_CDC_LINE_CODING_ON_ENUM { 115200, CDC_LINE_CODING_STOP_BITS_1, CDC_LINE_CODING_PARITY_NONE, 16 }

#define CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_ALIGN __attribute__ ((aligned(4)))
#define CFG_TUH_DEVICE_MAX (1)
#define CFG_TUH_ENUMERATION_BUFSIZE 256