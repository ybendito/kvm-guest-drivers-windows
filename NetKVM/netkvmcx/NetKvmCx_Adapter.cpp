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
	ReportLinkState();
	return status;
}

NTSTATUS CNetKvmAdapter::OnDx(WDF_POWER_DEVICE_STATE toState)
{
	NTSTATUS status = STATUS_SUCCESS;
	TraceNoPrefix(0, "%s: to D%d", __FUNCTION__, toState - WdfPowerDeviceD0);
	ReportLinkState(true);
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
	if (NT_SUCCESS(status)) {
		status = Initialize();
	}
	if (!NT_SUCCESS(status)) {
		VirtIOWdfShutdown(&m_VirtIO);
	}

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

void CNetKvmAdapter::ReadConfiguration()
{
	NETCONFIGURATION cfg = NULL;
	NTSTATUS status = NetAdapterOpenConfiguration(m_NetAdapter, WDF_NO_OBJECT_ATTRIBUTES, &cfg);
	if (!NT_SUCCESS(status)) {
		return;
	}
	status = NetConfigurationQueryLinkLayerAddress(cfg, &m_CurrentMacAddress);
	// BUGBUG
	m_Flags.fOverrideMac = NT_SUCCESS(status);
	// TODO: fRSSAllowed
	// TODO: Speed
	// TODO: MTU

	NetConfigurationClose(cfg);
}

void CNetKvmAdapter::SetCapabilities()
{
	NET_PACKET_FILTER_FLAGS all_filters = {};
	if (m_Flags.fHasPromiscuous) {
		all_filters |= NetPacketFilterFlagPromiscuous;
		all_filters |= NetPacketFilterFlagAllMulticast;
	}
	if (m_Flags.fHasExtPacketFilters) {
		all_filters |= NetPacketFilterFlagDirected;
		all_filters |= NetPacketFilterFlagMulticast;
		all_filters |= NetPacketFilterFlagBroadcast;
	}

	union {
		NET_ADAPTER_LINK_LAYER_CAPABILITIES linkLayerCapabilities;
		NET_ADAPTER_LINK_LAYER_ADDRESS permanentMac;
		NET_ADAPTER_PACKET_FILTER_CAPABILITIES packetFilterCapabilities;
		NET_ADAPTER_MULTICAST_CAPABILITIES multicastCapabilities;
		NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES checksumOffloadCapabilities;
		NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES lsoOffloadCapabilities;
	} u;

	if (!m_DeviceConfig.speed || m_DeviceConfig.speed > INT_MAX) {
		//BUGBUG
		m_DeviceConfig.speed = 10000;
		m_DeviceConfig.duplex = 1;
	}

	NET_ADAPTER_LINK_LAYER_CAPABILITIES_INIT(
		&u.linkLayerCapabilities,
		(ULONG64)1000000 * m_DeviceConfig.speed,
		(ULONG64)1000000 * m_DeviceConfig.speed);
	NetAdapterSetLinkLayerCapabilities(m_NetAdapter, &u.linkLayerCapabilities);

	ETH_COPY_NETWORK_ADDRESS(u.permanentMac.Address, m_DeviceConfig.mac);
	u.permanentMac.Length = sizeof(m_DeviceConfig.mac);
	NetAdapterSetPermanentLinkLayerAddress(m_NetAdapter, &u.permanentMac);
	NetAdapterSetCurrentLinkLayerAddress(m_NetAdapter, &m_CurrentMacAddress);

	NET_ADAPTER_PACKET_FILTER_CAPABILITIES_INIT(
		&u.packetFilterCapabilities, all_filters,
		[](NETADAPTER Adapter, NET_PACKET_FILTER_FLAGS PacketFilter)
		{
			CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
			a->SetPacketFilter(PacketFilter);
		});
	NetAdapterSetPacketFilterCapabilities(m_NetAdapter, &u.packetFilterCapabilities);

	m_DeviceConfig.mtu = 1500;
	NetAdapterSetLinkLayerMtuSize(m_NetAdapter, m_DeviceConfig.mtu);

	NET_ADAPTER_MULTICAST_CAPABILITIES_INIT(
		&u.multicastCapabilities,
		PARANDIS_MULTICAST_LIST_SIZE,
		[](NETADAPTER Adapter, ULONG MulticastAddressCount, NET_ADAPTER_LINK_LAYER_ADDRESS* MulticastAddressList)
		{
			CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
			a->SetMulticastList(MulticastAddressCount, MulticastAddressList);
		});
	NetAdapterSetMulticastCapabilities(m_NetAdapter, &u.multicastCapabilities);

	//TODO: RSS capabilities
	//TODO: Power capabilities, if needed

	NET_ADAPTER_OFFLOAD_CHECKSUM_CAPABILITIES_INIT(
		&u.checksumOffloadCapabilities,
		FALSE,
		m_Flags.fHasChecksum,
		m_Flags.fHasChecksum,
		[](NETADAPTER Adapter, NETOFFLOAD Offload)
		{
			CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
			a->SetChecksumOffload(Offload);
		});
	NetAdapterOffloadSetChecksumCapabilities(m_NetAdapter, &u.checksumOffloadCapabilities);

	NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES_INIT(
		&u.lsoOffloadCapabilities,
		m_Flags.fHasTSO4,
		m_Flags.fHasTSO6,
		(m_Flags.fHasTSO4 || m_Flags.fHasTSO6) ? PARANDIS_MAX_LSO_SIZE : 0,
		(m_Flags.fHasTSO4 || m_Flags.fHasTSO6) ? PARANDIS_MIN_LSO_SEGMENTS : 0,
		[](NETADAPTER Adapter, NETOFFLOAD Offload)
		{
			CNetKvmAdapter* a = GetNetxKvmAdapter(Adapter);
			a->SetLsoOffload(Offload);
		});
	NetAdapterOffloadSetLsoCapabilities(m_NetAdapter, &u.lsoOffloadCapabilities);
}

void CNetKvmAdapter::SetupGuestFeatures()
{
	m_Flags.fHasControlQueue = AckFeature(VIRTIO_NET_F_CTRL_VQ);
	m_Flags.fHasChecksum = AckFeature(VIRTIO_NET_F_CSUM);
	m_Flags.fHasTSO4 = AckFeature(VIRTIO_NET_F_HOST_TSO4);
	m_Flags.fHasTSO6 = AckFeature(VIRTIO_NET_F_HOST_TSO6);
	if (m_Flags.fHasControlQueue) {
		m_Flags.fHasMacConfig = AckFeature(VIRTIO_NET_F_CTRL_MAC_ADDR);
		m_Flags.fHasPromiscuous = AckFeature(VIRTIO_NET_F_CTRL_RX);
		m_Flags.fHasExtPacketFilters = AckFeature(VIRTIO_NET_F_CTRL_RX_EXTRA);
	}

}

static bool CheckMandatoryFeatures(ULONGLONG Features)
{
	bool b = virtio_is_feature_enabled(Features, VIRTIO_F_VERSION_1);
	b = b && virtio_is_feature_enabled(Features, VIRTIO_RING_F_INDIRECT_DESC);
	b = b && virtio_is_feature_enabled(Features, VIRTIO_NET_F_STATUS);
	return b;
}

#define ALLOW_FIELD(f) (RTL_SIZEOF_THROUGH_FIELD(struct virtio_net_config, f))
#define VIRTIO_GET_FIELD(f) virtio_get_config(&m_VirtIO.VIODevice, \
            FIELD_OFFSET(struct virtio_net_config, f), \
            &m_DeviceConfig.##f, RTL_FIELD_SIZE(struct virtio_net_config, f));

NTSTATUS CNetKvmAdapter::Initialize()
{
	NTSTATUS status = STATUS_SUCCESS;
	LPCSTR errorMessage = "Unknown";
	ULONG allowed_config_len = ALLOW_FIELD(max_virtqueue_pairs);
	m_HostFeatures = VirtIOWdfGetDeviceFeatures(&m_VirtIO);
	ParaNdis_DumpHostFeatures(m_HostFeatures);

	if (!CheckMandatoryFeatures(m_HostFeatures)) {
		errorMessage = "CheckMandatoryFeatures";
		status = STATUS_NDIS_ADAPTER_NOT_READY;
		TraceNoPrefix(0, "%s: failed at %s, error %X", __FUNCTION__, errorMessage, status);
		return status;
	}

	if (virtio_is_feature_enabled(m_HostFeatures, VIRTIO_NET_F_MTU)) {
		allowed_config_len = ALLOW_FIELD(mtu);
	}
	if (virtio_is_feature_enabled(m_HostFeatures, VIRTIO_NET_F_RSS) ||
		virtio_is_feature_enabled(m_HostFeatures, VIRTIO_NET_F_HASH_REPORT)) {
			allowed_config_len = ALLOW_FIELD(supported_hash_types);
	}
	virtio_get_config(&m_VirtIO.VIODevice, 0, &m_DeviceConfig, allowed_config_len);

	ReadConfiguration();

	if (!m_Flags.fOverrideMac) {
		ETH_COPY_NETWORK_ADDRESS(m_CurrentMacAddress.Address, m_DeviceConfig.mac);
		m_CurrentMacAddress.Length = sizeof(m_DeviceConfig.mac);
	}
	SetupGuestFeatures();

	SetCapabilities();

	if (NT_SUCCESS(status)) {
		errorMessage = "NetAdapterStart";
		status = NetAdapterStart(m_NetAdapter);
	}
	if (!NT_SUCCESS(status)) {
		TraceNoPrefix(0, "%s: failed at %s, error %X", __FUNCTION__, errorMessage, status);
	}
	return status;
}

void CNetKvmAdapter::SetPacketFilter(NET_PACKET_FILTER_FLAGS PacketFilter)
{
	UNREFERENCED_PARAMETER(PacketFilter);
}

void CNetKvmAdapter::SetMulticastList(
	ULONG MulticastAddressCount,
	NET_ADAPTER_LINK_LAYER_ADDRESS* MulticastAddressList)
{
	struct
	{
		ULONG entries;
		hardware_address mc[PARANDIS_MULTICAST_LIST_SIZE];
	} msg;
	if (MulticastAddressCount > PARANDIS_MULTICAST_LIST_SIZE) {
		MulticastAddressCount = PARANDIS_MULTICAST_LIST_SIZE;
	}
	msg.entries = MulticastAddressCount;
	for (ULONG i = 0; i < msg.entries; ++i) {
		ETH_COPY_NETWORK_ADDRESS(msg.mc[i].address, MulticastAddressList[i].Address);
	}
	//BUGBUG: send the message
}

void CNetKvmAdapter::SetChecksumOffload(NETOFFLOAD Offload)
{
	UNREFERENCED_PARAMETER(Offload);
}

void CNetKvmAdapter::SetLsoOffload(NETOFFLOAD Offload)
{
	UNREFERENCED_PARAMETER(Offload);
}

void CNetKvmAdapter::ReportLinkState(bool Unknown)
{
	NDIS_MEDIA_CONNECT_STATE connectState = MediaConnectStateUnknown;
	ULONG64 speed = NDIS_LINK_SPEED_UNKNOWN;
	NET_ADAPTER_LINK_STATE linkState;
	if (Unknown) {
		NET_ADAPTER_LINK_STATE_INIT(
			&linkState, speed, connectState,
			MediaDuplexStateUnknown,
			NetAdapterPauseFunctionTypeUnknown,
			NetAdapterAutoNegotiationFlagNone);
	} else {
		VIRTIO_GET_FIELD(status);
		m_Flags.fConnected = m_DeviceConfig.status & VIRTIO_NET_S_LINK_UP;
		connectState = m_Flags.fConnected ? MediaConnectStateConnected : MediaConnectStateDisconnected;
		if (m_Flags.fConnected) {
			speed = (ULONG64)1000000 * m_DeviceConfig.speed;
		}
		NET_ADAPTER_LINK_STATE_INIT(
			&linkState, speed, connectState,
			MediaDuplexStateFull,
			NetAdapterPauseFunctionTypeUnsupported,
			NetAdapterAutoNegotiationFlagXmitLinkSpeedAutoNegotiated |
				NetAdapterAutoNegotiationFlagRcvLinkSpeedautoNegotiated |
				NetAdapterAutoNegotiationFlagDuplexAutoNegotiated
		);
	}
	NetAdapterSetLinkState(m_NetAdapter, &linkState);
}
