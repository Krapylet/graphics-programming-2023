//Inputs
//Corresponds directly to the outputs of the previous parts of the fragmentShaderPaths/VertexShaderPaths
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
	FragAlbedo = vec4(Color, 1);

	FragNormal = vec2(0.5f,0.5f);

	FragOthers = vec4(1,1,1,1);
}