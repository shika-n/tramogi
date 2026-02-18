import camera;
import screen;
import commons.poisson;

[[vk_binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk_binding(0, 1)]]
Sampler2D sampler[5];
[[vk_binding(1, 1)]]
SamplerCube skybox;

static float3 light_color = float3(1.0, 1.0, 1.0);
static float shininess = 16.0;

float linearize_depth(float depth, float z_near, float z_far) {
	return (z_near * z_far / (z_far + depth * (z_near - z_far))) / z_far;
}

float4 blinn_phong(
	float4 albedo,
	float3 normal,
	float metalness,
	float3 position,
	float3 light_dir,
	float shadow_factor
) {
	float light_factor = max(dot(light_dir, normal), 0.0);
	float specular_factor = 0.0;

	if (light_factor > 0.0) {
		float3 view_dir = normalize(ubo.camera_position - position);
		float3 half_dir = normalize(light_dir + view_dir);
		specular_factor = 0.8 * pow(max(dot(half_dir, normal), 0.0), shininess) *
						  ((shininess + 2) / (8 * 3.14));
	}

	float3 reflect_color =
		skybox.Sample(reflect(normalize(position.xyz - ubo.camera_position), normal)).xyz;
	float3 base_color = albedo.xyz * (1.0 - metalness) + reflect_color * metalness;

	float3 ambient_color = float3(0.05, 0.05, 0.1) * base_color *
						   (1.0 + (0.5 * min(dot(light_dir, normal), 0.0)));
	float3 diffuse_color = light_factor * light_color * base_color;
	float3 specular_color = specular_factor * float3(1.0, 0.7, 0.6);
	float3 shadow_color = float3(0.05, 0.05, 0.1) * light_factor;

	if ((ubo.debug_options & (1 << 4)) != 0) {
		specular_color = 0.0;
	}

	return float4(
		ambient_color + (diffuse_color + specular_color) * shadow_factor +
			(shadow_color * (1.0 - shadow_factor)),
		1.0
	);
}

[shader("vertex")]
ScreenVertexOutput screen_vert(uint32_t vertex_id: SV_VertexID) {
	return screen_vertex(vertex_id);
}

static float shadow_map_texel_size = 1.0 / 4096;
// clang-format off
static float2 hw_offsets[4] = {
	{ -0.5, -0.5 },
	{ -0.5,  0.5 },
	{  0.5, -0.5 },
	{  0.5,  0.5 },
};
// clang-format on

