module basic_vertex;

public struct VertexInput {
	public uint instance_id : SV_InstanceID;
	public float3 position;
	public float3 color;
	public float3 normal;
	public float2 uv;
};


