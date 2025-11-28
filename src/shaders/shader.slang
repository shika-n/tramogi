struct VertexInput {
	float2 position;
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
		mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.position, 0.0, 1.0))));
	output.color = input.color;
	return output;
}

[shader("fragment")]
float4 frag_main(VertexOutput vertex_in) : SV_Target {
	return float4(vertex_in.color, 1.0);
}


