/*
 * Net CX adapter: virtqueue wrapper
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
#include "NetKvmCx_VirtQueue.tmh"
#endif

CNetKvmVirtQueue::~CNetKvmVirtQueue()
{
    if (m_VQ) {
        virtio_delete_queue(m_VQ);
        m_VQ = NULL;
    }
}

bool CNetKvmVirtQueue::InitQueue(UINT vqIndex)
{
    NTSTATUS status;
    m_Index = (USHORT)vqIndex;
    status = virtio_find_queue(&m_Adapter->m_VirtIO.VIODevice, vqIndex, &m_VQ);
    if (!NT_SUCCESS(status)) {
        return false;
    }
    m_Size = virtio_get_queue_size(m_VQ);
    if (!AllocateBlocks()) {
        return false;
    }
    virtio_set_queue_vector(m_VQ, m_Index);
    return true;
}

