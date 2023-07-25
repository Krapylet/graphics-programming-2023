//Inputs
layout (location = 0) in vec2 TexCoord;

//Outputs
out vec4 FragColor;

//Uniforms
uniform sampler2D DepthTexture;
uniform sampler2D AlbedoTexture;
uniform sampler2D NormalTexture;
uniform sampler2D OthersTexture;
uniform mat4 InvViewMatrix;
uniform mat4 InvProjMatrix;
uniform vec3 FogColor; 
uniform float FogStrength;
uniform float FogDistance;
uniform vec3 SpecularColor;
uniform float CameraFarPlane;
uniform float CameraCarDistance;

void main()
{
	// Extract information from g-buffers
	vec3 position = ReconstructViewPosition(DepthTexture, TexCoord, InvProjMatrix);
	vec3 albedo = texture(AlbedoTexture, TexCoord).rgb;
	vec3 normal = GetImplicitNormal(texture(NormalTexture, TexCoord).xy);
	vec4 others = texture(OthersTexture, TexCoord);

	// Compute view vector en view space
	vec3 viewDir = GetDirection(position, vec3(0));

	// Convert position, normal and view vector to world space
	position = (InvViewMatrix * vec4(position, 1)).xyz;
	normal = (InvViewMatrix * vec4(normal, 0)).xyz;
	viewDir = (InvViewMatrix * vec4(viewDir, 0)).xyz;

	// Set surface material data
	SurfaceData data;
	data.normal = normal;
	data.albedo = albedo;
	data.ambientOcclusion = others.x;
	data.roughness = others.y;
	data.metalness = others.z;
	data.shadowColor = SpecularColor; //vec3(0.8, 0.4, 0.2); 

	vec3 lighting = ComputeLighting(position, data, viewDir, true);

	// Add dust fade to final color after lighting
	// Camera Clipping planes
	float near = 0.1;
	float far  = CameraFarPlane; 

	// get depth of fragment
	float depth = texture(DepthTexture, TexCoord).r;;
	float ndc = depth * 2.0 - 1.0; 
	float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));

	// Since the camera moves, we also have to "push forward" all distances values a corresponding amount
	linearDepth = max(0, linearDepth - CameraCarDistance);

	// Now that we have the linear depth, we can then transform it to a gradual easing
	linearDepth /= 10;
	float quadraticDepth = linearDepth * linearDepth * linearDepth;


	// We want the fog to be a bit more round, so we increase the strength of the fog around the edges of the screen.
	// Since the UV matches the screen quad, we can just use that as our position
	float distFromCenter = length(TexCoord-vec2(0.5f, 0.5f));
	float edgeStrength = cos(distFromCenter * 3.14 + 3.14)/1.2 + 1;

	// We also don't want it to completely block out the furthest edges, so we shift it a tiny bit down
	float fogBlend = clamp(quadraticDepth * FogStrength * edgeStrength - FogDistance, 0, 1);

	vec3 fadedLight = mix(lighting, FogColor, fogBlend);

	FragColor = vec4(fadedLight, 1.0f);
}
