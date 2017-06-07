#define _USE_MATH_DEFINES
#include "../Externals/Include/Include.h"

#define MENU_NO_FILTER 1
#define MENU_IMAGE_ABSTRACTION 2
#define MENU_LAPLACIAN 3
#define MENU_SHRPEN 4
#define MENU_PIXELATION 5
#define MENU_FISH_EYE 6
#define MENU_SINE_WAVE 7
#define MENU_RED_BLUE 8
#define MENU_BLOOM 9
#define MENU_MAGNIFIER 10

#define DEFAULT_SHADER "fragment2.shader"

#include <string>
#include <set>

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

float comparisonBarX = 0.5;
float comparisonBarThickness = 0.005;
bool comparisonBarChanging = false;
bool comparisonBarBeing = false;

using namespace glm;
using namespace std;

#define SCENE_PATH "./lost-empire/"
char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
        width(0),
        height(0),
        data(0)
    {
    }

    int width;
    int height;
    unsigned char* data;
} TextureData;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadPNG(const char* const pngFilepath)
{
    TextureData texture;
    int components;

    // load the texture with stb image, force RGBA (4 components required)
    stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

    // is the image successfully loaded?
    if (data != NULL)
    {
        // copy the raw data
        size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
        texture.data = new unsigned char[dataSize];
        memcpy(texture.data, data, dataSize);

        // mirror the image vertically to comply with OpenGL convention
        for (size_t i = 0; i < texture.width; ++i)
        {
            for (size_t j = 0; j < texture.height / 2; ++j)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    size_t coord1 = (j * texture.width + i) * 4 + k;
                    size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
                    std::swap(texture.data[coord1], texture.data[coord2]);
                }
            }
        }

        // release the loaded image
        stbi_image_free(data);
    }

    return texture;
}

struct Shape
{
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint ibo;
	int drawCount;
	int materialID;

	vector<float> positions;
	vector<float> normals;
	vector<float> texcoords;
	vector<unsigned> indices;
};

struct Material
{
	GLuint diffuse_tex;
};

const aiScene *scene;
vector<TextureData> textureDatas;
vector<Material> materials;
vector<Shape> shapes;
void loadMaterials() {
	set<string> loadedTextureData;
	printf("Material count: %u\n", scene->mNumMaterials);
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
	{
		aiMaterial *material = scene->mMaterials[i];
		Material myMaterial;
		aiString texturePath;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
			aiReturn_SUCCESS)
		{
			string fullPath = string(SCENE_PATH) + texturePath.C_Str();
			//if (loadedTextureData.find(fullPath) != loadedTextureData.end()) continue;
			printf("Load texture data: %s%s\n", SCENE_PATH, texturePath.C_Str());

			TextureData textureData = loadPNG(fullPath.c_str());
			textureDatas.push_back(textureData);

			loadedTextureData.insert(fullPath);

			// load width, height and data from texturePath.C_Str();
			glGenTextures(1, &myMaterial.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, myMaterial.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData.width, textureData.height, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, textureData.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else
		{
			// load some default image as default_diffuse_tex
			//material.diffuse_tex = default_diffuse_tex;
		}
		// save material��
		materials.push_back(myMaterial);
	}
}

void loadMeshes() {
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh *mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		// create 3 vbos to hold data
		glGenBuffers(1, &shape.vbo_position);
		glGenBuffers(1, &shape.vbo_normal);
		glGenBuffers(1, &shape.vbo_texcoord);
		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
		{
			// mesh->mVertices[v][0~2] => position
			shape.positions.push_back(mesh->mVertices[v][0]);
			shape.positions.push_back(mesh->mVertices[v][1]);
			shape.positions.push_back(mesh->mVertices[v][2]);

			// mesh->mNormals[v][0~2] => normal
			shape.normals.push_back(mesh->mNormals[v][0]);
			shape.normals.push_back(mesh->mNormals[v][1]);
			shape.normals.push_back(mesh->mNormals[v][2]);

			// mesh->mTextureCoords[0][v][0~1] => texcoord
			shape.texcoords.push_back(mesh->mTextureCoords[0][v][0]);
			shape.texcoords.push_back(mesh->mTextureCoords[0][v][1]);
		}
		// create 1 ibo to hold data
		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			// mesh->mFaces[f].mIndices[0~2] => index
			//printf("mesh->mFaces[%u].mIndices[0] => %u\n", f, mesh->mFaces[f].mIndices[0]);
			//printf("mesh->mFaces[%u].mIndices[1] => %u\n", f, mesh->mFaces[f].mIndices[1]);
			//printf("mesh->mFaces[%u].mIndices[2] => %u\n", f, mesh->mFaces[f].mIndices[2]);
			shape.indices.push_back(mesh->mFaces[f].mIndices[0]);
			shape.indices.push_back(mesh->mFaces[f].mIndices[1]);
			shape.indices.push_back(mesh->mFaces[f].mIndices[2]);
		}
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.indices.size() * sizeof(unsigned), shape.indices.data(), GL_STATIC_DRAW);
		// glVertexAttribPointer/ glEnableVertexArraycalls��
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, shape.positions.size() * sizeof(float), shape.positions.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, shape.texcoords.size() * sizeof(float), shape.texcoords.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, shape.normals.size() * sizeof(float), shape.normals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		printf("Mesh[%u] -> Material[%u]\n", i, mesh->mMaterialIndex);
		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		// save shape��
		shapes.push_back(shape);
	}
}

