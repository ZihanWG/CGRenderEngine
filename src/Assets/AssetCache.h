// Generic on-demand cache used by the resource manager for shared engine assets.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

template <typename AssetT>
class AssetCache
{
public:
    template <typename LoaderT>
    std::shared_ptr<AssetT> GetOrCreate(const std::string& key, LoaderT&& loader)
    {
        // Assets are keyed by a stable string so repeated requests reuse GPU objects.
        const auto found = m_Assets.find(key);
        if (found != m_Assets.end())
        {
            return found->second;
        }

        std::shared_ptr<AssetT> asset = loader();
        m_Assets.emplace(key, asset);
        return asset;
    }

    void Clear()
    {
        m_Assets.clear();
    }

private:
    std::unordered_map<std::string, std::shared_ptr<AssetT>> m_Assets;
};
