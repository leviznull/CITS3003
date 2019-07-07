varying vec3 pos;
varying vec3 Lvec1, Lvec2;
varying vec3 N;

varying vec2 texCoord; // The third coordinate is always 0.0 and is discarded
vec4 color;

uniform float LightBrightness1, LightBrightness2;
uniform vec4 LightPosition1, LightPosition2;
uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform vec3 LightColor1, LightColor2;
uniform mat4 ModelView;
uniform float Shininess;

uniform sampler2D texture;

uniform float texScale;

void main()
{
  // [PART 1][G & I]
  // Duplicated to account for the two light sources
  vec3 L1 = normalize(Lvec1);  // Direction to the light source
  vec3 L2 = normalize(Lvec2);  // Direction to the light source
  vec3 E = normalize(-pos);    // Direction to the eye/camera
  vec3 H1 = normalize(L1 + E); // Halfway vector
  vec3 H2 = normalize(L2 + E); // Halfway vector

  // Compute terms in the illumination equation
  vec4 ambient1 = vec4((LightColor1 * LightBrightness1), 1.0) * vec4(AmbientProduct, 1.0);
  vec4 ambient2 = vec4((LightColor2 * LightBrightness2), 1.0) * vec4(AmbientProduct, 1.0);

  float Kd1 = max(dot(L1, N), 0.0);
  vec4 diffuse1 = Kd1 * vec4((LightColor1 * LightBrightness1), 1.0) * vec4(DiffuseProduct, 1.0);
  float Kd2 = max(dot(L2, N), 0.0);
  vec4 diffuse2 = Kd2 * vec4((LightColor2 * LightBrightness2), 1.0) * vec4(DiffuseProduct, 1.0);

  float Ks1 = pow(max(dot(N, H1), 0.0), Shininess);
  vec4 specular1 = Ks1 * LightBrightness1 * vec4(SpecularProduct, 1.0);
  float Ks2 = pow(max(dot(N, H2), 0.0), Shininess);
  vec4 specular2 = Ks2 * LightBrightness1 * vec4(SpecularProduct, 1.0);

  if (dot(L1, N) < 0.0)
  {
    specular1 = vec4(0.0, 0.0, 0.0, 1.0);
  }
  if (dot(L2, N) < 0.0)
  {
    specular2 = vec4(0.0, 0.0, 0.0, 1.0);
  }
  // [END]

  // The globalAmbient is independent of the distance from the light source
  vec4 globalAmbient = vec4(0.1, 0.1, 0.1, 1.0);

  // [PART 1][F]
  // Light reduction based on distance
  float dist = 0.01 + length(Lvec1);
  // [END]

  // [PART 2][A]
  // Added textures scaling
  color = globalAmbient + ((ambient1 + diffuse1) / dist) + (ambient2 + diffuse2);
  color.a = 1.0;
  gl_FragColor = color * texture2D(texture, texCoord * texScale) + (specular1 / dist) + specular2;
  // [END]
}
