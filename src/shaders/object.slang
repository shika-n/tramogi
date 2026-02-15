module object;

public struct ObjectTransformMatrices {
	public float4x4 model;
	public float4x4 normal_matrix;
};

public struct ObjectUniformBuffer {
	public ObjectTransformMatrices transform_matrices[2];
};



