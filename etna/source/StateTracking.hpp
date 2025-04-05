#pragma once
#ifndef ETNA_STATETRACKING_HPP_INCLUDED
#define ETNA_STATETRACKING_HPP_INCLUDED

#include "etna/Vulkan.hpp"
#include "etna/BarrierBehavoir.hpp"

#include <variant>
#include <unordered_map>

namespace etna
{

class ResourceStates
{
  using HandleType = uint64_t;
  struct TextureState
  {
    vk::PipelineStageFlags2 piplineStageFlags = {};
    vk::AccessFlags2 accessFlags = {};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::CommandBuffer owner = {};
    bool operator==(const TextureState& other) const = default;
  };
  struct BufferState
  {
    vk::PipelineStageFlags2 piplineStageFlags = {};
    vk::AccessFlags2 accessFlags = {};
    vk::CommandBuffer owner = {};
    bool operator==(const BufferState& other) const = default;
  };
  using State = std::variant<TextureState, BufferState>;
  std::unordered_map<HandleType, State> currentStates;
  std::vector<vk::ImageMemoryBarrier2> imageBarriersToFlush;
  std::vector<vk::BufferMemoryBarrier2> bufferBarriersToFlush;

public:
  void setExternalTextureState(
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout);

  void setExternalBufferState(
    vk::Buffer buffer, vk::PipelineStageFlags2 pipeline_stage_flag, vk::AccessFlags2 access_flags);

  void setTextureState(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout,
    vk::ImageAspectFlags aspect_flags,
    ForceSetState force = ForceSetState::eFalse);

  void setBufferState(
    vk::CommandBuffer com_buffer,
    vk::Buffer buffer,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::DeviceSize range = vk::WholeSize,
    vk::DeviceSize offset = 0,
    ForceSetState force = ForceSetState::eFalse);

  void setColorTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);
  void setDepthStencilTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);
  void setResolveTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);

  void flushBarriers(vk::CommandBuffer com_buf);
};

} // namespace etna

#endif // ETNA_STATETRACKING_HPP_INCLUDED
