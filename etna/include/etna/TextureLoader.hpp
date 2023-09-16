#pragma once
#ifndef ETNA_TEXTURE_LOADER_HPP_INCLUDED
#define ETNA_TEXTURE_LOADER_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <vk_mem_alloc.h>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

namespace etna 
{

struct Texture {
  Image image;
  Sampler imageSampler;
  Image::ViewParams viewParams;
  auto genBinding() { return image.genBinding(imageSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, viewParams); }
};

struct TextureLoader 
{
  struct TextureInfo 
  {
    std::string_view name;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    std::size_t layers = 1;
    std::size_t mipLevels = 1;
    vk::Filter filter = vk::Filter::eNearest;
    vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eClampToEdge;
    float minLod = 0.0f;
  };

  TextureLoader(VkCommandBuffer a_cmdBuff);
  Texture load(TextureInfo info, const char *path);
  VkCommandBuffer *getCommandBufferPtr() { return &cmdBuff; }
private:
  VkCommandBuffer cmdBuff = VK_NULL_HANDLE;
};

};

#endif // ETNA_TEXTURE_LOADER_HPP_INCLUDED