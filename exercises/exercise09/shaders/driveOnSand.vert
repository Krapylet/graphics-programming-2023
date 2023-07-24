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
uniform vec3 Right;
uniform vec2 DesertSize;

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

	// Also rotate it.
	// To rotate the object in place, subtract the traslation, add the rotation, add the translation again.
	// or just use the lookat tranformation matrix
	vec3 tangent;
	vec3 bitangent;
	vec3 normal;
	GetTangentSpaceVectorsFromSample(DesertUV, DepthMap, SampleDistance, OffsetStrength, DesertSize, tangent, bitangent, normal);

	// Transformation matrix that rotates the player model in the direction of the UV normal.
	// Hopefully the pivot is in the center of the model...
	vec3 eye = vec3(0,0,0);                     // We don't need an actual position, since we use directions to define at and up.
	vec3 at = cross(normalize(Right), normal);  // Swtiching the order inverts the model on the forward axis.
	vec3 up = normal;

	// Calculate rotation matrix values
	vec3 zaxis = normalize(at - eye);    
	vec3 xaxis = normalize(cross(zaxis, up));
	vec3 yaxis = cross(xaxis, zaxis);

	// Make rotation matrix work in a Right-handed coordinate system.
	zaxis = -zaxis;

	// Insert rotation matrix lookat values
	mat4 rotateInPlaceMatrix = {
		vec4(xaxis.x, xaxis.y, xaxis.z, dot(xaxis, eye)),
		vec4(yaxis.x, yaxis.y, yaxis.z, dot(yaxis, eye)),
		vec4(zaxis.x, zaxis.y, zaxis.z, dot(zaxis, eye)),
		vec4(0, 0, 0, 1)
	};

	// Apply rotation matrix to vertex point
	vec3 rotatedWorldPos = (rotateInPlaceMatrix * vec4(VertexPosition, 0)).xyz;


	// Replace VertexPosition with RotatedWorldPos to enable rotation
	gl_Position = WorldViewProjMatrix * vec4(rotatedWorldPos + vertexOffsetVector, 1.0);

	// also rotate normals by the rotation matrix.
	//TODO

	// Convert normal and tangents from world space to view space
	ViewTangent = (WorldViewMatrix * vec4(VertexTangent, 0.0)).xyz;
	ViewBitangent = (WorldViewMatrix * vec4(VertexBitangent, 0.0)).xyz;
	ViewNormal = (WorldViewMatrix * vec4(VertexNormal, 0.0)).xyz;
}
