
#include "Angel.h"

#include <stdlib.h>
#include <dirent.h>
#include <time.h>

// Open Asset Importer header files (in ../../assimp--3.0.1270/include)
// This is a standard open source library for loading meshes, see gnatidread.h
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

GLint windowHeight = 640, windowWidth = 960;

// gnatidread.cpp is the CITS3003 "Graphics n Animation Tool Interface & Data
// Reader" code.  This file contains parts of the code that you shouldn't need
// to modify (but, you can).
#include "gnatidread.h"
#include "gnatidread2.h"

using namespace std; // Import the C++ standard functions (e.g., min)

// IDs for the GLSL program and GLSL variables.
GLuint shaderProgram;                 // The number identifying the GLSL shader program
GLuint vPosition, vNormal, vTexCoord; // IDs for vshader input vars (from glGetAttribLocation)

// [PART 2][Preparation]
GLuint vBoneIDs, vBoneWeights, uBoneTransforms;
// [END]

GLuint projectionU, modelViewU; // IDs for uniform variables (from glGetUniformLocation)

// [PART 1][D]
// View distance increased by 4x
static float viewDist = 6.0; // Distance from the camera to the centre of the scene
// [END]

static float camRotSidewaysDeg = 0;   // rotates the camera sideways around the centre
static float camRotUpAndOverDeg = 20; // rotates the camera up and over the centre.

mat4 projection; // Projection matrix - set in the reshape function
mat4 view;       // View matrix - set in the display function.

// These are used to set the window title
char lab[] = "Project1";
char *programName = NULL; // Set in main
int numDisplayCalls = 0;  // Used to calculate the number of frames per second

//------Meshes----------------------------------------------------------------
// Uses the type aiMesh from ../../assimp--3.0.1270/include/assimp/mesh.h
//                           (numMeshes is defined in gnatidread.h)
aiMesh *meshes[numMeshes]; // For each mesh we have a pointer to the mesh to draw
GLuint vaoIDs[numMeshes];  // and a corresponding VAO ID from glGenVertexArrays

// [PART 2][Preparation]
const aiScene *scenes[numMeshes];
// [END]

// -----Textures--------------------------------------------------------------
//                           (numTextures is defined in gnatidread.h)
texture *textures[numTextures]; // An array of texture pointers - see gnatidread.h
GLuint textureIDs[numTextures]; // Stores the IDs returned by glGenTextures

//------Scene Objects---------------------------------------------------------
//
// For each object in a scene we store the following
// Note: the following is exactly what the sample solution uses, you can do things differently if you want.
typedef struct
{
    vec4 loc;
    float scale;
    float angles[3];                  // rotations around X, Y and Z axes.
    float diffuse, specular, ambient; // Amount of each light component
    float shine;
    vec3 rgb;
    float brightness; // Multiplies all colours
    int meshId;
    int texId;
    float texScale;
    // [PART 2][D]
    // Added frames counter for animation of models
    int frames;
    // [END]
} SceneObject;

const int maxObjects = 1024; // Scenes with more than 1024 objects seem unlikely

SceneObject sceneObjs[maxObjects]; // An array storing the objects currently in the scene.
int nObjects = 0;                  // How many objects are currenly in the scene.
int currObject = -1;               // The current object
int toolObj = -1;                  // The object currently being modified

// [PART 2][D]
// Declared variables for animation of model
// Animation Speed begins at 4 to allow for adjustment within menu
int frame = 0;
int animationSpeed = 4;
bool animationActive = true;
// [END]

//----------------------------------------------------------------------------
//
// Loads a texture by number, and binds it for later use.
void loadTextureIfNotAlreadyLoaded(int i)
{
    if (textures[i] != NULL)
        return; // The texture is already loaded.

    textures[i] = loadTextureNum(i);
    CheckError();
    glActiveTexture(GL_TEXTURE0);
    CheckError();

    // Based on: http://www.opengl.org/wiki/Common_Mistakes
    glBindTexture(GL_TEXTURE_2D, textureIDs[i]);
    CheckError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textures[i]->width, textures[i]->height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, textures[i]->rgbData);
    CheckError();
    glGenerateMipmap(GL_TEXTURE_2D);
    CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    CheckError();

    glBindTexture(GL_TEXTURE_2D, 0);
    CheckError(); // Back to default texture
}

