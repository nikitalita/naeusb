#include "naeusb_default.h"
#include <string.h>

#define uint8_t unsigned char

// static uint16_t MPSSE_TX_IDX = 0;
static uint8_t MPSSE_CUR_CMD = 0; // the current MPSSE command
static int16_t MPSSE_TX_IDX = 0; // the current TX buffer index
static int16_t MPSSE_TX_BYTES = 0; // the number of bytes in the tx buffer
static int16_t MPSSE_RX_BYTES = 2; // the number of bytes in the rx buffer
static int16_t MPSSE_TRANSMISSION_LEN = 0; // the number of bits/bytes in the current data transmission

static uint8_t MPSSE_LOOPBACK_ENABLED = 0; // connect TDI to TDO for loopback

//need lock to make sure we don't accept another transaction until we're done sending data back
//should be fine since  it looks like openocd just keeps retrying transaction until it works
static uint8_t MPSSE_TRANSACTION_LOCK = 1;  //Currently sending data back to the PC

static uint32_t NUM_PROCESSED_CMDS = 0;
static uint8_t MPSSE_TX_REQ = 0;
static uint8_t MPSSE_FIRST_BIT = 0; // AHHHHH handle first bit differently in every transmission

static uint8_t MPSSE_ENABLED = 0;
static uint8_t MPSSE_COMMAND_IDX = 0;

static uint8_t MPSSE_SCK_IDLE_LEVEL = 0;

static uint32_t NUM_BYTE_SINCE_SEND_IMM = 0;


COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_BUFFER[64] __attribute__ ((__section__(".mpssemem")));
// COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_CTRL_BUFFER[64] __attribute__ ((__section__(".mpssemem")));
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER[80] __attribute__ ((__section__(".mpssemem"))) = {0};
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER_BAK[64] __attribute__ ((__section__(".mpssemem"))) = {0};
// COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_COMMAND_HIST[256] __attribute__ ((__section__(".mpssemem"))) = {0};

void mpsse_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

#define BITMODE_MPSSE 0x02

#define SIO_RESET_REQUEST             0x00
#define SIO_SET_LATENCY_TIMER_REQUEST 0x09
#define SIO_GET_LATENCY_TIMER_REQUEST 0x0A
#define SIO_SET_BITMODE_REQUEST       0x0B

#define ADDR_IOROUTE 55

// TODO: DOUT and DIN are technically backwards, even though this chip is the master
#if AVRISP_USEUART
    #define MPSSE_DOUT_GPIO AVRISP_MOSI_GPIO
    #define MPSSE_DIN_GPIO  AVRISP_MISO_GPIO
    #define MPSSE_SCK_GPIO  AVRISP_SCK_GPIO
#else
    #define MPSSE_DOUT_GPIO SPI_MOSI_GPIO
    #define MPSSE_DIN_GPIO SPI_MISO_GPIO
    #define MPSSE_SCK_GPIO SPI_SPCK_GPIO
#endif

#define MPSSE_TMS_GPIO PIN_PDIC_GPIO
#define MPSSE_TRST_GPIO PIN_PDIDTX_GPIO

static uint32_t MPSSE_PINS_GPIO[8] = {
    MPSSE_SCK_GPIO,
    MPSSE_DIN_GPIO,
    MPSSE_DOUT_GPIO,
    MPSSE_TMS_GPIO,
    MPSSE_TRST_GPIO
};

#define SIO_RESET_SIO 0
#define SIO_RESET_PURGE_RX 1
#define SIO_RESET_PURGE_TX 2

extern void switch_configurations(void);

static int16_t mpsse_rx_buffer_remaining(void)
{
    return sizeof(MPSSE_RX_BUFFER) - MPSSE_RX_BYTES;
}

static int16_t mpsse_tx_buffer_remaining(void)
{
    return MPSSE_TX_BYTES - MPSSE_TX_IDX;
}

