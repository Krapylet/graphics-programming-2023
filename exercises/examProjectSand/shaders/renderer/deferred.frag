//Inputs
in vec2 TexCoord;

//Outputs
out vec4 FragColor;

//Uniforms
uniform sampler2D DepthTexture;
uniform sampler2D AlbedoTexture;
uniform sampler2D NormalTexture;
uniform sampler2D OthersTexture;
uniform mat4 InvViewMatrix;
uniform mat4 InvProjMatrix;

uniform vec3 FadeColor;
uniform float EnableFog;

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

	// Compute lighting
	vec3 lighting = ComputeLighting(position, data, viewDir, true);

	// Add dust fade to final color after lighting
	// Camera Clipping planes
	float near = 0.1;
	float far  = 100.0; 

	// nonlinear depth
	float depth = texture(DepthTexture, TexCoord).r;;
	float ndc = depth * 2.0 - 1.0; 
	float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));
	// Now that we have the linear depth, we can then transform it to a gradual easing
	float powerDepth = linearDepth * linearDepth;

	// We also don't want it to completely block out the furthest edges, so we shift it a tiny bit down
	powerDepth -= 0.1f;
	powerDepth = max(0, powerDepth);

	
	// Add uniform bool/int multiplied to powerDepth to enable/disable fog during playtest.
	vec3 fadedLight = mix(lighting, FadeColor, powerDepth * EnableFog);

	FragColor = vec4(fadedLight, 1.0f);
}