//------Mesh loading----------------------------------------------------------
//
// The following uses the Open Asset Importer library via loadMesh in
// gnatidread.h to load models in .x format, including vertex positions,
// normals, and texture coordinates.
// You shouldn't need to modify this - it's called from drawMesh below.

void loadMeshIfNotAlreadyLoaded(int meshNumber)
{
    if (meshNumber >= numMeshes || meshNumber < 0)
    {
        printf("Error - no such model number");
        exit(1);
    }

    if (meshes[meshNumber] != NULL)
        return; // Already loaded

    // [PART 2][Preparation]
    const aiScene *scene = loadScene(meshNumber);
    scenes[meshNumber] = scene;
    aiMesh *mesh = scene->mMeshes[0];
    meshes[meshNumber] = mesh;
    // [END]

    glBindVertexArray(vaoIDs[meshNumber]);

    // Create and initialize a buffer object for positions and texture coordinates, initially empty.
    // mesh->mTextureCoords[0] has space for up to 3 dimensions, but we only need 2.
    GLuint buffer[1];
    glGenBuffers(1, buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (3 + 3 + 3) * mesh->mNumVertices,
                 NULL, GL_STATIC_DRAW);

    int nVerts = mesh->mNumVertices;
    // Next, we load the position and texCoord data in parts.
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * nVerts, mesh->mVertices);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 3 * nVerts, sizeof(float) * 3 * nVerts, mesh->mTextureCoords[0]);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 6 * nVerts, sizeof(float) * 3 * nVerts, mesh->mNormals);

    // Load the element index data
    GLuint elements[mesh->mNumFaces * 3];
    for (GLuint i = 0; i < mesh->mNumFaces; i++)
    {
        elements[i * 3] = mesh->mFaces[i].mIndices[0];
        elements[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        elements[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    GLuint elementBufferId[1];
    glGenBuffers(1, elementBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * mesh->mNumFaces * 3, elements, GL_STATIC_DRAW);

    // vPosition it actually 4D - the conversion sets the fourth dimension (i.e. w) to 1.0
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vPosition);

    // vTexCoord is actually 2D - the third dimension is ignored (it's always 0.0)
    glVertexAttribPointer(vTexCoord, 3, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(float) * 3 * mesh->mNumVertices));
    glEnableVertexAttribArray(vTexCoord);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(float) * 6 * mesh->mNumVertices));
    glEnableVertexAttribArray(vNormal);
    CheckError();

    //PART 2
    // [PART 2][Preparation]
    // Get boneIDs and boneWeights for each vertex from the imported mesh data
    GLint boneIDs[mesh->mNumVertices][4];
    GLfloat boneWeights[mesh->mNumVertices][4];
    getBonesAffectingEachVertex(mesh, boneIDs, boneWeights);

    GLuint buffers[2];
    glGenBuffers(2, buffers); // Add two vertex buffer objects

    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    CheckError();
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 4 * mesh->mNumVertices, boneIDs, GL_STATIC_DRAW);
    CheckError();
    glVertexAttribPointer(vBoneIDs, 4, GL_INT, GL_FALSE, 0, BUFFER_OFFSET(0));
    CheckError();
    glEnableVertexAttribArray(vBoneIDs);
    CheckError();

    glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * mesh->mNumVertices, boneWeights, GL_STATIC_DRAW);
    glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vBoneWeights);
    CheckError();
    // [END]
}

//----------------------------------------------------------------------------

static void mouseClickOrScroll(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        if (glutGetModifiers() != GLUT_ACTIVE_SHIFT)
            activateTool(button);
        else
            activateTool(GLUT_LEFT_BUTTON);
    }
    else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
        deactivateTool();
    else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN)
    {
        activateTool(button);
    }
    else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_UP)
        deactivateTool();

    else if (button == 3)
    { // scroll up
        viewDist = (viewDist < 0.0 ? viewDist : viewDist * 0.8) - 0.05;
    }
    else if (button == 4)
    { // scroll down
        viewDist = (viewDist < 0.0 ? viewDist : viewDist * 1.25) + 0.05;
    }
}

