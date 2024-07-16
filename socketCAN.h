#ifndef CAN_SIMULATOR_SOCKETCAN_H
#define CAN_SIMULATOR_SOCKETCAN_H
#include "CAN.h"

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

#endif //CAN_SIMULATOR_SOCKETCAN_H
