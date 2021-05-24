/*
 * Debug/trace support for NetKVM CX driver
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
#include "NetKvmCx_Debug.tmh"
#endif

int virtioDebugLevel = 0;
int bDebugPrint = 1;

#ifndef NETKVM_WPP_ENABLED

static void NetKVMDebugPrint(const char* fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    PrintProcedure(DPFLTR_DEFAULT_ID, 9 | DPFLTR_MASK, fmt, list);
#if defined(VIRTIO_DBG_USE_IOPORT)
    {
        NTSTATUS status;
        // use this way of output only for DISPATCH_LEVEL,
        // higher requires more protection
        if (KeGetCurrentIrql() <= DISPATCH_LEVEL)
        {
            char buf[256];
            size_t len, i;
            buf[0] = 0;
            status = RtlStringCbVPrintfA(buf, sizeof(buf), fmt, list);
            if (status == STATUS_SUCCESS) len = strlen(buf);
            else if (status == STATUS_BUFFER_OVERFLOW) len = sizeof(buf);
            else { memcpy(buf, "Can't print", 11); len = 11; }
            NdisAcquireSpinLock(&CrashLock);
            for (i = 0; i < len; ++i)
            {
                NdisRawWritePortUchar(VIRTIO_DBG_USE_IOPORT, buf[i]);
            }
            NdisRawWritePortUchar(VIRTIO_DBG_USE_IOPORT, '\n');
            NdisReleaseSpinLock(&CrashLock);
        }
    }
#endif
    va_end(list);
}

#else

// TODO: unify with netkvm
static void NetKVMDebugPrint(const char* fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    char buf[256];
    buf[0] = 0;
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, list);
    TraceNoPrefix(0, "%s", buf);
}

tDebugPrintFunc VirtioDebugPrintProc = NetKVMDebugPrint;

#endif
