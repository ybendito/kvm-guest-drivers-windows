/*
 * Net CX adapter implementation
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
#include "NetKvmCx_Adapter.tmh"
#endif

CNetKvmAdapter::~CNetKvmAdapter()
{
	TraceNoPrefix(0, "%s", __FUNCTION__);
}

NTSTATUS CNetKvmAdapter::OnD0(WDF_POWER_DEVICE_STATE fromState)
{
	NTSTATUS status = STATUS_SUCCESS;
	TraceNoPrefix(0, "%s: from D%d", __FUNCTION__, fromState - WdfPowerDeviceD0);
	return status;
}

NTSTATUS CNetKvmAdapter::OnDx(WDF_POWER_DEVICE_STATE toState)
{
	NTSTATUS status = STATUS_SUCCESS;
	TraceNoPrefix(0, "%s: to D%d", __FUNCTION__, toState - WdfPowerDeviceD0);
	return status;
}

NTSTATUS CNetKvmAdapter::OnReleaseHardware()
{
	VirtIOWdfShutdown(&m_VirtIO);
	return STATUS_SUCCESS;
}

NTSTATUS CNetKvmAdapter::OnPrepareHardware(WDFCMRESLIST ResourcesTranslated)
{
	NTSTATUS status = VirtIOWdfInitialize(
		&m_VirtIO, m_WdfDevice, ResourcesTranslated,
		NULL, MemoryTag);

	return status;
}

NTSTATUS CNetKvmAdapter::CreateTxQueue(NETTXQUEUE_INIT* TxQueueInit)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(TxQueueInit);
	return status;
}

NTSTATUS CNetKvmAdapter::CreateRxQueue(NETRXQUEUE_INIT* RxQueueInit)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(RxQueueInit);
	return status;
}
