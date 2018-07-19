/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

   TcbRcb.H

Abstract:

    This module declares the TCB and RCB structures, and the functions to
    manipulate them.

    See the comments in TcbRcb.c.

--*/


#ifndef _TCBRCB_H
#define _TCBRCB_H



//
// TCB (Transmit Control Block)
// -----------------------------------------------------------------------------
//

typedef struct _TCB
{
    LIST_ENTRY              TcbLink;
    PNET_BUFFER             NetBuffer;
    ULONG                   FrameType;
    ULONG                   BytesActuallySent;
} TCB, *PTCB;



VOID
ReturnTCB(
    PMP_ADAPTER  Adapter,
    PTCB         Tcb);


//
// RCB (Receive Control Block)
// -----------------------------------------------------------------------------
//

typedef struct _RCB
{
    LIST_ENTRY              RcbLink;
    PNET_BUFFER_LIST        Nbl;
    PVOID                   Data;
#if (NDIS_SUPPORT_NDIS620)    
    PVOID                   LookaheadData;
#endif
} RCB, *PRCB;

PRCB
GetRCB(
    PMP_ADAPTER  Adapter,
    PNDIS_NET_BUFFER_LIST_8021Q_INFO Nbl1QInfo,
    PFRAME       Frame);

VOID
ReturnRCB(
    PMP_ADAPTER   Adapter,
    PRCB          Rcb);



#endif // _TCBRCB_H

