// Generic on-demand cache used by the resource manager for shared engine assets.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

template <typename AssetT>
class AssetCache
{
public:
    std::shared_ptr<AssetT> Find(const std::string& key) const
    {
        const auto found = m_Assets.find(key);
        return found != m_Assets.end() ? found->second : nullptr;
    }

    template <typename LoaderT>
    std::shared_ptr<AssetT> GetOrCreate(const std::string& key, LoaderT&& loader)
    {
        // Assets are keyed by a stable string so repeated requests reuse GPU objects.
        if (const std::shared_ptr<AssetT> cached = Find(key))
        {
            return cached;
        }

        std::shared_ptr<AssetT> asset = loader();
        Store(key, asset);
        return asset;
    }

    void Store(const std::string& key, std::shared_ptr<AssetT> asset)
    {
        m_Assets.insert_or_assign(key, std::move(asset));
    }

    void Clear()
    {
        m_Assets.clear();
    }

private:
    std::unordered_map<std::string, std::shared_ptr<AssetT>> m_Assets;
};
