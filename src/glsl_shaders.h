/*********************************************************************
*
*	file: glsl_shaders.h
*	note: string literals auto-generated by embed_text.py
*	date: 16/022023
*
**********************************************************************/
#ifndef __GLSL_SHADERS_H__
#define __GLSL_SHADERS_H__


//NOTE: string imported from src\glsl_shaders\common.glsl
const char* glsl_common = 
"\n"
"layout(std430) buffer;\n"
"\n"
"struct vertex {\n"
"	vec4 cubic;\n"
"	vec2 pos;\n"
"	int shapeIndex;\n"
"};\n"
"\n"
"struct shape {\n"
"	vec4 color;\n"
"	vec4 clip;\n"
"	vec2 uv;\n"
"};\n";

//NOTE: string imported from src\glsl_shaders\blit_vertex.glsl
const char* glsl_blit_vertex = 
"\n"
"precision mediump float;\n"
"\n"
"out vec2 uv;\n"
"\n"
"void main()\n"
"{\n"
"    float x = float(((uint(gl_VertexID) + 2u) / 3u)%2u);\n"
"    float y = float(((uint(gl_VertexID) + 1u) / 3u)%2u);\n"
"\n"
"    gl_Position = vec4(-1.0f + x*2.0f, -1.0f+y*2.0f, 0.0f, 1.0f);\n"
"    uv = vec2(x, y);\n"
"}\n";

//NOTE: string imported from src\glsl_shaders\blit_fragment.glsl
const char* glsl_blit_fragment = 
"\n"
"precision mediump float;\n"
"\n"
"in vec2 uv;\n"
"out vec4 fragColor;\n"
"\n"
"layout(location=0) uniform sampler2D tex;\n"
"\n"
"void main()\n"
"{\n"
"	fragColor = texture(tex, uv);\n"
"}\n";

//NOTE: string imported from src\glsl_shaders\clear_counters.glsl
const char* glsl_clear_counters = 
"\n"
"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
"\n"
"precision mediump float;\n"
"layout(std430) buffer;\n"
"\n"
"layout(binding = 0) coherent restrict writeonly buffer tileCounterBufferSSBO {\n"
"	uint elements[];\n"
"} tileCounterBuffer ;\n"
"\n"
"void main()\n"
"{\n"
"	uint tileIndex = gl_WorkGroupID.x;\n"
"	tileCounterBuffer.elements[tileIndex] = 0u;\n"
"}\n";

