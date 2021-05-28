/*
 * Net CX adapter: Transmit queue implementation
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

#include "netkvmcx.h"
#ifdef NETKVM_WPP_ENABLED
#include "NetKvmCx_TX.tmh"
#endif

const CQueueExtension::ExtensionDef CQueueExtension::ChecksumDef = 
    { NET_PACKET_EXTENSION_CHECKSUM_NAME, NET_PACKET_EXTENSION_CHECKSUM_VERSION_1, NetExtensionTypePacket };
const CQueueExtension::ExtensionDef CQueueExtension::LsoDef =
    { NET_PACKET_EXTENSION_LSO_NAME, NET_PACKET_EXTENSION_LSO_VERSION_1, NetExtensionTypePacket };
const CQueueExtension::ExtensionDef CQueueExtension::VirtualAddressDef =
    { NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_NAME, NET_FRAGMENT_EXTENSION_VIRTUAL_ADDRESS_VERSION_1, NetExtensionTypeFragment };
const CQueueExtension::ExtensionDef CQueueExtension::LogicalAddressDef =
    { NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_NAME, NET_FRAGMENT_EXTENSION_LOGICAL_ADDRESS_VERSION_1, NetExtensionTypeFragment };

CNetKvmTxQueue::~CNetKvmTxQueue()
{
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
}

CNetKvmTxQueue::CNetKvmTxQueue(CNetKvmAdapter* Adapter, NETTXQUEUE_INIT* TxQueueInit) :
    CNetKvmVirtQueue(Adapter),
    m_ChecksumExtension(m_NetQueue),
    m_LsoExtension(m_NetQueue),
    m_VirtualAddressExtension(m_NetQueue),
    m_LogicalAddressExtension(m_NetQueue),
    m_Id(NetTxQueueInitGetQueueId(TxQueueInit))
{
    WDF_OBJECT_ATTRIBUTES a;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, TxQueueContext);

    a.EvtDestroyCallback = [](WDFOBJECT o) { GetTxQueue(o)->Destroy(); };

    NET_PACKET_QUEUE_CONFIG txConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &txConfig,
        [](NETPACKETQUEUE q) {
            GetTxQueue(q)->Advance();
        },
        [](NETPACKETQUEUE q, BOOLEAN en) {
            GetTxQueue(q)->EnableNotification(en);
        },
        [](NETPACKETQUEUE q) {
            UNREFERENCED_PARAMETER(q);
        });
    txConfig.EvtStart = [](NETPACKETQUEUE q) { GetTxQueue(q)->Start(); };
    txConfig.EvtStop = [](NETPACKETQUEUE q) { GetTxQueue(q)->Stop(); };

    NTSTATUS status = NetTxQueueCreate(TxQueueInit, &a, &txConfig, &m_NetQueue);
    if (NT_SUCCESS(status)) {
        _TxQueueContext(m_NetQueue)->tx = this;
    }
}

bool CNetKvmTxQueue::Prepare()
{
    bool done = false;
    UINT queueIndex;
    if (!m_Adapter->GetVirtQueueIndex(true, m_Id, queueIndex))
        return false;
    
    if (!m_NetQueue || !InitQueue(queueIndex)) {
        return false;
    }
    m_ChecksumExtension.Init();
    m_LsoExtension.Init();
    m_VirtualAddressExtension.Init();
    m_LogicalAddressExtension.Init();

    done = true;
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
    return done;
}

void CNetKvmTxQueue::Advance()
{
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
}

void CNetKvmTxQueue::Start()
{
    TraceNoPrefix(0, "%s", __FUNCTION__);
}

void CNetKvmTxQueue::Stop()
{
    TraceNoPrefix(0, "%s", __FUNCTION__);
}

void CNetKvmTxQueue::EnableNotification(bool Enable)
{
    TraceNoPrefix(0, "%s: %sable", __FUNCTION__, Enable ? "en" : "dis");
}

bool CNetKvmTxQueue::AllocateBlocks()
{
    return true;
}
