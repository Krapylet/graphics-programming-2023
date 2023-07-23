//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec3 TangentNormal;
layout (location = 4) in vec2 TexCoord;

//Outputs
layout (location = 0) out vec4 FragAlbedo;
layout (location = 1) out vec2 FragNormal;
layout (location = 2) out vec4 FragOthers;
// Create another framebuffer for the celshading.

//Uniforms
uniform vec3 Color;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;
uniform vec2 ObjectSize; // world size of the two lengths of the plane
uniform float TileSize;
uniform mat4 WorldViewMatrix; // converts from world space to view space

void main()
{	

	// Read normalTexture
	// Multiply texcoord with world size to get a tiling texture.
	float u = mod(TexCoord.x * TileSize, 1);
	float v = mod(TexCoord.y * TileSize, 1);
	vec2 normalMap = texture(NormalTexture, vec2(u,v)).xy * 2 - vec2(1);

	// Get implicit Z component
	vec3 normalTangentSpace = GetImplicitNormal(normalMap);

	// convert normal map to world space. The fact that we have to do this conversion really feels like a bug. I must be doing something wrong somethere in the stack.
	normalTangentSpace = vec3(normalTangentSpace.x, normalTangentSpace.z, -normalTangentSpace.y);

	// Combine depth map normal and normal map normal in tangent space using UDN:
	// Since Y is the "up" direction, we switch that with z in the formular.
	vec3 combinedTangentSpaceNormal =  normalize(vec3(normalTangentSpace.x + TangentNormal.x, TangentNormal.y, normalTangentSpace.z + TangentNormal.z));

	// Return normal in view space
	vec3 screenSpaceMapNormal = normalize(WorldViewMatrix * vec4(combinedTangentSpaceNormal,0)).xyz;

	// Combine normals with the UDN method to make the normal waves softer and keep 
	// celshaded edges and to not have shadows on top edges of hills.
	// UDN could beneficially be modified to 
	// 1 - Viewnormal.y works well for square scale
	// Viewnormal.y works well for scaled.
	// height-weighted linear blend: vec3 combinedNormal =  normalize(vec3(screenSpaceMapNormal.x + ViewNormal.x, screenSpaceMapNormal.y + ViewNormal.y, ViewNormal.z));
	FragNormal = screenSpaceMapNormal.xy;

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