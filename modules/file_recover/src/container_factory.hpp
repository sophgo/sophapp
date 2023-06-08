#pragma once

#include "mpeg4.hpp"
#include <algorithm>
#include <memory>
#include <string>

namespace cvi_file_recover {

class ContainerFactory
{
public:
    static std::unique_ptr<Container> createContainer(const std::string &extension)
    {
        if ((extension == "mov") || (extension == "mp4")) {
            return std::make_unique<Mpeg4>();
        }

        return nullptr;
    }
};

} // namespace cvi_file_recover
