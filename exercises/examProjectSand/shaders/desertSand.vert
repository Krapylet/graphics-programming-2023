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
uniform sampler2D DepthMap;
uniform float OffsetStrength;
uniform float SampleDistance;
// Not yet used
uniform sampler2D NormalMap;
uniform sampler2D NoiseMap;

void main()
{
	

	// texture coordinates
	TexCoord = VertexTexCoord;

	// ------- Vertex position --------
	// Sample depth texture
	vec4 depthSample = texture(DepthMap, VertexTexCoord);

	// Since the depth map is blacka and white, each rgb value is the same. We therefore only need one.
	float vertexOffsetIntensity = depthSample.x * OffsetStrength;

	// Apply offset exclusivly on the view y axis. This is a bit hacky, and should probably be done in modelspace instead of view space so that we can rotate the model.
	//vec3 vertexOffset = vec3(0, vertexOffsetIntensity, 0);

	// final vertex position (for opengl rendering, not for lighting)
	//gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffset, 1.0);


	// -------- Normal and tangents ---------
	float sampleDist = SampleDistance; // This could be made into a uniform.

	// We have to update the normals since we just modified the curvature of the plane.
	// We find a simplified tangent by sampling two points on either side of the vertex on a cardinal axis,
	// And use that as the start/end of the tangent.
	vec4 tangentStart = texture(DepthMap, VertexTexCoord + vec2(-1,0) * sampleDist);
	vec4 tangentEnd = texture(DepthMap, VertexTexCoord + vec2(1,0) * sampleDist);
	float tangentDiff = tangentEnd.x - tangentStart.x;
	vec3 tangent = normalize(vec3(1, tangentEnd.x - tangentStart.x , 0));
	
	// Calculate the bitangent the same way
	vec4 bitangentStart = texture(DepthMap, VertexTexCoord + vec2(0,1) * sampleDist);
	vec4 bitangentEnd = texture(DepthMap, VertexTexCoord + vec2(0,-1) * sampleDist);
	vec3 bitangent = normalize(vec3(0, bitangentEnd.x - bitangentStart.x, 1));

	// Now we can calucalte the new normal as the cross product of the tangent and bitangent
	vec3 normal = cross(tangent, bitangent);

	// normal in view space (for lighting computation)
	ViewNormal = normal;  //(WorldViewMatrix * vec4(normal, 0.0)).xyz;

	// tangent in view space (for lighting computation)
	ViewTangent = tangent; //(WorldViewMatrix * vec4(tangent, 0.0)).xyz;

	// bitangent in view space (for lighting computation)
	ViewBitangent = bitangent; //(WorldViewMatrix * vec4(bitangent, 0.0)).xyz;

	// DEBUG STUFF
	vec3 vertexOffset = vec3(0, vertexOffsetIntensity, 0);
	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffset, 1.0);
}
