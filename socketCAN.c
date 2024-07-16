#include <unistd.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "socketCAN.h"

// The return value is one of CAN_STATUS
TCAN_HANDLE CAN_Open(CHAR *ComPort, CHAR *szBitrate, CHAR *acceptance_code, CHAR *acceptance_mask, VOID *flags , DWORD Mode){
    int s;
    struct sockaddr_can addr;
    struct ifreq ifr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0){
        perror("Error opening socket");
        return -1;
    }

    strcpy(ifr.ifr_name, ComPort );
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("Error getting socket index");
        return -1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    struct can_filter rfilter[1];

    rfilter[0].can_id   = *acceptance_code;
    rfilter[0].can_mask = *acceptance_mask;

    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    switch (Mode) {
        case Normal:
            break;
        case ListenOnly:{
            printf("This option needs to be set at the network interface level!");
            break;
        }
        case LoopBack: {
            int recv_own_msgs = 1; /* 0 = disabled (default), 1 = enabled */

            setsockopt(s, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                       &recv_own_msgs, sizeof(recv_own_msgs));
            break;
        }
    }

    if (!strcmp(szBitrate, "")) printf("To set the bitrate, use the corresponding option when invoking slcand!");

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        perror("Error binding socket");
        return -1;
    }
    return s;
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Close(TCAN_HANDLE Handle){
    return close(Handle);
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Flush(TCAN_HANDLE Handle){
    return CAN_ERR_OK;
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Write(TCAN_HANDLE Handle, CAN_MSG *Buf){
    struct can_frame frame;

    u_int32_t flag = 0x0;
    if (Buf->Flags == CAN_FLAGS_EXTENDED){
        flag = CAN_EFF_FLAG;
    }
    frame.can_id = Buf->Id | flag;
    frame.len = Buf->Size;

    for (int i = 0; i < Buf->Size; ++i) {
        frame.data[i] = Buf->Data[i];
    }

    int nbytes = write(Handle, &frame, sizeof(struct can_frame));

    if (nbytes != sizeof (struct can_frame)){
        return CAN_ERR_ERR;
    }
    return CAN_ERR_OK;
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Read(TCAN_HANDLE Handle, CAN_MSG *Buf){
    struct can_frame frame;

    int nbytes = read(Handle, &frame, sizeof(struct can_frame));

    if (nbytes != sizeof (struct can_frame)){
        return CAN_ERR_ERR;
    }

    Buf->Size = frame.len;
    Buf->Id = frame.can_id;
    if ((frame.can_id & CAN_EFF_FLAG) != 0)
        Buf->Flags = CAN_FLAGS_EXTENDED;
    for (int i = 0; i < frame.len; ++i) {
        Buf->Data[i] = frame.data[i];
    }
    return CAN_ERR_OK;
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Version(TCAN_HANDLE Handle,CHAR *buf){
    *buf = *"socketCAN";
    return CAN_ERR_OK;
}

// The return value is one of CAN_STATUS
TCAN_STATUS  CAN_Status(TCAN_HANDLE Handle){
    return CAN_ERR_OK;
}
