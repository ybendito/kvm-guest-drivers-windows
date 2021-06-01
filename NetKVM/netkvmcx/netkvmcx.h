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
#define NETKVMCX_MAX_INTERRUPTS             64

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

class CNetKvmAdapter;
class CNetKvmVirtQueue
{
public:
    CNetKvmVirtQueue(CNetKvmAdapter* Adapter) : m_Adapter(Adapter) { }
    ~CNetKvmVirtQueue();
    void Delete();
protected:
    CNetKvmAdapter* m_Adapter;
    virtqueue* m_VQ = NULL;
    ULONG           m_Size = 0;
    USHORT          m_Index;
protected:
    bool InitQueue(UINT vqIndex);
    virtual bool AllocateBlocks() = 0;
};

class CNetKvmControlQueue : public CNetKvmVirtQueue
{
public:
    CNetKvmControlQueue(CNetKvmAdapter* Adapter) : CNetKvmVirtQueue(Adapter) { }
    ~CNetKvmControlQueue();
    bool Prepare(ULONG index)
    {
        return InitQueue(index);
    }
protected:
    bool AllocateBlocks() override;
private:

};

class CNetKvmAdapter : public CAllocatable, public CRefCountingObject
{
    friend class CNetKvmVirtQueue;
public:
    CNetKvmAdapter(NETADAPTER NetAdapter, WDFDEVICE wdfDevice);
    ~CNetKvmAdapter();
    NTSTATUS OnD0(WDF_POWER_DEVICE_STATE fromState);
    NTSTATUS OnDx(WDF_POWER_DEVICE_STATE toState);
    NTSTATUS OnPrepareHardware(WDFCMRESLIST ResourcesTranslated);
    NTSTATUS OnReleaseHardware();
    NTSTATUS CreateTxQueue(NETTXQUEUE_INIT* TxQueueInit);
    NTSTATUS CreateRxQueue(NETRXQUEUE_INIT* RxQueueInit);
    ULONG GetExpectedQueueSize(UINT index);
    bool GetVirtQueueIndex(bool Tx, UINT id, UINT& index)
    {
        UINT ctrlIndex = m_DeviceConfig.max_virtqueue_pairs * 2;
        index = id * 2 + !!Tx;
        return index < ctrlIndex;
    }
private:
    NETADAPTER m_NetAdapter;
    WDFDEVICE  m_WdfDevice;
    WDFINTERRUPT m_Interrupts[NETKVMCX_MAX_INTERRUPTS] = {};
    VIRTIO_WDF_DRIVER m_VirtIO = {};
    CNetKvmControlQueue m_ControlQueue;
    ULONGLONG  m_HostFeatures = 0;
    ULONGLONG  m_GuestFeatures = 0;
    struct virtio_net_config m_DeviceConfig = {};
    NET_ADAPTER_LINK_LAYER_ADDRESS m_CurrentMacAddress = {};
    UINT m_VirtioHeaderSize;
    UINT m_NumActiveQueues;
    struct
    {
        ULONG fOverrideMac              : 1;
        ULONG fHasChecksum              : 1;
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
    bool SetupGuestFeatures();
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
    void SetRscOffload(NETOFFLOAD Offload);
    void ReportLinkState(bool Unknown = false);
    BOOLEAN  OnInterruptIsr(ULONG MessageId);
    void     OnInterruptDpc(ULONG MessageId);
    BOOLEAN  CheckInterrupts();
    void EnableInterrupt(ULONG MessageId);
    void DisableInterrupt(ULONG MessageId);
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

class CQueueExtension
{
public:
    void Init()
    {
        NET_EXTENSION_QUERY extq;
        NET_EXTENSION_QUERY_INIT(&extq, m_Def.Name, m_Def.Version, m_Def.Type);
        Get(m_NetQueue, extq);
    }
protected:
    virtual void Get(NETPACKETQUEUE, NET_EXTENSION_QUERY&) = 0;
    struct ExtensionDef
    {
        LPCWSTR Name;
        ULONG   Version;
        NET_EXTENSION_TYPE Type;
    };
    CQueueExtension(NETPACKETQUEUE& NetQueue, const ExtensionDef& ExtDef) :
        m_NetQueue(NetQueue), m_Def(ExtDef) { }
    NET_EXTENSION   m_Extension;
    NETPACKETQUEUE& m_NetQueue;
    const ExtensionDef& m_Def;
protected:
    // all known extensions
    static const ExtensionDef ChecksumDef;
    static const ExtensionDef LsoDef;
    static const ExtensionDef VirtualAddressDef;
    static const ExtensionDef LogicalAddressDef;
};

class CTxQueueExtension : public CQueueExtension
{
public:
protected:
    CTxQueueExtension(NETPACKETQUEUE& NetQueue, const ExtensionDef& ExtDef) :
        CQueueExtension(NetQueue, ExtDef) { }
    void Get(NETPACKETQUEUE NetQueue, NET_EXTENSION_QUERY& Query) override
    {
        NetTxQueueGetExtension(NetQueue, &Query, &m_Extension);
    }
};

class CRxQueueExtension : public CQueueExtension
{
public:
protected:
    CRxQueueExtension(NETPACKETQUEUE& NetQueue, const ExtensionDef& ExtDef) :
        CQueueExtension(NetQueue, ExtDef) { }
    void Get(NETPACKETQUEUE NetQueue, NET_EXTENSION_QUERY& Query) override
    {
        NetRxQueueGetExtension(NetQueue, &Query, &m_Extension);
    }
};

class CTxChecksumExtension : public CTxQueueExtension
{
public:
    CTxChecksumExtension(NETPACKETQUEUE& NetQueue) : CTxQueueExtension(NetQueue, ChecksumDef) {}
};

class CRxChecksumExtension : public CRxQueueExtension
{
public:
    CRxChecksumExtension(NETPACKETQUEUE& NetQueue) : CRxQueueExtension(NetQueue, ChecksumDef) {}
};

class CTxLsoExtension : public CTxQueueExtension
{
public:
    CTxLsoExtension(NETPACKETQUEUE& NetQueue) : CTxQueueExtension(NetQueue, LsoDef) {}
};

class CTxVirtualAddressExtension : public CTxQueueExtension
{
public:
    CTxVirtualAddressExtension(NETPACKETQUEUE& NetQueue) : CTxQueueExtension(NetQueue, VirtualAddressDef) {}
};

class CTxLogicalAddressExtension : public CTxQueueExtension
{
public:
    CTxLogicalAddressExtension(NETPACKETQUEUE& NetQueue) : CTxQueueExtension(NetQueue, LogicalAddressDef) {}
};

class CRxLogicalAddressExtension : public CRxQueueExtension
{
public:
    CRxLogicalAddressExtension(NETPACKETQUEUE& NetQueue) : CRxQueueExtension(NetQueue, LogicalAddressDef) {}
};

class CNetKvmTxQueue : public CAllocatable, public CNetKvmVirtQueue
{
public:
    CNetKvmTxQueue(CNetKvmAdapter* Adapter, NETTXQUEUE_INIT* TxQueueInit);
    ~CNetKvmTxQueue();
    bool Prepare();
    void Advance();
    void Start();
    void Stop();
    void EnableNotification(bool Enable);
    void Destroy()
    {
        // TODO: shutdown the queue?
        delete this;
    }
private:
    NETPACKETQUEUE  m_NetQueue = NULL;
    CTxChecksumExtension       m_ChecksumExtension;
    CTxLsoExtension            m_LsoExtension;
    CTxVirtualAddressExtension m_VirtualAddressExtension;
    CTxLogicalAddressExtension m_LogicalAddressExtension;
    UINT m_Id;
private:
    bool AllocateBlocks() override;
};

typedef struct _TxQueueContext
{
    CNetKvmTxQueue* tx;
} TxQueueContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TxQueueContext, _TxQueueContext)

static FORCEINLINE CNetKvmTxQueue* GetTxQueue(WDFOBJECT o)
{
    TxQueueContext* qc = _TxQueueContext(o);
    return qc->tx;
}

class CNetKvmRxQueue : public CAllocatable, public CNetKvmVirtQueue
{
public:
    CNetKvmRxQueue(CNetKvmAdapter* Adapter, NETRXQUEUE_INIT* RxQueueInit);
    ~CNetKvmRxQueue();
    bool Prepare();
    void Advance();
    void Start();
    void Stop();
    void EnableNotification(bool Enable);
    void Destroy()
    {
        // TODO: shutdown the queue?
        delete this;
    }
private:
    NETPACKETQUEUE  m_NetQueue = NULL;
    CRxChecksumExtension       m_ChecksumExtension;
    CRxLogicalAddressExtension m_LogicalAddressExtension;
    UINT m_Id;
private:
    bool AllocateBlocks() override;
};

typedef struct _RxQueueContext
{
    CNetKvmRxQueue* rx;
} RxQueueContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(RxQueueContext, _RxQueueContext)

static FORCEINLINE CNetKvmRxQueue* GetRxQueue(WDFOBJECT o)
{
    RxQueueContext* qc = _RxQueueContext(o);
    return qc->rx;
}

typedef struct _InterruptContext
{
    CNetKvmAdapter* adapter;
    ULONG           messageId;
} InterruptContext;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(InterruptContext, GetInterruptContext)
