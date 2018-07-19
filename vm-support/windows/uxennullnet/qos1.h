/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Qos.h

Abstract:

   This module declares the NDIS QoS related data types, flags, macros, and functions.

Revision History:

--*/

#pragma once

#if (NDIS_SUPPORT_NDIS630)

//
// The MP_ADAPTER_QOS_DATA structure is used to track the global QoS configuration for an adapter
//
typedef struct _MP_ADAPTER_QOS_DATA
{
    //
    // Tracks global NDIS QoS state
    //
#define fMP_QOS_ENABLED         0x00000001
    ULONG Flags;

    //
    // Current QoS capabilities
    //
    ULONG CurrentMaxNumTCs;
    ULONG CurrentMaxNumEtsCapableTCs;
    ULONG CurrentMaxNumPfcEnabledTCs;
} MP_ADAPTER_QOS_DATA, *PMP_ADAPTER_QOS_DATA;

#define QOS_ENABLED(_Adapter) \
    ((_Adapter)->QOSData.Flags & fMP_QOS_ENABLED)

NDIS_STATUS
ReadQOSConfig(
    NDIS_HANDLE ConfigurationHandle,
    struct _MP_ADAPTER *Adapter);

NDIS_STATUS
SetQOSParameters(
    struct _MP_ADAPTER *Adapter,
    PNDIS_QOS_PARAMETERS Params);

NDIS_STATUS
InitializeQOSConfig(
    struct _MP_ADAPTER *Adapter);

#else   // NDIS_SUPPORT_NDIS630

#define ReadQOSConfig(ConfigurationHandle, Adapter) NDIS_STATUS_SUCCESS
#define InitializeQOSConfig(Adapter) NDIS_STATUS_SUCCESS

#endif  // NDIS_SUPPORT_NDIS630
