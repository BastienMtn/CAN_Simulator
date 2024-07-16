#ifndef LINUXCAN_API_INCLUDED
#define LINUXCAN_API_INCLUDED

#include <semaphore.h>
#include "CAN.h"


#ifdef __cplusplus
extern "C"
{
#endif

TCAN_HANDLE  CAN_Open(CHAR *ComPort, CHAR *szBitrate, CHAR *acceptance_code, CHAR *acceptance_mask, VOID *flags , DWORD Mode);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Close(TCAN_HANDLE Handle);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Flush(TCAN_HANDLE Handle);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Write(TCAN_HANDLE Handle, CAN_MSG *Buf);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Read(TCAN_HANDLE Handle, CAN_MSG *Buf);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Version(TCAN_HANDLE Handle,CHAR *buf);
	// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Status(TCAN_HANDLE Handle);
	// The return value is one of CAN_STATUS

#ifdef __cplusplus
}
#endif


#endif // LINUXCAN_API_INCLUDED
