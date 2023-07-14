//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec2 TexCoord;

//Outputs
layout (location = 0) out vec4 FragAlbedo;
layout (location = 1) out vec2 FragNormal;
layout (location = 2) out vec4 FragOthers;

//Uniforms
uniform vec3 Color;
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;

void main()
{	
	FragNormal = ViewNormal.xy;

	FragAlbedo = vec4(Color, 1);

	FragOthers = vec4(1,0.5,0,1);


		// --------- Mix depth normal and normal map normal  --------
	// Sample the normal map for the smaller sand waves and mix them together with the normals from the depth map.
	// Use a mix and a universal property float to set the how controlling each of them are.

	// How do you make these variable names meaningfull while keeping them from becomming absolute gibberish???
	// vec3 normalMapTangetSpaceNormal = texture(NormalMap, VertexTexCoord).rgb * 2 - 1;
	// REmember to cconvert form 0-1 to -1 - 1

	// Combine normals with the UDN method
	// vec3 combinedNormal =  normalize(float3(n1.xy + n2.xy, n1.z));	
}