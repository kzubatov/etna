#include <etna/TextureLoader.hpp>
// #ifndef STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// #endif
#include <vulkan/vulkan_format_traits.hpp>
#include <etna/Assert.hpp>
#include <etna/Etna.hpp>

namespace etna 
{
  TextureLoader::TextureLoader(VkCommandBuffer a_cmdBuff) : cmdBuff(a_cmdBuff) {}
  
  Texture TextureLoader::load(TextureInfo info, const char *path) 
  {
    int texW, texH, texC;
    auto pixels = stbi_load(path, &texW, &texH, &texC, vk::blockSize(info.format));
    
    ETNA_ASSERTF(pixels != nullptr && texW > 0 && texH > 0, "Failed to load {}", path);
    
    size_t mipLevelsMax = (size_t) floor(log2(std::max(texW, texH))) + 1u;
    
    Image::CreateInfo imageInfo = {
      .extent = { 
        .width = static_cast<uint32_t>(texW),
        .height = static_cast<uint32_t>(texH),
        .depth = 1U,
      },
      .name = info.name,
      .format = info.format,
      .imageUsage = info.imageUsage,
      .memoryUsage = info.memoryUsage,
      .tiling = info.tiling,
      .layers = info.layers,
      .mipLevels = std::max((size_t) 1, std::min(info.mipLevels, mipLevelsMax)),
    };

    Image result = create_image_from_bytes(imageInfo, cmdBuff, pixels,
      vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eShaderReadOnlyOptimal);

    stbi_image_free(pixels);

    Sampler sampler(etna::Sampler::CreateInfo {
      .filter = info.filter,
      .addressMode = info.addressMode,
      .name = info.name,
      .minLod = info.minLod,
      .maxLod = (float) imageInfo.mipLevels,
    });

    return {std::move(result), std::move(sampler), {(uint32_t) info.minLod, (uint32_t) imageInfo.mipLevels}};
  }
};