//----------------------------------------------------------------------------

static void mousePassiveMotion(int x, int y)
{
    mouseX = x;
    mouseY = y;
}

//----------------------------------------------------------------------------

mat2 camRotZ()
{
    return rotZ(-camRotSidewaysDeg) * mat2(10.0, 0, 0, -10.0);
}

//------callback functions for doRotate below and later-----------------------

static void adjustCamrotsideViewdist(vec2 cv)
{
    cout << cv << endl;
    camRotSidewaysDeg += cv[0];
    viewDist += cv[1];
}

static void adjustcamSideUp(vec2 su)
{
    camRotSidewaysDeg += su[0];
    camRotUpAndOverDeg += su[1];
}

static void adjustLocXZ(vec2 xz)
{
    sceneObjs[toolObj].loc[0] += xz[0];
    sceneObjs[toolObj].loc[2] += xz[1];
}

static void adjustScaleY(vec2 sy)
{
    sceneObjs[toolObj].scale += sy[0];
    sceneObjs[toolObj].loc[1] += sy[1];
}

//----------------------------------------------------------------------------
//------Set the mouse buttons to rotate the camera----------------------------
//------around the centre of the scene.---------------------------------------
//----------------------------------------------------------------------------

static void doRotate()
{
    // [PART 1][D]
    // Scaled the sideview by 4x
    setToolCallbacks(adjustCamrotsideViewdist, mat2(400, 0, 0, -8),
                     adjustcamSideUp, mat2(400, 0, 0, -90));
    // [END]
}

//------Add an object to the scene--------------------------------------------

static void addObject(int id)
{
    vec2 currPos = currMouseXYworld(camRotSidewaysDeg);
    sceneObjs[nObjects].loc[0] = currPos[0];
    sceneObjs[nObjects].loc[1] = 0.0;
    sceneObjs[nObjects].loc[2] = currPos[1];
    sceneObjs[nObjects].loc[3] = 1.0;

    // [PART 2][D]
    // Scaled the human models by a factor of 10x to have right size
    if (id > 0 && id < 55)
    {
        sceneObjs[nObjects].scale = 0.005;
    }
    else if (id > 55)
    {
        sceneObjs[nObjects].scale = 0.05;
    }
    // [END]

    sceneObjs[nObjects].rgb[0] = 0.7;
    sceneObjs[nObjects].rgb[1] = 0.7;
    sceneObjs[nObjects].rgb[2] = 0.7;
    sceneObjs[nObjects].brightness = 1.0;

    sceneObjs[nObjects].diffuse = 1.0;
    sceneObjs[nObjects].specular = 0.5;
    sceneObjs[nObjects].ambient = 0.7;
    sceneObjs[nObjects].shine = 10.0;

    // [PART 2][D]
    // Rotated the models to face the right way when added to scene
    if (id > 55 && id < 59)
    {
        // Human models turning right way
        sceneObjs[nObjects].angles[0] = 0.0;
        sceneObjs[nObjects].angles[1] = 0.0;
        sceneObjs[nObjects].angles[2] = 0.0;
    }
    else if (id == 59)
    {
        // Anime model turning right way
        sceneObjs[nObjects].angles[0] = 0.0;
        sceneObjs[nObjects].angles[1] = 90.0;
        sceneObjs[nObjects].angles[2] = 0.0;
    }
    else
    {
        // All other base models
        sceneObjs[nObjects].angles[0] = 0.0;
        sceneObjs[nObjects].angles[1] = 180.0;
        sceneObjs[nObjects].angles[2] = 0.0;
    }
    // [END]

    sceneObjs[nObjects].meshId = id;
    sceneObjs[nObjects].texId = rand() % numTextures;
    sceneObjs[nObjects].texScale = 2.0;

    // [PART 2][D]
    // Added custom frame length for each animated human model
    if (id < 56)
    {
        sceneObjs[nObjects].frames = 1;
    }
    else if (id == 56)
    {
        sceneObjs[nObjects].frames = 300;
    }
    else if (id == 57)
    {
        sceneObjs[nObjects].frames = 300;
    }
    else if (id == 58)
    {
        sceneObjs[nObjects].frames = 500;
    }
    else if (id == 59)
    {
        sceneObjs[nObjects].frames = 300;
    }
    // [END]

    toolObj = currObject = nObjects++;
    setToolCallbacks(adjustLocXZ, camRotZ(),
                     adjustScaleY, mat2(0.05, 0, 0, 10.0));
    glutPostRedisplay();
}

