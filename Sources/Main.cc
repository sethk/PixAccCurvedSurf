#include "GLFWApp.hh"
#include "../Data/Teapot.h"
#include <istream>
#include <fstream>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SubLiME.h>

using std::runtime_error;
using std::array;
using std::string;
using std::ifstream;
using std::ostringstream;
using std::clog;
using std::endl;
using std::vector;
using glm::vec4;
using glm::vec3;
using glm::vec2;
using glm::mat4;
using glm::value_ptr;

static const GLuint threeD = 3;
static const GLuint numSlefeDivs = 3;

struct Slefe
{
    enum {LOWER, UPPER, NUM_BOUNDS};
    struct Bounds
    {
        vec3 points[numSlefeDivs + 1][numSlefeDivs + 1];
    } bounds[NUM_BOUNDS];
};

class PixAccCurvedSurf : public GLFWWindowedApp
{
    enum {VERTEX_ARRAY_TEAPOT, VERTEX_ARRAY_DEBUG, NUM_VERTEX_ARRAYS};
    GLuint vertexArrayObjects[NUM_VERTEX_ARRAYS];
    enum
    {
        BUFFER_CONTROL_POINTS,
        BUFFER_CONTROL_POINT_INDICES,
        BUFFER_DEBUG_VERTICES,
        BUFFER_DEBUG_INDICES,
        NUM_BUFFERS
    };
    GLuint buffers[NUM_BUFFERS];
    GLuint program;
    vec3 modelCentroid;
    GLint modelViewMatrixLocation;
	mat4 modelViewMatrix;
    GLint projectionMatrixLocation;
	mat4 projectionMatrix;
    bool showControlPoints = true;
    bool showControlMeshes = true;
	Slefe slefes[NumTeapotPatches];
    bool showSlefeTiles = true;
    GLint patchRange[2] = {5, 1};

    void
    LoadShader(GLenum type, const string &path)
    {
        GLuint shader = glCreateShader(type);

        ifstream file(path);
        if (!file)
            throw runtime_error("Could not open " + path);

        ostringstream oss;
        oss << file.rdbuf();

        string source = oss.str();
        const GLchar *c_source = source.c_str();
        glShaderSource(shader, 1, &c_source, NULL);

        glCompileShader(shader);
        try
        {
            CheckShaderStatus(shader, GL_COMPILE_STATUS, "glCompileShader(" + path + ")",
                              glGetShaderiv, glGetShaderInfoLog);
        }
        catch (std::runtime_error &error)
        {
            clog << "Error while compiling shader " << path << ':' << endl;
            clog << "<<<<<<<<<" << endl << source << endl << "<<<<<<<<<" << endl;
            throw;
        }

        glAttachShader(program, shader);
    }

    GLint
    GetUniformLocation(const char *name, bool required = true)
    {
        GLint location = glGetUniformLocation(program, name);
        if (location == -1 && required)
            throw runtime_error(string("Uniform location ") + name + " was not found in program");

        return location;
    }

    GLint
    GetAttribLocation(const char *name, bool required = true)
    {
        GLint location = glGetAttribLocation(program, name);
        if (location == -1 && required)
            throw runtime_error(string("Attribute location ") + name + " was not found in the program");

        return location;
    }

