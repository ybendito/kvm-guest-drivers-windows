#include "netkvmcx.h"
#ifdef NETKVM_WPP_ENABLED
#include "NetKvmCx_Driver.tmh"
#endif

EXTERN_C DRIVER_INITIALIZE DriverEntry;
EXTERN_C static EVT_WDF_DRIVER_DEVICE_ADD NetKVMCxEvtDeviceAdd;
EXTERN_C static EVT_WDF_OBJECT_CONTEXT_CLEANUP NetKVMCxEvtDriverContextCleanup;
static EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED __OnD0;
static EVT_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED __OnDx;
static EVT_WDF_DEVICE_PREPARE_HARDWARE __OnPrepareHardware;
static EVT_WDF_DEVICE_RELEASE_HARDWARE __OnReleaseHardware;
static EVT_NET_ADAPTER_CREATE_TXQUEUE __OnCreateTxQueue;
static EVT_NET_ADAPTER_CREATE_RXQUEUE __OnCreateRxQueue;
static EVT_WDF_OBJECT_CONTEXT_CLEANUP __OnDeviceCleanup;
static EVT_WDF_OBJECT_CONTEXT_CLEANUP __OnNetAdapterCleanup;

#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, NetKVMCxEvtDeviceAdd)
#pragma alloc_text (PAGE, NetKVMCxEvtDriverContextCleanup)

VOID __OnDeviceCleanup(_In_ WDFOBJECT Object)
{
    CNetKvmAdapter* a = GetNetxKvmAdapter(Object);
    TraceNoPrefix(0, "%s: adapter %p", __FUNCTION__, a);
    if (a) {
        a->Release();
    }
}

VOID __OnNetAdapterCleanup(_In_ WDFOBJECT Object)
{
    CNetKvmAdapter* a = GetNetxKvmAdapter(Object);
    TraceNoPrefix(0, "%s: adapter %p", __FUNCTION__, a);
    if (a) {
        a->Release();
    }
}

NTSTATUS NTAPI __OnCreateTxQueue(
    _In_ NETADAPTER Adapter,
    _Inout_ NETTXQUEUE_INIT* TxQueueInit
)
{
    CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
    return a->CreateTxQueue(TxQueueInit);
}

NTSTATUS NTAPI __OnCreateRxQueue(
    _In_ NETADAPTER Adapter,
    _Inout_ NETRXQUEUE_INIT* RxQueueInit
)
{
    CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
    return a->CreateRxQueue(RxQueueInit);
}

NTSTATUS __OnD0(_In_ WDFDEVICE Object, _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    CNetKvmAdapter *a = GetNetxKvmAdapter(Object);
    return a->OnD0(PreviousState);
}

NTSTATUS __OnDx(_In_ WDFDEVICE Object, _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    CNetKvmAdapter* a = GetNetxKvmAdapter(Object);
    return a->OnDx(TargetState);
}

NTSTATUS __OnPrepareHardware(
    _In_ WDFDEVICE Object,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesRaw);
    CNetKvmAdapter* a = GetNetxKvmAdapter(Object);
    return a->OnPrepareHardware(ResourcesTranslated);
}

NTSTATUS __OnReleaseHardware(_In_ WDFDEVICE Object, _In_ WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesTranslated);
    CNetKvmAdapter* a = GetNetxKvmAdapter(Object);
    return a->OnReleaseHardware();
}

NTSTATUS NetKVMCxEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDFDEVICE wdfDevice = NULL;
    NETADAPTER_INIT* adapterInit = NULL;
    NETADAPTER netAdapter = NULL;
    CNetKvmAdapter* adapter = NULL;

    UNREFERENCED_PARAMETER(Driver);

    WDF_OBJECT_ATTRIBUTES attributes;

    NTSTATUS status = NetDeviceInitConfig(DeviceInit);
    if (NT_SUCCESS(status)) {
        WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
        pnpPowerCallbacks.EvtDevicePrepareHardware = __OnPrepareHardware;
        pnpPowerCallbacks.EvtDeviceReleaseHardware = __OnReleaseHardware;
        pnpPowerCallbacks.EvtDeviceD0EntryPostInterruptsEnabled = __OnD0;
        pnpPowerCallbacks.EvtDeviceD0ExitPreInterruptsDisabled = __OnDx;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DevContext);
        attributes.EvtCleanupCallback = __OnDeviceCleanup;
        status = WdfDeviceCreate(&DeviceInit, &attributes, &wdfDevice);
    }

    if (NT_SUCCESS(status)) {
        adapterInit = NetAdapterInitAllocate(wdfDevice);
        if (!adapterInit) {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(status)) {
        NET_ADAPTER_DATAPATH_CALLBACKS datapathCallbacks;
        NET_ADAPTER_DATAPATH_CALLBACKS_INIT(
            &datapathCallbacks, __OnCreateTxQueue, __OnCreateRxQueue);
        NetAdapterInitSetDatapathCallbacks(adapterInit, &datapathCallbacks);
    }

    if (NT_SUCCESS(status)) {
        attributes.EvtCleanupCallback = __OnNetAdapterCleanup;
        status = NetAdapterCreate(adapterInit, &attributes, &netAdapter);
    }

    if (NT_SUCCESS(status)) {
        adapter = new CNetKvmAdapter(netAdapter, wdfDevice);
        if (adapter) {
            GetDeviceContext(wdfDevice)->adapter = adapter;
            GetDeviceContext(netAdapter)->adapter = adapter;
            adapter->AddRef();
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (adapterInit) {
        NetAdapterInitFree(adapterInit);
    }
    return status;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceNoPrefix(0, "%s %p =>", __FUNCTION__, DriverObject);

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = NetKVMCxEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, NetKVMCxEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        TraceNoPrefix(0, "WdfDriverCreate failed %X", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceNoPrefix(0, "%s <=", __FUNCTION__);

    return status;
}

VOID NetKVMCxEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    PAGED_CODE();

    TraceNoPrefix(0, "%s", __FUNCTION__);
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
