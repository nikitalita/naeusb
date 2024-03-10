#include "NuvoICP_CW.h"

#include <asf.h>
#include "board.h"
#include "usart.h"
#include "sysclk.h"
#include "gpio.h"
#include "ioport.h"
#include "delay.h"


#ifdef __cplusplus
extern "C" {
#endif
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

void pgm_set_dat(int val)
{
  gpio_set_pin(PIN_PDIDTX_GPIO, val);
}

int pgm_get_dat(void)
{
  return gpio_get_pin_value(PIN_PDIDRX_GPIO);
}

void pgm_set_rst(int val)
{
  gpio_set_pin(PIN_TARG_NRST_GPIO, val);
}

void pgm_set_clk(int val)
{
  gpio_set_pin(PIN_PDIC_GPIO, val);
}

void pgm_dat_dir(int state)
{
  if(state){
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_OUT_FLAGS);
  }else{
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);
  }
}

void pgm_deinit(void)
{
 	gpio_configure_pin(PIN_PDIC_GPIO, PIN_PDIC_IN_FLAGS);
	gpio_configure_pin(PIN_PDIDRX_GPIO, PIN_PDIDRX_FLAGS);
	gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);
  gpio_set_pin_high(PIN_TARG_NRST_GPIO);
}

void pgm_usleep(unsigned long usec)
{
  delay_us(usec);
}

void device_print(const char * msg){
}

#ifdef __cplusplus
} // extern "C"
#endif
