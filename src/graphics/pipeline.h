#pragma once

#include "tramogi/core/pointers.h"
#include "tramogi/core/types.h"
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

class Pipeline {
public:
	struct Option {
		enum class DepthTest {
			None,
			DepthTestOnly,
			DepthTestAndWrite,
		};
		enum class DepthCompare {
			Less,
			LessOrEqual,
		};
		enum class PolygonMode {
			Fill,
			Wireframe,
		};
		enum class CullMode {
			None,
			Back,
			Front,
			Both,
		};
		struct DepthBias {
			float bias = 0;
			float slope = 0;
		};

		DepthTest depth_test = DepthTest::None;
		DepthCompare depth_compare = DepthCompare::Less;
		core::Optional<DepthBias> depth_bias = core::optional::none;
		CullMode cull_mode = CullMode::Back;
		PolygonMode polygon_mode = PolygonMode::Fill;
	};
	Pipeline(
		const Device &device,
		const Shader &shader,
		std::initializer_list<DescriptorLayout *> descriptor_layouts,
		const VertexDescriptor &vertex_descriptor,
		const AttachmentLayout &attachment_layout,
		Option pipeline_option
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
