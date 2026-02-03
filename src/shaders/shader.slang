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
};

struct UniformBuffer {
	float4x4 projection_view;
	float4x4 inverse_projection_view;

	float3 camera_position;
	float3 world_light_direction;

	float4x4 model;
	float4x4 model_normal;

	int gbuffer_debug;
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
	output.position = mul(ubo.projection_view, world_position);
	output.color = input.color;
	output.normal = mul(ubo.model_normal, float4(input.normal, 0.0)).xyz;

	return output;
}

static float z_far = 100;
static float3 light_color = float3(1.0, 1.0, 1.0);
static float shininess = 16.0;

struct FragmentOutput {
	float4 albedo : SV_Target0;
	float4 normal : SV_Target1;
};

[shader("fragment")]
FragmentOutput frag_main(VertexOutput vertex_in) {
	float3 normal = normalize(vertex_in.normal);

	FragmentOutput output;
	output.albedo = float4(vertex_in.color, 1.0); // TODO: Sample texture if any
	output.normal = float4(normal, 1.0);
	return output;
}

float4 blinn_phong(float4 albedo, float3 normal, float3 position) {
	float3 light_dir = normalize(-ubo.world_light_direction);
	float3 normal = normalize(normal);

	float light_factor = max(dot(light_dir, normal), 0.0);
	float specular_factor = 0.0;

	if (light_factor > 0.0) {
		float3 view_dir = normalize(ubo.camera_position - position);
		float3 half_dir = normalize(light_dir + view_dir);
		specular_factor = 0.8 * pow(max(dot(half_dir, normal), 0.0), shininess) *
						  ((shininess + 2) / (8 * 3.14));
	}

	float3 ambient_color = 0.01 * albedo.xyz;
	float3 diffuse_color = light_factor * light_color * albedo.xyz;
	float3 specular_color = specular_factor * light_color * float3(1.0);

	return float4(ambient_color + diffuse_color + specular_color, 1.0);
}

Sampler2D sampler[3];

static float2 vertices[] = {
	{ -1.0, -1.0 },
	{ -1.0, 1.0 },
	{ 1.0, -1.0 },
	{ 1.0, -1.0 },
	{ -1.0, 1.0 },
	{ 1.0, 1.0 },
};

struct ScreenVertexOutput {
	float4 position : SV_Position;
	float2 pos;
	float2 screen_uv;
};

[shader("vertex")]
ScreenVertexOutput screen_vert(uint32_t vertex_id: SV_VertexID) {
	return {
		float4(vertices[vertex_id], 0.0, 1.0),
		vertices[vertex_id],
		(vertices[vertex_id] + 1.0) / 2.0,
	};
}

[shader("fragment")]
float4 screen_frag(ScreenVertexOutput in) : SV_Target {
	float4 albedo = sampler[0].Sample(in.screen_uv);
	float3 normal = sampler[1].Sample(in.screen_uv).xyz;
	float depth = sampler[2].Sample(in.screen_uv).r;
	float4 position = mul(ubo.inverse_projection_view, float4(in.pos, depth, 1.0));

	float4 color = blinn_phong(albedo, normal, position.xyz / position.w);

	if (ubo.gbuffer_debug == 1) {
		color = albedo;
	} else if (ubo.gbuffer_debug == 2) {
		color = float4(normal, 1.0);
	} else if (ubo.gbuffer_debug == 3) {
		color = float4(float3(depth), 1.0);
	} else if (ubo.gbuffer_debug == 4) {
		color = float4(position.xyz / position.w, 1.0);
	}

	return color;
}



