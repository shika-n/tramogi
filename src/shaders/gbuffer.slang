import camera;
import object;

struct VertexInput {
	uint instance_id : SV_InstanceID;
	float3 position;
	float3 color;
	float3 normal;
	float2 uv;
};

struct VertexOutput {
	float4 position : SV_Position;
	float3 color;
	float3 normal;
	float2 uv;
};

struct FragmentOutput {
	float4 albedo : SV_Target0;
	float4 normal : SV_Target1;
};

[[vk_binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk_binding(0, 1)]]
ConstantBuffer<ObjectUniformBuffer> object_ubo;

[[vk_binding(1, 1)]]
Sampler2D sampler;

[[vk_push_constant]]
const uint32_t texture_index;

[shader("vertex")]
VertexOutput vert_main(VertexInput input) {
	float4 world_position = mul(object_ubo.model, float4(input.position, 1.0));

	VertexOutput output;
	output.position = mul(ubo.projection_view, world_position);
	output.color = input.color;
	output.normal = mul(object_ubo.model_normal, float4(input.normal, 0.0)).xyz;
	output.uv = input.uv;

	return output;
}

[shader("fragment")]
FragmentOutput frag_main(VertexOutput vertex_in) {
	float3 normal = normalize(vertex_in.normal);
	float4 sampled_albedo = sampler.Sample(vertex_in.uv);

	FragmentOutput output;
	if (texture_index > 0) {
		output.albedo = float4(sampled_albedo.xyz, 1.0);
	} else {
		output.albedo = float4(vertex_in.color, 1.0);
	}
	output.normal = float4(normal, 1.0);
	return output;
}