//------The init function-----------------------------------------------------

void init(void)
{
    srand(time(NULL)); /* initialize random seed - so the starting scene varies */
    aiInit();

    glGenVertexArrays(numMeshes, vaoIDs);
    CheckError(); // Allocate vertex array objects for meshes
    glGenTextures(numTextures, textureIDs);
    CheckError(); // Allocate texture objects

    // Load shaders and use the resulting shader program
    shaderProgram = InitShader("vStart.glsl", "fStart.glsl");

    glUseProgram(shaderProgram);
    CheckError();

    // Initialize the vertex position attribute from the vertex shader
    vPosition = glGetAttribLocation(shaderProgram, "vPosition");
    vNormal = glGetAttribLocation(shaderProgram, "vNormal");
    CheckError();

    // Likewise, initialize the vertex texture coordinates attribute.
    vTexCoord = glGetAttribLocation(shaderProgram, "vTexCoord");
    CheckError();

    projectionU = glGetUniformLocation(shaderProgram, "Projection");
    modelViewU = glGetUniformLocation(shaderProgram, "ModelView");

    // [PART 2][Preparation]
    vBoneIDs = glGetAttribLocation(shaderProgram, "boneIDs");
    CheckError();

    vBoneWeights = glGetAttribLocation(shaderProgram, "boneWeights");
    CheckError();

    uBoneTransforms = glGetUniformLocation(shaderProgram, "boneTransforms");
    CheckError();
    // [END]

    // Objects 0, and 1 are the ground and the first light.
    addObject(0); // Square for the ground
    sceneObjs[0].loc = vec4(0.0, 0.0, 0.0, 1.0);
    sceneObjs[0].scale = 10.0;
    sceneObjs[0].angles[0] = 90.0; // Rotate it.
    sceneObjs[0].texScale = 5.0;   // Repeat the texture.

    addObject(55); // Sphere for the first light
    sceneObjs[1].loc = vec4(1.0, 1.0, 1.0, 1.0);
    sceneObjs[1].scale = 0.1;
    sceneObjs[1].texId = 0;        // Plain texture
    sceneObjs[1].brightness = 0.2; // The light's brightness is 5 times this (below).

    // [PART 1][I]
    // Added a second light source
    addObject(55);
    sceneObjs[2].loc = vec4(-2.0, 1.0, -1.0, 1.0);
    sceneObjs[2].scale = 0.2;
    sceneObjs[2].texId = 0;
    sceneObjs[2].brightness = 0.2;
    // [END]

    addObject(rand() % numMeshes); // A test mesh

    // We need to enable the depth test to discard fragments that
    // are behind previously drawn fragments for the same pixel.
    glEnable(GL_DEPTH_TEST);
    doRotate();                       // Start in camera rotate mode.
    glClearColor(0.0, 0.0, 0.0, 1.0); /* black background */
}

//----------------------------------------------------------------------------

