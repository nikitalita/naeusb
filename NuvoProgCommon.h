#pragma once
/**COMMANDS**/
// Standard commands
#define NUVO_CMD_UPDATE_APROM 0xa0
#define NUVO_CMD_UPDATE_CONFIG 0xa1
#define NUVO_CMD_READ_CONFIG 0xa2
#define NUVO_CMD_ERASE_ALL 0xa3
#define NUVO_CMD_SYNC_PACKNO 0xa4
#define NUVO_CMD_GET_FWVER 0xa6
#define NUVO_CMD_RUN_APROM 0xab
#define NUVO_CMD_CONNECT 0xae
#define NUVO_CMD_GET_DEVICEID 0xb1
#define NUVO_CMD_RESET 0xad         // not implemented in default N76E003 ISP rom
#define NUVO_CMD_GET_FLASHMODE 0xCA // not implemented in default N76E003 ISP rom
#define NUVO_CMD_RUN_LDROM 0xac     // not implemented in default N76E003 ISP rom

// Not implemented yet
#define NUVO_CMD_RESEND_PACKET 0xFF // not implemented in default N76E003 ISP rom

// Extended commands
#define NUVO_CMD_READ_ROM 0xa5       // non-official
#define NUVO_CMD_GET_UID 0xb2        // non-official
#define NUVO_CMD_GET_CID 0xb3        // non-official
#define NUVO_CMD_GET_UCID 0xb4       // non-official
#define NUVO_CMD_GET_BANDGAP 0xb5    // non-official
#define NUVO_CMD_ISP_PAGE_ERASE 0xD5 // non-official

// Arduino ISP-to-ICP bridge only
#define NUVO_CMD_UPDATE_WHOLE_ROM 0xE1 // non-official
#define NUVO_CMD_ISP_MASS_ERASE 0xD6   // non-official

// ** Unsupported by N76E003 **
// Dataflash commands (when a chip has the ability to deliniate between data and program flash)
#define NUVO_CMD_UPDATE_DATAFLASH 0xC3
// SPI flash commands
#define NUVO_CMD_ERASE_SPIFLASH 0xD0
#define NUVO_CMD_UPDATE_SPIFLASH 0xD1
// CAN commands
#define NUVO_CAN_CMD_READ_CONFIG 0xA2000000
#define NUVO_CAN_CMD_RUN_APROM 0xAB000000
#define NUVO_CAN_CMD_GET_DEVICEID 0xB1000000

// Deprecated, no ISP programmer uses these
#define NUVO_CMD_READ_CHECKSUM 0xC8
#define NUVO_CMD_WRITE_CHECKSUM 0xC9
#define NUVO_CMD_SET_INTERFACE 0xBA

// ChipWhisperer specific
#define NUVO_GET_RAMBUF 0xe4
#define NUVO_SET_RAMBUF 0xe5
#define NUVO_GET_STATUS 0xe6

#define NUVO_ERR_OK 0
#define NUVO_ERR_FAILED 1
#define NUVO_ERR_COLLISION 2
#define NUVO_ERR_TIMEOUT 3

// The modes returned by CMD_GET_FLASHMODE
#define NUVO_APMODE 1
#define NUVO_LDMODE 2

// packet constants
#define NUVO_PKT_CMD_START 0
#define NUVO_PKT_CMD_SIZE 4
#define NUVO_PKT_SEQ_START 4
#define NUVO_PKT_SEQ_SIZE 4
#define NUVO_PKT_HEADER_END 8

#define NUVO_PACKSIZE 64

#define NUVO_INITIAL_UPDATE_PKT_START 16 // PKT_HEADER_END + 8 bytes for addr and len
#define NUVO_INITIAL_UPDATE_PKT_SIZE 48

#define NUVO_SEQ_UPDATE_PKT_START PKT_HEADER_END
#define NUVO_SEQ_UPDATE_PKT_SIZE 56

#define NUVO_DUMP_PKT_CHECKSUM_START PKT_HEADER_END
#define NUVO_DUMP_PKT_CHECKSUM_SIZE 0       // disabled for now
#define NUVO_DUMP_DATA_START PKT_HEADER_END //(DUMP_PKT_CHECKSUM_START + DUMP_PKT_CHECKSUM_SIZE)
#define NUVO_DUMP_DATA_SIZE 56              //(PACKSIZE - DUMP_DATA_START)
