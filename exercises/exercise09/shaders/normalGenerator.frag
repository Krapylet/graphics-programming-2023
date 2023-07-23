//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec3 WorldNormal; // Fragment normal in world(?) space.
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
uniform sampler2D NoiseTexture;
uniform float NoiseStrength;
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
	// It probably stems from the same source that screws up the normal calculations in the depthMapUitls.
	normalTangentSpace = vec3(normalTangentSpace.x, normalTangentSpace.z, -normalTangentSpace.y);

	// Divide the normals size to make the height map more pronounced.
	normalTangentSpace /= 5;

	// Combine depth map normal and normal map normal in tangent space using UDN:
	// - Since Y is the "up" direction, we switch that with z in the formular.
	vec3 combinedTangentSpaceNormal =  normalize(vec3(normalTangentSpace.x + WorldNormal.x, WorldNormal.y, normalTangentSpace.z + WorldNormal.z));

	// add pixel noise to the normal.
	// - We only change the "up" value, making the sand grains more likely to turn in a more/less extreme direction of their current tilt.
	// - We sample with the UV to make each fragment sample from the same noise value each frame. That way we should get temporal consitency rather than random flickering.
	// - Noise is sampled such that grey is 0, white is 1 and black is -1.
	float noise = texture(NoiseTexture, vec2(u*2,v*2)).x * 2 - 1;

	combinedTangentSpaceNormal =  normalize(vec3(combinedTangentSpaceNormal.x, combinedTangentSpaceNormal.y + noise * NoiseStrength, combinedTangentSpaceNormal.z));

	// get normal in view space
	vec3 screenSpaceMapNormal = normalize(WorldViewMatrix * vec4(combinedTangentSpaceNormal,0)).xyz;

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