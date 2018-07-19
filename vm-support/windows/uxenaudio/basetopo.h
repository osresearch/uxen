
/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    basetopo.h

Abstract:

    Declaration of topology miniport.

--*/

#ifndef _UXENAUDIO_BASETOPO_H_
#define _UXENAUDIO_BASETOPO_H_

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportTopologyUXenAudio
//

class CMiniportTopologyUXenAudio
{
  protected:
    PADAPTERCOMMON              m_AdapterCommon;    // Adapter common object.
    PPCFILTER_DESCRIPTOR        m_FilterDescriptor; // Filter descriptor.

  public:
    CMiniportTopologyUXenAudio();
    ~CMiniportTopologyUXenAudio();

    NTSTATUS                    GetDescription
    (   
        OUT PPCFILTER_DESCRIPTOR *  Description
    );

    NTSTATUS                    DataRangeIntersection
    (   
        IN  ULONG               PinId,
        IN  PKSDATARANGE        ClientDataRange,
        IN  PKSDATARANGE        MyDataRange,
        IN  ULONG               OutputBufferLength,
        OUT PVOID               ResultantFormat OPTIONAL,
        OUT PULONG              ResultantFormatLength
    );

    NTSTATUS                    Init
    ( 
        IN  PUNKNOWN            UnknownAdapter,
        IN  PPORTTOPOLOGY       Port_ 
    );

    // PropertyHandlers.
    NTSTATUS                    PropertyHandlerBasicSupportVolume
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );
    
    NTSTATUS                    PropertyHandlerCpuResources
    ( 
        IN  PPCPROPERTY_REQUEST PropertyRequest 
    );

    NTSTATUS                    PropertyHandlerGeneric
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerMute
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerMuxSource
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerVolume
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerDevSpecific
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );
};

#endif

