#pragma once

#include "tramogi/core/pointers.h"
#include <cstdint>
#include <string_view>
#include <vector>

namespace vk {
class PipelineShaderStageCreateInfo;
namespace raii {
class ShaderModule;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class Device;

class Shader {
public:
	Shader(const Device &device, const std::vector<uint32_t> &bytes);
	~Shader();
	Shader(const Shader &) = delete;
	Shader &operator=(const Shader &) = delete;
	Shader(Shader &&);
	Shader &operator=(Shader &&);

	void add_vertex_stage(std::string_view func_name);
	void add_fragment_stage(std::string_view func_name);

	const std::vector<vk::PipelineShaderStageCreateInfo> &get_stages() const;

	const vk::raii::ShaderModule &get_shader_module() const;

private:
	struct Impl;
	core::UniquePtr<Impl> impl;
};

} // namespace tramogi::graphics
