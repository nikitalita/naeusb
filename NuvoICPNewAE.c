#define INCLUDE_FROM_NUVOICP_C 1
#include "NuvoICPNewAE.h"
#undef INCLUDE_FROM_NUVOICP_C
#include "NuvoProgCommon.h"
#include "NuvoICP.h"
#include <stdint.h>
#include <string.h>

#define NUVOICP_BUF_SIZE 256

/* Status of last executed command */
uint8_t N51_Status;


bool NuvoICP_Protocol_Command(void)
{
  static uint8_t status_payload[32];
  status_payload[0] = udd_g_ctrlreq.req.wValue & 0xff;
  static uint16_t nuvoicp_status_payload_size = 0;

  static uint8_t xprog_rambuf[NUVOICP_BUF_SIZE];
  uint8_t offset;

  switch (status_payload[0])
  {
  case NUVO_CMD_UPDATE_APROM:
    NuvoICP_WriteMemory(xprog_rambuf);
    break;

  case NUVO_CMD_UPDATE_CONFIG:
    NuvoICP_UpdateConfig();
    break;

  case NUVO_CMD_READ_CONFIG:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_READ_CONFIG, &status_payload[2]);
    break;

  case NUVO_CMD_PAGE_ERASE:
    NuvoICP_Page_Erase(xprog_rambuf);
    break;

  case NUVO_CMD_MASS_ERASE:
    NuvoICP_Mass_Erase();
    break;

  case NUVO_CMD_READ_ROM:
    NuvoICP_ReadMemory(xprog_rambuf);
    break;

  case NUVO_CMD_GET_FWVER:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_FWVER, &status_payload[2]);
    break;

  case NUVO_CMD_RUN_APROM:
  case NUVO_CMD_RESET:
    NuvoICP_LeaveXPROGMode();
    break;

  case NUVO_CMD_CONNECT:
    NuvoICP_EnterXPROGMode();
    break;

  case NUVO_CMD_GET_DEVICEID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_DEVICEID, &status_payload[2]);
    break;

  case NUVO_CMD_GET_UID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_UID,  &status_payload[2]);
    break;

  case NUVO_CMD_GET_CID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_CID, &status_payload[2]);
    break;

  case NUVO_CMD_GET_UCID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_UCID, &status_payload[2]);
    break;
  case NUVO_GET_RAMBUF:
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > NUVOICP_BUF_SIZE)
    {
      // nice try!
      return false;
    }

    udd_g_ctrlreq.payload = xprog_rambuf + offset;
    udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
    return true;
    break;

  // Write data to intername RAM buffer
  case NUVO_SET_RAMBUF:
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > NUVOICP_BUF_SIZE)
    {
      // nice try!
      return false;
    }

    memcpy(xprog_rambuf + offset, udd_g_ctrlreq.payload,
           udd_g_ctrlreq.req.wLength);
    return true;
    break;

  case NUVO_GET_STATUS:
    status_payload[1] = N51_Status;
    udd_g_ctrlreq.payload = status_payload;
    udd_g_ctrlreq.payload_size = nuvoicp_status_payload_size + 2;
    nuvoicp_status_payload_size = 0;
    return true;
    break;

  case NUVO_ENTER_ICP_MODE:
    icp_entry(true);
    break;

  case NUVO_EXIT_ICP_MODE:
    icp_exit();
    break;
  
  case NUVO_REENTER_ICP:
    icp_reentry(5000, 1000, 10);
    break;

  case NUVO_REENTRY_GLITCH:
    icp_reentry_glitch(5000, 1000, 0, 280);
    break;
  
  case NUVO_REENTRY_GLITCH_READ:
    icp_reentry_glitch_read(5000, 1000, 0, 280, xprog_rambuf);
    break;


  default:
    break;
  }

  return false;
}

void NuvoICP_EnterXPROGMode(void)
{
  if (!icp_init(true))
  {
    N51_Status = NUVO_ERR_FAILED;
  }
  else
  {
    N51_Status = NUVO_ERR_OK;
  }
}

void NuvoICP_LeaveXPROGMode(void)
{
  icp_deinit();
  N51_Status = NUVO_ERR_OK;
}

void NuvoICP_Mass_Erase(void)
{
  icp_mass_erase();
  N51_Status = NUVO_ERR_OK;
}

