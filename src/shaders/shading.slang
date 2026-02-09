import camera;
import screen;

[[vk_binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk_binding(0, 1)]]
Sampler2D sampler[3];

static float3 light_color = float3(1.0, 1.0, 1.0);
static float shininess = 16.0;

float linearize_depth(float depth, float z_near, float z_far) {
	return (z_near * z_far / (z_far + depth * (z_near - z_far))) / z_far;
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

	float3 ambient_color = float3(0.05, 0.05, 0.1) * albedo.xyz;
	float3 diffuse_color = light_factor * light_color * albedo.xyz;
	float3 specular_color = specular_factor * float3(1.0, 0.7, 0.6) * float3(1.0);

	if (((ubo.debug_options >> 1) & 0x1) == 1) {
		specular_color = 0.0;
	}

	return float4(ambient_color + diffuse_color + specular_color, 1.0);
}

[shader("vertex")]
ScreenVertexOutput screen_vert(uint32_t vertex_id: SV_VertexID) {
	return screen_vertex(vertex_id);
}

[shader("fragment")]
float4 shading_frag(ScreenVertexOutput in) : SV_Target {
	float4 albedo = sampler[0].Sample(in.screen_uv);
	float3 normal = sampler[1].Sample(in.screen_uv).xyz;
	float depth = sampler[2].Sample(in.screen_uv).r;
	float4 position = mul(ubo.inverse_projection_view, float4(in.ndc, depth, 1.0));
	float linear_depth = linearize_depth(depth, ubo.z_near, ubo.z_far);

	float4 color = blinn_phong(albedo, normal, position.xyz / position.w);

	if (ubo.gbuffer_debug == 1) {
		color = albedo;
	} else if (ubo.gbuffer_debug == 2) {
		color = float4(normal, 1.0);
	} else if (ubo.gbuffer_debug == 3) {
		color = float4(float3(linear_depth), 1.0);
	} else if (ubo.gbuffer_debug == 4) {
		color = float4(position.xyz / position.w, 1.0);
	}

	float fog_factor = 1.0 - exp(-(1.5 * linear_depth) * (1.5 * linear_depth));
	if ((ubo.debug_options & 0x1) == 1) {
		fog_factor = 0.0;
	}
	color = lerp(color, float4(0.8, 0.8, 1.0, 1.0), fog_factor);

	return color;
}



