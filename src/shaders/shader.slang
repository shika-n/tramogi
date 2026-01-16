struct VertexInput {
	float3 position;
	float3 color;
};

struct VertexOutput {
	float4 position : SV_Position;
	float3 color;
};

struct UniformBuffer {
	float4x4 projection;
	float4x4 view;
	float4x4 model;
};
ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VertexOutput vert_main(VertexInput input) {
	VertexOutput output;
	output.position =
		mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.position, 1.0))));
	output.color = input.color;
	return output;
}

// Sampler2D texture;

[shader("fragment")]
float4 frag_main(VertexOutput vertex_in) : SV_Target {
	float z_fog =
		clamp(1.0 - (((vertex_in.position.z / vertex_in.position.w) / 10) - 0.5) * 2.0, 0.0, 1.0);
	return float4(vertex_in.color * z_fog, 1.0);
	// return texture.Sample(vertex_in.tex_coord) * z_fog;
}



