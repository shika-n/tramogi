#include "shader.h"
#include "device.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct Shader::Impl {
	vk::raii::ShaderModule module = nullptr;

	std::vector<vk::PipelineShaderStageCreateInfo> stages;
};

Shader::Shader(const Device &device, const std::vector<uint32_t> &spirv)
	: impl(std::make_unique<Impl>()) {
	vk::ShaderModuleCreateInfo shader_module_create_info {
		.codeSize = spirv.size() * sizeof(uint32_t),
		.pCode = spirv.data(),
	};
	impl->module = vk::raii::ShaderModule(device.get_device(), shader_module_create_info);
};
Shader::~Shader() = default;
Shader::Shader(Shader &&) = default;
Shader &Shader::operator=(Shader &&) = default;

void Shader::add_vertex_stage(std::string_view func_name) {
	impl->stages.push_back(
		vk::PipelineShaderStageCreateInfo {
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = impl->module,
			.pName = func_name.data(),
		}
	);
}

void Shader::add_fragment_stage(std::string_view func_name) {
	impl->stages.push_back(
		vk::PipelineShaderStageCreateInfo {
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = impl->module,
			.pName = func_name.data(),
		}
	);
}

const std::vector<vk::PipelineShaderStageCreateInfo> &Shader::get_stages() const {
	return impl->stages;
}

const vk::raii::ShaderModule &Shader::get_shader_module() const {
	return impl->module;
}

} // namespace tramogi::graphics