void drawMesh(SceneObject sceneObj)
{

    // Activate a texture, loading if needed.
    loadTextureIfNotAlreadyLoaded(sceneObj.texId);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureIDs[sceneObj.texId]);

    // Texture 0 is the only texture type in this program, and is for the rgb
    // colour of the surface but there could be separate types for, e.g.,
    // specularity and normals.
    glUniform1i(glGetUniformLocation(shaderProgram, "texture"), 0);

    // Set the texture scale for the shaders
    glUniform1f(glGetUniformLocation(shaderProgram, "texScale"), sceneObj.texScale);

    // Set the projection matrix for the shaders
    glUniformMatrix4fv(projectionU, 1, GL_TRUE, projection);

    // Set the model matrix - this should combine translation, rotation and scaling based on what's
    // in the sceneObj structure (see near the top of the program).

    // [PART 1][B]
    // Object rotation
    mat4 rotation = RotateX(sceneObj.angles[0]) * RotateY(sceneObj.angles[1]) * RotateZ(sceneObj.angles[2]);
    mat4 model = Translate(sceneObj.loc) * rotation * Scale(sceneObj.scale);
    // [END]

    // Set the model-view matrix for the shaders
    glUniformMatrix4fv(modelViewU, 1, GL_TRUE, view * model);

    // Activate the VAO for a mesh, loading if needed.
    loadMeshIfNotAlreadyLoaded(sceneObj.meshId);
    CheckError();
    glBindVertexArray(vaoIDs[sceneObj.meshId]);
    CheckError();

    // [PART 2][Preparation]
    int nBones = meshes[sceneObj.meshId]->mNumBones;
    if (nBones == 0)
        // If no bones, just a single identity matrix is used
        nBones = 1;

    mat4 *boneTransforms;
    boneTransforms = (mat4 *)malloc(sizeof(mat4) * nBones);
    // initialize each boneTransforms[i] to a zero matrix
    for (int i = 0; i < nBones; i++)
        boneTransforms[i] = mat4(0.0);

    // [PART 2][D]
    // Adjusting animation speed through animationSpeed variable
    if (animationActive)
    {
        // '10' used to allow tweaking of the animation speed
        frame = glutGet(GLUT_ELAPSED_TIME) / (10 * animationSpeed) % sceneObj.frames;
    }
    // [END]

    // [PART 2][D]
    // Added frame variable to keep track of position time
    calculateAnimPose(meshes[sceneObj.meshId], scenes[sceneObj.meshId], 0, frame, boneTransforms);
    // [END]

    glUniformMatrix4fv(uBoneTransforms, nBones, GL_TRUE,
                       (const GLfloat *)boneTransforms);
    // Added error checking
    CheckError();
    // [END]

    glDrawElements(GL_TRIANGLES, meshes[sceneObj.meshId]->mNumFaces * 3,
                   GL_UNSIGNED_INT, NULL);
    CheckError();
}

//----------------------------------------------------------------------------

void display(void)
{
    numDisplayCalls++;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CheckError(); // May report a harmless GL_INVALID_OPERATION with GLEW on the first frame

    // Set the view matrix. To start with this just moves the camera
    // backwards.  You'll need to add appropriate rotations.

    // [PART 1][A]
    mat4 rotation = RotateX(camRotUpAndOverDeg) * RotateY(camRotSidewaysDeg);
    view = Translate(0.0, 0.0, -viewDist) * rotation;
    // [END]

    SceneObject lightObj1 = sceneObjs[1];
    vec4 lightPosition1 = view * lightObj1.loc;
    glUniform4fv(glGetUniformLocation(shaderProgram, "LightPosition1"),
                 1, lightPosition1);
    CheckError();

    // [PART 1][I]
    // Duplicated light source calculations
    SceneObject lightObj2 = sceneObjs[2];
    vec4 lightPosition2 = rotation * lightObj2.loc;
    glUniform4fv(glGetUniformLocation(shaderProgram, "LightPosition2"),
                 1, lightPosition2);
    CheckError();
    // [END]

    // [PART 1][H]
    // Separate calculations for two light sources
    glUniform3fv(glGetUniformLocation(shaderProgram, "LightColor1"),
                 1, lightObj1.rgb);
    CheckError();
    glUniform3fv(glGetUniformLocation(shaderProgram, "LightColor2"),
                 1, lightObj2.rgb);
    CheckError();
    glUniform1f(glGetUniformLocation(shaderProgram, "LightBrightness1"),
                lightObj1.brightness);
    CheckError();
    glUniform1f(glGetUniformLocation(shaderProgram, "LightBrightness2"),
                lightObj2.brightness);
    CheckError();
    // [END]

    for (int i = 0; i < nObjects; i++)
    {
        SceneObject so = sceneObjs[i];

        // [PART 1][H]
        // Accounting for two light sources
        vec3 rgb = so.rgb * so.brightness * 2.0;

        glUniform3fv(glGetUniformLocation(shaderProgram, "AmbientProduct"), 1, so.ambient * rgb);
        CheckError();
        glUniform3fv(glGetUniformLocation(shaderProgram, "DiffuseProduct"), 1, so.diffuse * rgb);
        CheckError();        
        glUniform3fv(glGetUniformLocation(shaderProgram, "SpecularProduct"), 1, so.specular * rgb);
        CheckError();        
        glUniform1f(glGetUniformLocation(shaderProgram, "Shininess"), so.shine);
        CheckError();
        // [END]

        drawMesh(sceneObjs[i]);
    }

    glutSwapBuffers();
}

