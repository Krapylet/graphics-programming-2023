//Inputs
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexTangent;
layout (location = 3) in vec3 VertexBitangent;
layout (location = 4) in vec2 VertexTexCoord;

//Outputs
out vec3 ViewNormal;
out vec3 ViewTangent;
out vec3 ViewBitangent;
out vec2 TexCoord;   // UV

//Uniforms
uniform mat4 WorldViewMatrix;
uniform mat4 WorldViewProjMatrix;
uniform float OffsetStrength;
uniform float SampleDistance;
uniform sampler2D DepthMap;
uniform sampler2D NormalMap; // Additional normal map for providing small waves on the bigger dunes.

// Not yet used
uniform sampler2D NoiseMap; // Pr pixel noise for the light intensity

vec3 GetTangentFromSample(vec2 startDirection, vec2 endDirection)
{
	vec2 tangentStart = VertexTexCoord + startDirection * SampleDistance;
	vec2 tangentEnd = VertexTexCoord + endDirection * SampleDistance;
	float tangentStartDepth = texture(DepthMap, tangentStart).x;
	float tangentEndDepth = texture(DepthMap, tangentEnd).x;
	float tangentHeight = tangentEndDepth - tangentStartDepth;
	vec3 tangent = normalize(vec3(tangentEnd - tangentStart, tangentHeight));
	return tangent;
}

void main()
{

	// texture coordinates
	TexCoord = VertexTexCoord;

	// ------- Vertex position --------
	// Sample depth texture
	// We take a couple of extra samples in each direction to smooth out any extreme pixels.
	vec4 depthSampleCenter = texture(DepthMap, VertexTexCoord);
	vec4 depthSampleNorth = texture(DepthMap, VertexTexCoord + vec2(1,0) * SampleDistance);
	vec4 depthSampleSouth = texture(DepthMap, VertexTexCoord + vec2(-1,0) * SampleDistance);
	vec4 depthSampleEast = texture(DepthMap, VertexTexCoord + vec2(0,1) * SampleDistance);
	vec4 depthSampleWest = texture(DepthMap, VertexTexCoord + vec2(0,-1) * SampleDistance);
	
	// Since the depth map is black and white, we only need to look at the red value.
	float depthSample = (depthSampleCenter.r + depthSampleNorth.r + depthSampleSouth.r + depthSampleEast.r + depthSampleWest.r)/5;

	// square to create a better depth distribution.
	depthSample = depthSample * depthSample;

	// We also invert so that darker values are lower to the ground.
	float vertexOffsetIntensity = (1 - depthSample) * OffsetStrength;
	
	// Apply offset exclusivly on the view y axis. This is a bit hacky, and should probably be done in modelspace instead of view space so that we can rotate the model.
	vec3 vertexOffset = vec3(0, (cos(3.14f * vertexOffsetIntensity) - 1) / 2, 0);

	// final vertex position (for opengl rendering, not for lighting)
	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffset, 1.0);


	// -------- Normal and tangents ---------
	// Normal should point in z direction
	//For each fragment, to calculate the normal direction, we sample four points in an x to create
	//A tangent and bitangent for the vector. We can then calculate the normal as the crossproduct between the two.	

	// I'm pretty sure these are all in model/tangent space?
	// And that we need to compute the Tangent->world transformation matrix to make the light hit them from any angle?
	vec3 tangent = GetTangentFromSample(vec2(-1, -1), vec2(1, 1));
	vec3 bitangent = GetTangentFromSample(vec2(1, -1), vec2(-1, 1));
	vec3 normal = normalize(cross(tangent, bitangent));

	// Convert normal and tangents to view space
	vec3 worldTangent = (WorldViewMatrix * vec4(tangent, 0.0)).xyz;
	vec3 worldBitantent = (WorldViewMatrix * vec4(bitangent, 0.0)).xyz;
	vec3 worldNormal = (WorldViewMatrix * vec4(normal, 0.0)).xyz;

	ViewTangent = worldTangent;
	ViewBitangent = worldBitantent;
	ViewNormal = worldNormal;


	// --------- Mix depth map and normal map --------
	// We need to do this here, since the deferred shaders run for every object, and we only want the sand to be wavy
	// Sample the normal map for the smaller sand waves and mix them together with the normals from the depth map.
	// Use a mix and a universal property float to set the how controlling each of them are.

	// translated into world space?
	//vec3 mapNormal = SampleNormalMap(NormalMap, VertexTexCoord, normal, tangent, bitangent);

	

}
