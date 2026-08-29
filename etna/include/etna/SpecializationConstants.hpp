#pragma once
#ifndef ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
#define ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED

#include <cstdint>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>

#if __cplusplus >= 202302L
#include <stdfloat>

using float16_t = std::float16_t; // NOLINT
using float32_t = std::float32_t; // NOLINT
using float64_t = std::float64_t; // NOLINT
#else
class float16_t // NOLINT
{
public:
  constexpr explicit float16_t(float v)
    : value{v}
  {
  }
  constexpr float16_t(const float16_t&) = default;
  constexpr float16_t(float16_t&&) = default;
  constexpr float16_t& operator=(const float16_t&) = default;
  constexpr float16_t& operator=(float16_t&&) = default;
  constexpr operator float() const { return value; } // NOLINT
  constexpr auto operator<=>(const float16_t&) const = default;

private:
  float value;
};
using float32_t = float;  // NOLINT
using float64_t = double; // NOLINT
#endif

namespace vk
{
struct SpecializationInfo;
struct SpecializationMapEntry;
struct PipelineShaderStageCreateInfo;
} // namespace vk

namespace etna
{

using SpecializationConstant = std::variant<
  bool,
  int8_t,
  uint8_t,
  int16_t,
  uint16_t,
  int32_t,
  uint32_t,
  int64_t,
  uint64_t,
  float16_t,
  float32_t,
  float64_t>;
using SpecializationConstants = std::unordered_map<std::string, SpecializationConstant>;
using SpecializationConstantsView = std::add_lvalue_reference_t<const SpecializationConstants>;

struct ShaderModuleSpecializationConstant
{
  enum class Type : uint8_t
  {
    Bool,

    Uint8,
    Int8,
    Uint16,
    Int16,
    Uint32,
    Int32,
    Uint64,
    Int64,

    Float16,
    Float32,
    Float64,
  };

  uint32_t id;
  Type type;
};

using ShaderModuleSpecializationConstants =
  std::unordered_map<std::string, ShaderModuleSpecializationConstant>;

class ShaderProgramSpecConstsOverrider
{
public:
  explicit ShaderProgramSpecConstsOverrider(size_t num_stages, size_t const_storage_capacity);

  ShaderProgramSpecConstsOverrider(const ShaderProgramSpecConstsOverrider&) = delete;
  ShaderProgramSpecConstsOverrider& operator=(const ShaderProgramSpecConstsOverrider&) = delete;
  ShaderProgramSpecConstsOverrider(ShaderProgramSpecConstsOverrider&&) = default;
  ShaderProgramSpecConstsOverrider& operator=(ShaderProgramSpecConstsOverrider&&) = default;

  void overrideSpecializationConstants(
    vk::PipelineShaderStageCreateInfo& shader_info,
    const ShaderModuleSpecializationConstants& available_constants,
    SpecializationConstantsView overrides);

  std::string_view getLog() const { return log; }

private:
  std::string log;
  std::vector<vk::SpecializationInfo> specInfos;

  // Spv specs: word is 32 bits.
  // https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#_instructions Spv specs: Spec
  // const size is word or multiple words.
  // https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpSpecConstant
  using SpvWordT = uint32_t;
  std::vector<SpvWordT> specConstStorage;
  std::vector<vk::SpecializationMapEntry> specMapEntries;
};

} // namespace etna

#endif // ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
