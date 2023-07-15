float Ease(float value)
{
	// sine easing curve. Experiment with more interesting distributions.
	//float easedOffset = (cos(3.14f * value) - 1) / 2;
	return value;
}

float GetHeightFromSample(vec2 pos, sampler2D depthMap, float sampleDistance, float offsetStrength)
{
	// Sample depth texture
	// We take a couple of extra samples in each direction to smooth out any extreme pixels.
	// Since the depth map is black and white, we only need to look at the red value.
	float depthSampleCenter = texture(depthMap, pos).r;
	
	float depthSampleNorth = texture(depthMap, pos + vec2(1,0) * sampleDistance).r;
	float depthSampleSouth = texture(depthMap, pos + vec2(-1,0) * sampleDistance).r;
	float depthSampleEast = texture(depthMap, pos + vec2(0,1) * sampleDistance).r;
	float depthSampleWest = texture(depthMap, pos + vec2(0,-1) * sampleDistance).r;
	
	float depthSample = (depthSampleCenter + depthSampleNorth + depthSampleSouth + depthSampleEast + depthSampleWest)/5;

	// We also invert so that darker values are higher.
	float vertexOffsetIntensity = 1 - depthSample * offsetStrength;
	
	// apply easing to create an intesting distribution.
	float easedOffset = Ease(vertexOffsetIntensity);

	// Apply offset on y axis. We don't need a transformation vertex since tangent space and is aligned with every single vertex in the plane.
	return easedOffset;
}

void GetTangetnSpaceVectorsFromSample(vec2 uv, sampler2D depthMap, float sampleDistance, float offsetStrength, out vec3 tangent, out vec3 bitangent, out vec3 normal){
	// "Height" should be stored in z.

	// Sample depth texture to calculate the normal
	// use min to avoid going out of bounds
	vec2 northUV = min(uv + vec2(0,1) * sampleDistance, vec2(1,1));
	vec2 southUV = max(uv + vec2(0,-1) * sampleDistance, vec2(0,0));
	vec2 eastUV = min(uv + vec2(1,0) * sampleDistance, vec2(1,1));
	vec2 westUV = max(uv + vec2(-1,0) * sampleDistance, vec2(0,0));

	// Since the depth map is black and white, we only need to look at the red value.
	float depthSampleNorth = Ease(texture(depthMap, northUV).r);
	float depthSampleSouth = Ease(texture(depthMap, southUV).r);
	float depthSampleEast = Ease(texture(depthMap, eastUV).r);
	float depthSampleWest = Ease(texture(depthMap, westUV).r);

	// change in height across samples pr. unit.
	float deltaX = (depthSampleEast - depthSampleWest)/(eastUV.x - westUV.x);
	float deltaY = (depthSampleNorth - depthSampleSouth)/(northUV.y - southUV.y);
	
	// this looks more correct than having the constant in z, probably because we're workign in worldspace where y is "up" instread of how z usually is in tangent space.
	// We're setting the normal length to higher than 1 because the normals generated tend to be extremely tilted on the height map
	// compared to the weight we're giving each vertex offset. 8 seemed like a good contant value.

	normal = normalize(vec3(deltaX, 1 + offsetStrength, deltaY));

	// we can also use one of the direction pairs to create a tangent
	tangent = normalize(vec3(northUV, depthSampleNorth) - vec3(southUV, depthSampleNorth));

	// And this tangent can be used to compute the bitangent
	bitangent = normalize(cross(tangent, normal));
}