//----------------------------------------------------------------------------
//------Menus-----------------------------------------------------------------
//----------------------------------------------------------------------------

static void objectMenu(int id)
{
    deactivateTool();
    addObject(id);
}

static void texMenu(int id)
{
    deactivateTool();
    if (currObject >= 0)
    {
        sceneObjs[currObject].texId = id;
        glutPostRedisplay();
    }
}

static void groundMenu(int id)
{
    deactivateTool();
    sceneObjs[0].texId = id;
    glutPostRedisplay();
}

static void adjustBrightnessY(vec2 by)
{
    sceneObjs[toolObj].brightness += by[0];
    sceneObjs[toolObj].loc[1] += by[1];
}

static void adjustRedGreen(vec2 rg)
{
    sceneObjs[toolObj].rgb[0] += rg[0];
    sceneObjs[toolObj].rgb[1] += rg[1];
}

static void adjustBlueBrightness(vec2 bl_br)
{
    sceneObjs[toolObj].rgb[2] += bl_br[0];
    sceneObjs[toolObj].brightness += bl_br[1];
}

static void lightMenu(int id)
{
    deactivateTool();
    if (id == 70)
    {
        toolObj = 1;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2(1.0, 0.0, 0.0, 10.0));
    }
    else if (id >= 71 && id <= 74)
    {
        toolObj = 1;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0, 0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0, 0, 1.0));
    }

    // [PART 1][I]
    // Changed to include the two light sources
    else if (id == 80)
    {
        toolObj = 2;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2(1.0, 0.0, 0.0, 10.0));
    }
    else if (id >= 81 && id <= 84)
    {
        toolObj = 2;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0, 0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0, 0, 1.0));
    }
    // [END]
    else
    {
        printf("Error in lightMenu\n");
        exit(1);
    }
}

static int createArrayMenu(int size, const char menuEntries[][128], void (*menuFn)(int))
{
    int nSubMenus = (size - 1) / 10 + 1;
    int subMenus[nSubMenus];

    for (int i = 0; i < nSubMenus; i++)
    {
        subMenus[i] = glutCreateMenu(menuFn);
        for (int j = i * 10 + 1; j <= min(i * 10 + 10, size); j++)
            glutAddMenuEntry(menuEntries[j - 1], j);
        CheckError();
    }
    int menuId = glutCreateMenu(menuFn);

    for (int i = 0; i < nSubMenus; i++)
    {
        char num[6];
        sprintf(num, "%d-%d", i * 10 + 1, min(i * 10 + 10, size));
        glutAddSubMenu(num, subMenus[i]);
        CheckError();
    }
    return menuId;
}

// [PART 1][C]
// Created function for Ambient and Diffuse
static void adjustAmbientDiffuse(vec2 ambience)
{
    sceneObjs[toolObj].ambient = max(0.0f, sceneObjs[toolObj].ambient + ambience[0]);
    sceneObjs[toolObj].diffuse = max(0.0f, sceneObjs[toolObj].diffuse + ambience[1]);
}
// Created function for Specular and Shine
static void adjustSpecularShine(vec2 shiny)
{
    sceneObjs[toolObj].specular = max(0.0f, sceneObjs[toolObj].specular + shiny[0]);
    sceneObjs[toolObj].shine = max(0.0f, sceneObjs[toolObj].shine + shiny[1]);
}
// [END]

