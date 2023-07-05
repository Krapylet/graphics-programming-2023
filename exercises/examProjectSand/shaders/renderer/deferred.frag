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
	float depth = gl_FragCoord.z;
	float ndc = depth * 2.0 - 1.0; 
	float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));	

	
	vec3 fadedLight = mix(lighting, FadeColor, linearDepth);
	FragColor = vec4(fadedLight, 1.0f);
}
