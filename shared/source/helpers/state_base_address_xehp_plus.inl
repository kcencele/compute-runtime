/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void setSbaStatelessCompressionParams(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (memoryCompressionState == MemoryCompressionState::Enabled) {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED);
    } else {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED);
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap *ssh,
    bool setGeneralStateBaseAddress,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable,
    MemoryCompressionState memoryCompressionState,
    bool overrideBindlessSurfaceStateBase,
    bool useGlobalAtomics,
    bool areMultipleSubDevicesInContext) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (setGeneralStateBaseAddress && is64bit) {
        stateBaseAddress->setGeneralStateBaseAddress(GmmHelper::decanonize(internalHeapBase));
    }

    if (overrideBindlessSurfaceStateBase && ssh) {
        stateBaseAddress->setBindlessSurfaceStateBaseAddress(ssh->getHeapGpuBase());
        stateBaseAddress->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        const auto surfaceStateCount = ssh->getMaxAvailableSpace() / sizeof(RENDER_SURFACE_STATE);
        stateBaseAddress->setBindlessSurfaceStateSize(static_cast<uint32_t>(surfaceStateCount - 1));
    }

    stateBaseAddress->setBindlessSamplerStateBaseAddressModifyEnable(true);

    auto l3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
    auto l1L3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC;

    if (DebugManager.flags.DisableCachingForHeaps.get()) {
        l3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
        l1L3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
        stateBaseAddress->setInstructionMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED));
    }

    stateBaseAddress->setIndirectObjectMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(l1L3CacheOnPolicy));
    stateBaseAddress->setSurfaceStateMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(l3CacheOnPolicy));
    stateBaseAddress->setDynamicStateMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(l3CacheOnPolicy));
    stateBaseAddress->setGeneralStateMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(l3CacheOnPolicy));
    stateBaseAddress->setBindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(l3CacheOnPolicy));

    bool enableMultiGpuAtomics = isMultiOsContextCapable;
    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics = useGlobalAtomics && (isMultiOsContextCapable || areMultipleSubDevicesInContext);
    }
    stateBaseAddress->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!enableMultiGpuAtomics);

    stateBaseAddress->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!isMultiOsContextCapable);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        stateBaseAddress->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        stateBaseAddress->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }

    if (memoryCompressionState != MemoryCompressionState::NotApplicable) {
        setSbaStatelessCompressionParams<GfxFamily>(stateBaseAddress, memoryCompressionState);
    }

    int32_t cachingPolicySetting = DebugManager.flags.UseCachingPolicyForIndirectObjectHeap.get();
    uint32_t indirectObjectHeapCachingPolicy = l1L3CacheOnPolicy;

    if (cachingPolicySetting != -1) {
        if (cachingPolicySetting == 0) {
            indirectObjectHeapCachingPolicy = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
        } else if (cachingPolicySetting == 1) {
            indirectObjectHeapCachingPolicy = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
        }
    }
    stateBaseAddress->setIndirectObjectMemoryObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(indirectObjectHeapCachingPolicy));

    if (stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState() == gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) && DebugManager.flags.ForceL1Caching.get() != 0) {
        stateBaseAddress->setStatelessDataPortAccessMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    }

    appendExtraCacheSettings(stateBaseAddress, gmmHelper);
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto bindingTablePoolAlloc = commandStream.getSpaceForCmd<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd = GfxFamily::cmdInitStateBindingTablePoolAlloc;
    cmd.setBindingTablePoolBaseAddress(ssh.getHeapGpuBase());
    cmd.setBindingTablePoolBufferSize(ssh.getHeapSizeInPages());
    cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    if (DebugManager.flags.DisableCachingForHeaps.get()) {
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED));
    }

    *bindingTablePoolAlloc = cmd;
}

} // namespace NEO
