// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

namespace D3D12TranslationLayer
{

class BatchedResource : public BatchedDeviceChild
{
public:
    BatchedResource(BatchedContext& Context, Resource* underlyingResource, bool ownsResource)
        : BatchedDeviceChild(Context)
        , m_spResource(ownsResource ? underlyingResource : nullptr, { Context })
        , m_pResource(underlyingResource)
        // Resources constructed without ownership don't support Map(NO_OVERWRITE) until they've been discarded once.
        , m_LastRenamedResource(ownsResource ? m_pResource->GetIdentity()->m_suballocation : decltype(m_LastRenamedResource){})
    {
        // For simplicity we'll let the immediate resource be constructed before the batched, but it should've been constructed properly.
        assert(m_pResource->m_isValid);

        if (ownsResource &&
            m_pResource->AppDesc()->Usage() == RESOURCE_USAGE_DYNAMIC &&
            m_pResource->AppDesc()->BindFlags() == RESOURCE_BIND_CONSTANT_BUFFER &&
            m_pResource->NumSubresources() == 1 &&
            m_pResource->UnderlyingResourceIsSuballocated())
        {
            UINT alignedSize = m_pResource->GetSubresourcePlacement(0).Footprint.RowPitch;
            UINT width = m_pResource->AppDesc()->Width();
            if (width > 0 && width <= alignedSize && alignedSize <= 65536)
            {
                m_CBWidth = width;
                m_CBAlignedSize = alignedSize;
                m_spCBStagingMemory = std::make_unique<BYTE[]>(static_cast<size_t>(alignedSize) * 2);
                m_pCBWriteScratch = m_spCBStagingMemory.get();
                m_pCBLastCommitted = m_spCBStagingMemory.get() + alignedSize;
                if (BYTE* pInitCpu = m_LastRenamedResource.GetMappedCpuPtr())
                {
                    memcpy(m_pCBWriteScratch, pInitCpu, width);
                    memcpy(m_pCBLastCommitted, pInitCpu, width);
                }
            }
        }
    }

    struct BatchedDeleter
    {
        BatchedContext& Context;
        void operator()(Resource* p) { Context.ReleaseResource(p); }
    };
    std::unique_ptr<Resource, BatchedDeleter> const m_spResource;
    Resource* const m_pResource;
    // Used to implement Map(NO_OVERWRITE)
    D3D12ResourceSuballocation m_LastRenamedResource;

    // Fast dynamic constant buffer scratch & payload deduplication cache
    std::unique_ptr<BYTE[]> m_spCBStagingMemory;
    BYTE* m_pCBWriteScratch = nullptr;
    BYTE* m_pCBLastCommitted = nullptr;
    UINT m_CBWidth = 0;
    UINT m_CBAlignedSize = 0;
    bool m_bHasCommittedCBData = false;
    bool m_bFastCBMapActive = false;

    // Currently, the batched context only supports D3D11 dynamic resources, which are single-subresource and write-only.
    Resource::DynamicTexturePlaneData m_DynamicTexturePlaneData;
    SafeRenameResourceCookie m_PendingRenameViaCopyCookie;
};

}
