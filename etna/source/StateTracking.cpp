#include "StateTracking.hpp"
#include "etna/GlobalContext.hpp"

#include <bit>


namespace etna
{

void ResourceStates::setExternalTextureState(
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkImage>(image));
  currentStates[resHandle] = TextureState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .layout = layout,
    .owner = {},
  };
}

void ResourceStates::setExternalBufferState(
  vk::Buffer buffer, vk::PipelineStageFlags2 pipeline_stage_flag, vk::AccessFlags2 access_flags)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkBuffer>(buffer));
  currentStates[resHandle] = BufferState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .owner = {},
  };
}

void ResourceStates::setTextureState(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags,
  ForceSetState force)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkImage>(image));
  if (currentStates.count(resHandle) == 0)
  {
    currentStates[resHandle] = TextureState{.owner = com_buffer};
  }
  TextureState newState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .layout = layout,
    .owner = com_buffer,
  };
  auto& oldState = std::get<0>(currentStates[resHandle]);
  if (force == ForceSetState::eFalse && newState == oldState)
    return;
  imageBarriersToFlush.push_back(vk::ImageMemoryBarrier2{
    .srcStageMask = oldState.piplineStageFlags,
    .srcAccessMask = oldState.accessFlags,
    .dstStageMask = newState.piplineStageFlags,
    .dstAccessMask = newState.accessFlags,
    .oldLayout = oldState.layout,
    .newLayout = newState.layout,
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .image = image,
    .subresourceRange =
      {
        .aspectMask = aspect_flags,
        .baseMipLevel = 0,
        .levelCount = vk::RemainingMipLevels,
        .baseArrayLayer = 0,
        .layerCount = vk::RemainingArrayLayers,
      },
  });
  oldState = newState;
}

void ResourceStates::setBufferState(
  vk::CommandBuffer com_buffer,
  vk::Buffer buffer,
  vk::PipelineStageFlags2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::DeviceSize range,
  vk::DeviceSize offset,
  ForceSetState force)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkBuffer>(buffer));
  if (currentStates.count(resHandle) == 0)
  {
    currentStates[resHandle] = BufferState{.owner = com_buffer};
  }
  BufferState newState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .owner = com_buffer,
  };
  auto& oldState = std::get<1>(currentStates[resHandle]);
  if (force == ForceSetState::eFalse && newState == oldState)
    return;
  bufferBarriersToFlush.push_back(vk::BufferMemoryBarrier2{
    .srcStageMask = oldState.piplineStageFlags,
    .srcAccessMask = oldState.accessFlags,
    .dstStageMask = newState.piplineStageFlags,
    .dstAccessMask = newState.accessFlags,
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .buffer = buffer,
    .offset = offset,
    .size = range,
  });
  oldState = newState;
}

void ResourceStates::flushBarriers(vk::CommandBuffer com_buf)
{
  const bool noImageBarriers = imageBarriersToFlush.empty();
  const bool noBufferBarriers = bufferBarriersToFlush.empty();
  if (noImageBarriers && noBufferBarriers)
    return;

  vk::DependencyInfo depInfo{
    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriersToFlush.size()),
    .pBufferMemoryBarriers = noBufferBarriers ? nullptr : bufferBarriersToFlush.data(),
    .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriersToFlush.size()),
    .pImageMemoryBarriers = noImageBarriers ? nullptr : imageBarriersToFlush.data(),
  };
  com_buf.pipelineBarrier2(depInfo);

  imageBarriersToFlush.clear();
  bufferBarriersToFlush.clear();
}

void ResourceStates::setColorTarget(
  vk::CommandBuffer com_buffer, vk::Image image, BarrierBehavoir behavoir)
{
  if (get_context().shouldGenerateBarriersWhen(behavoir))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);
  }
}

void ResourceStates::setDepthStencilTarget(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::ImageAspectFlags aspect_flags,
  BarrierBehavoir behavoir)
{
  if (get_context().shouldGenerateBarriersWhen(behavoir))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests |
        vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::ImageLayout::eDepthStencilAttachmentOptimal,
      aspect_flags);
  }
}

void ResourceStates::setResolveTarget(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::ImageAspectFlags aspect_flags,
  BarrierBehavoir behavoir)
{
  if (get_context().shouldGenerateBarriersWhen(behavoir))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eResolve,
      vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eGeneral,
      aspect_flags);
  }
}

} // namespace etna