void NuvoICP_UpdateConfig(void) {
  N51_Status = NUVO_ERR_OK;

  if (udd_g_ctrlreq.req.wLength < 2 + CFG_FLASH_LEN)
  {
    N51_Status = NUVO_ERR_FAILED;
  }
  icp_write_flash(CFG_FLASH_ADDR, CFG_FLASH_LEN, &udd_g_ctrlreq.payload[2]);
  N51_Status = NUVO_ERR_OK;
}

void NuvoICP_WriteMemory(uint8_t *buf)
{
  N51_Status = NUVO_ERR_OK;

  if (udd_g_ctrlreq.req.wLength < 8)
  {
    N51_Status = NUVO_ERR_FAILED;
  }

  uint32_t Address = (udd_g_ctrlreq.payload[5] << 24) | (udd_g_ctrlreq.payload[4] << 16) | (udd_g_ctrlreq.payload[3] << 8) | (udd_g_ctrlreq.payload[2]);
  uint16_t Length = udd_g_ctrlreq.payload[6] | (udd_g_ctrlreq.payload[7] << 8);

  if (Length > NUVOICP_BUF_SIZE)
  {
    Length = NUVOICP_BUF_SIZE;
  }

  icp_write_flash(Address, Length, buf);
  N51_Status = NUVO_ERR_OK;
}

void NuvoICP_Page_Erase(uint8_t * xprog_rambuf){
  N51_Status = NUVO_ERR_OK;

  uint32_t Address = NU51_PAGE_MASK & ((udd_g_ctrlreq.payload[5] << 24) | (udd_g_ctrlreq.payload[4] << 16) | (udd_g_ctrlreq.payload[3] << 8) | (udd_g_ctrlreq.payload[2]));
  icp_page_erase(Address);
}

/**
 * @brief Get the value of a parameter
 * 
 * @param cmd The command to get the value of
 * @param buf The buffer to store the value in
 * @returns The length of the value
*/
uint16_t NuvoICP_GetParam(uint8_t cmd, uint8_t *buf)
{
  N51_Status = NUVO_ERR_OK;
  uint32_t value = 0;
  bool in_val = false;
  uint16_t val_length = 0;
  switch (cmd)
  {
    case NUVO_CMD_GET_DEVICEID:
      value = icp_read_device_id();
      in_val = true;
      break;
    case NUVO_CMD_GET_UID:
      icp_read_uid(buf);
      val_length = 12;
      break;
    case NUVO_CMD_GET_CID:
      value = icp_read_cid();
      in_val = true;
      break;
    case NUVO_CMD_GET_UCID:
      icp_read_ucid(buf);
      val_length = 16;
      break;
    case NUVO_CMD_GET_FWVER:
      value = 0xD0;
      in_val = true;
      break;
    case NUVO_CMD_GET_FLASHMODE:
      icp_read_flash(CFG_FLASH_ADDR, CFG_FLASH_LEN, buf);
      if (buf[0] & 0x80)
      {
        value = NUVO_APMODE;
      }
      else
      {
        value = NUVO_LDMODE;
      }
      for (uint8_t i = 0; i < 5; i++)
      {
        buf[i] = 0;
      }
      buf[0] = value;
      val_length = 1;
      
      break;
    case NUVO_CMD_READ_CONFIG:
      icp_read_flash(CFG_FLASH_ADDR, CFG_FLASH_LEN, buf);
      val_length = CFG_FLASH_LEN;
      break;
  }
  if (in_val)
  {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    val_length = 4;
  }

  N51_Status = NUVO_ERR_OK;
  return val_length;
}

void NuvoICP_ReadMemory(uint8_t *buf)
{
  N51_Status = NUVO_ERR_OK;
  // uint8_t MemoryType = udd_g_ctrlreq.payload[0]; //Not used
  uint32_t Address = (udd_g_ctrlreq.payload[4] << 24) | (udd_g_ctrlreq.payload[3] << 16) | (udd_g_ctrlreq.payload[2] << 8) | (udd_g_ctrlreq.payload[1]);
  uint16_t Length = udd_g_ctrlreq.payload[5] | (udd_g_ctrlreq.payload[6] << 8);

  if (Length > NUVOICP_BUF_SIZE)
  {
    Length = NUVOICP_BUF_SIZE;
  }

  icp_read_flash(Address, Length, buf);
  N51_Status = NUVO_ERR_OK;
}