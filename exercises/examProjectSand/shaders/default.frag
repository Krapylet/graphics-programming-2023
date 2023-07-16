//Inputs
in vec3 ViewNormal;
in vec3 ViewTangent;
in vec3 ViewBitangent;
in vec2 TexCoord;

//Outputs
out vec4 FragAlbedo;
out vec2 FragNormal;
out vec4 FragOthers;

//Uniforms
uniform vec3 Color;
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;
uniform sampler2D SpecularTexture;

void main()
{
	FragAlbedo = vec4(Color.rgb * texture(ColorTexture, TexCoord).rgb, 1);

	vec3 viewNormal = SampleNormalMap(NormalTexture, TexCoord, normalize(ViewNormal), normalize(ViewTangent), normalize(ViewBitangent));
	FragNormal = viewNormal.xy;

	// hardcode metal metalness etc. since we don't have a texture for that.
	// ambientOcclusion, metalness, roughness, 
	FragOthers = vec4(0, 0.8f, 0.3f, 0);
}
