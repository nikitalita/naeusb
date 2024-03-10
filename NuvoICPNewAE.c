#define INCLUDE_FROM_NUVOICP_C 1
#include "NuvoICPNewAE.h"
#undef INCLUDE_FROM_NUVOICP_C
#include "NuvoICP.h"
#include <stdint.h>

#define XMEGA_BUF_SIZE 256

/* Status of last executed command */
uint8_t N51_Status;

bool NuvoICP_Protocol_Command(void) {
  static uint8_t status_payload[4];
  status_payload[0] = udd_g_ctrlreq.req.wValue & 0xff;

  static uint8_t xprog_rambuf[XMEGA_BUF_SIZE];
  uint8_t offset;

  switch (status_payload[0]) {
  case N51_CMD_UPDATE_APROM:
    NuvoICP_WriteMemory(xprog_rambuf);
    break;

  case N51_CMD_UPDATE_CONFIG:
    NuvoICP_WriteMemory(xprog_rambuf);
    break;

  case N51_CMD_READ_CONFIG:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_ERASE_ALL:
    NuvoICP_Mass_Erase();
    break;

  case N51_CMD_SYNC_PACKNO:
    NuvoICP_SetParam();
    break;

  case N51_CMD_READ_ROM:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_DUMP_ROM:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_GET_FWVER:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_RUN_APROM:
    NuvoICP_LeaveXPROGMode();
    break;

  case N51_CMD_CONNECT:
    NuvoICP_EnterXPROGMode();
    break;

  case N51_CMD_GET_DEVICEID:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_GET_UID:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_GET_CID:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case N51_CMD_GET_UCID:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;
  case N51_GET_RAMBUF:
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > XMEGA_BUF_SIZE) {
      // nice try!
      return false;
    }

    udd_g_ctrlreq.payload = xprog_rambuf + offset;
    udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
    return true;
    break;

  // Write data to intername RAM buffer
  case N51_SET_RAMBUF:
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > XMEGA_BUF_SIZE) {
      // nice try!
      return false;
    }

    memcpy(xprog_rambuf + offset, udd_g_ctrlreq.payload,
           udd_g_ctrlreq.req.wLength);
    return true;
    break;

  case N51_GET_STATUS:
    status_payload[1] = N51_Status;
    status_payload[2] = 0;
    udd_g_ctrlreq.payload = status_payload;
    udd_g_ctrlreq.payload_size = 3;
    return true;
    break;

  default:
    break;
  }

  return false;
}

void NuvoICP_EnterXPROGMode(void) {
  if (!icp_init(true)) {
    N51_Status = N51_ERR_FAILED;
  } else {
    N51_Status = N51_ERR_OK;
  }
}
void NuvoICP_LeaveXPROGMode(void) {
  icp_exit();

  N51_Status = N51_ERR_OK;
}
void NuvoICP_SetParam(void) {
    
}
void NuvoICP_Mass_Erase(void) {
    icp_mass_erase();
    N51_Status = N51_ERR_OK;
}
void NuvoICP_WriteMemory(uint8_t *buf) {
    N51_Status = N51_ERR_OK;
	
	if (udd_g_ctrlreq.req.wLength < 8) {
		N51_Status = N51_ERR_FAILED;
	}
	
	// uint8_t MemoryType = udd_g_ctrlreq.payload[0]; // not used
	// uint8_t  PageMode = udd_g_ctrlreq.payload[1]; // not used
	uint32_t Address = (udd_g_ctrlreq.payload[5] << 24) | (udd_g_ctrlreq.payload[4] << 16) | (udd_g_ctrlreq.payload[3] << 8) | (udd_g_ctrlreq.payload[2]);
	uint16_t Length = udd_g_ctrlreq.payload[6] | (udd_g_ctrlreq.payload[7] << 8);

	if (Length > XMEGA_BUF_SIZE) {
		Length = XMEGA_BUF_SIZE;
	}	

    icp_write_flash(Address, Length, buf);
    N51_Status = N51_ERR_OK;
}
void NuvoICP_ReadMemory(uint8_t *buf) {
    N51_Status = N51_ERR_OK;
	//uint8_t MemoryType = udd_g_ctrlreq.payload[0]; //Not used
	uint32_t Address = (udd_g_ctrlreq.payload[4] << 24) | (udd_g_ctrlreq.payload[3] << 16) | (udd_g_ctrlreq.payload[2] << 8) | (udd_g_ctrlreq.payload[1]);
	uint16_t Length = udd_g_ctrlreq.payload[5] | (udd_g_ctrlreq.payload[6] << 8);

    if (Length > XMEGA_BUF_SIZE) {
		Length = XMEGA_BUF_SIZE;
	}

    icp_read_flash(Address, Length, buf);
    N51_Status = N51_ERR_OK;
}