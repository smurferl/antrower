#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrfx_gpiote.h>
#include "nrfx_power.h"
#include <nrfx_spi.h>
#include <tusb_config.h>
#include <tusb.h>
#include <common/tusb_types.h>

LOG_MODULE_REGISTER(max3421e, LOG_LEVEL_DBG);

static void max3421_init(void);
void cdc_app_task();
static nrfx_spi_t _spi = NRFX_SPI_INSTANCE(1);
static nrfx_gpiote_t _gpiote = NRFX_GPIOTE_INSTANCE(0);

#define LFCLK_SRC_RC CLOCK_LFCLKSRC_SRC_RC
#define VBUSDETECT_Msk POWER_USBREGSTATUS_VBUSDETECT_Msk
#define OUTPUTRDY_Msk POWER_USBREGSTATUS_OUTPUTRDY_Msk

int main(void)
{
        max3421_init();

        tusb_rhport_init_t host_init = {
                .role = TUSB_ROLE_HOST,
                .speed = TUSB_SPEED_AUTO
        };
        
        tusb_init(0, &host_init); // initialize host stack on roothub port 0

        // FeatherWing MAX3421E use MAX3421E's GPIO0 for VBUS enable
        enum { IOPINS1_ADDR  = 20u << 3, /* 0xA0 */ };
        tuh_max3421_reg_write(BOARD_TUH_RHPORT, IOPINS1_ADDR, 0x01, false);

        while(1) {
                tuh_task();

                //cdc_app_task();
        }

        return 0;
}

