#version 450                          
layout (location = 0) in vec3 vPos;  
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec3 vTangent;

uniform mat4 _Model;
uniform mat4 _View;
uniform mat4 _Projection;

/*
* Outputs vertex data relative to
* the world space as opposed to
* model space.
*/
out struct Vertex
{
    vec3 worldNormal;
    vec3 worldPosition;
    vec2 uv; // world?
}vertexOutput;

out mat3 TBN;

void main(){    

    /* 
    * Converts the vertex position to world space by multiplying
    * it by the model matrix (because the model matrix represents
    * the model's positioning, scale, and rotation relative to the
    * world space?)
    */
    vertexOutput.worldPosition = vec3(_Model * vec4(vPos, 1.0f));

    /*
    * Converts the vertex normal to world space. To account for
    * non-uniform scaling induced by the model matrix's scale and
    * rotation, the inverse transpose of the model matrix to used
    * to get a transformed normal that is still perpendicular to
    * the surface of the vertex(?)
    */
    vertexOutput.worldNormal = transpose(inverse(mat3(_Model))) * vNormal;

    vertexOutput.uv = vUV;

    // Calculate the TBN 
    vec3 t = normalize(transpose(inverse(mat3(_Model))) * vTangent);
    vec3 n = normalize(transpose(inverse(mat3(_Model))) * vNormal);
    vec3 b = normalize(cross(t, n));

    TBN = mat3(t, b, n);

    gl_Position = _Projection * _View * _Model * vec4(vPos,1);
}