	void
	BindTeapotVertices()
	{
        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);
	}

	void
	SetCamera(mat4 &modelView, mat4 &projection)
	{
        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, value_ptr(modelView));
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, value_ptr(projection));
	}

	void
	Set3DCamera()
	{
		SetCamera(modelViewMatrix, projectionMatrix);
	}

	void
	RenderDebugPrimitives(GLenum type, GLfloat r, GLfloat g, GLfloat b, const vector<GLuint> &indices)
	{
		GLint positionLocation = GetAttribLocation("Position");
		glEnableVertexAttribArray(positionLocation);
		glVertexAttribPointer(positionLocation, threeD, GL_FLOAT, GL_FALSE, 0, 0);

		GLint colorLocation = GetUniformLocation("Color");
		glUniform4f(colorLocation, r, g, b, 1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_DEBUG_INDICES]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STREAM_DRAW);

		glDrawElements(type, indices.size(), GL_UNSIGNED_INT, 0);

        CheckGLErrors("RenderDebugPrimitives()");
	}

	void
	RenderControlPoints(bool showPatches)
	{
		BindTeapotVertices();

		vector<GLuint> anchorIndices;
		vector<GLuint> controlIndices;

		bool patchesOpen = false;
		if (showPatches)
			patchesOpen = ImGui::TreeNode("Patches");

		for (GLint i = patchRange[0]; i < patchRange[0] + patchRange[1]; ++i)
		{
			bool patchOpen = false;
			if (patchesOpen)
				patchOpen = ImGui::TreeNode((string("Patch ") + std::to_string(i)).c_str());

			for (GLuint j = 0; j < 4; ++j)
				for (GLuint k = 0; k < 4; ++k)
				{
					if (patchOpen)
					{
						const float (&vertex)[threeD] = TeapotVertices[TeapotIndices[i][j][k]];
						ImGui::Text("[%u][%u] = %f %f %f", j, k, vertex[0], vertex[1], vertex[2]);
					}

					if ((j == 0 || j == 3) && (k == 0 || k == 3))
						anchorIndices.push_back(TeapotIndices[i][j][k]);
					else
						controlIndices.push_back(TeapotIndices[i][j][k]);
				}

			if (patchOpen)
				ImGui::TreePop();
		}

		if (patchesOpen)
			ImGui::TreePop();

		Set3DCamera();

		glPointSize(5);
		RenderDebugPrimitives(GL_POINTS, 1, 1, 1, anchorIndices);
		RenderDebugPrimitives(GL_POINTS, 1, 0, 0, controlIndices);
	}

	void
	RenderControlMeshes()
	{
		BindTeapotVertices();

		vector<GLuint> indices;

		for (GLint i = patchRange[0]; i < patchRange[0] + patchRange[1]; ++i)
			for (GLuint j = 0; j < 4; ++j)
				for (GLuint k = 0; k < 4; ++k)
					if (k < 3)
					{
						indices.push_back(TeapotIndices[i][j][k]);
						indices.push_back(TeapotIndices[i][j][k + 1]);
						indices.push_back(TeapotIndices[i][k][j]);
						indices.push_back(TeapotIndices[i][k + 1][j]);
					}

		Set3DCamera();
		RenderDebugPrimitives(GL_LINES, 0.6, 0.6, 0.6, indices);
	}

	void
	ComputeSlefes()
	{
		for (GLint patchIndex = 0; patchIndex < NumTeapotPatches; ++patchIndex)
		{
			REAL coeff[4][4][threeD];
			for (GLuint u = 0; u < 4; ++u)
				for (GLuint v = 0; v < 4; ++v)
				{
					const float *vertex = TeapotVertices[TeapotIndices[patchIndex][u][v]];
					for (GLuint dim = 0; dim < threeD; ++dim)
						coeff[u][v][dim] = vertex[dim];
				}

			REAL lower[numSlefeDivs + 1][numSlefeDivs + 1][3];
			REAL upper[numSlefeDivs + 1][numSlefeDivs + 1][3];

			for (GLuint dim = 0; dim < threeD; ++dim)
				tpSlefe(coeff[0][0] + dim, sizeof(coeff[0]) / sizeof(REAL), sizeof(coeff[0][0]) / sizeof(REAL),
						3, 3, numSlefeDivs, numSlefeDivs,
						lower[0][0] + dim, upper[0][0] + dim,
						sizeof(lower[0]) / sizeof(REAL), sizeof(lower[0][0]) / sizeof(REAL));

			struct Slefe &slefe = slefes[patchIndex];
			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					slefe.bounds[Slefe::LOWER].points[u][v] = vec3(lower[u][v][0], lower[u][v][1], lower[u][v][2]);
					slefe.bounds[Slefe::UPPER].points[u][v] = vec3(upper[u][v][0], upper[u][v][1], upper[u][v][2]);
				}
		}
	}

    void
    RenderSlefeTiles(bool showPatches)
    {
		bool patchesOpen = false;
		if (showPatches)
			patchesOpen = ImGui::TreeNode("Patch slefes");

		const vec3 *slefeBase = &(slefes[0].bounds[0].points[0][0]);
		vector<GLuint> slefeIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			Slefe &slefe = slefes[patchIndex];

			bool showPatch = false;
			if (patchesOpen && showPatches)
				showPatch = ImGui::TreeNode((string("Patch ") + std::to_string(patchIndex)).c_str());

			for (GLuint whichBound = 0; whichBound < Slefe::NUM_BOUNDS; ++whichBound)
			{
				struct Slefe::Bounds &bounds = slefe.bounds[whichBound];
				for (GLuint udiv = 0; udiv <= numSlefeDivs; ++udiv)
				{
					for (GLuint vdiv = 0; vdiv <= numSlefeDivs; ++vdiv)
					{
						if (showPatch)
							ImGui::Text("%s[%u][%u]: %f, %f, %f",
										(whichBound == Slefe::LOWER) ? "Lower" : "Upper", udiv, vdiv,
										bounds.points[udiv][vdiv][0],
										bounds.points[udiv][vdiv][1],
										bounds.points[udiv][vdiv][2]);

						vec3 point;
						for (GLuint dim = 0; dim < threeD; ++dim)
							point[dim] = bounds.points[udiv][vdiv][dim];

						if (udiv < numSlefeDivs)
						{
							slefeIndices.push_back(&(bounds.points[udiv][vdiv]) - slefeBase);
							slefeIndices.push_back(&(bounds.points[udiv + 1][vdiv]) - slefeBase);
						}

						if (vdiv < numSlefeDivs)
						{
							slefeIndices.push_back(&(bounds.points[udiv][vdiv]) - slefeBase);
							slefeIndices.push_back(&(bounds.points[udiv][vdiv + 1]) - slefeBase);
						}

						if (whichBound == Slefe::LOWER)
						{
							struct Slefe::Bounds &upperBounds = slefe.bounds[Slefe::UPPER];
							slefeIndices.push_back(&(bounds.points[udiv][vdiv]) - slefeBase);
							slefeIndices.push_back(&(upperBounds.points[udiv][vdiv]) - slefeBase);
						}
					}
				}
			}

			if (showPatch)
				ImGui::TreePop();
		}

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(slefes), slefes, GL_STREAM_DRAW);

		Set3DCamera();
		RenderDebugPrimitives(GL_LINES, 0, 1, 1, slefeIndices);

		if (patchesOpen)
			ImGui::TreePop();
    }

	void
	RenderSlefeBoxes()
	{
		int width, height;
		glfwGetWindowSize(window.get(), &width, &height);
		vec2 halfWindowSize = vec2(width / 2.0, height / 2.0);

		vector<vec3> boxVertices;
		vector<GLuint> boxIndices;
		vector<GLuint> screenRectIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					Slefe &slefe = slefes[patchIndex];
					vec3 &lower = slefe.bounds[Slefe::LOWER].points[u][v];
					vec3 &upper = slefe.bounds[Slefe::UPPER].points[u][v];
					vec3 center = (lower + upper) / vec3(2.0);
					vec3 halfSize = (upper - lower) / vec3(2.0);

					GLuint startIndex = boxVertices.size();

					boxVertices.push_back(center + halfSize * vec3(1, -1, -1));
					boxVertices.push_back(center + halfSize * vec3(1, -1, 1));
					boxVertices.push_back(center + halfSize * vec3(1, 1, 1));
					boxVertices.push_back(center + halfSize * vec3(1, 1, -1));
					boxVertices.push_back(center + halfSize * vec3(-1, -1, -1));
					boxVertices.push_back(center + halfSize * vec3(-1, -1, 1));
					boxVertices.push_back(center + halfSize * vec3(-1, 1, 1));
					boxVertices.push_back(center + halfSize * vec3(-1, 1, -1));

					for (GLuint i = 0; i < 4; ++i)
					{
						boxIndices.push_back(startIndex + i);
						boxIndices.push_back(startIndex + ((i + 1) % 4));
						boxIndices.push_back(startIndex + 4 + i);
						boxIndices.push_back(startIndex + 4 + ((i + 1) % 4));

						boxIndices.push_back(startIndex + i);
						boxIndices.push_back(startIndex + 4 + i);
					}

					vec2 screenRectMin = vec2(FLT_MAX, FLT_MAX), screenRectMax = vec2(FLT_MIN, FLT_MIN);
					for (GLuint i = 0; i < 8; ++i)
					{
						vec4 vertex = vec4(boxVertices[startIndex + i], 1);
						vec4 clipVertex = projectionMatrix * modelViewMatrix * vertex;
						vec2 normVertex = vec2(clipVertex[0], clipVertex[1]) / vec2(clipVertex.w);
						vec3 winVertex = vec3(halfWindowSize + normVertex * halfWindowSize, 0);

						screenRectMin.x = glm::min(screenRectMin.x, winVertex.x);
						screenRectMin.y = glm::min(screenRectMin.y, winVertex.y);
						screenRectMax.x = glm::max(screenRectMax.x, winVertex.x);
						screenRectMax.y = glm::max(screenRectMax.y, winVertex.y);
					}

					GLuint rectStartIndex = boxVertices.size();
					boxVertices.push_back(vec3(screenRectMin, 0));
					boxVertices.push_back(vec3(screenRectMin.x, screenRectMax.y, 0));
					boxVertices.push_back(vec3(screenRectMax, 0));
					boxVertices.push_back(vec3(screenRectMax.x, screenRectMin.y, 0));

					for (GLuint i = 0; i < 4; ++i)
					{
						screenRectIndices.push_back(rectStartIndex + i);
						screenRectIndices.push_back(rectStartIndex + ((i + 1) % 4));
					}
				}
		}

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER, boxVertices.size() * sizeof(boxVertices[0]), boxVertices.data(), GL_STREAM_DRAW);

		Set3DCamera();
		RenderDebugPrimitives(GL_LINES, 1, 0, 1, boxIndices);

		mat4 identity(1);
		mat4 screenSpace = glm::ortho(0.0f, float(width), 0.0f, float(height), -1.0f, 1.0f);
		SetCamera(identity, screenSpace);
		RenderDebugPrimitives(GL_LINES, 0.5, 0.5, 1, screenRectIndices);

		CheckGLErrors("RenderSlefeBoxes()");
	}

