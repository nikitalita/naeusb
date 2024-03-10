#include "NuvoICP_CW.h"

#include <asf.h>
#include "board.h"
#include "usart.h"
#include "sysclk.h"
#include "gpio.h"
#include "ioport.h"
#include "delay.h"

int pgm_init(void)
{
	gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
  gpio_set_pin_high(PIN_TARG_NRST_GPIO);
	gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_OUT_FLAGS);
	gpio_configure_pin(PIN_PDIDRX_GPIO, PIN_PDIDRX_FLAGS);
	gpio_configure_pin(PIN_PDIC_GPIO, PIN_PDIC_OUT_FLAGS);
  

#ifdef PIN_PDIDWR_GPIO
	gpio_set_pin_high(PIN_PDIDWR_GPIO);
	gpio_set_pin_high(PIN_PDICWR_GPIO);
#endif

  gpio_set_pin_low(PIN_PDIDTX_GPIO);
  gpio_set_pin_low(PIN_PDIC_GPIO);

  return 0;
}

void gpio_set_pin(uint32_t pin, int val){
  if(val){
    gpio_set_pin_high(pin);
  }else{
    gpio_set_pin_low(pin);
  }
}

void pgm_set_dat(uint8_t val)
{
  gpio_set_pin(PIN_PDIDTX_GPIO, val);
}

uint8_t pgm_get_dat(void)
{
  return gpio_get_pin_value(PIN_PDIDRX_GPIO);
}

void pgm_set_rst(uint8_t val)
{
  gpio_set_pin(PIN_TARG_NRST_GPIO, val);
}

void pgm_set_clk(uint8_t val)
{
  gpio_set_pin(PIN_PDIC_GPIO, val);
}

void pgm_dat_dir(uint8_t state)
{
  if(state){
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_OUT_FLAGS);
  }else{
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);
  }
}

void pgm_deinit(uint8_t leave_reset_high)
{
 	gpio_configure_pin(PIN_PDIC_GPIO, PIN_PDIC_IN_FLAGS);
	gpio_configure_pin(PIN_PDIDRX_GPIO, PIN_PDIDRX_FLAGS);
	gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);
  if (leave_reset_high)
    gpio_set_pin_high(PIN_TARG_NRST_GPIO);
}

uint32_t pgm_usleep(uint32_t usec)
{
  delay_us(usec);
  return usec;
}

void device_print(const char * msg){
}