bool mpsse_setup_out_received(void)
{
    // don't handle 
    uint8_t wValue = udd_g_ctrlreq.req.wValue & 0xFF;
    if ((wValue == 0x42) && (udd_g_ctrlreq.req.bRequest == REQ_SAM_CFG)) {
        udc_stop();
        switch_configurations();
        udc_start();
        return true;
    }
    if ((udd_g_ctrlreq.req.wIndex != 0x01) && (udd_g_ctrlreq.req.wIndex != 0x02)) {
        return false;
    }
    if ((udd_g_ctrlreq.req.bRequest == SIO_RESET_REQUEST)) {
        // TODO: cancel old run if one setup
        memset(MPSSE_RX_BUFFER, 0, sizeof(MPSSE_RX_BUFFER));
        // memset(MPSSE_RX_CTRL_BUFFER, 0, sizeof(MPSSE_RX_CTRL_BUFFER));
        memset(MPSSE_TX_BUFFER, 0, sizeof(MPSSE_TX_BUFFER_BAK));
        memset(MPSSE_TX_BUFFER_BAK, 0, sizeof(MPSSE_TX_BUFFER_BAK));
        udd_ep_abort(UDI_MPSSE_EP_BULK_OUT);
        udd_ep_abort(UDI_MPSSE_EP_BULK_IN);
        gpio_configure_pin(MPSSE_DIN_GPIO, PIO_DEFAULT | PIO_TYPE_PIO_INPUT);
        gpio_configure_pin(MPSSE_DOUT_GPIO, PIO_OUTPUT_0);
        gpio_configure_pin(MPSSE_SCK_GPIO, PIO_OUTPUT_0);
        gpio_configure_pin(MPSSE_TMS_GPIO, PIN_PDIC_OUT_FLAGS);
		MPSSE_ENABLED = 1;
        MPSSE_TRANSACTION_LOCK = 1;
        NUM_PROCESSED_CMDS = 0;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
    }
    return true;
}



bool mpsse_setup_in_received(void)
{
    // don't handle 
    if (udd_g_ctrlreq.req.wIndex != 0x01) {
        return false;
    }
    if (udd_g_ctrlreq.req.bRequest == 0xA0) {
        MPSSE_RX_BUFFER[0] = MPSSE_CUR_CMD;
        MPSSE_RX_BUFFER[1] = MPSSE_TX_IDX & 0xFF;
        MPSSE_RX_BUFFER[2] = MPSSE_TX_BYTES & 0xFF;
        MPSSE_RX_BUFFER[3] = MPSSE_RX_BYTES & 0xFF;
        MPSSE_RX_BUFFER[4] = MPSSE_TRANSMISSION_LEN & 0xFF;
        MPSSE_RX_BUFFER[5] = MPSSE_TRANSACTION_LOCK;
        MPSSE_RX_BUFFER[6] = NUM_PROCESSED_CMDS & 0xFF;
        MPSSE_RX_BUFFER[7] = (NUM_PROCESSED_CMDS >> 8) & 0xFF;
        MPSSE_RX_BUFFER[8] = (NUM_PROCESSED_CMDS >> 16) & 0xFF;
        MPSSE_RX_BUFFER[9] = (NUM_PROCESSED_CMDS >> 24) & 0xFF;
        MPSSE_RX_BUFFER[10] = MPSSE_LOOPBACK_ENABLED;
        udd_g_ctrlreq.payload = MPSSE_RX_BUFFER;
        udd_g_ctrlreq.payload_size = 11;
        return true;
    }

    uint16_t wValue = udd_g_ctrlreq.req.wValue;
    if (udd_g_ctrlreq.req.bRequest == 0xA1) {
        uint32_t addr = (uint32_t)(MPSSE_RX_BUFFER + wValue);
        addr &= ~(0b11);
        udd_g_ctrlreq.payload = (void *) addr;
        udd_g_ctrlreq.payload_size = 256;
    }
    if (udd_g_ctrlreq.req.bRequest == 0xA2) {
        uint32_t addr = (uint32_t)(MPSSE_TX_BUFFER + wValue);
        addr &= ~(0b11);
        udd_g_ctrlreq.payload = (void *) addr;
        udd_g_ctrlreq.payload_size = 256;
    }
    return true;
}


// 
enum FTDI_CMD_BITS {
    FTDI_NEG_CLK_WRITE  = 1 << 0,
    FTDI_BIT_MODE       = 1 << 1,
    FTDI_NEG_CLK_READ   = 1 << 2,
    FTDI_LITTLE_ENDIAN  = 1 << 3,
    FTDI_WRITE_TDI      = 1 << 4,
    FTDI_READ_TDO       = 1 << 5,
    FTDI_WRITE_TMS      = 1 << 6,
    FTDI_SPECIAL_CMD    = 1 << 7,
};

