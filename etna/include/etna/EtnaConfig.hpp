#pragma once
#ifndef ETNA_ETNA_CONFIG_HPP_INCLUDED
#define ETNA_ETNA_CONFIG_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{

#if NDEBUG
inline constexpr std::array<const char*, 0> VALIDATION_LAYERS = {};
#else
inline constexpr std::array VALIDATION_LAYERS = {
  "VK_LAYER_KHRONOS_validation",
  "VK_LAYER_LUNARG_monitor",
};
#endif

inline constexpr uint32_t VULKAN_API_VERSION = VK_API_VERSION_1_3;

inline constexpr const char* ETNA_ENGINE_NAME = "etna";
inline constexpr uint32_t ETNA_VERSION = VK_MAKE_VERSION(0, 1, 0);

inline constexpr size_t MAX_FRAMES_INFLIGHT = 4;

} // namespace etna

#endif // ETNA_ETNA_CONFIG_HPP_INCLUDED
