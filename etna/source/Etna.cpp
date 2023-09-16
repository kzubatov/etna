#include <memory>

#include "StateTracking.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <vulkan/vulkan_format_traits.hpp>


namespace etna
{
  static std::unique_ptr<GlobalContext> g_context {};

  GlobalContext &get_context()
  {
    return *g_context;
  }
  
  bool is_initilized()
  {
    return static_cast<bool>(g_context);
  }
  
  void initialize(const InitParams &params)
  {
    g_context.reset(new GlobalContext(params));
  }
  
  void shutdown()
  {
    g_context->getDescriptorSetLayouts().clear(g_context->getDevice());
    g_context.reset(nullptr);
  }

  ShaderProgramId create_program(const std::string &name, const std::vector<std::string> &shaders_path)
  {
    return g_context->getShaderManager().loadProgram(name, shaders_path);
  }

  void reload_shaders()
  {
    g_context->getDescriptorSetLayouts().clear(g_context->getDevice());
    g_context->getShaderManager().reloadPrograms();
    g_context->getPipelineManager().recreate();
    g_context->getDescriptorPool().destroyAllocatedSets();
  }

  ShaderProgramInfo get_shader_program(ShaderProgramId id)
  {
    return g_context->getShaderManager().getProgramInfo(id);
  }
  
  ShaderProgramInfo get_shader_program(const std::string &name)
  {
    return g_context->getShaderManager().getProgramInfo(name);
  }

  DescriptorSet create_descriptor_set(DescriptorLayoutId layout, vk::CommandBuffer command_buffer, std::vector<Binding> bindings)
  {
    auto set = g_context->getDescriptorPool().allocateSet(layout, bindings, command_buffer);
    write_set(set);
    return set;
  }

  Image create_image_from_bytes(Image::CreateInfo info, vk::CommandBuffer command_buffer, const void *data,
    vk::AccessFlags2 final_image_access_flags, vk::ImageLayout final_image_layout)
  {
    const auto block_size = vk::blockSize(info.format);
    const auto image_size = block_size * info.extent.width * info.extent.height * info.extent.depth;
    etna::Buffer staging_buf = g_context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = image_size, 
      .bufferUsage = vk::BufferUsageFlagBits::eTransferSrc,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
      .name = "tmp_staging_buf"
    });

    auto *mapped_mem = staging_buf.map();
    memcpy(mapped_mem, data, image_size);
    staging_buf.unmap();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &beginInfo);

    info.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    if (info.mipLevels > 1) {
      info.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;  
    }

    auto image = g_context->createImage(info);
    etna::set_state(command_buffer, image.get(), vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal,
      image.getAspectMaskByFormat(), 0, info.mipLevels);
    etna::flush_barriers(command_buffer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = (VkImageAspectFlags)image.getAspectMaskByFormat();
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = static_cast<uint32_t>(info.layers);

    region.imageOffset = {0, 0, 0};
    region.imageExtent = info.extent;

    vkCmdCopyBufferToImage(
        command_buffer,
        staging_buf.get(),
        image.get(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    int32_t mipWidth = info.extent.width;
    int32_t mipHeight = info.extent.height;

    for (uint32_t i = 1; i < info.mipLevels; ++i) {
      etna::set_state(command_buffer, image.get(), vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead, vk::ImageLayout::eTransferSrcOptimal,
        image.getAspectMaskByFormat(), i - 1, 1);
      
      etna::flush_barriers(command_buffer);

      VkImageBlit blit{};
      blit.srcOffsets[0] = {0, 0, 0};
      blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
      blit.srcSubresource.aspectMask = (VkImageAspectFlags)image.getAspectMaskByFormat();
      blit.srcSubresource.mipLevel = i - 1;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = 1;
      blit.dstOffsets[0] = {0, 0, 0};
      blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1, mipHeight > 1 ? mipHeight >> 1 : 1, 1 };
      blit.dstSubresource.aspectMask = (VkImageAspectFlags)image.getAspectMaskByFormat();
      blit.dstSubresource.mipLevel = i;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = 1;

      vkCmdBlitImage(command_buffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

      etna::set_state(command_buffer, image.get(), vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal,
        image.getAspectMaskByFormat(), i - 1, 1);
      etna::flush_barriers(command_buffer);
      
      if (mipWidth > 1) mipWidth >>= 1;
      if (mipHeight > 1) mipHeight >>= 1;
    }

    etna::set_state(command_buffer, image.get(), vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal,
      image.getAspectMaskByFormat(), info.mipLevels - 1, 1);
    etna::flush_barriers(command_buffer);

    etna::set_state(command_buffer, image.get(), vk::PipelineStageFlagBits2::eAllCommands,
      final_image_access_flags, final_image_layout,
      image.getAspectMaskByFormat(), 0, info.mipLevels);
    etna::flush_barriers(command_buffer);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = (VkCommandBuffer *)&command_buffer;

    vkQueueSubmit(g_context->getQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_context->getQueue());

    staging_buf.reset();

    return image;
  }

  /*Todo: submit logic here*/
  void submit()
  {
    g_context->getDescriptorPool().flip();
  }

  void set_state(vk::CommandBuffer com_buffer, vk::Image image,
    vk::PipelineStageFlagBits2 pipeline_stage_flag, vk::AccessFlags2 access_flags,
    vk::ImageLayout layout, vk::ImageAspectFlags aspect_flags, 
    uint32_t baseMipLevel, uint32_t levelCount)
  {
    etna::get_context().getResourceTracker().setTextureState(com_buffer, image,
      pipeline_stage_flag, access_flags, layout, aspect_flags, baseMipLevel, levelCount);
  }

  void finish_frame(vk::CommandBuffer com_buffer)
  {
    etna::get_context().getResourceTracker().flushBarriers(com_buffer);
  }

  void flush_barriers(vk::CommandBuffer com_buffer)
  {
    etna::get_context().getResourceTracker().flushBarriers(com_buffer);
  }
}
