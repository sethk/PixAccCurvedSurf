#include "GLFWApp.hh"
#include "ShaderProgram.hh"
#include "../Data/Teapot.h"
#include <istream>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <SubLiME.h>

using std::runtime_error;
using std::array;
using std::string;
using std::ostringstream;
using std::clog;
using std::endl;
using std::vector;
using std::unique_ptr;
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

struct AABB
{
	vec3 min, max;
};

struct SlefeBox
{
	struct AABB worldAxisBox;
	struct AABB screenAxisBox;
	float maxScreenEdge;
};

class PixAccCurvedSurf : public GLFWWindowedApp
{
    enum {VERTEX_ARRAY_TEAPOT, VERTEX_ARRAY_DEBUG, NUM_VERTEX_ARRAYS};
    GLuint vertexArrayObjects[NUM_VERTEX_ARRAYS];
    enum
    {
        BUFFER_CONTROL_POINTS,
        BUFFER_CONTROL_POINT_INDICES,
		BUFFER_TESS_LEVELS,
        BUFFER_DEBUG_VERTICES,
        BUFFER_DEBUG_INDICES,
        NUM_BUFFERS
    };
    GLuint buffers[NUM_BUFFERS];
	unique_ptr<ShaderProgram> mainProgram;
    ShaderProgram debugProgram;
    vec3 modelCentroid;
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	enum {TESS_IPASS, TESS_UNIFORM};
	int tessMode = TESS_IPASS;
	int uniformLevel = 9;
	Slefe slefes[NumTeapotPatches];
	SlefeBox patchSlefeBoxes[NumTeapotPatches][numSlefeDivs + 1][numSlefeDivs + 1];
    GLint patchRange[2] = {0, NumTeapotPatches};
	bool showModel = true;
	bool showWireframe = false;
	bool showNormals = false;
	float ambientIntensity = 0.1;
	vec3 diffuseColor = vec3(0.5, 0, 0);
	vec3 lightPosition = vec3(10);
	float lightIntensity = 1;
	float shininess = 32;
	bool showControlPoints = false;
	bool showControlMeshes = false;
	bool showSlefeTiles = false;
	bool showSlefeBoxes = false;

	void
	RebuildMainProgram()
	{
		mainProgram = nullptr;
		mainProgram = unique_ptr<ShaderProgram>(new ShaderProgram());

		mainProgram->LoadShader(GL_VERTEX_SHADER, "iPASS.vert");
		mainProgram->LoadShader(GL_TESS_CONTROL_SHADER, "iPASS.tesc");
		mainProgram->LoadShader(GL_TESS_EVALUATION_SHADER, "iPASS.tese");
		if (showNormals)
			mainProgram->LoadShader(GL_FRAGMENT_SHADER, "DebugNormal.frag");
		else
			mainProgram->LoadShader(GL_FRAGMENT_SHADER, "BlinnPhong.frag");

		mainProgram->Link();
	}

	void
	Set3DCamera(ShaderProgram &program)
	{
		program.SetCamera(modelViewMatrix, projectionMatrix);
	}

	void
	RenderDebugPrimitives(GLenum type, GLfloat r, GLfloat g, GLfloat b, const vector<GLuint> &indices)
	{
		GLint positionLocation = debugProgram.GetAttribLocation("Position");
		glEnableVertexAttribArray(positionLocation);
		glVertexAttribPointer(positionLocation, threeD, GL_FLOAT, GL_FALSE, 0, 0);

		GLint colorLocation = debugProgram.GetUniformLocation("Color");
		glUniform4f(colorLocation, r, g, b, 1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_DEBUG_INDICES]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STREAM_DRAW);

		glDrawElements(type, indices.size(), GL_UNSIGNED_INT, 0);

