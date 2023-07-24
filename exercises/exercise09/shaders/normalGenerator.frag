//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec3 WorldNormal; // Fragment normal in world(?) space. THis values isn't in tangent space, and causes a bug when mixed with the normalMap.
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
uniform float NoiseTileFrequency;
uniform vec2 ObjectSize; // world size of the two lengths of the plane
uniform float TileSize;
uniform mat4 WorldViewMatrix; // converts from world space to view space
uniform float AmbientOcclusion;
uniform float Metalness;
uniform float Roughness;
uniform float Unused;


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

	// add pixel noise to the normal. We do this by genrating random vector and lerping randomly towards it.
	// - We sample with the UV to make each fragment sample from the same noise value each frame. That way we should get temporal consitency rather than random flickering.
	// - Usually you would sample the noise such that grey is 0, white is 1 and black is -1.
	// - We take the power of the texture to get a more grainy texture as pr the journy GDC talk.
	float noiseX = pow(texture(NoiseTexture, vec2(u,v) * NoiseTileFrequency).x, 2.0) * 2 - 1;
	float noiseZ = pow(texture(NoiseTexture, vec2(v,u) * NoiseTileFrequency).x, 2.0) * 2 - 1;
	float noiseY = pow(texture(NoiseTexture, vec2((u+v),(u-v)) * NoiseTileFrequency).x, 2.0) * 2 - 1;
	float noiseW = pow(texture(NoiseTexture, vec2((u-v),(u+v)) * NoiseTileFrequency).x, 2.0) * 2 - 1;
	vec3 randomDir = normalize(vec3(noiseX, noiseY, noiseZ));
	vec3 noisedTangentSpaceNormal = mix(combinedTangentSpaceNormal, randomDir, NoiseStrength * noiseW);

	// get noised normal in view space
	vec3 noisedViewNormal = normalize(WorldViewMatrix * vec4(noisedTangentSpaceNormal,0)).xyz;

	FragNormal = noisedViewNormal.xy;

	FragAlbedo = vec4(Color, 1);

	// ambientOcclusion, metalness, roughness, unused,
	FragOthers = vec4(AmbientOcclusion, Metalness, Roughness, Unused); //texture(SpecularTexture, TexCoord);
}