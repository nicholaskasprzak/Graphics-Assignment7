#version 450                          
layout (location = 0) in vec3 vPos;  
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec3 vTangent;

uniform mat4 _Model;
uniform mat4 _View;
uniform mat4 _Projection;

uniform mat4 _LightViewProj;

out struct Vertex
{
    vec3 worldNormal;
    vec3 worldPosition;
    vec2 uv;
}vertexOutput;

out mat3 TBN;
out vec4 lightSpacePos;

void main(){    

    vertexOutput.worldPosition = vec3(_Model * vec4(vPos, 1.0f));
    vertexOutput.worldNormal = transpose(inverse(mat3(_Model))) * vNormal;
    vertexOutput.uv = vUV;

    vec3 t = normalize(transpose(inverse(mat3(_Model))) * vTangent);
    vec3 n = normalize(transpose(inverse(mat3(_Model))) * vNormal);
    vec3 b = normalize(cross(t, n));
    TBN = mat3(t, b, n);

    lightSpacePos = _LightViewProj * _Model * vec4(vPos, 1);
    gl_Position = _Projection * _View * _Model * vec4(vPos,1);
}
