//Inputs
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec2 TexCoord;

//Outputs
layout (location = 0) out vec4 FragAlbedo;
layout (location = 1) out vec2 FragNormal;
layout (location = 2) out vec4 FragOthers;

//Uniforms
uniform vec3 Color;
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;

void main()
{
	FragAlbedo = vec4(Color.rgb * texture(ColorTexture, TexCoord).rgb, 1);

	vec3 viewNormal = SampleNormalMap(NormalTexture, TexCoord, normalize(ViewNormal), normalize(ViewTangent), normalize(ViewBitangent));
	FragNormal = ViewNormal.xy;

	//FragAlbedo = vec4(ViewNormal.xy, 0,1);

	// Not all models have every value specified, so we assign some fallback values here in case.
	// ambientOcclusion, metalness, roughness, unused,
	vec4 textureSample = texture(SpecularTexture, TexCoord);
	float ambientOcclusion = step(0.000001f, textureSample.x) * textureSample.x + (1 - step(0.001f, textureSample.x)) * 1;  // Default to 1
	float metalness = step(0.000001f, textureSample.y) * textureSample.y + (1 - step(0.001f, textureSample.y)) * 0.35;      // Default to 0.35
	float roughness = step(0.000001f, textureSample.x) * textureSample.y;                                                   // Default to 0
	float unused = 0;

	FragOthers = vec4(ambientOcclusion, metalness, roughness, unused);
}