//NOTE: string imported from src\glsl_shaders\tile.glsl
const char* glsl_tile = 
"\n"
"layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;\n"
"\n"
"precision mediump float;\n"
"\n"
"layout(binding = 0) restrict readonly buffer vertexBufferSSBO {\n"
"	vertex elements[];\n"
"} vertexBuffer ;\n"
"\n"
"layout(binding = 1) restrict readonly buffer shapeBufferSSBO {\n"
"	shape elements[];\n"
"} shapeBuffer ;\n"
"\n"
"layout(binding = 2) restrict readonly buffer indexBufferSSBO {\n"
"	uint elements[];\n"
"} indexBuffer ;\n"
"\n"
"layout(binding = 3) coherent restrict buffer tileCounterBufferSSBO {\n"
"	uint elements[];\n"
"} tileCounterBuffer ;\n"
"\n"
"layout(binding = 4) coherent restrict writeonly buffer tileArrayBufferSSBO {\n"
"	uint elements[];\n"
"} tileArrayBuffer ;\n"
"\n"
"layout(location = 0) uniform uint indexCount;\n"
"layout(location = 1) uniform uvec2 tileCount;\n"
"layout(location = 2) uniform uint tileSize;\n"
"layout(location = 3) uniform uint tileArraySize;\n"
"layout(location = 4) uniform vec2 scaling;\n"
"\n"
"void main()\n"
"{\n"
"	uint triangleIndex = (gl_WorkGroupID.x*gl_WorkGroupSize.x + gl_LocalInvocationIndex) * 3u;\n"
"	if(triangleIndex >= indexCount)\n"
"	{\n"
"		return;\n"
"	}\n"
"\n"
"	uint i0 = indexBuffer.elements[triangleIndex];\n"
"	uint i1 = indexBuffer.elements[triangleIndex+1u];\n"
"	uint i2 = indexBuffer.elements[triangleIndex+2u];\n"
"\n"
"	vec2 p0 = vertexBuffer.elements[i0].pos * scaling;\n"
"	vec2 p1 = vertexBuffer.elements[i1].pos * scaling;\n"
"	vec2 p2 = vertexBuffer.elements[i2].pos * scaling;\n"
"\n"
"	int shapeIndex = vertexBuffer.elements[i0].shapeIndex;\n"
"	vec4 clip = shapeBuffer.elements[shapeIndex].clip * vec4(scaling, scaling);\n"
"\n"
"	vec4 fbox = vec4(max(min(min(p0.x, p1.x), p2.x), clip.x),\n"
"		             max(min(min(p0.y, p1.y), p2.y), clip.y),\n"
"		             min(max(max(p0.x, p1.x), p2.x), clip.z),\n"
"		             min(max(max(p0.y, p1.y), p2.y), clip.w));\n"
"\n"
"	ivec4 box = ivec4(floor(fbox))/int(tileSize);\n"
"\n"
"	//NOTE(martin): it's importat to do the computation with signed int, so that we can have negative xMax/yMax\n"
"	//              otherwise all triangles on the left or below the x/y axis are attributed to tiles on row/column 0.\n"
"	int xMin = max(0, box.x);\n"
"	int yMin = max(0, box.y);\n"
"	int xMax = min(box.z, int(tileCount.x) - 1);\n"
"	int yMax = min(box.w, int(tileCount.y) - 1);\n"
"\n"
"	for(int y = yMin; y <= yMax; y++)\n"
"	{\n"
"		for(int x = xMin ; x <= xMax; x++)\n"
"		{\n"
"			uint tileIndex = uint(y)*tileCount.x + uint(x);\n"
"			uint tileCounter = atomicAdd(tileCounterBuffer.elements[tileIndex], 1u);\n"
"			if(tileCounter < tileArraySize)\n"
"			{\n"
"				tileArrayBuffer.elements[tileArraySize*tileIndex + tileCounter] = triangleIndex;\n"
"			}\n"
"		}\n"
"	}\n"
"}\n";

//NOTE: string imported from src\glsl_shaders\sort.glsl
const char* glsl_sort = 
"\n"
"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
"\n"
"precision mediump float;\n"
"\n"
"layout(binding = 0) restrict readonly buffer vertexBufferSSBO {\n"
"	vertex elements[];\n"
"} vertexBuffer ;\n"
"\n"
"layout(binding = 1) restrict readonly buffer shapeBufferSSBO {\n"
"	shape elements[];\n"
"} shapeBuffer ;\n"
"\n"
"layout(binding = 2) restrict readonly buffer indexBufferSSBO {\n"
"	uint elements[];\n"
"} indexBuffer ;\n"
"\n"
"layout(binding = 3) coherent readonly restrict buffer tileCounterBufferSSBO {\n"
"	uint elements[];\n"
"} tileCounterBuffer ;\n"
"\n"
"layout(binding = 4) coherent restrict buffer tileArrayBufferSSBO {\n"
"	uint elements[];\n"
"} tileArrayBuffer ;\n"
"\n"
"layout(location = 0) uniform uint indexCount;\n"
"layout(location = 1) uniform uvec2 tileCount;\n"
"layout(location = 2) uniform uint tileSize;\n"
"layout(location = 3) uniform uint tileArraySize;\n"
"\n"
"int get_shape_index(uint tileArrayOffset, uint tileArrayIndex)\n"
"{\n"
"	uint triangleIndex = tileArrayBuffer.elements[tileArrayOffset + tileArrayIndex];\n"
"	uint i0 = indexBuffer.elements[triangleIndex];\n"
"	int shapeIndex = vertexBuffer.elements[i0].shapeIndex;\n"
"	return(shapeIndex);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	uint tileIndex = gl_WorkGroupID.x;\n"
"	uint tileArrayOffset = tileArraySize * tileIndex;\n"
"	uint tileArrayCount = min(tileCounterBuffer.elements[tileIndex], tileArraySize);\n"
"\n"
"	for(uint tileArrayIndex=1u; tileArrayIndex < tileArrayCount; tileArrayIndex++)\n"
"	{\n"
"		for(uint sortIndex = tileArrayIndex; sortIndex > 0u; sortIndex--)\n"
"		{\n"
"			int shapeIndex = get_shape_index(tileArrayOffset, sortIndex);\n"
"			int prevShapeIndex = get_shape_index(tileArrayOffset, sortIndex-1u);\n"
"\n"
"			if(shapeIndex >= prevShapeIndex)\n"
"			{\n"
"				break;\n"
"			}\n"
"			uint tmp = tileArrayBuffer.elements[tileArrayOffset + sortIndex];\n"
"			tileArrayBuffer.elements[tileArrayOffset + sortIndex] = tileArrayBuffer.elements[tileArrayOffset + sortIndex - 1u];\n"
"			tileArrayBuffer.elements[tileArrayOffset + sortIndex - 1u] = tmp;\n"
"		}\n"
"	}\n"
"}\n";

