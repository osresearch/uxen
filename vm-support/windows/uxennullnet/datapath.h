/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

   DataPath.H

Abstract:

    This module declares the TCB and RCB structures, and the functions to
    manipulate them.

    See the comments in TcbRcb.c.

--*/


#ifndef _DATAPATH_H
#define _DATAPATH_H




NDIS_STATUS
TXNblReference(
    PMP_ADAPTER       Adapter,
    PNET_BUFFER_LIST  NetBufferList);

VOID
TXNblRelease(
    PMP_ADAPTER       Adapter,
    PNET_BUFFER_LIST  NetBufferList,
    BOOLEAN           fAtDispatch);

NDIS_TIMER_FUNCTION TXSendCompleteDpc;

KDEFERRED_ROUTINE RXReceiveIndicateDpc;

VOID
RXDeliverFrameToEveryAdapter(
    PMP_ADAPTER  SendAdapter,
    PNDIS_NET_BUFFER_LIST_8021Q_INFO Nbl1QInfo,
    PFRAME       Frame,
    BOOLEAN      fAtDispatch);

VOID
RXFlushReceiveQueue(
    PMP_ADAPTER Adapter,
    PMP_ADAPTER_RECEIVE_DPC AdapterDpc);

VOID
TXFlushSendQueue(
    PMP_ADAPTER  Adapter,
    NDIS_STATUS  CompleteStatus);

VOID
NICStartTheDatapath(
    PMP_ADAPTER  Adapter);

VOID
NICStopTheDatapath(
    PMP_ADAPTER  Adapter);

ULONG
NICGetFrameTypeFromDestination(
    PUCHAR  DestAddress);

#endif // _DATAPATH_H