/////////////////////////////////////// CALLBACKS ///////////////////////////////////////
void tuh_mount_cb(uint8_t dev_addr) {
  // application set-up
  LOG_INF("A device with address %u is mounted", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
  // application tear-down
  LOG_INF("A device with address %u is unmounted", dev_addr);
}

void cdc_app_task(void) {
  uint8_t buf[] = "USB\r\n"; // +1 for extra null character
  uint32_t const bufsize = sizeof(buf) - 1;

  // loop over all mounted interfaces
  for (uint8_t idx = 0; idx < CFG_TUH_CDC; idx++) {
    if (tuh_cdc_mounted(idx)) {
      // console --> cdc interfaces
      LOG_INF("request: %s", (char*) buf);
      tuh_cdc_write(idx, buf, bufsize);
      tuh_cdc_write_flush(idx);
    }
  }
}

void tuh_cdc_mount_cb(uint8_t idx) {
  tuh_itf_info_t itf_info = {0};
  tuh_cdc_itf_get_info(idx, &itf_info);

  LOG_INF("CDC Interface is mounted: address = %u, itf_num = %u", itf_info.daddr,
         itf_info.desc.bInterfaceNumber);

  cdc_line_coding_t line_coding = {0};
  if (tuh_cdc_get_line_coding_local(idx, &line_coding)) {
    LOG_INF("  Baudrate: %" PRIu32 ", Stop Bits : %u", line_coding.bit_rate, line_coding.stop_bits);
    LOG_INF("  Parity  : %u, Data Width: %u", line_coding.parity, line_coding.data_bits);
  }

  // FOR TESTING!!!
  cdc_app_task();
}

void tuh_cdc_umount_cb(uint8_t idx) {
  tuh_itf_info_t itf_info = {0};
  tuh_cdc_itf_get_info(idx, &itf_info);

  LOG_INF("CDC Interface is unmounted: address = %u, itf_num = %u", itf_info.daddr,
         itf_info.desc.bInterfaceNumber);
}

void tuh_cdc_rx_cb(uint8_t idx) {
  uint8_t buf[64 + 1]; // +1 for extra null character
  uint32_t const bufsize = sizeof(buf) - 1;

  // forward cdc interfaces -> console
  const uint32_t count = tuh_cdc_read(idx, buf, bufsize);
  if (count) {
    buf[count] = 0;
    LOG_INF("response: %s", (char*) buf);
    fflush(stdout);
  }
}

/////////////////////////////////////// setup ///////////////////////////////////////
void max3421_int_handler(nrfx_gpiote_pin_t pin, nrfx_gpiote_trigger_t action, void* p_context) {
  (void) p_context;
  if (action != NRFX_GPIOTE_TRIGGER_HITOLO) {
    return;
  }
  if (pin != 31) {
    return;
  }

  tusb_int_handler(0, true);
}

static void max3421_init(void) {
  /////////////////////////
  // stop LF clock just in case we jump from application without reset
  NRF_CLOCK->TASKS_LFCLKSTOP = 1UL;

  // Use Internal OSC to compatible with all boards
  NRF_CLOCK->LFCLKSRC = LFCLK_SRC_RC;
  NRF_CLOCK->TASKS_LFCLKSTART = 1UL;

  /////////////MAX//////////////

  // manually manage CS
  nrf_gpio_cfg_output(11);
  nrf_gpio_pin_write(11, 1);

  nrfx_spi_config_t cfg = {
      .sck_pin        = 12,
      .mosi_pin       = 13,
      .miso_pin       = 14,
      .ss_pin         = NRFX_SPI_PIN_NOT_USED,
      .frequency      = 4000000u,      
      .irq_priority   = 3,
      .orc            = 0xFF,
      // default setting 4 Mhz, Mode 0, MSB first
      .mode           = NRF_SPI_MODE_0,
      .bit_order      = NRF_SPI_BIT_ORDER_MSB_FIRST,
      .miso_pull      = NRF_GPIO_PIN_NOPULL,
  };

  // no handler --> blocking
  TU_ASSERT(NRFX_SUCCESS == nrfx_spi_init(&_spi, &cfg, NULL, NULL), );

  // max3421e interrupt pin
  nrf_gpio_pin_pull_t intr_pull = NRF_GPIO_PIN_PULLUP;
  nrfx_gpiote_trigger_config_t intr_trigger = {
      .trigger = NRFX_GPIOTE_TRIGGER_HITOLO,
      .p_in_channel = NULL, // sensing mechanism
  };
  nrfx_gpiote_handler_config_t intr_handler = {
      .handler = max3421_int_handler,
      .p_context = NULL,
  };
  nrfx_gpiote_input_pin_config_t intr_config = {
      .p_pull_config = &intr_pull,
      .p_trigger_config = &intr_trigger,
      .p_handler_config = &intr_handler,
  };

  nrfx_gpiote_init(&_gpiote, 1);
  NVIC_SetPriority(GPIOTE_IRQn, 2);

  nrfx_gpiote_input_configure(&_gpiote, 31, &intr_config);
  nrfx_gpiote_trigger_enable(&_gpiote, 31, true);
}

// API to enable/disable MAX3421 INTR pin interrupt
void tuh_max3421_int_api(uint8_t rhport, bool enabled) {
  (void) rhport;

  // use NVIC_Enable/Disable instead since nrfx_gpiote_trigger_enable/disable clear pending and can miss interrupt
  // when disabled and re-enabled.
  if (enabled) {
    NVIC_EnableIRQ(GPIOTE_IRQn);
  } else {
    NVIC_DisableIRQ(GPIOTE_IRQn);
  }
}

// API to control MAX3421 SPI CS
void tuh_max3421_spi_cs_api(uint8_t rhport, bool active) {
  (void) rhport;
  nrf_gpio_pin_write(11, active ? 0 : 1);
}

// API to transfer data with MAX3421 SPI
// Either tx_buf or rx_buf can be NULL, which means transfer is write or read only
bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const* tx_buf, uint8_t* rx_buf, size_t xfer_bytes) {
  (void) rhport;

  nrfx_spi_xfer_desc_t xfer = {
      .p_tx_buffer = tx_buf,
      .tx_length   = tx_buf ? xfer_bytes : 0,
      .p_rx_buffer = rx_buf,
      .rx_length   = rx_buf ? xfer_bytes : 0,
  };

  return nrfx_spi_xfer(&_spi, &xfer, 0) == NRFX_SUCCESS;
}