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

// Not yet used
uniform sampler2D NoiseMap; // Pr pixel noise for the light intensity

float GetHeightFromSample(vec2 pos)
{
	// Sample depth texture
	// We take a couple of extra samples in each direction to smooth out any extreme pixels.
	// Since the depth map is black and white, we only need to look at the red value.
	float depthSampleCenter = texture(DepthMap, pos).r;
	float depthSampleNorth = texture(DepthMap, pos + vec2(1,0) * SampleDistance).r;
	float depthSampleSouth = texture(DepthMap, pos + vec2(-1,0) * SampleDistance).r;
	float depthSampleEast = texture(DepthMap, pos + vec2(0,1) * SampleDistance).r;
	float depthSampleWest = texture(DepthMap, pos + vec2(0,-1) * SampleDistance).r;
	
	float depthSample = (depthSampleCenter + depthSampleNorth + depthSampleSouth + depthSampleEast + depthSampleWest)/5;

	// square to create a better depth distribution. Otherwise we get a bunch of extreme spikes.
	depthSample = depthSample * depthSample;

	// We also invert so that darker values are higher.
	float vertexOffsetIntensity = (1 - depthSample) * OffsetStrength;
	
	// sine easing curve. Experiment with more interesting distributions.
	float easedOffset = (cos(3.14f * vertexOffsetIntensity) - 1) / 2;

	// Apply offset on y axis. We don't need a transformation vertex since tangent space and is aligned with every single vertex in the plane.
	return easedOffset;
}


void main()
{

	// texture coordinates
	TexCoord = VertexTexCoord;

	// ------- Vertex position --------
	
	// final vertex position (for opengl rendering, not for lighting)
	// transforms from model space into world space.
	float vertexOffset = GetHeightFromSample(TexCoord);

	vec3 vertexOffsetVector = vec3(0, vertexOffset, 0);

	gl_Position = WorldViewProjMatrix * vec4(VertexPosition + vertexOffsetVector, 1.0);





	// ------------- normal --------------
	// To calculat the new normal of that point, we also need two adjacent points to create a corss product.
	// "Height" should be stored in z.

	// Sample depth texture to calculate the normal
	vec2 northUV = TexCoord + vec2(0,1) * SampleDistance;
	vec2 southUV = TexCoord + vec2(0,-1) * SampleDistance;
	vec2 eastUV = TexCoord + vec2(1,0) * SampleDistance;
	vec2 westUV = TexCoord + vec2(-1,0) * SampleDistance;

	// Since the depth map is black and white, we only need to look at the red value.
	float depthSampleNorth = texture(DepthMap, northUV).r;
	float depthSampleSouth = texture(DepthMap, southUV).r;
	float depthSampleEast = texture(DepthMap, eastUV).r;
	float depthSampleWest = texture(DepthMap, westUV).r;

	// change in height across samples pr. unit.
	float deltaX = (depthSampleEast - depthSampleWest)/(eastUV.x - westUV.x);
	float deltaY = (depthSampleNorth - depthSampleSouth)/(northUV.y - southUV.y);
	
	vec3 normal = normalize(vec3(deltaX, deltaY, 1));

	// we can also use one of the direction pairs to create a tangent
	vec3 tangent = normalize(vec3(northUV, depthSampleNorth) - vec3(southUV, depthSampleNorth));

	// And this tangent can be used to compute the bitangent
	vec3 bitangent = normalize(cross(tangent, normal));


	// Convert normal and tangents from world space to view space
	ViewTangent = (WorldViewMatrix * vec4(bitangent, 0.0)).xyz;
	ViewBitangent = (WorldViewMatrix * vec4(tangent, 0.0)).xyz;
	ViewNormal = (WorldViewMatrix * vec4(normal, 0.0)).xyz;
}
