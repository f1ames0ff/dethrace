# Vulkan-Headers (vendored)

Minimal subset of the Khronos Vulkan headers, vendored so that builds with
`-DDETHRACE_VULKAN=ON` do not require the Vulkan SDK (including CI):

- `vulkan/vulkan.h`
- `vulkan/vulkan_core.h`
- `vulkan/vk_platform.h`
- `vk_video/*` (included unconditionally by `vulkan_core.h`)

Taken from Vulkan SDK 1.4.321.1 (`VK_HEADER_VERSION 321`). Upstream:
https://github.com/KhronosGroup/Vulkan-Headers

License: Apache-2.0 OR MIT. See the SPDX headers in the files and
https://github.com/KhronosGroup/Vulkan-Headers/blob/main/LICENSE.md
