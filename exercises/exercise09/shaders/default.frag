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
	FragOthers = vec4(1, 1, 0, 0); //texture(SpecularTexture, TexCoord);
}