GLuint program;
GLuint mv_location;
GLuint proj_location;
mat4 model;
mat4 view;
mat4 projection;

GLuint program2;
GLuint um4offset;
GLuint offset;
GLuint um4mouse;
GLuint um4height;
GLuint um4comparisonBarX;
GLuint window_vao;
GLuint window_buffer;
GLuint FBO;
GLuint depthRBO;
GLuint FBODataTexture;

static const GLfloat window_positions[] =
{
	1.0f,-1.0f,1.0f,0.0f,
	-1.0f,-1.0f,0.0f,0.0f,
	-1.0f,1.0f,0.0f,1.0f,
	1.0f,1.0f,1.0f,1.0f
};

void render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	glUniformMatrix4fv(mv_location, 1, GL_FALSE, (const GLfloat*)&(view * model));
	glUniformMatrix4fv(proj_location, 1, GL_FALSE, (const GLfloat*)&projection);
	glActiveTexture(GL_TEXTURE0);
	//glUniform1i(tex_location, 0);
	for (int i = 0; i < shapes.size(); ++i)
	{
		glBindVertexArray(shapes[i].vao);
		int materialID = shapes[i].materialID;
		glBindTexture(GL_TEXTURE_2D, materials[materialID].diffuse_tex);
		glDrawElements(GL_TRIANGLES, shapes[i].drawCount, GL_UNSIGNED_INT, 0);
	}

}

GLuint createProgram(char *fragmentShaderSourcePath) {
	// Create Shader Program
	GLuint newProgram = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader2 = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader2 = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource2 = loadShaderSource("vertex2.shader");
	char** fragmentShaderSource2 = loadShaderSource(fragmentShaderSourcePath);

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader2, 1, vertexShaderSource2, NULL);
	glShaderSource(fragmentShader2, 1, fragmentShaderSource2, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource2);
	freeShaderSource(fragmentShaderSource2);

	// Compile these shaders
	glCompileShader(vertexShader2);
	glCompileShader(fragmentShader2);

	glPrintShaderLog(fragmentShader2);

	// Assign the program we created before with these shaders
	glAttachShader(newProgram, vertexShader2);
	glAttachShader(newProgram, fragmentShader2);
	glLinkProgram(newProgram);

	um4offset = glGetUniformLocation(newProgram, "offset");
	um4mouse = glGetUniformLocation(newProgram, "mouse");
	um4height = glGetUniformLocation(newProgram, "height");
	um4comparisonBarX = glGetUniformLocation(newProgram, "comparisonBarX");

	glUseProgram(newProgram);
	glUniform1f(um4comparisonBarX, comparisonBarX);
	glUseProgram(program);

	return newProgram;
}

void My_Reshape(int width, int height);
void initShader() {
	// Create Shader Program
	program = glCreateProgram();

	// Create customize shader by tell openGL specify shader type 
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load shader file
	char** vertexShaderSource = loadShaderSource("vertex.vs.shader");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.shader");

	// Assign content of these shader files to those shaders we created before
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	// Free the shader file string(won't be used any more)
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);

	// Compile these shaders
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);

	// Assign the program we created before with these shaders
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	// Tell OpenGL to use this shader program now
	glUseProgram(program);

	mv_location = glGetUniformLocation(program, "um4mv");
	proj_location = glGetUniformLocation(program, "um4p");

	program2 = createProgram(DEFAULT_SHADER);

	glGenVertexArrays(1, &window_vao);
	glBindVertexArray(window_vao);

	glGenBuffers(1, &window_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, window_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_positions), window_positions, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glGenFramebuffers(1, &FBO);

	My_Reshape(600, 600);
}

