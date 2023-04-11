#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "EW/Shader.h"
#include "EW/EwMath.h"
#include "EW/Camera.h"
#include "EW/Mesh.h"
#include "EW/Transform.h"
#include "EW/ShapeGen.h"

void processInput(GLFWwindow* window);
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height);
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void mousePosCallback(GLFWwindow* window, double xpos, double ypos);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

float lastFrameTime;
float deltaTime;

int SCREEN_WIDTH = 1080;
int SCREEN_HEIGHT = 720;

double prevMouseX;
double prevMouseY;
bool firstMouseInput = false;

/* Button to lock / unlock mouse
* 1 = right, 2 = middle
* Mouse will start locked. Unlock it to use UI
* */
const int MOUSE_TOGGLE_BUTTON = 1;
const float MOUSE_SENSITIVITY = 0.1f;
const float CAMERA_MOVE_SPEED = 5.0f;
const float CAMERA_ZOOM_SPEED = 3.0f;

Camera camera((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT);

glm::vec3 bgColor = glm::vec3(0);
glm::vec3 lightColor = glm::vec3(1.0f);
glm::vec3 lightPosition = glm::vec3(0.0f, 3.0f, 0.0f);

bool wireFrame = false;

struct Light
{
	glm::vec3 position;
	glm::vec3 color;
	float intensity;
};

struct DirectionalLight
{
	glm::vec3 direction;
	Light light;
};

struct PointLight
{
	glm::vec3 position;
	Light light;

	float constK, linearK, quadraticK;
};

struct SpotLight
{
	glm::vec3 position;
	glm::vec3 direction;
	Light light;

	float range;
	float innerAngle;
	float outerAngle;
	float angleFalloff;
};

struct Material
{
	glm::vec3 color;
	float ambientK, diffuseK, specularK; // (0-1 range)
	float shininess = 1; // (1-512 range)
};

int numPointLights = 0;
glm::vec3 pointLightOrbitCenter;
float pointLightOrbitRange;
float pointLightOrbitSpeed;

DirectionalLight _DirectionalLight;
PointLight _PointLight;
SpotLight _SpotLight;
Material _Material;

// Not sure if this is the best spot to be putting this
GLuint getTexture(const char* texturePath)
{
	// Generate a new texture and bind its location
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Have the loaded texture wrap on the s and t axes
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Generate mipmaps for minification of a texture and linearly filter between them
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// Linearly filter textures when magnifying
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Load image data from path
	int width, height, numComponents = 3;
	unsigned char* data = stbi_load(texturePath, &width, &height, &numComponents, 0);

	if (data != NULL)
	{
		// Generate a texture and mipmap from the given image data
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	else
	{
		std::cout << "Failed." << std::endl;
	}

	stbi_image_free(data);
	// Return the generated texture
	return texture;
}

// Going fullscreen messes this up because it isn't being properly
// resized to fit the new dimensions of the fullscreen viewport. (I think)
class FrameBuffer
{
public:
	/* 
	* Init frame buffer
	* Limitation of this implementation is that it messes with glSetActiveTexture
	* by overwriting the active texture with the texture being created here.
	*/
	FrameBuffer(int colorBuffers, int width, int height)
	{
		mWidth = width;
		mHeight = height;
		mTexturesLength = colorBuffers;
		textures = new unsigned int[mTexturesLength];

		// Create frame buffer object
		glGenFramebuffers(1, &fbo);

		// Bind frame buffer to GL_FRAMEBUFFER target
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		// Create texture color buffers
		glGenTextures(mTexturesLength, textures);

		// Stores attachments to be passed into glDrawBuffers
		// This implementation doesn't feel right
		unsigned int* attachments = new unsigned int[mTexturesLength];

		// Create textures for each generate color buffer
		for (int i = 0; i < mTexturesLength; i++)
		{
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glBindTexture(GL_TEXTURE_2D, 0);

			// Attach the texture to the corresponding frame buffer slot
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i], 0);

			// Add the attachment to the attachment array
			attachments[i] = GL_COLOR_ATTACHMENT0 + i;
		}

		// Create render buffer object
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);

		// Allocate space for the depth component
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// Attach render buffer object to the frame buffer
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

		// Specify how many attachments are being used in drawing
		glDrawBuffers(mTexturesLength, attachments);

		// Deallocate attachments array
		delete[] attachments;

		// Check for completeness
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("Frame buffer is incomplete.\n");
		}

		else
		{
			printf("Successfully created frame buffer.\n");
		}

		// Unbind framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Delete the frame buffer and its components
	// Not sure if memory with textures is cleaned up properly.
	~FrameBuffer()
	{
		glDeleteRenderbuffers(1, &rbo);
		glDeleteTextures(mTexturesLength, textures);
		glDeleteFramebuffers(1, &fbo);

		delete[] textures;
		textures = nullptr;
		printf("Unloaded buffer.\n");
	}

	// Getter for the current frame buffer
	unsigned int getFBO() { return fbo; }

	// Gett for the buffer's texture
	unsigned int getTexture(int texNum) { return textures[texNum]; }

