#pragma once

#include "tramogi/core/pointers.h"
#include <initializer_list>

namespace vk {
class VertexInputAttributeDescription;
class VertexInputBindingDescription;
namespace raii {
class Pipeline;
class PipelineLayout;
} // namespace raii
} // namespace vk

namespace tramogi::graphics {

class AttachmentLayout;
class DepthImage;
class DescriptorLayout;
class Device;
enum class Format;
class Shader;
class Swapchain;
class VertexDescriptor;

struct PipelineOption {
	bool is_depth_test;
	bool is_depth_write;
};

class Pipeline {
public:
	Pipeline(
		const Device &device,
		const Shader &shader,
		std::initializer_list<DescriptorLayout *> descriptor_layouts,
		const VertexDescriptor &vertex_descriptor,
		const AttachmentLayout &attachment_layout,
		PipelineOption pipeline_option
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
	core::UniquePtr<Impl> impl;
};

} // namespace tramogi::graphics
