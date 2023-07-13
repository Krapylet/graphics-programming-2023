//Inputs
//All vertex inputs are in model space.
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexTangent;
layout (location = 3) in vec3 VertexBitangent;
layout (location = 4) in vec2 VertexTexCoord;

//Outputs
layout (location = 0) out vec3 ViewNormal;
layout (location = 1) out vec3 ViewTangent;
layout (location = 2) out vec3 ViewBitangent;
layout (location = 3) out vec2 TexCoord;   // UV

//Uniforms
uniform mat4 WorldViewMatrix; // converts from world space to view space
uniform mat4 WorldViewProjMatrix; // Converts from world space to clip space
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

	// The vertical direction is along the y axsis.
	vec2 normalPlaneProjection = vec2(tangentEnd - tangentStart);
	// Does the lighting calculations assume height is in y or z coordinate?
	vec3 tangent = normalize(vec3(normalPlaneProjection.x, tangentHeight, normalPlaneProjection.y));
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

	// square to create a better depth distribution. Otherwise we get a bunch of extreme spikes.
	depthSample = depthSample * depthSample;

	// We also invert so that darker values are lower to the ground.
	float vertexOffsetIntensity = (1 - depthSample) * OffsetStrength;
	
	// sine easing curve. Experiment with more interesting distributions.
	float easedOffset = (cos(3.14f * vertexOffsetIntensity) - 1) / 2;

	// Apply offset on y axis. We don't need a transformation vertex since tangent space and is aligned with every single normal in the plane.
	vec3 vertexOffset = vec3(0, easedOffset, 0);

	// final vertex position (for opengl rendering, not for lighting)
	// transforms from model space into world space.
	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffset, 1.0);


	// -------- Normal and tangents ---------
	// For each fragment, to calculate the normal direction, we sample four points in an x to create
	// A tangent and bitangent for the vector. We can then calculate the normal as the crossproduct between the two.
	// This doesn't currently handle the case where samples are outside the texture edge that well.

	// To get the normal of the height map, we need the tangent and bitangent first.
	vec3 tangentSpaceTangent = GetTangentFromSample(vec2(-1, 0), vec2(1, 0));
	vec3 tangentSpaceBitangent = GetTangentFromSample(vec2(0, -1), vec2(0, 1));
	vec3 tangentSpaceNormal = normalize(cross(tangentSpaceTangent, tangentSpaceBitangent));

	// We use the vertex parameters to create the tangent matrix.
	mat4 tangentMatrix = mat4(
		vec4(VertexTangent, 0),  // these are collumns, not rows.
		vec4(VertexBitangent, 0),
		vec4(VertexNormal, 0),
		vec4(0,0,0,0)
	);

	// Use this tangent matrix to convert normals from tangent space to world space.
	vec3 worldTangent = (tangentMatrix * vec4(tangentSpaceTangent, 0)).xyz;
	vec3 worldBitangent = (tangentMatrix * vec4(tangentSpaceBitangent, 0)).xyz;
	vec3 worldNormal = (tangentMatrix * vec4(tangentSpaceNormal, 0)).xyz;

	// Convert normal and tangents from world space to view space
	ViewTangent = (WorldViewMatrix * vec4(tangentSpaceTangent, 0.0)).xyz;
	ViewBitangent = (WorldViewMatrix * vec4(tangentSpaceBitangent, 0.0)).xyz;
	ViewNormal = (WorldViewMatrix * vec4(tangentSpaceNormal, 0.0)).xyz;
}