private:
	unsigned int fbo;
	unsigned int* textures;
	unsigned int depth;
	unsigned int rbo;

	int mWidth, mHeight;
	int mTexturesLength;
};

class ShadowBuffer
{
public:
	ShadowBuffer(int width, int height)
	{
		mWidth = width;
		mHeight = height;

		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		glGenTextures(1, &depthTexture);
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("Frame buffer is incomplete.\n");
		}

		else
		{
			printf("Successfully created frame buffer.\n");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	~ShadowBuffer()
	{
		
	}

	unsigned int getFBO() { return fbo; }
	unsigned int getTexture() { return depthTexture; }

private:
	unsigned int fbo;
	unsigned int depthTexture;

	int mWidth, mHeight;
};

// Models
// Global for the sake of convenience
ew::Transform cubeTransform;
ew::Transform rectangleTransform;
ew::Transform sphereTransform;
ew::Transform planeTransform;
ew::Transform cylinderTransform;
ew::Transform quadTransform;
ew::Transform depthQuadTransform;
ew::Transform lightTransform;

ew::MeshData cubeMeshData;
ew::MeshData sphereMeshData;
ew::MeshData rectangleMeshData;
ew::MeshData planeMeshData;
ew::MeshData cylinderMeshData;
ew::MeshData quadMeshData;
ew::MeshData depthQuadMeshData;

ew::Mesh* cubeMesh;
ew::Mesh* sphereMesh;
ew::Mesh* rectangleMesh;
ew::Mesh* planeMesh;
ew::Mesh* cylinderMesh;
ew::Mesh* quadMesh;
ew::Mesh* depthQuadMesh;

void drawScene(Shader& targetShader, glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	targetShader.setMat4("_View", viewMatrix);
	targetShader.setMat4("_Projection", projectionMatrix);

	//Draw cube
	targetShader.setMat4("_Model", cubeTransform.getModelMatrix());
	cubeMesh->draw();

	//Draw rectangle
	targetShader.setMat4("_Model", rectangleTransform.getModelMatrix());
	rectangleMesh->draw();

	//Draw sphere
	targetShader.setMat4("_Model", sphereTransform.getModelMatrix());
	sphereMesh->draw();

	//Draw cylinder
	targetShader.setMat4("_Model", cylinderTransform.getModelMatrix());
	cylinderMesh->draw();

	//Draw plane
	targetShader.setMat4("_Model", planeTransform.getModelMatrix());
	planeMesh->draw();
}

int main() {
	if (!glfwInit()) {
		printf("glfw failed to init");
		return 1;
	}

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Lighting", 0, 0);
	glfwMakeContextCurrent(window);

	if (glewInit() != GLEW_OK) {
		printf("glew failed to init");
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, resizeFrameBufferCallback);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetScrollCallback(window, mouseScrollCallback);
	glfwSetCursorPosCallback(window, mousePosCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);

	//Hide cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Setup UI Platform/Renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	//Dark UI theme.
	ImGui::StyleColorsDark();

	//Used to draw shapes. This is the shader you will be completing.
	Shader litShader("shaders/defaultLit.vert", "shaders/defaultLit.frag");

	//Used to draw light sphere
	Shader unlitShader("shaders/defaultLit.vert", "shaders/unlit.frag");

	// Used to draw to the depth buffer only
	Shader depthOnly("shaders/depthOnly.vert", "shaders/depthOnly.frag");

	// Used to draw post processing effects
	Shader postProc("shaders/postProcessing.vert", "shaders/postProcessing.frag");

	// Create frame buffer instance with two frame buffers
	FrameBuffer screenBuffer = FrameBuffer(1, SCREEN_WIDTH, SCREEN_HEIGHT);

	// Create frame buffer to manage shadow depth buffer
	ShadowBuffer depthBuffer = ShadowBuffer(2048, 2048);

	ew::createCube(1.0f, 1.0f, 1.0f, cubeMeshData);
	ew::createCube(1.0f, 2.0f, 1.0f, rectangleMeshData);
	ew::createSphere(0.5f, 64, sphereMeshData);
	ew::createPlane(1.0f, 1.0f, planeMeshData);
	ew::createCylinder(1.0f, 0.5f, 64, cylinderMeshData);
	ew::createQuad(2.0f, 2.0f, quadMeshData);
	ew::createQuad(0.5f, 0.5f, depthQuadMeshData);

	cubeMesh = new ew::Mesh(&cubeMeshData);
	rectangleMesh = new ew::Mesh(&rectangleMeshData);
	sphereMesh = new ew::Mesh(&sphereMeshData);
	planeMesh = new ew::Mesh(&planeMeshData);
	cylinderMesh = new ew::Mesh(&cylinderMeshData);
	quadMesh = new ew::Mesh(&quadMeshData);
	depthQuadMesh = new ew::Mesh(&depthQuadMeshData);

	//Enable back face culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	//Enable blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	quadTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
	depthQuadTransform.position = glm::vec3(0.5f, 0.5f, 0.0f);

	cubeTransform.position = glm::vec3(-2.0f, 0.0f, 0.0f);
	rectangleTransform.position = glm::vec3(0.0f, 0.0f, -2.0f);
	sphereTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);

