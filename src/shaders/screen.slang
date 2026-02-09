module screen;

static float2 vertices[] = {
	{ -1.0, -1.0 },
	{ -1.0, 3.0 },
	{ 3.0, -1.0 },
};

public struct ScreenVertexOutput {
	public float4 position : SV_Position;
	public float2 ndc;
	public float2 screen_uv;
};

public ScreenVertexOutput screen_vertex(uint32_t vertex_id: SV_VertexID) {
	return {
		float4(vertices[vertex_id], 1.0, 1.0),
		vertices[vertex_id],
		(vertices[vertex_id] + 1.0) / 2.0,
	};
}



