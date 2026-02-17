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
	uint32_t object_id : SV_Target2;
};

[[vk_binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk_binding(0, 1)]]
ConstantBuffer<ObjectUniformBuffer> object_ubo;

[[vk_binding(1, 1)]]
Sampler2D sampler;

struct PushConstantData {
	const uint32_t model_id;
	const uint32_t texture_id;
};
[[vk_push_constant]]
const PushConstantData pc;

[shader("vertex")]
VertexOutput vert_main(VertexInput input) {
	float4 world_position =
		mul(object_ubo.transform_matrices[pc.model_id].model, float4(input.position, 1.0));

	VertexOutput output;
	output.position = mul(ubo.projection_view, world_position);
	output.color = input.color;
	output.normal = mul(object_ubo.transform_matrices[pc.model_id].normal_matrix,
						float4(input.normal, 0.0))
						.xyz;
	output.uv = input.uv;

	return output;
}

[shader("fragment")]
FragmentOutput frag_main(VertexOutput vertex_in) {
	float3 normal = normalize(vertex_in.normal);
	float4 sampled_albedo = sampler.Sample(vertex_in.uv);

	FragmentOutput output;
	if (pc.texture_id > 0) {
		output.albedo = float4(sampled_albedo.xyz, 1.0);
	} else {
		output.albedo = float4(vertex_in.color, 1.0);
	}
	output.normal = float4(normal, object_ubo.transform_matrices[pc.model_id].metalness);
	output.object_id = pc.model_id;
	return output;
}

[shader("vertex")]
float4 shadow_vert(VertexInput input) : SV_Position {
	float4 world_position =
		mul(object_ubo.transform_matrices[pc.model_id].model, float4(input.position, 1.0));
	return mul(ubo.shadow_projection_view, world_position);
}

[shader("fragment")]
float4 shadow_frag() : SV_Target0 {
	return float4(1.0);
}