	planeTransform.position = glm::vec3(0.0f, -1.0f, 0.0f);
	planeTransform.scale = glm::vec3(10.0f);

	cylinderTransform.position = glm::vec3(2.0f, 0.0f, 0.0f);

	lightTransform.scale = glm::vec3(0.5f);
	lightTransform.position = glm::vec3(0.0f, 5.0f, 0.0f);

	_Material.color = glm::vec3(1, 1, 1);
	_Material.ambientK = 1;
	_Material.diffuseK = 1;
	_Material.specularK = 1;
	_Material.shininess = 1;

	_DirectionalLight.direction = glm::vec3(2, 2, 2);
	_DirectionalLight.light.intensity = 0.5f;
	_DirectionalLight.light.color = glm::vec3(1, 1, 1);

	float minBias = 0.000f;
	float maxBias = 0.001f;
	bool showShadowMap = false;

	const char* effectNames[5] = { "None", "Invert", "Red Overlay", "Zooming Out", "Wave"};
	int effectIndex = 0;

	GLuint brickTexture = getTexture("Bricks.jpg");
	GLuint tileTexture = getTexture("Tiles.jpg");
	GLuint brickNormal = getTexture("BricksNormal.jpg");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, brickTexture);
	litShader.setInt("_Texture1", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tileTexture);
	litShader.setInt("_Texture2", 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, brickNormal);
	litShader.setInt("_Normal", 2);

	while (!glfwWindowShouldClose(window)) {

		processInput(window);
		glClearColor(bgColor.r,bgColor.g,bgColor.b, 1.0f);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		float time = (float)glfwGetTime();
		deltaTime = time - lastFrameTime;
		lastFrameTime = time;

		depthOnly.use();

		glViewport(0, 0, 2048, 2048);
		glBindFramebuffer(GL_FRAMEBUFFER, depthBuffer.getFBO());

		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 lightView = glm::lookAt(_DirectionalLight.direction, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 200.0f);

		glCullFace(GL_FRONT);
		drawScene(depthOnly, lightView, lightProjection);

		// Set active frame buffer to screenBuffer
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, screenBuffer.getFBO());

		// Enable depth testing for 3D sorting
		glEnable(GL_DEPTH_TEST);

		// Clear screenBuffer (was here before)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		litShader.use();

		litShader.setFloat("time", time);

		litShader.setVec3("_DirectionalLight.direction", _DirectionalLight.direction);
		litShader.setFloat("_DirectionalLight.light.intensity", _DirectionalLight.light.intensity);
		litShader.setVec3("_DirectionalLight.light.color", _DirectionalLight.light.color);

		litShader.setVec3("_Material.color", _Material.color);
		litShader.setFloat("_Material.ambientK", _Material.ambientK);
		litShader.setFloat("_Material.diffuseK", _Material.diffuseK);
		litShader.setFloat("_Material.specularK", _Material.specularK);
		litShader.setFloat("_Material.shininess", _Material.shininess);

		litShader.setMat4("_LightViewProj", lightProjection * lightView);
		litShader.setVec3("_CameraPosition", camera.getPosition());
		
		litShader.setFloat("_MinBias", minBias);
		litShader.setFloat("_MaxBias", maxBias);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, depthBuffer.getTexture());
		litShader.setInt("_ShadowMap", 3);

		glCullFace(GL_BACK);
		drawScene(litShader, camera.getViewMatrix(), camera.getProjectionMatrix());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Disable depth testing
		glDisable(GL_DEPTH_TEST);

		// Clear color buffer bit
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Set post processing shader
		postProc.use();

		// Bind screen buffer's texture to the shader's texture
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, screenBuffer.getTexture(0));
		postProc.setInt("_Texture1", 4);

		postProc.setInt("effectIndex", effectIndex);
		postProc.setFloat("time", time);

		// Draw screen quad
		postProc.setMat4("_Model", quadTransform.getModelMatrix());
		quadMesh->draw();

		if (showShadowMap)
		{
			glBindTexture(GL_TEXTURE_2D, depthBuffer.getTexture());
			postProc.setInt("_Texture1", 4);

			postProc.setMat4("_Model", depthQuadTransform.getModelMatrix());
			depthQuadMesh->draw();
		}

		//Draw UI
		ImGui::Begin("Directional Light");

		ImGui::DragFloat3("Direction", &_DirectionalLight.direction.x, 1, -360, 360);
		ImGui::DragFloat("Intensity", &_DirectionalLight.light.intensity, 0.01, 0.01, 1);
		ImGui::ColorEdit3("Color", &_DirectionalLight.light.color.r);
		ImGui::End();

		ImGui::Begin("Post Processing");

		ImGui::Combo("Effects", &effectIndex, effectNames, IM_ARRAYSIZE(effectNames));
		ImGui::End();

		ImGui::Begin("Shadows");

		ImGui::DragFloat("Min Bias", &minBias, 0.001f, 0.001f, 0.1f);
		ImGui::DragFloat("Max Bias", &maxBias, 0.001f, 0.001f, 0.1f);
		ImGui::Checkbox("Show Shadow Map", &showShadowMap);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwPollEvents();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}