static void materialMenu(int id)
{
    deactivateTool();
    if (currObject < 0)
        return;
    if (id == 10)
    {
        toolObj = currObject;
        setToolCallbacks(adjustRedGreen, mat2(1, 0, 0, 1),
                         adjustBlueBrightness, mat2(1, 0, 0, 1));
    }
    // [PART 1][C]
    // Created menu items to allow adjustment
    else if (id == 20)
    {
        toolObj = currObject;
        setToolCallbacks(adjustAmbientDiffuse, mat2(1, 0, 0, 1), adjustSpecularShine, mat2(1, 0, 0, 20));
    }
    // [END]
    else
    {
        printf("Error in materialMenu\n");
    }
}

static void adjustAngleYX(vec2 angle_yx)
{
    sceneObjs[currObject].angles[1] += angle_yx[0];
    sceneObjs[currObject].angles[0] += angle_yx[1];
}

static void adjustAngleZTexscale(vec2 az_ts)
{
    sceneObjs[currObject].angles[2] += az_ts[0];
    sceneObjs[currObject].texScale += az_ts[1];
}

static void mainmenu(int id)
{
    deactivateTool();
    if (id == 41 && currObject >= 0)
    {
        toolObj = currObject;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustScaleY, mat2(0.05, 0, 0, 10));
    }
    // [PART ][D]
    // Modified to simply call doRotate()
    else if (id == 50)
    {
        doRotate();
    }
    // [END] [PART 1][B]
    else if (id == 55 && currObject >= 0)
    {
        setToolCallbacks(adjustAngleYX, mat2(400, 0, 0, 400),
                         adjustAngleZTexscale, mat2(-400, 0, 0, 15));
    }
    else if (id == 99)
    {
        exit(0);
    }
}

// [PART 2][D]
// Added new menu to allow for animation tweaking
static void animationMenu(int id)
{
    // Pause and Play option
    if (id == 100)
    {
        animationActive = !animationActive;
    }
    // Speed Down option
    else if (id == 101)
    {
        animationSpeed += 1;
    }
    // Speed Up option
    else if (id == 102)
    {
        // Limit the speed to above 1 so it cannot crash
        if (animationSpeed > 1)
        {
            animationSpeed -= 1;
        }
    }
    // Speed Reset option
    else if (id == 103)
    {
        animationSpeed = 4;
    }
}
// [END]

static void makeMenu()
{
    int objectId = createArrayMenu(numMeshes, objectMenuEntries, objectMenu);

    int materialMenuId = glutCreateMenu(materialMenu);
    glutAddMenuEntry("R/G/B/All", 10);

    // [PART 1][C]
    // Added menu entry for Adjustments
    glutAddMenuEntry("Ambient/Diffuse/Specular/Shine", 20);
    // [END]

    int texMenuId = createArrayMenu(numTextures, textureMenuEntries, texMenu);
    int groundMenuId = createArrayMenu(numTextures, textureMenuEntries, groundMenu);

    int lightMenuId = glutCreateMenu(lightMenu);
    glutAddMenuEntry("Move Light 1", 70);
    glutAddMenuEntry("R/G/B/All Light 1", 71);
    glutAddMenuEntry("Move Light 2", 80);
    glutAddMenuEntry("R/G/B/All Light 2", 81);

    // [PART 2][D]
    // Added sub-menu entry for Animation
    int animationMenuId = glutCreateMenu(animationMenu);
    glutAddMenuEntry("Pause / Play", 100);
    glutAddMenuEntry("Speed Down", 101);
    glutAddMenuEntry("Speed Up", 102);
    glutAddMenuEntry("Speed Reset", 103);
    // [END]

    glutCreateMenu(mainmenu);
    glutAddMenuEntry("Rotate/Move Camera", 50);
    glutAddSubMenu("Add object", objectId);
    glutAddMenuEntry("Position/Scale", 41);
    glutAddMenuEntry("Rotation/Texture Scale", 55);
    glutAddSubMenu("Material", materialMenuId);
    glutAddSubMenu("Texture", texMenuId);
    glutAddSubMenu("Ground Texture", groundMenuId);
    glutAddSubMenu("Lights", lightMenuId);
    // [PART 2][D]
    // Added Animation main menu entry
    glutAddSubMenu("Animation", animationMenuId);
    // [END]
    glutAddMenuEntry("EXIT", 99);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

//----------------------------------------------------------------------------

void keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 033:
        exit(EXIT_SUCCESS);
        break;
    }
}

