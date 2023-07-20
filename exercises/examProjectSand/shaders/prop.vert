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
uniform vec3 PivotPosition; // position of pivot in world position
uniform vec2 DesertUV; // position of pivot in desert UV space.
uniform vec3 Right;

void main()
{


	// texture coordinates
	TexCoord = VertexTexCoord;

	// ------- Vertex position --------
	// Add the height map sample at the center of the mdoel to every vertex
	// final vertex position (for opengl rendering, *AND* for lighting)
	// This is the UV position of the pivot of the player object on the desert. Thus, the same height is retreived for all vertexes.
	float vertexOffset = GetHeightFromSample(DesertUV, DepthMap, SampleDistance, OffsetStrength);

	vec3 vertexOffsetVector = vec3(0, vertexOffset -0.1f, 0);

	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffsetVector, 1.0);

	vec3 at = Right;  




	// Convert normal and tangents from world space to view space
	ViewTangent = (WorldViewMatrix * vec4(VertexTangent, 0.0)).xyz;
	ViewBitangent = (WorldViewMatrix * vec4(VertexBitangent, 0.0)).xyz;
	ViewNormal = (WorldViewMatrix * vec4(VertexNormal, 0.0)).xyz;
}
