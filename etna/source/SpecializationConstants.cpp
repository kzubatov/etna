#include <etna/SpecializationConstants.hpp>

namespace etna
{

ShaderProgramSpecializationConstants::ShaderProgramSpecializationConstants(
  size_t num_stages, size_t const_storage_capacity)
  : specInfos()
  , specConstStorage()
  , specMapEntries()
{
  specInfos.reserve(num_stages);
  specMapEntries.reserve(const_storage_capacity);
  specConstStorage.reserve(const_storage_capacity);
}

void ShaderProgramSpecializationConstants::overrideSpecializationConstants(
  vk::PipelineShaderStageCreateInfo& shader_info,
  const ShaderModuleSpecializationConstants& available_constants,
  const SpecializationConstants& overrides)
{
  if (overrides.size() == 0)
    return;

  std::string stageLog;
  auto logIt = std::back_inserter(stageLog);

  auto specMapEntriesStart = specMapEntries.end();
  auto specConstStorageStart = specConstStorage.end();
  for (const auto& [name, value] : overrides)
  {
    auto it = available_constants.find(name);
    if (it == available_constants.end())
    {
      fmt::format_to(logIt, "    [Warning] Specialization constant {} not found, ignoring\n", name);
      continue;
    }

    const auto& specConst = it->second;
    ShaderModuleSpecializationConstant::Type overrideType = std::visit(
      [](auto&& arg) -> ShaderModuleSpecializationConstant::Type {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>)
          return ShaderModuleSpecializationConstant::Type::Bool;
        else if constexpr (std::is_same_v<T, int32_t>)
          return ShaderModuleSpecializationConstant::Type::Int;
        else
          return ShaderModuleSpecializationConstant::Type::Float;
      },
      value);

    if (specConst.type != overrideType)
    {
      fmt::format_to(
        logIt,
        "    [Warning] Specialization constant {} type mismatch (expected: {}, got: {}), "
        "ignoring\n",
        name,
        to_string(specConst.type),
        to_string(overrideType));
      continue;
    }

    const uint32_t bits = std::visit(
      [](auto&& arg) -> uint32_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>)
          return static_cast<uint32_t>(arg);
        else if constexpr (std::is_same_v<T, int32_t>)
          return static_cast<uint32_t>(arg);
        else
          return std::bit_cast<uint32_t>(arg);
      },
      value);

    const uint32_t offset = static_cast<uint32_t>(specConstStorage.size() * sizeof(uint32_t));
    specConstStorage.push_back(bits);
    specMapEntries.push_back(vk::SpecializationMapEntry{specConst.id, offset, sizeof(uint32_t)});

    fmt::format_to(
      logIt,
      "    Overriding specialization constant {} (id: {}) with value {}\n",
      name,
      specConst.id,
      std::visit([](auto&& arg) -> std::string { return std::to_string(arg); }, value));
  }

  if (!stageLog.empty())
  {
    auto it = std::back_inserter(log);
    fmt::format_to(
      it,
      "  Specialization constants for shader stage {}:\n{}",
      vk::to_string(shader_info.stage),
      stageLog);
  }

  if (specMapEntriesStart != specMapEntries.end())
  {
    vk::SpecializationInfo specInfo{};
    specInfo.setPData(specConstStorageStart.base());
    specInfo.setDataSize(
      std::distance(specConstStorageStart, specConstStorage.end()) * sizeof(uint32_t));
    specInfo.setPMapEntries(specMapEntriesStart.base());
    specInfo.setMapEntryCount(
      static_cast<uint32_t>(std::distance(specMapEntriesStart, specMapEntries.end())));
    specInfos.push_back(std::move(specInfo));
    shader_info.setPSpecializationInfo(&specInfos.back());
  }
}

std::string to_string(ShaderModuleSpecializationConstant::Type type)
{
  switch (type)
  {
  case ShaderModuleSpecializationConstant::Type::Bool:
    return "bool";
  case ShaderModuleSpecializationConstant::Type::Int:
    return "int";
  case ShaderModuleSpecializationConstant::Type::Float:
    return "float";
  default:
    return "unknown";
  }
}

} // namespace etna