enum FTDI_SPECIAL_CMDS {
    FTDI_SET_OPLB = 0x80,
    FTDI_READ_IPLB,
    FTDI_SET_OPHB,
    FTDI_READ_IPHB,
    FTDI_EN_LOOPBACK,
    FTDI_DIS_LOOPBACK,
    FTDI_DIS_CLK_DIV_5 = 0x8A,
    FTDI_EN_CLK_DIV_5 = 0x8A,
    FTDI_SEND_IMM=0x87,
    FTDI_ADT_CLK_ON=0x96,
    FTDI_ADT_CLK_OFF
};


#define DOUT_NMATCH_SCK ((gpio_pin_is_low(MPSSE_SCK_GPIO) ? \
                    !!(MPSSE_CUR_CMD & FTDI_NEG_CLK_WRITE) : \
                    !(MPSSE_CUR_CMD & FTDI_NEG_CLK_WRITE)))

#define DIN_NMATCH_SCK ((gpio_pin_is_low(MPSSE_SCK_GPIO) ? \
                    !!(MPSSE_CUR_CMD & FTDI_NEG_CLK_READ) : \
                    !(MPSSE_CUR_CMD & FTDI_NEG_CLK_READ)))


uint8_t mpsse_send_bit(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;
    if (MPSSE_LOOPBACK_ENABLED) {
        return value & 0x01;
    }

        // actually do write
    dpin = MPSSE_DOUT_GPIO;

    if (1) { //Clock out data on IDLE->NON_IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }
    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //Clock out data on NON_IDLE->IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }

    if (1) { //read data on IDLE->NON_IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }



    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //read data on NON_IDLE->IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }
    
    return read_value & 0x01;
}

uint8_t mpsse_send_bits(uint8_t value, uint8_t num_bits)
{
    uint8_t read_value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
            read_value >>= 1;
            read_value |= mpsse_send_bit((value >> i) & 0x01) << 7;
        } else {
            read_value |= mpsse_send_bit((value >> (7 - i)) & 0x01) << (7 - i);
        }
    }
    return read_value;
}

uint8_t mpsse_send_byte(uint8_t value)
{

    return mpsse_send_bits(value, 8);
}

