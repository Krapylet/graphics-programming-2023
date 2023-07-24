//Inputs
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
uniform vec2 DesertUV; // The object's position on the desert in the desert's UV coordinates.

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


	// normal in view space (for lighting computation)
	ViewNormal = (WorldViewMatrix * vec4(VertexNormal, 0.0)).xyz;

	// tangent in view space (for lighting computation)
	ViewTangent = (WorldViewMatrix * vec4(VertexTangent, 0.0)).xyz;

	// bitangent in view space (for lighting computation)
	ViewBitangent = (WorldViewMatrix * vec4(VertexBitangent, 0.0)).xyz;



	// final vertex position (for opengl rendering, not for lighting)
	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffsetVector, 1.0);
}
