#include <etna/SpecializationConstants.hpp>
#include <etna/Vulkan.hpp>
#include <fmt/color.h>


namespace etna
{

std::string to_string(ShaderModuleSpecializationConstant::Type type)
{
  using Type = ShaderModuleSpecializationConstant::Type;
  switch (type)
  {
  case Type::Bool:
    return "bool";
  case Type::Int8:
    return "int8";
  case Type::Uint8:
    return "uint8";
  case Type::Int16:
    return "int16";
  case Type::Uint16:
    return "uint16";
  case Type::Int32:
    return "int32";
  case Type::Uint32:
    return "uint32";
  case Type::Int64:
    return "int64";
  case Type::Uint64:
    return "uint64";
  case Type::Float16:
    return "float16";
  case Type::Float32:
    return "float32";
  case Type::Float64:
    return "float64";
  default:
    return "unknown";
  }
}

// https://stackoverflow.com/questions/76799117/how-to-convert-a-float-to-a-half-type-and-the-other-way-around-in-c
static uint16_t float2half_rn(float16_t a)
{
#if __STDCPP_FLOAT16_T__
  return std::bit_cast<uint16_t>(a);
#else
  uint32_t ia = std::bit_cast<uint32_t>(static_cast<float>(a));
  uint16_t ir = (ia >> 16) & 0x8000;

  if ((ia & 0x7f800000) == 0x7f800000)
  {
    if ((ia & 0x7fffffff) == 0x7f800000)
    {
      ir |= 0x7c00; /* infinity */
    }
    else
    {
      ir |= 0x7e00 | ((ia >> (24 - 11)) & 0x1ff); /* NaN, quietened */
    }
  }
  else if ((ia & 0x7f800000) >= 0x33000000)
  {
    int shift = (int)((ia >> 23) & 0xff) - 127;
    if (shift > 15)
    {
      ir |= 0x7c00; /* infinity */
    }
    else
    {
      ia = (ia & 0x007fffff) | 0x00800000; /* extract mantissa */
      if (shift < -14)
      { /* denormal */
        ir |= ia >> (-1 - shift);
        ia = ia << (32 - (-1 - shift));
      }
      else
      { /* normal */
        ir |= ia >> (24 - 11);
        ia = ia << (32 - (24 - 11));
        ir = ir + static_cast<uint16_t>((14 + shift) << 10);
      }
      /* IEEE-754 round to nearest of even */
      if ((ia > 0x80000000) || ((ia == 0x80000000) && ((ir & 1) == 1)))
      {
        ir++;
      }
    }
  }
  return ir;
#endif
}

ShaderProgramSpecConstsOverrider::ShaderProgramSpecConstsOverrider(
  size_t num_stages, size_t const_storage_capacity)
  : specInfos()
  , specConstStorage()
  , specMapEntries()
{
  specInfos.reserve(num_stages);
  specMapEntries.reserve(const_storage_capacity);
  // assume all consts are 64-bit
  specConstStorage.reserve(2 * const_storage_capacity);
}

void ShaderProgramSpecConstsOverrider::overrideSpecializationConstants(
  vk::PipelineShaderStageCreateInfo& shader_info,
  const ShaderModuleSpecializationConstants& available_constants,
  SpecializationConstantsView overrides)
{
  if (overrides.empty())
    return;

  std::string stageLog;
  auto logIt = std::back_inserter(stageLog);
  const auto warningLabel =
    fmt::format(fmt::emphasis::bold | fg(fmt::terminal_color::yellow), "warning");

  auto specMapEntriesStart = std::to_address(specMapEntries.end());
  for (const auto& [name, value] : overrides)
  {
    auto it = available_constants.find(name);
    if (it == available_constants.end())
    {
      fmt::format_to(
        logIt, "  [{}] Specialization constant {} not found, ignoring\n", warningLabel, name);
      continue;
    }

    auto overrideType = std::visit(
      [](auto&& arg) -> ShaderModuleSpecializationConstant::Type {
        using T = std::decay_t<decltype(arg)>;
        using Type = ShaderModuleSpecializationConstant::Type;

        if constexpr (std::is_same_v<T, bool>)
          return Type::Bool;
        else if constexpr (std::is_same_v<T, uint8_t>)
          return Type::Uint8;
        else if constexpr (std::is_same_v<T, int8_t>)
          return Type::Int8;
        else if constexpr (std::is_same_v<T, uint16_t>)
          return Type::Uint16;
        else if constexpr (std::is_same_v<T, int16_t>)
          return Type::Int16;
        else if constexpr (std::is_same_v<T, uint32_t>)
          return Type::Uint32;
        else if constexpr (std::is_same_v<T, int32_t>)
          return Type::Int32;
        else if constexpr (std::is_same_v<T, uint64_t>)
          return Type::Uint64;
        else if constexpr (std::is_same_v<T, int64_t>)
          return Type::Int64;
        else if constexpr (std::is_same_v<T, float16_t>)
          return Type::Float16;
        else if constexpr (std::is_same_v<T, float32_t>)
          return Type::Float32;
        else if constexpr (std::is_same_v<T, float64_t>)
          return Type::Float64;
        else
          // can't use static_assert due to support of old compilers
          ETNA_PANIC("Unsupported specialization constant type");
      },
      value);

    const auto& specConst = it->second;
    if (specConst.type != overrideType)
    {
      fmt::format_to(
        logIt,
        "  [{}] Specialization constant {} type mismatch (expected: {}, got: {}), "
        "ignoring\n",
        warningLabel,
        name,
        to_string(specConst.type),
        to_string(overrideType));
      continue;
    }

    const uint32_t offset = static_cast<uint32_t>(specConstStorage.size() * sizeof(SpvWordT));
    const size_t size = std::visit(
      [this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::disjunction_v<
                        std::is_same<T, uint64_t>,
                        std::is_same<T, int64_t>,
                        std::is_same<T, float64_t>>)
        {
          using SpvDoubleWordT = uint64_t;
          const auto bits = std::bit_cast<SpvDoubleWordT>(arg);
          specConstStorage.push_back(
            static_cast<SpvWordT>(bits & std::numeric_limits<SpvWordT>::max()));
          specConstStorage.push_back(
            static_cast<SpvWordT>((bits >> 32) & std::numeric_limits<SpvWordT>::max()));
          return sizeof(bits);
        }
        else if constexpr (std::is_same_v<T, float16_t>)
        {
          const auto bits = float2half_rn(arg);
          specConstStorage.push_back(bits);
          return sizeof(bits);
        }
        else if constexpr (std::is_same_v<T, float32_t>)
        {
          const auto bits = std::bit_cast<SpvWordT>(arg);
          specConstStorage.push_back(bits);
          return sizeof(bits);
        }
        else if constexpr (std::is_integral_v<T> && sizeof(T) <= sizeof(SpvWordT))
        {
          specConstStorage.push_back(static_cast<SpvWordT>(arg));
          return sizeof(std::conditional_t<std::is_same_v<T, bool>, SpvWordT, T>);
        }
        else
        {
          ETNA_PANIC("Unsupported specialization constant type");
          return 0;
        }
      },
      value);

    specMapEntries.push_back(vk::SpecializationMapEntry{specConst.id, offset, size});

    std::visit(
      [&logIt, name = name.c_str(), &specConst](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        using F = std::conditional_t<std::is_same_v<T, float16_t>, float32_t, T>;
        fmt::format_to(
          logIt,
          "  Overriding specialization constant {} (id: {}) with value {}\n",
          name,
          specConst.id,
          static_cast<F>(arg));
      },
      value);
  }

  if (!stageLog.empty())
  {
    fmt::format_to(
      std::back_inserter(log),
      " Specialization constants for shader stage {}:\n{}",
      vk::to_string(shader_info.stage),
      stageLog);
  }

  if (auto specMapEntriesEnd = std::to_address(specMapEntries.end());
      specMapEntriesStart != specMapEntriesEnd)
  {
    specInfos.push_back(
      vk::SpecializationInfo{}
        .setData(vk::ArrayProxyNoTemporaries<const uint32_t>(specConstStorage))
        .setMapEntries(vk::ArrayProxyNoTemporaries<const vk::SpecializationMapEntry>(
          static_cast<uint32_t>(specMapEntriesEnd - specMapEntriesStart), specMapEntriesStart)));
    shader_info.setPSpecializationInfo(std::addressof(specInfos.back()));
  }
}
} // namespace etna
