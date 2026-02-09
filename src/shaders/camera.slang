module camera;

public struct UniformBuffer {
	public float4x4 projection_view;
	public float4x4 inverse_projection_view;

	public float3 camera_position;
	public float3 camera_forward;
	public float3 camera_right;
	public float3 camera_up;
	public float z_near;
	public float z_far;

	public float3 world_light_direction;

	public int gbuffer_debug;
	public uint8_t debug_options;
};



