#pragma once

#include <memory>

namespace vk {
class VertexInputAttributeDescription;
class VertexInputBindingDescription;
namespace raii {
class Pipeline;
class PipelineLayout;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class DescriptorLayout;
class Device;
class Shader;
class Swapchain;
class VertexDescriptor;

class Pipeline {
public:
	Pipeline(
		const Device &device,
		const DescriptorLayout &descriptor_layout,
		const Shader &shader,
		const Swapchain &swapchain,
		const VertexDescriptor &vertex_descriptor
	);
	~Pipeline();
	Pipeline(const Pipeline &) = delete;
	Pipeline &operator=(const Pipeline &) = delete;
	Pipeline(Pipeline &&);
	Pipeline &operator=(Pipeline &&);

	const vk::raii::Pipeline &get_pipeline() const;
	const vk::raii::PipelineLayout &get_layout() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