void My_Init()
{
    glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	initShader();

	scene = aiImportFile(SCENE_PATH "lost_empire.obj", aiProcessPreset_TargetRealtime_Fast);
	
	loadMaterials();
	loadMeshes();
}

void My_Display()
{	// TODO :
	// (1) Bind the framebuffer object correctly
	// (2) Draw the buffer with color
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);

	static const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const GLfloat one = 1.0f;

	// TODO :
	// (1) Clear the color buffer (GL_COLOR) with the color of white
	// (2) Clear the depth buffer (GL_DEPTH) with value one 
	glClearBufferfv(GL_COLOR, 0, white);
	glClearBufferfv(GL_DEPTH, 0, &one);

	render();

	// Re-bind the framebuffer and clear it 
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FBODataTexture);

	// TODO :
	// (1) Bind the vao we want to render
	// (2) Use the correct shader program
	glBindVertexArray(window_vao);
	glUseProgram(program2);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	if (um4offset != -1) {
		glUseProgram(program2);
		glUniform1i(um4offset, offset);
		glUseProgram(program);
	}

	glutSwapBuffers();
}

//glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
//glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
//glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraPos = glm::vec3(-35.72, 70.82, 96.06);
glm::vec3 cameraFront = glm::vec3(69.83, -93.05, -94.45);
glm::vec3 cameraUp = glm::vec3(-0.02, 0.90, -0.44);
int windowWidth, windowHeight;
void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);
	windowWidth = width;
	windowHeight = height;

	float viewportAspect = (float)width / (float)height;
	projection = perspective(radians(60.0f), viewportAspect, 0.1f, 1000.0f);

	view = lookAt(cameraPos, cameraFront, cameraUp);

	// If the windows is reshaped, we need to reset some settings of framebuffer
	glDeleteRenderbuffers(1, &depthRBO);
	glDeleteTextures(1, &FBODataTexture);
	glGenRenderbuffers(1, &depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);


	// TODO :
	// (1) Generate a texture for FBO
	// (2) Bind it so that we can specify the format of the textrue
	glGenTextures(1, &FBODataTexture);
	glBindTexture(GL_TEXTURE_2D, FBODataTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// TODO :
	// (1) Bind the framebuffer with first parameter "GL_DRAW_FRAMEBUFFER" 
	// (2) Attach a renderbuffer object to a framebuffer object
	// (3) Attach a texture image to a framebuffer object
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBODataTexture, 0);

	if (um4height != -1) {
		glUseProgram(program2);
		glUniform1i(um4height, height);
		glUseProgram(program);
	}
}

