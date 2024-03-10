#pragma once

#include <asf.h>

bool NuvoICP_Protocol_Command(void);
		#if defined(INCLUDE_FROM_NUVOICP_C)
            static void NuvoICP_EnterXPROGMode(void);
			static void NuvoICP_LeaveXPROGMode(void);
			static void NuvoICP_SetParam(void);
			static void NuvoICP_Mass_Erase(void);
			static void NuvoICP_WriteMemory(uint8_t * buf);
			static void NuvoICP_ReadMemory(uint8_t * buf);

		#endif
