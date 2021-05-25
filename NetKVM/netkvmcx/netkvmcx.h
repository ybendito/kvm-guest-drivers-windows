/*
 * This file contains general definitions for NetKVM CX driver
 *
 * Copyright (c) 2021 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "stdafx.h"
EXTERN_C {
#include "../Common/Trace.h"
#include "../VirtIO/WDF/VirtIOWdf.h"
#include "../VirtIO/virtio_ring.h"
#include "../VirtIO/virtio_pci.h"
#include "../Common/virtio_net.h"
}
#include "../Common/ParaNdis_Reusable.h"

#define PARANDIS_MULTICAST_LIST_SIZE        32
#define PARANDIS_MAX_LSO_SIZE               0xF800
#define PARANDIS_MIN_LSO_SEGMENTS           2

class CAllocatable
{
public:
    static const ULONG MemoryTag = 'XmvK';

    void* operator new(size_t /* size */, void* ptr) throw()
    {
        return ptr;
    }

    void* operator new[](size_t /* size */, void* ptr) throw()
    { return ptr; }

        void* operator new(size_t Size) throw()
    {
        return ExAllocatePoolWithTag(NonPagedPoolNx, Size, MemoryTag);
    }

    void* operator new[](size_t Size) throw()
    { return ExAllocatePoolWithTag(NonPagedPoolNx, Size, MemoryTag); }

        void operator delete(void* ptr)
    {
        if (ptr) { ExFreePoolWithTag(ptr, MemoryTag); }
    }

    void operator delete[](void* ptr)
    { if (ptr) { ExFreePoolWithTag(ptr, MemoryTag); } }

protected:
    CAllocatable() {};
    ~CAllocatable() {};
};

class CRefCounter
{
public:
    CRefCounter() = default;

    LONG AddRef() { return InterlockedIncrement(&m_Counter); }
    LONG Release() { return InterlockedDecrement(&m_Counter); }
    operator LONG () { return m_Counter; }
private:
    LONG m_Counter = 0;

    CRefCounter(const CRefCounter&) = delete;
    CRefCounter& operator= (const CRefCounter&) = delete;
};

class CRefCountingObject
{
public:
    CRefCountingObject()
    {
        AddRef();
    }

    void AddRef()
    {
        m_RefCounter.AddRef();
    }

    void Release()
    {
        if (m_RefCounter.Release() == 0)
        {
            OnLastReferenceGone();
        }
    }

protected:
    virtual void OnLastReferenceGone() = 0;

private:
    CRefCounter m_RefCounter;

    CRefCountingObject(const CRefCountingObject&) = delete;
    CRefCountingObject& operator= (const CRefCountingObject&) = delete;
};


class CNetKvmAdapter : public CAllocatable, public CRefCountingObject
{
public: 
    CNetKvmAdapter(NETADAPTER NetAdapter, WDFDEVICE wdfDevice) :
        m_NetAdapter(NetAdapter),
        m_WdfDevice(wdfDevice)
    {

    }
    ~CNetKvmAdapter();
    NTSTATUS OnD0(WDF_POWER_DEVICE_STATE fromState);
    NTSTATUS OnDx(WDF_POWER_DEVICE_STATE toState);
    NTSTATUS OnPrepareHardware(WDFCMRESLIST ResourcesTranslated);
    NTSTATUS OnReleaseHardware();
    NTSTATUS CreateTxQueue(NETTXQUEUE_INIT* TxQueueInit);
    NTSTATUS CreateRxQueue(NETRXQUEUE_INIT* RxQueueInit);
private:
    NETADAPTER m_NetAdapter;
    WDFDEVICE  m_WdfDevice;
    VIRTIO_WDF_DRIVER m_VirtIO = {};
    ULONGLONG  m_HostFeatures = 0;
    ULONGLONG  m_GuestFeatures = 0;
    struct virtio_net_config m_DeviceConfig = {};
    NET_ADAPTER_LINK_LAYER_ADDRESS m_CurrentMacAddress = {};
    struct
    {
        ULONG fOverrideMac              : 1;
        ULONG fHasChecksum              : 1;
        ULONG fHasControlQueue          : 1;
        ULONG fHasMacConfig             : 1;
        ULONG fHasPromiscuous           : 1;
        ULONG fHasExtPacketFilters      : 1;
        ULONG fHasTSO4                  : 1;
        ULONG fHasTSO6                  : 1;
        ULONG fHasMQ                    : 1;
        ULONG fRSSAllowed               : 1;
        ULONG fHasRSS                   : 1;
        ULONG fHasHash                  : 1;
        ULONG fConnected                : 1;
    } m_Flags = {};
private:
    NTSTATUS Initialize();
    void ReadConfiguration();
    void SetupGuestFeatures();
    void SetCapabilities();
    bool AckFeature(ULONG Feature)
    {
        return ::AckFeature(m_HostFeatures, m_GuestFeatures, Feature);
    }
    void OnLastReferenceGone() override
    {
        delete this;
    }
    void SetPacketFilter(NET_PACKET_FILTER_FLAGS PacketFilter);
    void SetMulticastList(ULONG MulticastAddressCount, NET_ADAPTER_LINK_LAYER_ADDRESS* MulticastAddressList);
    void SetChecksumOffload(NETOFFLOAD Offload);
    void SetLsoOffload(NETOFFLOAD Offload);
    void ReportLinkState(bool Unknown = false);
};

typedef struct _DevContext
{
    CNetKvmAdapter * adapter;
} DevContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DevContext, GetDeviceContext)

static FORCEINLINE CNetKvmAdapter* GetNetxKvmAdapter(WDFOBJECT o)
{
    DevContext* dc = GetDeviceContext(o);
    return dc->adapter;
}
