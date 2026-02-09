import camera;
import screen;

struct SkyBoxParams {
	Sampler2D depth_sampler;
	SamplerCube skybox_sampler;
};

[[vk_binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk_binding(0, 1)]]
ParameterBlock<SkyBoxParams> skybox;

[shader("vertex")]
ScreenVertexOutput screen_vert(uint32_t vertex_id: SV_VertexID) {
	return screen_vertex(vertex_id);
}

[shader("fragment")]
float4 skybox_frag(ScreenVertexOutput in) : SV_Target {
	float3 camera_texel_dir = normalize(
		ubo.camera_forward + (in.ndc.x * ubo.camera_right) + (-in.ndc.y * ubo.camera_up)
	);
	float4 color = float4(skybox.skybox_sampler.Sample(camera_texel_dir).xyz, 1.0);

	return color;
}