public:
    PixAccCurvedSurf() : GLFWWindowedApp("PixAccCurvedSurf")
    {
        glGenVertexArrays(NUM_VERTEX_ARRAYS, vertexArrayObjects);
        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_TEAPOT]);

        glGenBuffers(NUM_BUFFERS, buffers);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(TeapotVertices), TeapotVertices, GL_STATIC_DRAW);

        modelCentroid = vec3(0);
        for (GLuint i = 0; i < NumTeapotVertices; ++i)
            modelCentroid+= vec3(TeapotVertices[i][0], TeapotVertices[i][1], TeapotVertices[i][2]);
        modelCentroid/= NumTeapotVertices;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TeapotIndices), TeapotIndices, GL_STATIC_DRAW);

        glPatchParameteri(GL_PATCH_VERTICES, NumTeapotVerticesPerPatch);

        program = glCreateProgram();

        LoadShader(GL_VERTEX_SHADER, "Debug.vert");
        LoadShader(GL_FRAGMENT_SHADER, "Debug.frag");

        glLinkProgram(program);
        CheckShaderStatus(program, GL_LINK_STATUS, "glLinkProgram()", glGetProgramiv, glGetProgramInfoLog);

        glUseProgram(program);

        projectionMatrixLocation = GetUniformLocation("ProjectionMatrix");
        modelViewMatrixLocation = GetUniformLocation("ModelViewMatrix");

        glClearColor(0, 0, 0, 1);

        setenv("SUBLIMEPATH", ".", false);
        InitBounds();

		ComputeSlefes();

        CheckGLErrors("PixAccCurvedSurf()");
    }

	void
	RenderPatches()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);
	}

    virtual void
    Render(double /*time*/)
    {
        ImWindow gui("Controls", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        glClear(GL_COLOR_BUFFER_BIT);

		static vec3 cameraOffset(0, 0, 5);
        static float cameraParams[2];
        static bool perspective = true;
        static float fov = 70;
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
			ImGui::DragFloat3("X/Y/Z Offset", value_ptr(cameraOffset), -0.01, -10, 10.0);
            ImGui::DragFloat2("Azi./Elev.", cameraParams, 1, -179, 180, "%.0f deg");
            ImGui::Checkbox("Perspective", &perspective);
            if (perspective)
                ImGui::DragFloat("FOV", &fov, 1, 30, 120);
        }

		ImGuiIO &io = ImGui::GetIO();
		if (!io.WantCaptureMouse)
		{
			if (ImGui::IsMouseDown(0))
			{
				ImVec2 delta = ImGui::GetMouseDragDelta(0, 0);
				cameraParams[0]+= delta.x / 3;
				cameraParams[1]+= delta.y / 3;
				ImGui::ResetMouseDragDelta(0);
			}
			else if (ImGui::IsMouseDown(1))
			{
				ImVec2 delta = ImGui::GetMouseDragDelta(1, 0);
				cameraOffset[0]-= delta.x / 50.0;
				cameraOffset[1]+= delta.y / 50.0;
				ImGui::ResetMouseDragDelta(1);
			}

			if (io.MouseWheel != 0)
				cameraOffset[2]+= io.MouseWheel / 10.0;
		}
		while (cameraParams[0] <= -180)
			cameraParams[0]+= 360;
		while (cameraParams[0] > 180)
			cameraParams[0]-= 360;
		cameraParams[1] = glm::clamp(cameraParams[1], -90.0f, 90.0f);

        modelViewMatrix = glm::translate(mat4(1), -cameraOffset);
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[1]), vec3(1, 0, 0));
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[0]), vec3(0, 1, 0));

        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);
		float aspect = float(width) / float(height);
        if (perspective)
            projectionMatrix = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
        else
            projectionMatrix = glm::ortho(-3.0f * aspect, 3.0f * aspect, -3.0f, 3.0f, -10.0f, 10.0f);

        static vec3 modelPos;
		bool showPatches = false;
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Position", value_ptr(modelPos), 0.01, -1, 1, "%.2f");
            if (ImGui::DragInt2("Draw patches", patchRange, 0.2, 0, NumTeapotPatches))
            {
                patchRange[0] = std::min(std::max(patchRange[0], 0), NumTeapotPatches - 1);
                patchRange[1] = std::max(std::min(patchRange[1], NumTeapotPatches - patchRange[0]), 1);
            }
            ImGui::Checkbox("Show control points", &showControlPoints);
            ImGui::Checkbox("Show control meshes", &showControlMeshes);

			showPatches = true;
        }
        modelViewMatrix = glm::translate(modelViewMatrix, modelPos - modelCentroid);

		if (showControlPoints)
			RenderControlPoints(showPatches);
		if (showControlMeshes)
			RenderControlMeshes();

		static bool showSlefeBoxes = true;
		bool showSlefeNodes = ImGui::CollapsingHeader("iPASS", ImGuiTreeNodeFlags_DefaultOpen);
		if (showSlefeNodes)
        {
            ImGui::Checkbox("Show slefe tiles", &showSlefeTiles);
			ImGui::Checkbox("Show slefe boxes", &showSlefeBoxes);
        }

		if (showSlefeTiles)
			RenderSlefeTiles(showSlefeNodes);
		if (showSlefeBoxes)
			RenderSlefeBoxes();

        CheckGLErrors("Render()");
    }
};

int
main()
{
    PixAccCurvedSurf app;
    app.Run();
    return 0;
}