//----------------------------------------------------------------------------

void idle(void)
{
    glutPostRedisplay();
}

//----------------------------------------------------------------------------

void reshape(int width, int height)
{
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    // [PART 1][D]
    // Increased near distance by one decimal point
    GLfloat nearDist = 0.02;
    // [END]

    // [PART 1][E]
    GLfloat bottom, top, left, right;

    if (width < height)
    {
        bottom = -nearDist * (float)height / (float)width;
        top = nearDist * (float)height / (float)width;
        left = -nearDist;
        right = nearDist;
    }
    else
    {
        bottom = -nearDist;
        top = nearDist;
        right = nearDist * (float)width / (float)height;
        left = -nearDist * (float)width / (float)height;
    }

    projection = Frustum(left, right, bottom, top, nearDist, 100.0);
    // [END]
}

//----------------------------------------------------------------------------

void timer(int unused)
{
    char title[256];
    sprintf(title, "%s %s: %d Frames Per Second @ %d x %d",
            lab, programName, numDisplayCalls, windowWidth, windowHeight);

    glutSetWindowTitle(title);

    numDisplayCalls = 0;
    glutTimerFunc(1000, timer, 1);
}

//----------------------------------------------------------------------------

char dirDefault1[] = "models-textures";
char dirDefault3[] = "/tmp/models-textures";
char dirDefault4[] = "/d/models-textures";
char dirDefault2[] = "/cslinux/examples/CITS3003/project-files/models-textures";

void fileErr(char *fileName)
{
    printf("Error reading file: %s\n", fileName);
    printf("When not in the CSSE labs, you will need to include the directory containing\n");
    printf("the models on the command line, or put it in the same folder as the exectutable.");
    exit(1);
}

//----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Get the program name, excluding the directory, for the window title
    programName = argv[0];
    for (char *cpointer = argv[0]; *cpointer != 0; cpointer++)
        if (*cpointer == '/' || *cpointer == '\\')
            programName = cpointer + 1;

    // Set the models-textures directory, via the first argument or some handy defaults.
    if (argc > 1)
        strcpy(dataDir, argv[1]);
    else if (opendir(dirDefault1))
        strcpy(dataDir, dirDefault1);
    else if (opendir(dirDefault2))
        strcpy(dataDir, dirDefault2);
    else if (opendir(dirDefault3))
        strcpy(dataDir, dirDefault3);
    else if (opendir(dirDefault4))
        strcpy(dataDir, dirDefault4);
    else
        fileErr(dirDefault1);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);

    //glutInitContextVersion( 3, 2);
    glutInitContextProfile(GLUT_CORE_PROFILE); // May cause issues
    // glutInitContextProfile( GLUT_COMPATIBILITY_PROFILE ); // should still use only OpenGL 3.2 Core
    // features.
    glutCreateWindow("Initialising...");

    glewInit();   // With some old hardware yields GL_INVALID_ENUM, if so use glewExperimental.
    CheckError(); // This bug is explained at: http://www.opengl.org/wiki/OpenGL_Loading_Library

    init();
    CheckError();

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);

    glutMouseFunc(mouseClickOrScroll);
    glutPassiveMotionFunc(mousePassiveMotion);
    glutMotionFunc(doToolUpdateXY);

    glutReshapeFunc(reshape);
    glutTimerFunc(1000, timer, 1);
    CheckError();

    makeMenu();
    CheckError();

    glutMainLoop();
    return 0;
}
