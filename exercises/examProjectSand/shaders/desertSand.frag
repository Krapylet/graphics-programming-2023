//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
in vec3 ViewNormal;
in vec3 ViewTangent;
in vec3 ViewBitangent;
in vec2 TexCoord;

//Outputs
out vec4 FragAlbedo;
out vec2 FragNormal;
out vec4 FragOthers;

//Uniforms
uniform vec3 Color;
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;

void main()
{	
	


	// --------- Convert normals to worldspace ------------

	// Create tangent space matrix
	mat3 tangentMatrix = mat3(ViewTangent, ViewBitangent, ViewNormal);

	vec2 normal = normalize(vec2(1,1));
	float z = sqrt(max(1.0f - normal.x * normal.x - normal.y * normal.y, 0.0f));
	vec3 normalTangentSpace = vec3(normal, z);

	// Return matrix in world space
	vec2 worldSpaceNormal = normalize(tangentMatrix * normalTangentSpace).xy;

	FragNormal = vec2(0.5,0.5);

	//FragAlbedo = vec4(worldSpaceNormal, 0, 1); //vec4(texture(ColorTexture, TexCoord).rgb, 1);

	FragAlbedo = vec4(ViewNormal, 1);

	FragOthers = vec4(1,1,1,1);
}