void My_Timer(int val)
{
	++offset;
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

int downX, downY;
vec3 downCameraFront, downCameraUp;
float speed = 0.5;
void traceMouse(int x, int y)
{
	printf("Mouse is over at (%d, %d)\n", x, y);

	if (um4mouse != -1) {
		glUseProgram(program2);
		glUniform2i(um4mouse, x, y);
		glUseProgram(program);
	}

	if (comparisonBarChanging) {
		comparisonBarX = (float)x / windowWidth;
		if (um4comparisonBarX != -1) {
			glUseProgram(program2);
			glUniform1f(um4comparisonBarX, comparisonBarX);
			glUseProgram(program);
		}
	} else {
		//mat4 rotation = rotate(mat4(), (float)deg2rad(y - downY), vec3(1, 0, 0)) *
			//rotate(mat4(), (float)deg2rad(x - downX), vec3(0, 1, 0));

		mat4 rotation = rotate(mat4(), (float)deg2rad(y - downY) * speed, cross(downCameraFront, downCameraUp)) *
			rotate(mat4(), (float)deg2rad(x - downX) * speed, downCameraUp);
		cameraFront = mat3(rotation) * downCameraFront;
		cameraUp = mat3(rotation) * downCameraUp;
		view = lookAt(cameraPos, cameraFront, cameraUp);
		//view = rotate(downView, (float)deg2rad(x - downX), vec3(0, 1, 0));
		//view = rotate(view, (float)deg2rad(y - downY), vec3(1, 0, 0));
	}

	glutPostRedisplay();
}

bool mouseInComparisonBar(int x, int y) {
	if (!comparisonBarBeing) return false;

	float px = (float)x / windowWidth, py = (float)y / windowHeight;
	return comparisonBarX - comparisonBarThickness < px && px < comparisonBarX + comparisonBarThickness && 0.4 < py && py < 0.6;
}

void recordMouse(int x, int y) {
	if (um4mouse != -1) {
		glUseProgram(program2);
		glUniform2i(um4mouse, x, y);
		glUseProgram(program);
	}

	if (mouseInComparisonBar(x, y)) {
		glutSetCursor(GLUT_CURSOR_LEFT_RIGHT);
	} else {
		glutSetCursor(GLUT_CURSOR_INHERIT);
	}
}

void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
		if (mouseInComparisonBar(x, y)) {
			comparisonBarChanging = true;
		} else {
			downX = x;
			downY = y;
			downCameraFront = cameraFront;
			downCameraUp = cameraUp;
		}
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
		if (comparisonBarChanging) comparisonBarChanging = false;
	}
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
	vec3 dPos;
	switch (key)
	{
	case 'w':
		//view = translate(view, vec3(0, 0, 1));
		dPos = normalize(cameraFront - cameraPos);
		break;
	case 's':
		//view = translate(view, vec3(0, 0, -1));
		dPos = -normalize(cameraFront - cameraPos);
		break;
	case 'a':
		//view = translate(view, vec3(-1, 0, 0));
		dPos = -normalize(cross(cameraFront, cameraUp));
		break;
	case 'd':
		//view = translate(view, vec3(1, 0, 0));
		dPos = normalize(cross(cameraFront, cameraUp));
		break;
	case 'z':
		dPos = normalize(cameraUp);
		break;
	case 'x':
		dPos = -normalize(cameraUp);
		break;
	default:
		break;
	}
	cameraPos += dPos;
	cameraFront += dPos;
	//cameraUp += dPos;
	view = lookAt(cameraPos, cameraFront, cameraUp);
	printf("cameraPos: (%.2f, %.2f, %.2f)\n", cameraPos.x, cameraPos.y, cameraPos.z);
	printf("cameraFront: (%.2f, %.2f, %.2f)\n", cameraFront.x, cameraFront.y, cameraFront.z);
	printf("cameraUp: (%.2f, %.2f, %.2f)\n", cameraUp.x, cameraUp.y, cameraUp.z);
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	glDeleteProgram(program2);
	char *shaderSource;
	switch(id)
	{
	case MENU_NO_FILTER:
		program2 = createProgram("fragment2.shader");
		break;
	case MENU_IMAGE_ABSTRACTION:
		program2 = createProgram("image abstraction.shader");
		break;
	case MENU_LAPLACIAN:
		program2 = createProgram("laplacian.shader");
		break;
	case MENU_SHRPEN:
		program2 = createProgram("sharpen.shader");
		break;
	case MENU_PIXELATION:
		program2 = createProgram("pixelation.shader");
		break;
	case MENU_FISH_EYE:
		program2 = createProgram("fish-eye.shader");
		break;
	case MENU_SINE_WAVE:
		program2 = createProgram("sine wave.shader");
		break;
	case MENU_RED_BLUE:
		program2 = createProgram("red-blue stereo.shader");
		break;
	case MENU_BLOOM:
		program2 = createProgram("bloom.shader");
		break;
	case MENU_MAGNIFIER:
		program2 = createProgram("magnifier.shader");
		break;
	default:
		break;
	}

	if (um4height != -1) {
		glUseProgram(program2);
		glUniform1i(um4height, windowHeight);
		glUseProgram(program);
	}

	comparisonBarBeing = id != MENU_NO_FILTER;

	glutPostRedisplay();
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("X1057117_AS3"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddMenuEntry("No Filter", MENU_NO_FILTER);
	glutAddMenuEntry("Image Abstraction", MENU_IMAGE_ABSTRACTION);
	glutAddMenuEntry("Laplacian Filter", MENU_LAPLACIAN);
	glutAddMenuEntry("Sharpen Filter", MENU_SHRPEN);
	glutAddMenuEntry("Pixelation", MENU_PIXELATION);
	glutAddMenuEntry("Fish-eye distortion", MENU_FISH_EYE);
	glutAddMenuEntry("Sine wave distortion", MENU_SINE_WAVE);
	glutAddMenuEntry("Red-Blue Stereo", MENU_RED_BLUE);
	glutAddMenuEntry("Bloom Effect", MENU_BLOOM);
	glutAddMenuEntry("Magnifier", MENU_MAGNIFIER);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 
	glutMotionFunc(traceMouse);
	glutPassiveMotionFunc(recordMouse);
	
	// Enter main event loop.
	glutMainLoop();

	return 0;
}