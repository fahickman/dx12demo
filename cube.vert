/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

static const float4 boxVerts[8] = {
   { -1.0, -1.0, -1.0,  1.0 },
   {  1.0, -1.0, -1.0,  1.0 },
   { -1.0,  1.0, -1.0,  1.0 },
   {  1.0,  1.0, -1.0,  1.0 },
   { -1.0, -1.0,  1.0,  1.0 },
   {  1.0, -1.0,  1.0,  1.0 },
   { -1.0,  1.0,  1.0,  1.0 },
   {  1.0,  1.0,  1.0,  1.0 },
};

static const float4 boxColors[8] = {
   { 0.0, 0.0, 0.0, 1.0 },
   { 1.0, 0.0, 0.0, 1.0 },
   { 0.0, 1.0, 0.0, 1.0 },
   { 1.0, 1.0, 0.0, 1.0 },
   { 0.0, 0.0, 1.0, 1.0 },
   { 1.0, 0.0, 1.0, 1.0 },
   { 0.0, 1.0, 1.0, 1.0 },
   { 1.0, 1.0, 1.0, 1.0 },
};

static const uint boxIndices[36] = {
   1, 0, 2,
   1, 2, 3,
   5, 1, 3,
   5, 3, 7,
   4, 5, 7,
   4, 7, 6,
   0, 4, 6,
   0, 6, 2,
   6, 7, 3,
   6, 3, 2,
   1, 4, 0,
   1, 5, 4,
};

cbuffer cb0 : register(b0)  {
   float4x4 clipFromLocal;
};

struct VsInput {
   uint vertexIndex : SV_VERTEXID;
};

struct VsOutput {
   float4 position : SV_POSITION;
   float4 color : COLOR0;
};

VsOutput main(VsInput input)
{
   VsOutput output;

   uint index = boxIndices[input.vertexIndex];
   float4 localPos = boxVerts[index];
   output.color = boxColors[index];
   output.position = mul(clipFromLocal, localPos);

   return output;
}