//Author: Eric Winebrenner
void resizeFrameBufferCallback(GLFWwindow* window, int width, int height)
{
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
	camera.setAspectRatio((float)SCREEN_WIDTH / SCREEN_HEIGHT);
	glViewport(0, 0, width, height);
}
//Author: Eric Winebrenner
void keyboardCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
	if (keycode == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
	//Reset camera
	if (keycode == GLFW_KEY_R && action == GLFW_PRESS) {
		camera.setPosition(glm::vec3(0, 0, 5));
		camera.setYaw(-90.0f);
		camera.setPitch(0.0f);
		firstMouseInput = false;
	}
	if (keycode == GLFW_KEY_1 && action == GLFW_PRESS) {
		wireFrame = !wireFrame;
		glPolygonMode(GL_FRONT_AND_BACK, wireFrame ? GL_LINE : GL_FILL);
	}
}
//Author: Eric Winebrenner
void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (abs(yoffset) > 0) {
		float fov = camera.getFov() - (float)yoffset * CAMERA_ZOOM_SPEED;
		camera.setFov(fov);
	}
}
//Author: Eric Winebrenner
void mousePosCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		return;
	}
	if (!firstMouseInput) {
		prevMouseX = xpos;
		prevMouseY = ypos;
		firstMouseInput = true;
	}
	float yaw = camera.getYaw() + (float)(xpos - prevMouseX) * MOUSE_SENSITIVITY;
	camera.setYaw(yaw);
	float pitch = camera.getPitch() - (float)(ypos - prevMouseY) * MOUSE_SENSITIVITY;
	pitch = glm::clamp(pitch, -89.9f, 89.9f);
	camera.setPitch(pitch);
	prevMouseX = xpos;
	prevMouseY = ypos;
}
//Author: Eric Winebrenner
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	//Toggle cursor lock
	if (button == MOUSE_TOGGLE_BUTTON && action == GLFW_PRESS) {
		int inputMode = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
		glfwSetInputMode(window, GLFW_CURSOR, inputMode);
		glfwGetCursorPos(window, &prevMouseX, &prevMouseY);
	}
}

//Author: Eric Winebrenner
//Returns -1, 0, or 1 depending on keys held
float getAxis(GLFWwindow* window, int positiveKey, int negativeKey) {
	float axis = 0.0f;
	if (glfwGetKey(window, positiveKey)) {
		axis++;
	}
	if (glfwGetKey(window, negativeKey)) {
		axis--;
	}
	return axis;
}

//Author: Eric Winebrenner
//Get input every frame
void processInput(GLFWwindow* window) {

	float moveAmnt = CAMERA_MOVE_SPEED * deltaTime;

	//Get camera vectors
	glm::vec3 forward = camera.getForward();
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
	glm::vec3 up = glm::normalize(glm::cross(forward, right));

	glm::vec3 position = camera.getPosition();
	position += forward * getAxis(window, GLFW_KEY_W, GLFW_KEY_S) * moveAmnt;
	position += right * getAxis(window, GLFW_KEY_D, GLFW_KEY_A) * moveAmnt;
	position += up * getAxis(window, GLFW_KEY_Q, GLFW_KEY_E) * moveAmnt;
	camera.setPosition(position);
}