uint8_t mpsse_tms_bit_send(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;

        // actually do write
    dpin = MPSSE_TMS_GPIO;

    if (1) { //Clock out data on IDLE->NON_IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }
    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (1) { //read data on IDLE->NON_IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }

    if (0) { //Clock out data on NON_IDLE->IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }

    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //read data on NON_IDLE->IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }
    
    return read_value & 0x01;

}

uint8_t mpsse_tms_send(uint8_t value, uint8_t num_bits)
{
    uint8_t read_value = 0;
    uint8_t i = 0;
    // one bit is clocked output on TDI for some reason
    if (num_bits == 7) {
        uint8_t bitval = 0;
        bitval = (value >> 7) & 0x01;
        if (bitval) {
            gpio_set_pin_high(MPSSE_DOUT_GPIO);
        } else {
            gpio_set_pin_low(MPSSE_DOUT_GPIO);
        }
        i++;
    }
    for (; i < num_bits; i++) {
        if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
            read_value |= mpsse_tms_bit_send((value >> i) & 0x01) << i;
        } else {
            read_value |= mpsse_tms_bit_send((value >> (7 - i)) & 0x01) << (7 - i);
        }

    }
	return read_value;
}

// TODO: handle tx length too short
void mpsse_handle_transmission(void)
{
    uint8_t read_val = 0;
    if (MPSSE_TRANSMISSION_LEN == 0) {
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need more data
        }
        // not doing a transmission currently, so read length in
        // TODO: handle invalid lengths
        MPSSE_TRANSMISSION_LEN = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];

        if (!(MPSSE_CUR_CMD & FTDI_BIT_MODE)) {
            // clock in high byte of length if in byte mode
            MPSSE_TRANSMISSION_LEN |= MPSSE_TX_BUFFER[MPSSE_TX_IDX++] << 8;
            MPSSE_FIRST_BIT = 1;
            MPSSE_TRANSMISSION_LEN++; //0x00 sends 1 byte
        } else {
            // take care of bit transmission right now
            MPSSE_TRANSMISSION_LEN++; //0x00 sends 1 byte
            if (MPSSE_CUR_CMD & FTDI_WRITE_TMS) {
                read_val = mpsse_tms_send(MPSSE_TX_BUFFER[MPSSE_TX_IDX++], MPSSE_TRANSMISSION_LEN);
                MPSSE_TRANSMISSION_LEN = 0;
            } else {
                uint8_t value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
				uint8_t bit;
                if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
                    bit = value & 0x01;
                } else {
                    bit = value & 0x80;
                }

                if (bit) {
                    gpio_set_pin_high(MPSSE_DOUT_GPIO);
                } else {
                    gpio_set_pin_low(MPSSE_DOUT_GPIO);
                }
                read_val = mpsse_send_bits(value, MPSSE_TRANSMISSION_LEN);
            }
            MPSSE_TRANSMISSION_LEN = 0;
            if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
                MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val;
                if (mpsse_rx_buffer_remaining() > 0) {
                    // do nothing
                } else {
                    MPSSE_TRANSACTION_LOCK = 1;
                    udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                        sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
                }
            }
            MPSSE_CUR_CMD = 0;
            return;
        }
    }
    if (mpsse_tx_buffer_remaining() < 1) {
        MPSSE_TX_REQ = 1;
        return;
    }

    // todo: handle invalid command: no write or read
    // TMS works differently for some reason, so handle it separately
    if (MPSSE_CUR_CMD & (FTDI_WRITE_TDI)) {
        // handle first bit separately for some reason
        // AHHHHHHHH
        if ((!MPSSE_LOOPBACK_ENABLED)) {
            uint8_t value = (MPSSE_TX_BUFFER[MPSSE_TX_IDX]);
            uint8_t bit = 0;
            if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
                bit = value & 0x01;
            } else {
                bit = value & 0x80;
            }

            if (bit) {
                gpio_set_pin_high(MPSSE_DOUT_GPIO);
            } else {
                gpio_set_pin_low(MPSSE_DOUT_GPIO);
            }
            MPSSE_FIRST_BIT = 0;
        }
        read_val = mpsse_send_byte(MPSSE_TX_BUFFER[MPSSE_TX_IDX++]);
    } else {
        read_val = mpsse_send_byte(0);
    }

    if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
        MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val;
        if (mpsse_rx_buffer_remaining() > 0) {
        } else {
            MPSSE_TRANSACTION_LOCK = 1;
            udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
            // return;
        }
    }

    // we're at the end of the transmission
    if (--MPSSE_TRANSMISSION_LEN == 0) {
        MPSSE_CUR_CMD = 0;
    }

}

void mpsse_handle_special(void)
{
    uint8_t value = 0;
    uint8_t direction = 0;
    switch (MPSSE_CUR_CMD) {
    case FTDI_SET_OPLB:
        // value, direction
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need to read more data in
        }
        MPSSE_CUR_CMD = 0x00;
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        for (uint8_t i = 0; i < 5; i++) {
            if (direction & (1 << i)) {
				if (value & (1 << i)) {
					gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_OUTPUT_1);
                    if (i == 0)
                        MPSSE_SCK_IDLE_LEVEL = 1;
				} else {
					gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_OUTPUT_0);
                    if (i == 0)
                        MPSSE_SCK_IDLE_LEVEL = 0;
				}
            } else {
                gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_DEFAULT);
            }
        }

        FPGA_releaselock();
        while(!FPGA_setlock(fpga_generic));
        FPGA_setaddr(ADDR_IOROUTE);
        uint8_t gpio_state[8];
        memcpy(gpio_state, xram, 8);
        if (direction & (1 << 6)) {
            gpio_state[6] |= 1 << 0;
            if (value & (1 << 6)) {
                gpio_state[6] |= 1 << 1;
            } else {
                gpio_state[6] &= ~(1 << 1);
            }
        } else {
            gpio_state[6] &= ~(1 << 0);
        }

        FPGA_setaddr(ADDR_IOROUTE);
        memcpy(xram, gpio_state, 8);
        FPGA_releaselock();

        break;
    case FTDI_SET_OPHB:
        // value, direction
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need to read more data in
        }
        MPSSE_CUR_CMD = 0x00;
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        break;
    case FTDI_READ_IPLB:
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[2] = 0x00;
        MPSSE_RX_BYTES = 3;
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_READ_IPHB:
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[2] = 0x01;
        MPSSE_RX_BYTES = 3;
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_EN_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 1;
        MPSSE_CUR_CMD = 0x00;
        break;
    case FTDI_DIS_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 0;
        MPSSE_CUR_CMD = 0x00;
        break;
    case FTDI_SEND_IMM:
        MPSSE_TRANSACTION_LOCK = 1;
        MPSSE_CUR_CMD = 0x00;
        NUM_BYTE_SINCE_SEND_IMM = 0;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case 0x86:
        // some clock command
        MPSSE_TX_IDX += 2;
        MPSSE_CUR_CMD = 0x00;
        break;
    case 0x8A:
        MPSSE_CUR_CMD = 0x00;
        break;
    default:
        MPSSE_CUR_CMD = 0x00;
        break;
    }
}