//NOTE: string imported from src\glsl_shaders\draw.glsl
const char* glsl_draw = 
"\n"
"#extension GL_ARB_gpu_shader_int64 : require\n"
"layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
"\n"
"precision mediump float;\n"
"//precision mediump image2D;\n"
"\n"
"layout(binding = 0) restrict readonly buffer vertexBufferSSBO {\n"
"	vertex elements[];\n"
"} vertexBuffer ;\n"
"\n"
"layout(binding = 1) restrict readonly buffer shapeBufferSSBO {\n"
"	shape elements[];\n"
"} shapeBuffer ;\n"
"\n"
"layout(binding = 2) restrict readonly buffer indexBufferSSBO {\n"
"	uint elements[];\n"
"} indexBuffer ;\n"
"\n"
"layout(binding = 3) restrict readonly buffer tileCounterBufferSSBO {\n"
"	uint elements[];\n"
"} tileCounterBuffer ;\n"
"\n"
"layout(binding = 4) restrict readonly buffer tileArrayBufferSSBO {\n"
"	uint elements[];\n"
"} tileArrayBuffer ;\n"
"\n"
"layout(location = 0) uniform uint indexCount;\n"
"layout(location = 1) uniform uvec2 tileCount;\n"
"layout(location = 2) uniform uint tileSize;\n"
"layout(location = 3) uniform uint tileArraySize;\n"
"layout(location = 4) uniform vec2 scaling;\n"
"\n"
"layout(rgba8, binding = 0) uniform restrict writeonly image2D outTexture;\n"
"\n"
"bool is_top_left(ivec2 a, ivec2 b)\n"
"{\n"
"	return( (a.y == b.y && b.x < a.x)\n"
"	      ||(b.y < a.y));\n"
"}\n"
"\n"
"//////////////////////////////////////////////////////////////////////////////\n"
"//TODO: we should do these computations on 64bits, because otherwise\n"
"//      we might overflow for values > 2048.\n"
"//		Unfortunately this is costly.\n"
"//	    Another way is to precompute triangle edges (b - a) in full precision\n"
"//      once to avoid doing it all the time...\n"
"//////////////////////////////////////////////////////////////////////////////\n"
"int orient2d(ivec2 a, ivec2 b, ivec2 p)\n"
"{\n"
"	return((b.x-a.x)*(p.y-a.y) - (b.y-a.y)*(p.x-a.x));\n"
"}\n"
"\n"
"int is_clockwise(ivec2 p0, ivec2 p1, ivec2 p2)\n"
"{\n"
"	return((p1 - p0).x*(p2 - p0).y - (p1 - p0).y*(p2 - p0).x);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	ivec2 pixelCoord = ivec2(gl_WorkGroupID.xy*uvec2(16, 16) + gl_LocalInvocationID.xy);\n"
"	uvec2 tileCoord = uvec2(pixelCoord) / tileSize;\n"
"	uint tileIndex =  tileCoord.y * tileCount.x + tileCoord.x;\n"
"	uint tileCounter = min(tileCounterBuffer.elements[tileIndex], tileArraySize);\n"
"\n"
"	const float subPixelFactor = 16.;\n"
"	ivec2 centerPoint = ivec2((vec2(pixelCoord) + vec2(0.5, 0.5)) * subPixelFactor);\n"
"\n"
"//*\n"
"	const int sampleCount = 8;\n"
"	ivec2 samplePoints[sampleCount] = ivec2[sampleCount](centerPoint + ivec2(1, 3),\n"
"	                                                     centerPoint + ivec2(-1, -3),\n"
"	                                                     centerPoint + ivec2(5, -1),\n"
"	                                                     centerPoint + ivec2(-3, 5),\n"
"	                                                     centerPoint + ivec2(-5, -5),\n"
"	                                                     centerPoint + ivec2(-7, 1),\n"
"	                                                     centerPoint + ivec2(3, -7),\n"
"	                                                     centerPoint + ivec2(7, 7));\n"
"/*/\n"
"	const int sampleCount = 4;\n"
"	ivec2 samplePoints[sampleCount] = ivec2[sampleCount](centerPoint + ivec2(-2, 6),\n"
"	                                                     centerPoint + ivec2(6, 2),\n"
"	                                                     centerPoint + ivec2(-6, -2),\n"
"	                                                     centerPoint + ivec2(2, -6));\n"
"//*/\n"
"	//DEBUG\n"
"/*\n"
"	{\n"
"		vec4 fragColor = vec4(0);\n"
"\n"
"		if( pixelCoord.x % 16 == 0\n"
"	  	  ||pixelCoord.y % 16 == 0)\n"
"		{\n"
"			fragColor = vec4(0, 0, 0, 1);\n"
"		}\n"
"		else if(tileCounterBuffer.elements[tileIndex] == 0xffffu)\n"
"		{\n"
"			fragColor = vec4(1, 0, 1, 1);\n"
"		}\n"
"		else if(tileCounter != 0u)\n"
"		{\n"
"			fragColor = vec4(0, 1, 0, 1);\n"
"		}\n"
"		else\n"
"		{\n"
"			fragColor = vec4(1, 0, 0, 1);\n"
"		}\n"
"		imageStore(outTexture, pixelCoord, fragColor);\n"
"		return;\n"
"	}\n"
"//*/\n"
"	//----\n"
"\n"
"	vec4 sampleColor[sampleCount];\n"
"	vec4 currentColor[sampleCount];\n"
"    int currentShapeIndex[sampleCount];\n"
"    int flipCount[sampleCount];\n"
"\n"
"    for(int i=0; i<sampleCount; i++)\n"
"    {\n"
"		currentShapeIndex[i] = -1;\n"
"		flipCount[i] = 0;\n"
"		sampleColor[i] = vec4(0, 0, 0, 0);\n"
"		currentColor[i] = vec4(0, 0, 0, 0);\n"
"    }\n"
"\n"
"    for(uint tileArrayIndex=0u; tileArrayIndex < tileCounter; tileArrayIndex++)\n"
"    {\n"
"    	uint triangleIndex = tileArrayBuffer.elements[tileArraySize * tileIndex + tileArrayIndex];\n"
"\n"
"		uint i0 = indexBuffer.elements[triangleIndex];\n"
"		uint i1 = indexBuffer.elements[triangleIndex+1u];\n"
"		uint i2 = indexBuffer.elements[triangleIndex+2u];\n"
"\n"
"		ivec2 p0 = ivec2((vertexBuffer.elements[i0].pos * scaling) * subPixelFactor);\n"
"		ivec2 p1 = ivec2((vertexBuffer.elements[i1].pos * scaling) * subPixelFactor);\n"
"		ivec2 p2 = ivec2((vertexBuffer.elements[i2].pos * scaling) * subPixelFactor);\n"
"\n"
"		int shapeIndex = vertexBuffer.elements[i0].shapeIndex;\n"
"		vec4 color = shapeBuffer.elements[shapeIndex].color;\n"
"		ivec4 clip = ivec4(round((shapeBuffer.elements[shapeIndex].clip * vec4(scaling, scaling) + vec4(0.5, 0.5, 0.5, 0.5)) * subPixelFactor));\n"
"\n"
"		//NOTE(martin): reorder triangle counter-clockwise and compute bias for each edge\n"
"		int cw = is_clockwise(p0, p1, p2);\n"
"		if(cw < 0)\n"
"		{\n"
"			uint tmpIndex = i1;\n"
"			i1 = i2;\n"
"			i2 = tmpIndex;\n"
"\n"
"			ivec2 tmpPoint = p1;\n"
"			p1 = p2;\n"
"			p2 = tmpPoint;\n"
"		}\n"
"\n"
"		vec4 cubic0 = vertexBuffer.elements[i0].cubic;\n"
"		vec4 cubic1 = vertexBuffer.elements[i1].cubic;\n"
"		vec4 cubic2 = vertexBuffer.elements[i2].cubic;\n"
"\n"
"		int bias0 = is_top_left(p1, p2) ? 0 : -1;\n"
"		int bias1 = is_top_left(p2, p0) ? 0 : -1;\n"
"		int bias2 = is_top_left(p0, p1) ? 0 : -1;\n"
"\n"
"		for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)\n"
"		{\n"
"			ivec2 samplePoint = samplePoints[sampleIndex];\n"
"\n"
"			if(  samplePoint.x < clip.x\n"
"			  || samplePoint.x > clip.z\n"
"			  || samplePoint.y < clip.y\n"
"			  || samplePoint.y > clip.w)\n"
"			{\n"
"				continue;\n"
"			}\n"
"\n"
"			int w0 = orient2d(p1, p2, samplePoint);\n"
"			int w1 = orient2d(p2, p0, samplePoint);\n"
"			int w2 = orient2d(p0, p1, samplePoint);\n"
"\n"
"			if((w0+bias0) >= 0 && (w1+bias1) >= 0 && (w2+bias2) >= 0)\n"
"			{\n"
"				vec4 cubic = (cubic0*float(w0) + cubic1*float(w1) + cubic2*float(w2))/(float(w0)+float(w1)+float(w2));\n"
"\n"
"				float eps = 0.0001;\n"
"				if(cubic.w*(cubic.x*cubic.x*cubic.x - cubic.y*cubic.z) <= eps)\n"
"				{\n"
"					if(shapeIndex == currentShapeIndex[sampleIndex])\n"
"					{\n"
"						flipCount[sampleIndex]++;\n"
"					}\n"
"					else\n"
"					{\n"
"						if((flipCount[sampleIndex] & 0x01) != 0)\n"
"						{\n"
"							sampleColor[sampleIndex] = currentColor[sampleIndex];\n"
"						}\n"
"						currentColor[sampleIndex] = sampleColor[sampleIndex]*(1.-color.a) + color.a*color;\n"
"						currentShapeIndex[sampleIndex] = shapeIndex;\n"
"						flipCount[sampleIndex] = 1;\n"
"					}\n"
"				}\n"
"			}\n"
"		}\n"
"    }\n"
"    vec4 pixelColor = vec4(0);\n"
"    for(int sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)\n"
"    {\n"
"    	if((flipCount[sampleIndex] & 0x01) != 0)\n"
"    	{\n"
"			sampleColor[sampleIndex] = currentColor[sampleIndex];\n"
"    	}\n"
"    	pixelColor += sampleColor[sampleIndex];\n"
"	}\n"
"\n"
"	imageStore(outTexture, pixelCoord, pixelColor/float(sampleCount));\n"
"}\n";

#endif // __GLSL_SHADERS_H__
