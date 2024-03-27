/*
 * nuvoicp, a RPi ICP flasher for the Nuvoton N76E003
 * https://github.com/steve-m/N76E003-playground
 *
 * Copyright (c) 2021 Steve Markgraf <steve@steve-m.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "NuvoICP.h"
#include "NuvoICP_CW.h"
#include "NuvoProgCommon.h"

#define DEFAULT_BIT_DELAY 2
#define LEAVE_RESET_HIGH_AFTER_PROG 0

// These are MCU dependent (default for N76E003)
static int program_time = 20;
static int page_erase_time = 5000;

#ifdef DYNAMIC_DELAY
static int ICP_CMD_DELAY = DEFAULT_BIT_DELAY;
static int ICP_READ_DELAY = DEFAULT_BIT_DELAY;
static int ICP_WRITE_DELAY = DEFAULT_BIT_DELAY;
#else
#define ICP_CMD_DELAY DEFAULT_BIT_DELAY
#define ICP_READ_DELAY DEFAULT_BIT_DELAY
#define ICP_WRITE_DELAY DEFAULT_BIT_DELAY
#endif

// to avoid overhead from calling usleep() for 0 us
#define USLEEP(x) if (x > 0) pgm_usleep(x)

#ifdef _DEBUG
#define DEBUG_PRINT(x) icp_outputf(x)
// time measurement
static unsigned long usstart_time = 0;
static unsigned long usend_time = 0;
#define DEBUG_TIMER_START usstart_time = pgm_get_time();
#define DEBUG_TIMER_END usend_time = pgm_get_time();
#define DEBUG_PRINT_TIME(funcname) icp_outputf(#funcname " took %d us\n", usend_time - usstart_time)
#else
#define DEBUG_PRINT(x)
#define TIMER_START
#define DEBUG_TIMER_END
#define DEBUG_PRINT_TIME(funcname)
#endif
#define ENTRY_BIT_DELAY 60

// ICP Commands
#define CMD_READ_UID		0x04
#define CMD_READ_CID		0x0b
#define CMD_READ_DEVICE_ID	0x0c
#define CMD_READ_FLASH		0x00
#define CMD_WRITE_FLASH		0x21
#define CMD_MASS_ERASE		0x26
#define CMD_PAGE_ERASE		0x22

#ifdef _DEBUG
#include "delay.h"
#include "gpio.h"

void test_command(){
	pgm_init();
	pgm_dat_dir(1);
	for (int i = 0; i < 50; i++){
		pgm_set_rst(1);
		pgm_set_dat(1);
		pgm_set_clk(1);
		delay_ms(100);
		pgm_set_rst(0);
		pgm_set_dat(0);
		pgm_set_clk(0);
		delay_ms(100);
	}
}
#endif

static void icp_bitsend(uint32_t data, int len, uint32_t udelay)
{
	pgm_dat_dir(1);
	int i = len;
	while (i--){
			pgm_set_dat((data >> i) & 1);
			USLEEP(udelay);
			pgm_set_clk(1);
			USLEEP(udelay);
			pgm_set_clk(0);
	}	
}

static void icp_send_command(uint8_t cmd, uint32_t dat)
{
	icp_bitsend((dat << 6) | cmd, 24, ICP_CMD_DELAY);
}

int send_reset_seq(uint32_t reset_seq, int len){
	for (int i = 0; i < len + 1; i++) {
		pgm_set_rst((reset_seq >> (len - i)) & 1);
		USLEEP(10000);
	}
	return 0;
}

void icp_send_entry_bits() {
	icp_bitsend(ENTRY_BITS, 24, ENTRY_BIT_DELAY);
}

void icp_send_exit_bits(){
	icp_bitsend(EXIT_BITS, 24, ENTRY_BIT_DELAY);
}

int icp_init(uint8_t do_reset)
{
	int rc;

	rc = pgm_init();
    if (rc < 0) {
		return rc;
	} else if (rc != 0){
		return -1;
	}
	icp_entry(do_reset);
	// uint32_t dev_id = icp_read_device_id();
	// if (dev_id >> 8 == 0x2F){
	// 	// printf("Device ID mismatch: %x\n", dev_id);
	// 	return -2;
	// }
	return 0;
}

void icp_entry(uint8_t do_reset) {
	if (do_reset) {
		send_reset_seq(ICP_RESET_SEQ, 24);
	} else {
		pgm_set_rst(1);
		USLEEP(5000);
		pgm_set_rst(0);
		USLEEP(1000);
	}
	pgm_set_rst(0);
	USLEEP(100);
	icp_send_entry_bits();
	USLEEP(10);
}

void icp_reentry(uint32_t delay1, uint32_t delay2, uint32_t delay3) {
	USLEEP(10);
	if (delay1 > 0) {
		pgm_set_rst(1);
		USLEEP(delay1);
	}
	pgm_set_rst(0);
	USLEEP(delay2);
	icp_send_entry_bits();
	USLEEP(delay3);
}

void icp_fullexit_entry_glitch(uint32_t delay1, uint32_t delay2, uint32_t delay3){
	icp_exit();
}

void icp_reentry_glitch(uint32_t delay1, uint32_t delay2, uint32_t delay_after_trigger_high, uint32_t delay_before_trigger_low){
	USLEEP(200);
	// this bit here it to ensure that the config bytes are read at the correct time (right next to the reset high)
	pgm_set_rst(1);
	USLEEP(delay1);
	pgm_set_rst(0);
	USLEEP(delay2);

	//now we do a the full reentry, set the trigger
	pgm_set_trigger(1);
	USLEEP(delay_after_trigger_high);
	pgm_set_rst(1);

	// by default, we sleep for 280us, the length of the config load
	if (delay_before_trigger_low == 0) {
		delay_before_trigger_low = 280;
	}

	if (delay_before_trigger_low > delay1){
		USLEEP(delay1);
		pgm_set_rst(0);
		USLEEP(delay_before_trigger_low - delay1);
		pgm_set_trigger(0);
	} else {
		USLEEP(delay_before_trigger_low);
		pgm_set_trigger(0);
		USLEEP(delay1 - delay_before_trigger_low);
		pgm_set_rst(0);
	}
	USLEEP(delay2);
	icp_send_entry_bits();
	USLEEP(10);
}


void icp_deinit(void)
{
	icp_exit();
	pgm_deinit(LEAVE_RESET_HIGH_AFTER_PROG);
}

void icp_pgm_deinit_only(uint8_t leave_reset_high){
	pgm_deinit(leave_reset_high);
}

void icp_exit(void)
{
	pgm_set_rst(1);
	USLEEP(5000);
	pgm_set_rst(0);
	USLEEP(10000);
	icp_send_exit_bits();
	USLEEP(500);
	pgm_set_rst(1);
}


static uint8_t icp_read_byte(int end)
{
	pgm_dat_dir(0);
	USLEEP(ICP_READ_DELAY);
	uint8_t data = 0;
	int i = 8;

	while (i--) {
		USLEEP(ICP_READ_DELAY);
		int state = pgm_get_dat();
		pgm_set_clk(1);
		USLEEP(ICP_READ_DELAY);
		pgm_set_clk(0);
		data |= (state << i);
	}

	pgm_dat_dir(1);
	USLEEP(ICP_READ_DELAY);
	pgm_set_dat(end);
	USLEEP(ICP_READ_DELAY);
	pgm_set_clk(1);
	USLEEP(ICP_READ_DELAY);
	pgm_set_clk(0);
	USLEEP(ICP_READ_DELAY);
	pgm_set_dat(0);

	return data;
}

static void icp_write_byte(uint8_t data, uint8_t end, uint32_t delay1, uint32_t delay2)
{
	icp_bitsend(data, 8, ICP_WRITE_DELAY);

	pgm_set_dat(end);
	USLEEP(delay1);
	pgm_set_clk(1);
	USLEEP(delay2);
	pgm_set_dat(0);
	pgm_set_clk(0);
}

uint32_t icp_read_device_id(void)
{
	icp_send_command(CMD_READ_DEVICE_ID, 0);

	uint8_t devid[2];
	devid[0] = icp_read_byte(0);
	devid[1] = icp_read_byte(1);

	return (devid[1] << 8) | devid[0];
}

uint32_t icp_read_pid(void){
	icp_send_command(CMD_READ_DEVICE_ID, 2);
	uint8_t pid[2];
	pid[0] = icp_read_byte(0);
	pid[1] = icp_read_byte(1);
	return (pid[1] << 8) | pid[0];
}

uint8_t icp_read_cid(void)
{
	icp_send_command(CMD_READ_CID, 0);
	return icp_read_byte(1);
}

void icp_read_uid(uint8_t * buf)
{
	for (uint8_t  i = 0; i < 12; i++) {
		icp_send_command(CMD_READ_UID, i);
		buf[i] = icp_read_byte(1);
	}
}

void icp_read_ucid(uint8_t * buf)
{
	for (uint8_t i = 0; i < 16; i++) {
		icp_send_command(CMD_READ_UID, i + 0x20);
		buf[i] = icp_read_byte(1);
	}
}

uint32_t icp_read_flash(uint32_t addr, uint32_t len, uint8_t *data)
{
	icp_send_command(CMD_READ_FLASH, addr);

	for (uint32_t i = 0; i < len; i++){
		data[i] = icp_read_byte(i == (len-1));
	}
	return addr + len;
}

uint32_t icp_write_flash(uint32_t addr, uint32_t len, uint8_t *data)
{
	icp_send_command(CMD_WRITE_FLASH, addr);
	int delay1 = program_time;
	for (uint32_t i = 0; i < len; i++) {
		icp_write_byte(data[i], i == (len-1), delay1, 5);
	}
		
	return addr + len;
}

void icp_mass_erase(void)
{
	icp_send_command(CMD_MASS_ERASE, 0x3A5A5);
	icp_write_byte(0xff, 1, 50000, 500);
}

void icp_page_erase(uint32_t addr)
{
	icp_send_command(CMD_PAGE_ERASE, addr);
	icp_write_byte(0xff, 1, page_erase_time, 100);
}

void icp_outputf(const char *s, ...)
{
  char buf[160];
  va_list ap;
  va_start(ap, s);
  vsnprintf(buf, 160, s, ap);
  va_end(ap);
  pgm_print(buf);
}

#ifdef DYNAMIC_DELAY
void icp_set_cmd_bit_delay(int delay_us) {
	ICP_CMD_DELAY = delay_us;
}
void icp_set_read_bit_delay(int delay_us) {
	ICP_READ_DELAY = delay_us;
}
void icp_set_write_bit_delay(int delay_us) {
	ICP_WRITE_DELAY = delay_us;
}
#endif
int icp_get_cmd_bit_delay() {
	return ICP_CMD_DELAY;
}
int icp_get_read_bit_delay() {
	return ICP_READ_DELAY;
}
int icp_get_write_bit_delay() {
	return ICP_WRITE_DELAY;
}