void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    // we just receive stuff here, then handle in main()
    if (UDD_EP_TRANSFER_OK != status) {
        // restart
        //TODO: Separate for BAK an non BAK cases
        if (MPSSE_TX_REQ) {
            udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER_BAK, 
                sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);

        }
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, 
            sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        MPSSE_TRANSACTION_LOCK = 1;
        return;
    }

    NUM_BYTE_SINCE_SEND_IMM += nb_transfered;
    if (MPSSE_TX_REQ) {
        // we read into the backup buffer, move the data over to the usual one
        for (uint16_t i = 0; i < nb_transfered; i++) {
            MPSSE_TX_BUFFER[i + mpsse_tx_buffer_remaining()] = MPSSE_TX_BUFFER_BAK[i];
        }
        MPSSE_TX_BYTES = mpsse_tx_buffer_remaining() + nb_transfered;
    } else {
        MPSSE_TX_BYTES = nb_transfered;
    }
    MPSSE_TX_REQ = 0;
    MPSSE_TX_IDX = 0;
    MPSSE_TRANSACTION_LOCK = 0;
}

void mpsse_vendor_bulk_in_received(udd_ep_status_t status, iram_size_t nb_transferred, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        // I think this is the right thing to do
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        return;
    }
    for (uint16_t i = 0; i < (MPSSE_RX_BYTES - nb_transferred); i++) {
        // move data to start of buffer
        // simplifies things, plus avoids buffer alignment issues
        MPSSE_RX_BUFFER[i] = MPSSE_RX_BUFFER[nb_transferred + i];
    }
    MPSSE_RX_BYTES -= nb_transferred;
    
    if (MPSSE_RX_BYTES) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
    } else {
        MPSSE_RX_BYTES = 2;
        MPSSE_RX_BUFFER[0] = 0x00;
        MPSSE_RX_BUFFER[1] = 0x00;
        MPSSE_TRANSACTION_LOCK = 0;
    }

}

void mpsse_register_handlers(void)
{
    naeusb_add_out_handler(mpsse_setup_out_received);
    naeusb_add_in_handler(mpsse_setup_in_received);
}

// TODO: do writing here as we have time
// TODO: if we need to implement adaptive clock, should do in a GPIO based ISR I think?
void MPSSE_main_sendrecv_byte(void)
{
    // return;
	if (!MPSSE_ENABLED) return;

    if (MPSSE_TRANSACTION_LOCK) {
        // current doing a send back to PC, so wait until that's done
        return;
    }

    if (MPSSE_TX_REQ) {
        // command split between USB transactions,
        // so move unused data back to start and read more in
        for (uint16_t i = 0; i < (mpsse_tx_buffer_remaining()); i++) {
            MPSSE_TX_BUFFER[i] = MPSSE_TX_BUFFER[i+MPSSE_TX_IDX];
        }
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, 
            MPSSE_TX_BUFFER_BAK, sizeof(MPSSE_TX_BUFFER_BAK), 
            mpsse_vendor_bulk_out_received);
        return;
    }

    // we're at end of the TX buffer, so read more in
    if (mpsse_tx_buffer_remaining() <= 0) {
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        return;
    }

    if (MPSSE_CUR_CMD == 0x00) {
        MPSSE_CUR_CMD = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        //MPSSE_COMMAND_HIST[MPSSE_COMMAND_IDX++] = MPSSE_CUR_CMD;
        NUM_PROCESSED_CMDS++;
    }

    if (MPSSE_CUR_CMD & 0x80) {
        mpsse_handle_special();
    } else {
        mpsse_handle_transmission();
    }

}