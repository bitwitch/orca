
#include<metal_stdlib>
#include<simd/simd.h>
#include<metal_simdgroup>

#include"mtl_shader.h"

using namespace metal;

struct vs_out
{
    float4 pos [[position]];
    float2 uv;
};

vertex vs_out VertexShader(ushort vid [[vertex_id]])
{
	vs_out out;
	out.uv = float2((vid << 1) & 2, vid & 2);
	out.pos = float4(out.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return(out);
}

fragment float4 FragmentShader(vs_out i [[stage_in]], texture2d<float> tex [[texture(0)]])
{
	constexpr sampler smp(mip_filter::nearest, mag_filter::linear, min_filter::linear);
	return(tex.sample(smp, i.uv));
}

bool is_top_left(float2 a, float2 b)
{
	return( (a.y == b.y && b.x < a.x)
	      ||(b.y < a.y));
}

//////////////////////////////////////////////////////////////////////////////
//TODO: we should do these computations on 64bits, because otherwise
//      we might overflow for values > 2048.
//		Unfortunately this is costly.
//	    Another way is to precompute triangle edges (b - a) in full precision
//      once to avoid doing it all the time...
//////////////////////////////////////////////////////////////////////////////

int orient2d(int2 a, int2 b, int2 c)
{
	return((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x));
}

kernel void TriangleKernel(constant mg_vertex* vertexBuffer [[buffer(0)]],
		                   constant uint* indexBuffer [[buffer(1)]],
		                   constant mg_shape* shapeBuffer [[buffer(2)]],
                           device mg_triangle_data* triangleArray [[buffer(3)]],
                           constant float* scaling [[buffer(4)]],
                           uint gid [[thread_position_in_grid]])
{
	uint triangleIndex = gid * 3;

	uint i0 = indexBuffer[triangleIndex];
	uint i1 = indexBuffer[triangleIndex+1];
	uint i2 = indexBuffer[triangleIndex+2];

	float2 p0 = vertexBuffer[i0].pos * scaling[0];
	float2 p1 = vertexBuffer[i1].pos * scaling[0];
	float2 p2 = vertexBuffer[i2].pos * scaling[0];

	int shapeIndex = vertexBuffer[i0].shapeIndex;

	//NOTE(martin): compute triangle bounding box and clip it
	float4 clip = shapeBuffer[shapeIndex].clip * scaling[0];
	float4 fbox = float4(min(min(p0, p1), p2), max(max(p0, p1), p2));
	fbox = float4(max(fbox.xy, clip.xy), min(fbox.zw, clip.zw));

	//NOTE(martin): fill triangle data
	const float subPixelFactor = 16;

	triangleArray[gid].box = int4(fbox * subPixelFactor);
	triangleArray[gid].shapeIndex = shapeIndex;

	triangleArray[gid].color = shapeBuffer[shapeIndex].color;

	constant float* uvTransform2x3 = shapeBuffer[shapeIndex].uvTransform;
	triangleArray[gid].uvTransform = (matrix_float3x3){{uvTransform2x3[0], uvTransform2x3[3], 0},
		                                               {uvTransform2x3[1], uvTransform2x3[4], 0},
		                                               {uvTransform2x3[2], uvTransform2x3[5], 1}};

	triangleArray[gid].cubic0 = vertexBuffer[i0].cubic;
	triangleArray[gid].cubic1 = vertexBuffer[i1].cubic;
	triangleArray[gid].cubic2 = vertexBuffer[i2].cubic;

	int2 ip0 = int2(p0 * subPixelFactor);
	int2 ip1 = int2(p1 * subPixelFactor);
	int2 ip2 = int2(p2 * subPixelFactor);

	triangleArray[gid].p0 = ip0;
	triangleArray[gid].p1 = ip1;
	triangleArray[gid].p2 = ip2;

	//NOTE(martin): compute triangle orientation and bias for each edge
	int cw = orient2d(ip0, ip1, ip2) > 0 ? 1 : -1;

	triangleArray[gid].cw = cw;
	triangleArray[gid].bias0 = is_top_left(p1, p2) ? -(1-cw)/2 : -(1+cw)/2;
	triangleArray[gid].bias1 = is_top_left(p2, p0) ? -(1-cw)/2 : -(1+cw)/2;
	triangleArray[gid].bias2 = is_top_left(p0, p1) ? -(1-cw)/2 : -(1+cw)/2;

	triangleArray[gid].tileBox = int4(fbox)/RENDERER_TILE_SIZE;
}

kernel void TileKernel(const device mg_triangle_data* triangleArray [[buffer(0)]],
                       device uint* tileCounters [[buffer(1)]],
                       device uint* tileArrayBuffer [[buffer(2)]],
                       constant int* triangleCount [[buffer(3)]],
                       constant uint2* viewport [[buffer(4)]],
                       constant float* scaling [[buffer(5)]],
                       uint3 gid [[thread_position_in_grid]])
{
	uint2 tilesMatrixDim = (*viewport - 1) / RENDERER_TILE_SIZE + 1;
	int nTilesX = tilesMatrixDim.x;

	int tileX = gid.x;
	int tileY = gid.y;
	int tileIndex = tileY * nTilesX + tileX;
	int groupIndex = gid.z;

	const int groupSize = 16;
	int count = 0;
	int mask = 0xffff>>(16-groupIndex);

	for(int triangleBatchIndex=0; triangleBatchIndex<triangleCount[0]; triangleBatchIndex += groupSize)
	{
		int triangleIndex = triangleBatchIndex + groupIndex;
		bool active = false;
//		if(triangleIndex + groupIndex < triangleCount[0])
		{
			int4 box = triangleArray[triangleIndex].tileBox;
/*
			if(  tileX >= box.x && tileX <= box.z
		  	&& tileY >= box.y && tileY <= box.w)
			{
				active = true;
			}
			*/
		}

		int vote = uint64_t(simd_ballot(active));
		if(active)
		{
			int batchOffset = popcount(vote & mask);
			tileArrayBuffer[tileIndex*RENDERER_TILE_BUFFER_SIZE + count + batchOffset] = triangleIndex;
		}
		count += popcount(vote);
	}
	if(groupIndex == 0)
	{
		tileCounters[tileIndex] = count;
	}
}

kernel void SortKernel(constant mg_triangle_data* triangleArray [[buffer(0)]],
                       const device uint* tileCounters [[buffer(1)]],
                       device uint* tileArrayBuffer [[buffer(2)]],
                       uint gid [[thread_position_in_grid]])
{
	uint tileIndex = gid;
	uint tileArrayOffset = tileIndex * RENDERER_TILE_BUFFER_SIZE;
	uint tileArrayCount = min(tileCounters[tileIndex], (uint)RENDERER_TILE_BUFFER_SIZE);

	for(uint tileArrayIndex=1; tileArrayIndex < tileArrayCount; tileArrayIndex++)
	{
		for(uint sortIndex = tileArrayIndex; sortIndex > 0; sortIndex--)
		{
			int shapeIndex = triangleArray[tileArrayBuffer[tileArrayOffset + sortIndex]].shapeIndex;
			int prevShapeIndex = triangleArray[tileArrayBuffer[tileArrayOffset + sortIndex - 1]].shapeIndex;

			if(shapeIndex >= prevShapeIndex)
			{
				break;
			}
			uint tmp = tileArrayBuffer[tileArrayOffset + sortIndex];
			tileArrayBuffer[tileArrayOffset + sortIndex] = tileArrayBuffer[tileArrayOffset + sortIndex - 1];
			tileArrayBuffer[tileArrayOffset + sortIndex - 1] = tmp;
		}
	}
}

kernel void RenderKernel(const device uint* tileCounters [[buffer(0)]],
                         const device uint* tileArrayBuffer [[buffer(1)]],
                         const device mg_triangle_data* triangleArray [[buffer(2)]],

                         constant int* useTexture [[buffer(3)]],
                         constant float* scaling [[buffer(4)]],

                         texture2d<float, access::write> outTexture [[texture(0)]],
                         texture2d<float> texAtlas [[texture(1)]],

                         uint2 gid [[thread_position_in_grid]],
                         uint2 tgid [[threadgroup_position_in_grid]],
                         uint2 threadsPerThreadgroup [[threads_per_threadgroup]],
                         uint2 gridSize [[threads_per_grid]])
{
	//TODO: guard against thread group size not equal to tile size?
	const int2 pixelCoord = int2(gid);
	const uint2 tileCoord = uint2(pixelCoord)/ RENDERER_TILE_SIZE;
	const uint2 tilesMatrixDim = (gridSize - 1) / RENDERER_TILE_SIZE + 1;
	const uint tileIndex = tileCoord.y * tilesMatrixDim.x + tileCoord.x;
	const uint tileCounter = min(tileCounters[tileIndex], (uint)RENDERER_TILE_BUFFER_SIZE);

#ifdef RENDERER_DEBUG_TILES
	//NOTE(martin): color code debug values and show the tile grid
	{
		float4 fragColor = float4(0);

		if( pixelCoord.x % 16 == 0
	  	  ||pixelCoord.y % 16 == 0)
		{
			fragColor = float4(0, 0, 0, 1);
		}
		else if(tileCounters[tileIndex] == 0xffffu)
		{
			fragColor = float4(1, 0, 1, 1);
		}
		else if(tileCounter != 0u)
		{
			fragColor = float4(0, 1, 0, 1);
		}
		else
		{
			fragColor = float4(1, 0, 0, 1);
		}
		outTexture.write(fragColor, gid);
		return;
	}
#endif

	const int subPixelFactor = 16;
	const int2 centerPoint = int2((float2(pixelCoord) + float2(0.5, 0.5)) * subPixelFactor);

	const int sampleCount = 8;
	int2 samplePoints[sampleCount] = {centerPoint + int2(1, 3),
	                                  centerPoint + int2(-1, -3),
	                                  centerPoint + int2(5, -1),
	                                  centerPoint + int2(-3, 5),
	                                  centerPoint + int2(-5, -5),
	                                  centerPoint + int2(-7, 1),
	                                  centerPoint + int2(3, -7),
	                                  centerPoint + int2(7, 7)};

	float4 sampleColor[sampleCount];
	float4 currentColor[sampleCount];
    int currentShapeIndex[sampleCount];
    int flipCount[sampleCount];

    for(int i=0; i<sampleCount; i++)
    {
		currentShapeIndex[i] = -1;
		flipCount[i] = 0;
		sampleColor[i] = float4(0, 0, 0, 0);
		currentColor[i] = float4(0, 0, 0, 0);
    }

    for(uint tileArrayIndex=0; tileArrayIndex < tileCounter; tileArrayIndex++)
    {
    	int triangleIndex = tileArrayBuffer[RENDERER_TILE_BUFFER_SIZE * tileIndex + tileArrayIndex];
		const device mg_triangle_data* triangle = &triangleArray[triangleIndex];

		int2 p0 = triangle->p0;
		int2 p1 = triangle->p1;
		int2 p2 = triangle->p2;

		int cw = triangle->cw;

		int bias0 = triangle->bias0;
		int bias1 = triangle->bias1;
		int bias2 = triangle->bias2;

		float4 cubic0 = triangle->cubic0;
		float4 cubic1 = triangle->cubic1;
		float4 cubic2 = triangle->cubic2;

		int shapeIndex = triangle->shapeIndex;
		float4 color = triangle->color;
		color.rgb *= color.a;

		int4 clip = triangle->box;

		matrix_float3x3 uvTransform = triangle->uvTransform;

		for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
		{
			int2 samplePoint = samplePoints[sampleIndex];

			if(  samplePoint.x < clip.x
			  || samplePoint.x > clip.z
			  || samplePoint.y < clip.y
			  || samplePoint.y > clip.w)
			{
				continue;
			}

			int w0 = cw*orient2d(p1, p2, samplePoint);
			int w1 = cw*orient2d(p2, p0, samplePoint);
			int w2 = cw*orient2d(p0, p1, samplePoint);

			if((w0+bias0) >= 0 && (w1+bias1) >= 0 && (w2+bias2) >= 0)
			{
				float4 cubic = (cubic0*w0 + cubic1*w1 + cubic2*w2)/(w0+w1+w2);

				float eps = 0.0001;
				if(cubic.w*(cubic.x*cubic.x*cubic.x - cubic.y*cubic.z) <= eps)
				{
					if(shapeIndex == currentShapeIndex[sampleIndex])
					{
						flipCount[sampleIndex]++;
					}
					else
					{
						if(flipCount[sampleIndex] & 0x01)
						{
							sampleColor[sampleIndex] = currentColor[sampleIndex];
						}

						float4 nextColor = color;

						if(useTexture[0])
						{
							float3 sampleFP = float3(float2(samplePoint).xy/(subPixelFactor*2.), 1);
							float2 uv = (uvTransform * sampleFP).xy;

							constexpr sampler smp(mip_filter::nearest, mag_filter::linear, min_filter::linear);
							float4 texColor = texAtlas.sample(smp, uv);

							texColor.rgb *= texColor.a;
							nextColor *= texColor;
						}

						currentColor[sampleIndex] = sampleColor[sampleIndex]*(1.-nextColor.a) + nextColor;
						currentShapeIndex[sampleIndex] = shapeIndex;
						flipCount[sampleIndex] = 1;
					}
				}
			}
		}
    }

    float4 pixelColor = float4(0);
    for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
    {
    	if(flipCount[sampleIndex] & 0x01)
    	{
			sampleColor[sampleIndex] = currentColor[sampleIndex];
    	}
    	pixelColor += sampleColor[sampleIndex];
	}

	outTexture.write(pixelColor/float(sampleCount), gid);
}
