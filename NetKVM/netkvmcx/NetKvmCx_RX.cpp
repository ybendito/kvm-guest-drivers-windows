/*
 * Net CX adapter: Receive queue implementation
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
#include "NetKvmCx_RX.tmh"
#endif

CNetKvmRxQueue::~CNetKvmRxQueue()
{
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
}

CNetKvmRxQueue::CNetKvmRxQueue(CNetKvmAdapter* Adapter, NETRXQUEUE_INIT* RxQueueInit) :
    CNetKvmVirtQueue(Adapter),
    m_ChecksumExtension(m_NetQueue),
    m_LogicalAddressExtension(m_NetQueue),
    m_Id(NetRxQueueInitGetQueueId(RxQueueInit))
{
    WDF_OBJECT_ATTRIBUTES a;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a, RxQueueContext);

    a.EvtDestroyCallback = [](WDFOBJECT o) { GetRxQueue(o)->Destroy(); };

    NET_PACKET_QUEUE_CONFIG rxConfig;
    NET_PACKET_QUEUE_CONFIG_INIT(
        &rxConfig,
        [](NETPACKETQUEUE q) {
            GetRxQueue(q)->Advance();
        },
        [](NETPACKETQUEUE q, BOOLEAN en) {
            GetRxQueue(q)->EnableNotification(en);
        },
            [](NETPACKETQUEUE q) {
            UNREFERENCED_PARAMETER(q);
        });
    rxConfig.EvtStart = [](NETPACKETQUEUE q) { GetRxQueue(q)->Start(); };
    rxConfig.EvtStop = [](NETPACKETQUEUE q) { GetRxQueue(q)->Stop(); };

    NTSTATUS status = NetRxQueueCreate(RxQueueInit, &a, &rxConfig, &m_NetQueue);
    if (NT_SUCCESS(status)) {
        _RxQueueContext(m_NetQueue)->rx = this;
    }
}

bool CNetKvmRxQueue::Prepare()
{
    bool done = false;
    UINT queueIndex;
    if (!m_Adapter->GetVirtQueueIndex(false, m_Id, queueIndex))
        return false;

    if (!m_NetQueue || !InitQueue(queueIndex)) {
        return false;
    }
    m_ChecksumExtension.Init();
    m_LogicalAddressExtension.Init();

    done = true;
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
    return done;
}

void CNetKvmRxQueue::Advance()
{
    TraceNoPrefix(0, "%s #%d", __FUNCTION__, m_Id);
}

void CNetKvmRxQueue::Start()
{
    TraceNoPrefix(0, "%s", __FUNCTION__);
}

void CNetKvmRxQueue::Stop()
{
    TraceNoPrefix(0, "%s", __FUNCTION__);
}

void CNetKvmRxQueue::EnableNotification(bool Enable)
{
    TraceNoPrefix(0, "%s: %sable", __FUNCTION__, Enable ? "en" : "dis");
}

bool CNetKvmRxQueue::AllocateBlocks()
{
    return true;
}
