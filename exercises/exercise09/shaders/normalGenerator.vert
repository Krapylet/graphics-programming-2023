//Inputs
layout (location = 0) in vec3 VertexPosition; //All vertex inputs are in model space.
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexTangent;
layout (location = 3) in vec3 VertexBitangent;
layout (location = 4) in vec2 VertexTexCoord;

//Outputs
layout (location = 0) out vec3 ViewNormal;
layout (location = 1) out vec3 ViewTangent;
layout (location = 2) out vec3 ViewBitangent;
layout (location = 3) out vec3 TangentNormal;
layout (location = 4) out vec2 TexCoord;   // UV

//Uniforms
uniform mat4 WorldViewMatrix; // converts from world space to view space
uniform mat4 WorldViewProjMatrix; // Converts from world space to clip space
uniform float OffsetStrength;
uniform float SampleDistance;
uniform sampler2D DepthMap;
uniform vec2 ObjectSize; // world size of the two lengths of the plane
uniform vec3[12] PlayerPositions;

// constants
const int playerPositionCount = 12;

float calculateWaveEffect(float distToClosesPoint){
	float distThreshold = 6;
	bool distIsWithinEffect = distToClosesPoint < distThreshold;

	// Effect times wave strength.
	// dist divided with wave width.
	// distThreshold multiplied with wave width.
	return distIsWithinEffect ? -cos(distToClosesPoint) - cos(distToClosesPoint/2) : 0;
}


void main()
{

	// texture coordinates
	TexCoord = VertexTexCoord;

	// ------- Vertex position height offset --------
	
	// final vertex position (for opengl rendering, *AND* for lighting)
	float vertexOffset = GetHeightFromSample(TexCoord, DepthMap, SampleDistance, OffsetStrength);


	// ------ Vetex position player dist offset ------

	// compute distance to each of the saved positions
	float bestDistSoFar = 99999999;
	int bestIndexSoFar = 0;
	for (int i = 0; i < playerPositionCount; i += 1){
		float dist = GetManhattenDistance(PlayerPositions[i], VertexPosition);

		// Update best dist and index if new distance is lower
		// only one branch is evaluated https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.50.pdf
		bestDistSoFar = dist < bestDistSoFar ? dist : bestDistSoFar;
		bestIndexSoFar = dist < bestDistSoFar ? i : bestIndexSoFar;
	}

	// Now that we know which point is closes and how far it is, we can compute the effect of the car driving by
	// We use a cosinus curve to get a nice wave shape.
	float waveEffect = calculateWaveEffect(bestDistSoFar);


	// We use an easing function and the index to reduce the wave effect based on how old the position is.



	// ------- combine offsets -----------

	vec3 vertexOffsetVector = vec3(0, vertexOffset + waveEffect, 0);

	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffsetVector, 1.0);

	vec3 tangent;
	vec3 bitangent;
	vec3 normal;
	GetTangentSpaceVectorsFromSample(TexCoord, DepthMap, SampleDistance, OffsetStrength, ObjectSize, tangent, bitangent, normal);

	// Convert normal and tangents from world space to view space
	ViewTangent = (WorldViewMatrix * vec4(tangent, 0.0)).xyz;
	ViewBitangent = (WorldViewMatrix * vec4(bitangent, 0.0)).xyz;
	ViewNormal = (WorldViewMatrix * vec4(normal, 0.0)).xyz;

	TangentNormal = normal;
}
