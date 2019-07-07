attribute vec2 vTexCoord;
attribute vec3 vNormal;
attribute vec3 vPosition;

varying vec2 texCoord;
varying vec3 N;
varying vec3 pos;
varying vec3 Lvec1, Lvec2;

uniform mat4 ModelView;
uniform mat4 Projection;
uniform vec4 LightPosition1, LightPosition2;

attribute vec4 boneIDs;
attribute vec4 boneWeights;
uniform mat4 boneTransforms[64];

void main()
{
    // [PART 2][Preparation]
    mat4 uBoneTransform = boneWeights[0] * boneTransforms[int(boneIDs[0])] +
                          boneWeights[1] * boneTransforms[int(boneIDs[1])] +
                          boneWeights[2] * boneTransforms[int(boneIDs[2])] +
                          boneWeights[3] * boneTransforms[int(boneIDs[3])];
    // [END]

    // [PART 2][D]
    // Animation of bones
    vec4 vpos = uBoneTransform * vec4(vPosition, 1.0);
    vec4 vnorm = uBoneTransform * vec4(vNormal, 0.0);
    // [END]

    // [PART 1][G]
    // Transformed the position
    pos = (ModelView * vpos).xyz;
    // [END]

    // [PART 1][F]
    // Vector from the vertex to the light
    Lvec1 = LightPosition1.xyz - pos;
    Lvec2 = LightPosition2.xyz;
    // [END]

    N = normalize((ModelView * vnorm).xyz);

    gl_Position = Projection * ModelView * vpos;
    texCoord = vTexCoord;
}