float calculate_shadow_factor(
	Sampler2D shadow_map,
	float4 shadow_coord,
	float2 position,
	float3 light_dir,
	float3 normal,
	uint8_t mode,
	bool dither,
	bool random_poisson
) {
	if (mode == 6) {
		return 1.0;
	}

	float cos = clamp(dot(light_dir, normal), 0.0, 1.0);
	float flat_bias = 0.0001 * cos;
	float slope_bias = 0.001 * (1.0 - cos);
	float bias = flat_bias + slope_bias;

	shadow_coord /= shadow_coord.w;
	shadow_coord.xy = shadow_coord.xy * 0.5 + 0.5;
	float receiver_depth = shadow_coord.z;

	uint8_t dither_index = 0;
	if (dither) {
		float2 dither_offset = float2(frac(position.xy * 0.5) > 0.25);
		dither_offset.y += dither_offset.x;
		// if (dither_offset.y > 1.1) {
		// 	dither_offset.y = 0;
		// }
		dither_index = uint8_t(dither_offset.x) + uint8_t(dither_offset.y) * 2;
	}

	float visibility = 0.0;
	uint8_t poisson_group_index = 0;
	if (random_poisson) {
		poisson_group_index =
			(uint8_t(frac(sin(dot(shadow_coord.xy, float2(625.61, 227.14))) * 34121.23154) * 10) %
			 4);
	}
	uint8_t sample_count = 1;
	float poisson_radius = 2.0;

	switch (mode) {
	case 0: // Poisson + HW x16
		sample_count = 4;
		for (uint8_t i = 0; i < sample_count; ++i) {
			float2 shadow_sample_coord = float2(
				shadow_coord.xy + (poisson_disk
								   [(i + poisson_group_index * sample_count + dither_index) %
									POISSON_DISK_SAMPLE_COUNT] *
								   poisson_radius) *
									  shadow_map_texel_size
			);

			visibility += shadow_map.SampleCmp(shadow_sample_coord, receiver_depth - bias);
		}
		visibility /= sample_count;
		break;
	case 1: // Poisson x16
		sample_count = 16;
		for (uint8_t i = 0; i < sample_count; ++i) {
			float2 shadow_sample_coord = float2(
				shadow_coord.xy + (poisson_disk
								   [(i + poisson_group_index * sample_count + dither_index) %
									POISSON_DISK_SAMPLE_COUNT] *
								   poisson_radius) *
									  shadow_map_texel_size
			);

			if (receiver_depth - bias < shadow_map.Sample(shadow_sample_coord).r) {
				visibility += 1;
			}
		}
		visibility /= sample_count;
		break;
	case 2: // HW x16
		sample_count = 4;
		for (uint8_t i = 0; i < sample_count; ++i) {
			float2 shadow_sample_coord = float2(
				shadow_coord.xy + (hw_offsets[(i + dither_index) % 4]) * shadow_map_texel_size
			);

			visibility += shadow_map.SampleCmp(shadow_sample_coord, receiver_depth - bias);
		}
		visibility /= sample_count;
		break;
	case 3: // Poisson x8
		sample_count = 8;
		for (uint8_t i = 0; i < sample_count; ++i) {
			float2 shadow_sample_coord = float2(
				shadow_coord.xy + (poisson_disk
								   [(i + poisson_group_index * sample_count + dither_index) %
									POISSON_DISK_SAMPLE_COUNT] *
								   poisson_radius) *
									  shadow_map_texel_size
			);

			if (receiver_depth - bias < shadow_map.Sample(shadow_sample_coord).r) {
				visibility += 1;
			}
		}
		visibility /= sample_count;
		break;
	case 4: // HW x4
		visibility = shadow_map.SampleCmp(
			shadow_coord.xy + (hw_offsets[dither_index]) * shadow_map_texel_size,
			receiver_depth - bias
		);
		break;
	case 5: // x1
		if (receiver_depth - bias <
			shadow_map.Sample(shadow_coord.xy + (hw_offsets[dither_index]) * shadow_map_texel_size)
				.r) {
			visibility += 1;
		}
		break;
	}

	return visibility;
}

[shader("fragment")]
float4 shading_frag(ScreenVertexOutput in) : SV_Target {
	float4 albedo = sampler[0].Sample(in.screen_uv);
	float4 normal_metal = sampler[1].Sample(in.screen_uv);
	float3 normal = normalize(normal_metal.xyz);
	float depth = sampler[2].Sample(in.screen_uv).r;
	float4 position = mul(ubo.inverse_projection_view, float4(in.ndc, depth, 1.0));
	position /= position.w;
	float linear_depth = linearize_depth(depth, ubo.z_near, ubo.z_far);

	float3 light_dir = -normalize(ubo.world_light_direction);

	float4 shadow_coord = mul(ubo.shadow_projection_view, position);
	uint8_t shadow_debug = uint8_t((ubo.debug_options >> 5) & 0x7);
	Sampler2D shadow_sampler = sampler[3];
	if (shadow_debug != 0 && shadow_debug != 2 && shadow_debug != 4) {
		shadow_sampler = sampler[4];
	}
	float shadow_factor = calculate_shadow_factor(
		shadow_sampler,
		shadow_coord,
		in.position.xy,
		light_dir,
		normal,
		shadow_debug,
		(ubo.debug_options & (0x1 << 8)) != 0,
		(ubo.debug_options & (0x1 << 9)) != 0
	);

	float4 color = float4(0.0);

	uint8_t gbuffer_debug = uint8_t(ubo.debug_options & 0x7);
	if (gbuffer_debug == 0) {
		color = blinn_phong(albedo, normal, normal_metal.w, position.xyz, light_dir, shadow_factor);
	} else if (gbuffer_debug == 1) {
		color = albedo;
	} else if (gbuffer_debug == 2) {
		color = float4(normal, 1.0);
	} else if (gbuffer_debug == 3) {
		color = float4(float3(linear_depth), 1.0);
	} else if (gbuffer_debug == 4) {
		color = float4(position.xyz, 1.0);
	}

	float fog_factor = 1.0 - exp(-(1.5 * linear_depth) * (1.5 * linear_depth));
	if ((ubo.debug_options & (1 << 3)) != 0) {
		fog_factor = 0.0;
	}
	color = lerp(color, float4(0.8, 0.8, 1.0, 1.0), fog_factor);

	return color;
}



