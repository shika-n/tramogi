module object;

public struct ObjectTransformMatrices {
	public float4x4 model;
	public float4x4 normal_matrix;

	public float metalness;
	public uint32_t texture_normal_id;
};

public struct ObjectUniformBuffer {
	public ObjectTransformMatrices transform_matrices[100];
};



