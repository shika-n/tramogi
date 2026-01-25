struct VertexInput {
	uint instance_id : SV_InstanceID;
	float3 position;
	float3 color;
	float3 normal;
};

struct VertexOutput {
	float4 position : SV_Position;
	float3 color;
	float3 normal;
	float3 world_position;
};

struct UniformBuffer {
	float4x4 projection;
	float4x4 view;
	float4x4 model;
	float4x4 model_normal;
	float3 camera_position;
};
ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VertexOutput vert_main(VertexInput input) {
	float gap = 3.0;
	float gap_mult = 1 + gap;
	uint grid = 11;
	float mid = (gap_mult * (grid - 1)) / 2.0;

	input.position.x += gap_mult * (input.instance_id % grid) - mid;
	input.position.z += gap_mult * ((input.instance_id % (grid * grid)) / grid) - mid;
	input.position.y += gap_mult * (input.instance_id / (grid * grid)) - mid;

	float4 world_position = mul(ubo.model, float4(input.position, 1.0));

	VertexOutput output;
	output.position = mul(ubo.projection, mul(ubo.view, world_position));
	output.world_position = world_position.xyz;
	output.color = input.color;
	output.normal = mul(ubo.model_normal, float4(input.normal, 0.0)).xyz;

	return output;
}

static float z_far = 100;
static float3 light_world_dir = normalize(float3(0.3, -0.6, 0.1));

[shader("fragment")]
float4 frag_main(VertexOutput vertex_in) : SV_Target {
	float z_fog = clamp(
		1.0 - (((vertex_in.position.z / vertex_in.position.w) / z_far) - 0.5) * 2.0,
		0.0,
		1.0,
	);

	float3 light_dir = normalize(-light_world_dir);
	float3 normal = normalize(vertex_in.normal);

	float light_factor = max(dot(light_dir, normal), 0.0);
	float specular_factor = 0.0;

	if (light_factor > 0.0) {
		float3 view_dir = normalize(ubo.camera_position - vertex_in.world_position);
		float3 half_dir = normalize(light_dir + view_dir);
		specular_factor = pow(max(dot(half_dir, normal), 0.0), 128.0);
	}

	float3 ambient_color = 0.005 * vertex_in.color;
	float3 diffuse_color = light_factor * vertex_in.color;
	float3 specular_color = specular_factor * float3(1.0);

	return float4((ambient_color + diffuse_color + specular_color) * z_fog, 1.0);
}


