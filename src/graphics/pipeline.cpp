#include "pipeline.h"
#include "attachment_info.h"
#include "descriptor.h"
#include "device.h"
#include "format.h"
#include "image.h"
#include "shader.h"
#include "swapchain.h"
#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include "vertex_descriptor.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace tramogi::graphics {

using core::Optional;

struct Pipeline::Impl {
	vk::raii::PipelineLayout layout = nullptr;
	vk::raii::Pipeline pipeline = nullptr;
};

uint32_t native(bool value) {
	if (value) {
		return vk::True;
	} else {
		return vk::False;
	}
}

Pipeline::Pipeline(
	const Device &device,
	const Shader &shader,
	std::initializer_list<DescriptorLayout *> descriptor_layouts,
	const VertexDescriptor &vertex_descriptor,
	const AttachmentLayout &attachment_layout,
	PipelineOption pipeline_option
)
	: impl(std::make_unique<Impl>()) {
	assert(
		attachment_layout.get_depth_format() ||
		(!pipeline_option.is_depth_test && !pipeline_option.is_depth_write &&
		 "You need to specify depth format if either depth test or write is enabled")
	);

	std::span color_formats = attachment_layout.get_color_formats();

	std::vector<vk::DescriptorSetLayout> set_layouts;
	set_layouts.reserve(descriptor_layouts.size());
	for (auto descriptor_layout : descriptor_layouts) {
		set_layouts.push_back(descriptor_layout->get_layout());
	}

	vk::PipelineLayoutCreateInfo pipeline_layout_info {
		.setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
		.pSetLayouts = set_layouts.data(),
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
	std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments(
		color_formats.size(),
		{
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
							  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
		}
	);

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_info {
		.depthTestEnable = native(pipeline_option.is_depth_test),
		.depthWriteEnable = native(pipeline_option.is_depth_write),
		.depthCompareOp = vk::CompareOp::eLessOrEqual,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
	};

	vk::PipelineColorBlendStateCreateInfo color_blending {
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
		.pAttachments = color_blend_attachments.data(),
	};

	std::vector<vk::Format> native_formats;
	native_formats.reserve(color_formats.size());
	for (auto format : color_formats) {
		native_formats.emplace_back(native(format));
	}

	vk::Format using_depth_format = vk::Format::eUndefined;
	if (attachment_layout.get_depth_format()) {
		using_depth_format = native(attachment_layout.get_depth_format().value());
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
				.colorAttachmentCount = static_cast<uint32_t>(native_formats.size()),
				.pColorAttachmentFormats = native_formats.data(),
				.depthAttachmentFormat = using_depth_format,
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
