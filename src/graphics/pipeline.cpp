#include "pipeline.h"
#include "descriptor.h"
#include "device.h"
#include "physical_device.h"
#include "shader.h"
#include "swapchain.h"
#include "vertex_descriptor.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct Pipeline::Impl {
	vk::raii::PipelineLayout layout = nullptr;
	vk::raii::Pipeline pipeline = nullptr;
};

Pipeline::Pipeline(
	const Device &device,
	const DescriptorLayout &descriptor_layout,
	const Shader &shader,
	const Swapchain &swapchain,
	const VertexDescriptor &vertex_descriptor
)
	: impl(std::make_unique<Impl>()) {
	vk::PipelineLayoutCreateInfo pipeline_layout_info {
		.setLayoutCount = 1,
		.pSetLayouts = &*descriptor_layout.get_layout(),
		.pushConstantRangeCount = 0,
	};
	impl->layout = vk::raii::PipelineLayout(device.get_device(), pipeline_layout_info);

	std::array<vk::DynamicState, 2> dynamic_states {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	vk::PipelineDynamicStateCreateInfo dynamic_state_create_info {
		.dynamicStateCount = dynamic_states.size(),
		.pDynamicStates = dynamic_states.data(),
	};

	vk::VertexInputBindingDescription binding_description =
		vertex_descriptor.get_binding_description();
	std::vector<vk::VertexInputAttributeDescription> attribute_descriptions =
		vertex_descriptor.get_attribute_description();
	vk::PipelineVertexInputStateCreateInfo vertex_input_info {
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding_description,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
		.pVertexAttributeDescriptions = attribute_descriptions.data(),
	};
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_info {
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	vk::PipelineViewportStateCreateInfo viewport_state_info {
		.viewportCount = 1,
		.scissorCount = 1,
	};
	vk::PipelineRasterizationStateCreateInfo rasterization_state_info {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.depthBiasSlopeFactor = 1,
		.lineWidth = 1,
	};
	vk::PipelineMultisampleStateCreateInfo multisample_info {
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False,
	};
	vk::PipelineColorBlendAttachmentState color_blend_attachment {
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
						  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};
	vk::PipelineDepthStencilStateCreateInfo depth_stencil_info {
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
	};
	vk::PipelineColorBlendStateCreateInfo color_blending {
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
	};

	auto depth_format = device.get_physical_device().get_depth_format();
	if (!depth_format) {
		throw std::runtime_error(depth_format.error());
	}

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo>
		pipeline_info {
			vk::GraphicsPipelineCreateInfo {
				.stageCount = static_cast<uint32_t>(shader.get_stages().size()),
				.pStages = shader.get_stages().data(),
				.pVertexInputState = &vertex_input_info,
				.pInputAssemblyState = &input_assembly_info,
				.pViewportState = &viewport_state_info,
				.pRasterizationState = &rasterization_state_info,
				.pMultisampleState = &multisample_info,
				.pDepthStencilState = &depth_stencil_info,
				.pColorBlendState = &color_blending,
				.pDynamicState = &dynamic_state_create_info,
				.layout = impl->layout,
				.renderPass = nullptr,
			},
			{
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &swapchain.get_format(),
				.depthAttachmentFormat = depth_format.value(),
			}
		};

	impl->pipeline = vk::raii::Pipeline(device.get_device(), nullptr, pipeline_info.get());
}
Pipeline::~Pipeline() = default;
Pipeline::Pipeline(Pipeline &&) = default;
Pipeline &Pipeline::operator=(Pipeline &&) = default;

const vk::raii::Pipeline &Pipeline::get_pipeline() const {
	return impl->pipeline;
}

const vk::raii::PipelineLayout &Pipeline::get_layout() const {
	return impl->layout;
}

} // namespace tramogi::graphics