        CheckGLErrors("RenderDebugPrimitives()");
	}

	void
	RenderControlPoints()
	{
		vector<GLuint> anchorIndices;
		vector<GLuint> controlIndices;

		bool patchesOpen = (showDebugWindow && ImGui::TreeNode("Patches"));

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

		debugProgram.Use();

        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);

		Set3DCamera(debugProgram);

		glPointSize(5);
		RenderDebugPrimitives(GL_POINTS, 1, 1, 1, anchorIndices);
		RenderDebugPrimitives(GL_POINTS, 1, 0, 0, controlIndices);
	}

	void
	RenderControlMeshes()
	{
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

		debugProgram.Use();

        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);

		Set3DCamera(debugProgram);

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
	ComputeTessLevels(const SlefeBox patchSlefeBoxes[NumTeapotPatches][numSlefeDivs + 1][numSlefeDivs + 1],
	                  float vertexTessLevels[NumTeapotVertices])
	{
		for (GLuint i = 0; i < NumTeapotVertices; ++i)
			vertexTessLevels[i] = 0;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			const SlefeBox (&slefeBoxes)[4][4] = patchSlefeBoxes[patchIndex];

			float maxScreenEdge = 0;

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
					maxScreenEdge = glm::max(slefeBoxes[u][v].maxScreenEdge, maxScreenEdge);

			float tessLevel = numSlefeDivs * sqrt(maxScreenEdge);

			vertexTessLevels[TeapotIndices[patchIndex][0][2]] =
					vertexTessLevels[TeapotIndices[patchIndex][2][3]] =
					vertexTessLevels[TeapotIndices[patchIndex][3][1]] =
					vertexTessLevels[TeapotIndices[patchIndex][1][0]] = tessLevel;

			vertexTessLevels[TeapotIndices[patchIndex][1][1]] = tessLevel;
		}
	}

    void
    RenderSlefeTiles()
    {
		bool patchesOpen = (showDebugWindow && ImGui::TreeNode("Patch slefes"));

		const vec3 *slefeBase = &(slefes[0].bounds[0].points[0][0]);
		vector<GLuint> slefeIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			Slefe &slefe = slefes[patchIndex];

			bool showPatch = false;
			if (patchesOpen)
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

		debugProgram.Use();

        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(slefes), slefes, GL_STREAM_DRAW);

		Set3DCamera(debugProgram);
		RenderDebugPrimitives(GL_LINES, 0, 1, 1, slefeIndices);

		if (patchesOpen)
			ImGui::TreePop();
    }

	void
	GetAABBVertices(const AABB &box, vec3 vertices[8])
	{
		vertices[0] = box.min;
		vertices[1] = vec3(box.min.x, box.min.y, box.max.z);
		vertices[2] = vec3(box.min.x, box.max.y, box.max.z);
		vertices[3] = vec3(box.min.x, box.max.y, box.min.z);
		vertices[4] = vec3(box.max.x, box.min.y, box.min.z);
		vertices[5] = vec3(box.max.x, box.min.y, box.max.z);
		vertices[6] = box.max;
		vertices[7] = vec3(box.max.x, box.max.y, box.min.z);
	}

	void
	RenderAABBWireframe(const AABB &box, vector<vec3> &vertices, vector<GLuint> &indices)
	{
		GLuint startIndex = vertices.size();
		vertices.resize(startIndex + 8);

		GetAABBVertices(box, vertices.data() + startIndex);

		for (GLuint i = 0; i < 4; ++i)
		{
			indices.push_back(startIndex + i);
			indices.push_back(startIndex + ((i + 1) % 4));
			indices.push_back(startIndex + 4 + i);
			indices.push_back(startIndex + 4 + ((i + 1) % 4));

			indices.push_back(startIndex + i);
			indices.push_back(startIndex + 4 + i);
		}
	}

	void
	RenderSlefeBoxes()
	{
		vector<vec3> boxVertices;
		vector<GLuint> boxIndices;
		vector<GLuint> screenRectIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			const SlefeBox (&slefeBoxes)[numSlefeDivs + 1][numSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					const SlefeBox &box = slefeBoxes[u][v];

					RenderAABBWireframe(box.worldAxisBox, boxVertices, boxIndices);
					RenderAABBWireframe(box.screenAxisBox, boxVertices, screenRectIndices);
				}
		}

		debugProgram.Use();

		glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_TEAPOT]);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER, boxVertices.size() * sizeof(boxVertices[0]), boxVertices.data(), GL_STREAM_DRAW);

		Set3DCamera(debugProgram);

		RenderDebugPrimitives(GL_LINES, 1, 0, 1, boxIndices);

		mat4 identity(1);

		int width, height;
		glfwGetWindowSize(window.get(), &width, &height);
		mat4 screenSpace = glm::ortho(0.0f, float(width), 0.0f, float(height), -1.0f, 1.0f);

		debugProgram.SetCamera(identity, screenSpace);

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
            modelCentroid+= vec3(0/*TeapotVertices[i][0]*/, TeapotVertices[i][1], 0/*TeapotVertices[i][2]*/);
        modelCentroid/= NumTeapotVertices;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TeapotIndices), TeapotIndices, GL_STATIC_DRAW);

        RebuildMainProgram();

        glPatchParameteri(GL_PATCH_VERTICES, NumTeapotVerticesPerPatch);

        debugProgram.LoadShader(GL_VERTEX_SHADER, "Debug.vert");
        debugProgram.LoadShader(GL_FRAGMENT_SHADER, "UniformColor.frag");

		debugProgram.Link();

        glClearColor(0, 0, 0, 1);

        setenv("SUBLIMEPATH", ".", false);
        InitBounds();

		ComputeSlefes();

		glEnable(GL_DEPTH_TEST);

        CheckGLErrors("PixAccCurvedSurf()");
    }

	void
	ComputeSlefeBoxes()
	{
		int width, height;
		glfwGetWindowSize(window.get(), &width, &height);
		vec3 halfWindowSize = vec3(width / 2.0, height / 2.0, 0.5);

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			Slefe &slefe = slefes[patchIndex];
			SlefeBox (&slefeBoxes)[numSlefeDivs + 1][numSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					struct SlefeBox &box = slefeBoxes[u][v];

					vec3 &lower = slefe.bounds[Slefe::LOWER].points[u][v];
					vec3 &upper = slefe.bounds[Slefe::UPPER].points[u][v];

					vec3 center = (lower + upper) / vec3(2.0);
					vec3 halfSize = (upper - lower) / vec3(2.0);

					box.worldAxisBox.min = center - halfSize;
					box.worldAxisBox.max = center + halfSize;

					vec3 worldBoxVertices[8];
					GetAABBVertices(box.worldAxisBox, worldBoxVertices);

					vec3 &screenMin = box.screenAxisBox.min;
					vec3 &screenMax = box.screenAxisBox.max;

					screenMin = vec3(FLT_MAX);
					screenMax = vec3(FLT_MIN);

					for (GLuint i = 0; i < 8; ++i)
					{
						vec4 worldVertex = vec4(worldBoxVertices[i], 1);
						vec4 clipVertex = projectionMatrix * modelViewMatrix * worldVertex;
						vec3 normVertex = vec3(clipVertex[0], clipVertex[1], clipVertex[2]) / vec3(clipVertex.w);
						vec3 winVertex = halfWindowSize + normVertex * halfWindowSize;

						screenMin.x = glm::min(screenMin.x, winVertex.x);
						screenMin.y = glm::min(screenMin.y, winVertex.y);
						screenMin.z = glm::min(screenMin.z, winVertex.z);
						screenMax.x = glm::max(screenMax.x, winVertex.x);
						screenMax.y = glm::max(screenMax.y, winVertex.y);
						screenMin.z = glm::max(screenMax.z, winVertex.z);
					}

					box.maxScreenEdge = glm::max(screenMax.x - screenMin.x, screenMax.y - screenMin.y);
				}
		}
	}

	void
	RenderModel(const float vertexTessLevels[NumTeapotPatches])
	{
		glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_TEAPOT]);

		//
        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);
		//

		mainProgram->Use();

		GLint positionLocation = mainProgram->GetAttribLocation("Position");
		glEnableVertexAttribArray(positionLocation);
		glVertexAttribPointer(positionLocation, threeD, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_TESS_LEVELS]);
		glBufferData(GL_ARRAY_BUFFER, NumTeapotVertices * sizeof(vertexTessLevels[0]), vertexTessLevels, GL_STREAM_DRAW);

		GLint tessLevelLocation = mainProgram->GetAttribLocation("TessLevel");
		glEnableVertexAttribArray(tessLevelLocation);
		glVertexAttribPointer(tessLevelLocation, 1, GL_FLOAT, GL_FALSE, 0, 0);

		Set3DCamera(*mainProgram);

		if (!showNormals)
		{
			mainProgram->SetUniform("AmbientIntensity", ambientIntensity);
			mainProgram->SetUniform("DiffuseColor", diffuseColor);
			vec3 worldLightPos = vec3(modelViewMatrix * vec4(lightPosition, 1));
			mainProgram->SetUniform("LightPosition", worldLightPos);
			mainProgram->SetUniform("LightIntensity", lightIntensity);
			mainProgram->SetUniform("Shininess", shininess);
		}

		if (showModel)
			glDrawElements(GL_PATCHES,
					NumTeapotVerticesPerPatch * patchRange[1],
					GL_UNSIGNED_INT, (void *)(patchRange[0] * sizeof(TeapotIndices[0])));

		if (showWireframe)
		{
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(GL_INVERT);

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			static float polygonOffset[2] = {0, -30};
			if (showDebugWindow)
				ImGui::DragFloat2("Polygon offset", polygonOffset, 0.01, -1000, 1000);
			glPolygonOffset(polygonOffset[0], polygonOffset[1]);

			glEnable(GL_POLYGON_OFFSET_LINE);

			glDrawElements(GL_PATCHES,
					NumTeapotVerticesPerPatch * patchRange[1],
					GL_UNSIGNED_INT, (void *)(patchRange[0] * sizeof(TeapotIndices[0])));

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDisable(GL_POLYGON_OFFSET_LINE);

			glDisable(GL_COLOR_LOGIC_OP);
		}

		glBindVertexArray(0);
	}

	void
	RenderUI(void)
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImWindow gui("Controls", NULL, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Combo("Tess. mode", &tessMode, "iPASS\0Uniform\0\0");
		if (tessMode == TESS_IPASS)
		{
			if (ImGui::CollapsingHeader("iPASS", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Show slefe tiles", &showSlefeTiles);
				ImGui::Checkbox("Show slefe boxes", &showSlefeBoxes);
			}
		}
		else
			ImGui::DragInt("Level", &uniformLevel, 0.2, 1, 100);

		static vec3 cameraOffset(0, 0, 5);
        static float cameraParams[2] = {0, 30};
        static bool perspective = true;
        static float fov = 70;

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

        if (ImGui::CollapsingHeader("Camera"))
        {
			ImGui::DragFloat3("X/Y/Z Offset", value_ptr(cameraOffset), -0.01, -10, 10.0);
            ImGui::DragFloat2("Azi./Elev.", cameraParams, 1, -179, 180, "%.0f deg");
            ImGui::Checkbox("Perspective", &perspective);
            if (perspective)
                ImGui::DragFloat("FOV", &fov, 1, 30, 120);
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
        if (ImGui::CollapsingHeader("Model"))
        {
            ImGui::DragFloat3("Position", value_ptr(modelPos), 0.01, -10, 10, "%.2f");
            if (ImGui::DragInt2("Draw patches", patchRange, 0.2, 0, NumTeapotPatches))
            {
                patchRange[0] = std::min(std::max(patchRange[0], 0), NumTeapotPatches - 1);
                patchRange[1] = std::max(std::min(patchRange[1], NumTeapotPatches - patchRange[0]), 1);
            }
			ImGui::Checkbox("Solid", &showModel);
			ImGui::SameLine();
			ImGui::Checkbox("Wireframe", &showWireframe);
            ImGui::Checkbox("Control points", &showControlPoints);
			ImGui::SameLine();
            ImGui::Checkbox("Control meshes", &showControlMeshes);
        }

        modelViewMatrix = glm::translate(modelViewMatrix, modelPos - modelCentroid);

		if (ImGui::CollapsingHeader("Material"))
		{
			bool materialChanged = false;

			materialChanged|= ImGui::Checkbox("Show normals", &showNormals);

			ImGui::SliderFloat("Ambient", &ambientIntensity, 0, 1);
			ImGui::ColorEdit3("Diffuse col.", value_ptr(diffuseColor), ImGuiColorEditFlags_NoInputs);
			ImGui::DragFloat3("Light pos.", value_ptr(lightPosition), 0.1, -100, 100);
			ImGui::SliderFloat("Intensity", &lightIntensity, 0, 1);
			ImGui::SliderFloat("Shininess", &shininess, 4, 120);

			if (materialChanged)
				RebuildMainProgram();
		}
	}

    virtual void
    Render(double /*time*/)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RenderUI();

	    float vertexTessLevels[NumTeapotVertices];
		if (tessMode == TESS_IPASS)
		{
			ComputeSlefeBoxes();
			ComputeTessLevels(vertexTessLevels);
		}
		else
			for (GLuint i = 0; i < NumTeapotVertices; ++i)
				vertexTessLevels[i] = uniformLevel;

		RenderModel(vertexTessLevels);

		if (showControlPoints)
			RenderControlPoints();
		if (showControlMeshes)
			RenderControlMeshes();
		if (showSlefeTiles)
			RenderSlefeTiles();
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
