//Inputs
layout (location = 0) in vec3 ViewNormal;
layout (location = 1) in vec3 ViewTangent;
layout (location = 2) in vec3 ViewBitangent;
layout (location = 3) in vec2 TexCoord;

//Outputs
out vec4 FragAlbedo;
out vec2 FragNormal;
out vec4 FragOthers;

//Uniforms
uniform vec3 Color;
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;
uniform float AmbientOcclusion;
uniform float Metalness;
uniform float Roughness;
uniform float Unused;

void main()
{
	FragAlbedo = vec4(Color.rgb * texture(ColorTexture, TexCoord).rgb, 1);

	vec3 viewNormal = SampleNormalMap(NormalTexture, TexCoord, normalize(ViewNormal), normalize(ViewTangent), normalize(ViewBitangent));
	FragNormal = ViewNormal.xy;

	//FragAlbedo = vec4(ViewNormal.xy, 0,1);

	// ambientOcclusion, metalness, roughness, unused,
	FragOthers = vec4(AmbientOcclusion, Metalness, Roughness, Unused); //texture(SpecularTexture, TexCoord);
}
