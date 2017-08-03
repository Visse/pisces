#pragma once

#include "Fwd.h"

#include <vector>
#include <memory>
#include <algorithm>

namespace Pisces
{
    template< typename ResourceType >
    class ResourceMap {
        using ResourcePtr = std::shared_ptr<ResourceType>;


    public:
        void add( ResourceName name, const ResourcePtr &resource ) {
            auto loc = std::upper_bound( mResources.begin(), mResources.end(), name );
            mResources.insert( loc, {name, resource} );
        }

        bool lookup( ResourceName name, ResourcePtr &resource )
        {
            auto loc = std::lower_bound( mResources.begin(), mResources.end(), name );
            if( loc != mResources.end() && loc->name == name ) {
                resource = loc->resource;
                return true;
            }
            return false;
        }


    private:
        struct Entry {
            ResourceName name;
            ResourcePtr resource;

            friend bool operator < ( const Entry &lhs, const Entry &rhs ) {
                return lhs.name < rhs.name;
            }
            friend bool operator < ( const Entry &lhs, ResourceName name ) {
                return lhs.name < name;
            }
            friend bool operator < ( ResourceName name, const Entry &rhs  ) {
                return name < rhs.name;
            }
        };
    private:
        std::vector<Entry> mResources;
    };
}
