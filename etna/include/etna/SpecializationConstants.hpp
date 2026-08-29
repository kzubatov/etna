#pragma once
#ifndef ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
#define ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED

#include <etna/Vulkan.hpp>

#include <vulkan/vulkan_structs.hpp>

#include <cstdint>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>


namespace etna
{

using SpecializationConstant = std::pair<const char*, std::variant<bool, int32_t, float>>;
using SpecializationConstants = std::initializer_list<SpecializationConstant>;

struct ShaderModuleSpecializationConstant
{
  enum class Type : uint8_t
  {
    Bool,
    // spv doesn't distinguish between signed and unsigned integers, so we don't either
    Int,
    Float,
  };

  uint32_t id;
  Type type;
};

std::string to_string(ShaderModuleSpecializationConstant::Type type);

using ShaderModuleSpecializationConstants =
  std::unordered_map<std::string, ShaderModuleSpecializationConstant>;

class ShaderProgramSpecializationConstants
{
public:
  explicit ShaderProgramSpecializationConstants(size_t num_stages, size_t const_storage_capacity);

  ShaderProgramSpecializationConstants(const ShaderProgramSpecializationConstants&) = delete;
  ShaderProgramSpecializationConstants& operator=(const ShaderProgramSpecializationConstants&) =
    delete;
  ShaderProgramSpecializationConstants(ShaderProgramSpecializationConstants&&) = delete;
  ShaderProgramSpecializationConstants& operator=(ShaderProgramSpecializationConstants&&) = delete;

  void overrideSpecializationConstants(
    vk::PipelineShaderStageCreateInfo& shader_info,
    const ShaderModuleSpecializationConstants& available_constants,
    const SpecializationConstants& overrides);

  std::string_view getLog() const { return log; }

private:
  std::string log;
  std::vector<vk::SpecializationInfo> specInfos;
  // 64-bit constants are not supported, therefore each constant size is 4 bytes
  std::vector<uint32_t> specConstStorage;
  std::vector<vk::SpecializationMapEntry> specMapEntries;
};

} // namespace etna

#endif // ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
