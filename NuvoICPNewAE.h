#pragma once

#include <asf.h>

#define N51_CMD_UPDATE_APROM     0xa0
#define N51_CMD_UPDATE_CONFIG    0xa1
#define N51_CMD_READ_CONFIG      0xa2
#define N51_CMD_ERASE_ALL	     0xa3
#define N51_CMD_SYNC_PACKNO	     0xa4
#define N51_CMD_READ_ROM         0xa5 // added
#define N51_CMD_DUMP_ROM         0xaa // added
#define N51_CMD_GET_FWVER	     0xa6
#define N51_CMD_RUN_APROM	     0xab
#define N51_CMD_CONNECT		     0xae

#define N51_CMD_GET_DEVICEID    0xb1 // added
#define N51_CMD_GET_UID         0xb2 // added
#define N51_CMD_GET_CID         0xb3 // added
#define N51_CMD_GET_UCID        0xb4

#define N51_GET_RAMBUF 0xc0 // added
#define N51_SET_RAMBUF 0xc1 // added
#define N51_GET_STATUS 0xc2 // added


#define N51_ERR_OK                         0
#define N51_ERR_FAILED                     1
#define N51_ERR_COLLISION                  2
#define N51_ERR_TIMEOUT                    3


bool NuvoICP_Protocol_Command(void);
		#if defined(INCLUDE_FROM_NUVOICP_C)
            static void NuvoICP_EnterXPROGMode(void);
			static void NuvoICP_LeaveXPROGMode(void);
			static void NuvoICP_SetParam(void);
			static void NuvoICP_Mass_Erase(void);
			static void NuvoICP_WriteMemory(uint8_t * buf);
			static void NuvoICP_ReadMemory(uint8_t * buf);

		#endif
