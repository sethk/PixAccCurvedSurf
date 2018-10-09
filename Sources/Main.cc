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

struct TessParams
{
	struct SlefeBoxes
	{
		struct AABB worldAxisBox;
		struct AABB screenAxisBox;
		float maxScreenEdge;
	} slefeBoxes[numSlefeDivs + 1][numSlefeDivs + 1];
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
    ShaderProgram debugProgram;
    vec3 modelCentroid;
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
    bool showControlPoints = true;
    bool showControlMeshes = true;
	Slefe slefes[NumTeapotPatches];
    bool showSlefeTiles = true;
    GLint patchRange[2] = {5, 1};

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
	RenderControlPoints(bool showPatches)
	{
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
	RenderSlefeBoxes(const TessParams patchTessParams[])
	{
		vector<vec3> boxVertices;
		vector<GLuint> boxIndices;
		vector<GLuint> screenRectIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			const TessParams &tessParams = patchTessParams[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					const TessParams::SlefeBoxes &boxes = tessParams.slefeBoxes[u][v];

					RenderAABBWireframe(boxes.worldAxisBox, boxVertices, boxIndices);
					RenderAABBWireframe(boxes.screenAxisBox, boxVertices, screenRectIndices);
				}
		}

		debugProgram.Use();

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
            modelCentroid+= vec3(TeapotVertices[i][0], TeapotVertices[i][1], TeapotVertices[i][2]);
        modelCentroid/= NumTeapotVertices;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TeapotIndices), TeapotIndices, GL_STATIC_DRAW);

        glPatchParameteri(GL_PATCH_VERTICES, NumTeapotVerticesPerPatch);

        debugProgram.LoadShader(GL_VERTEX_SHADER, "Debug.vert");
        debugProgram.LoadShader(GL_FRAGMENT_SHADER, "Debug.frag");

		debugProgram.Link();

        glClearColor(0, 0, 0, 1);

        setenv("SUBLIMEPATH", ".", false);
        InitBounds();

		ComputeSlefes();

        CheckGLErrors("PixAccCurvedSurf()");
    }

	void
	ComputeTessParams(struct TessParams patchTessParams[])
	{
		int width, height;
		glfwGetWindowSize(window.get(), &width, &height);
		vec2 halfWindowSize = vec2(width / 2.0, height / 2.0);

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			Slefe &slefe = slefes[patchIndex];
			TessParams &tessParams = patchTessParams[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					struct TessParams::SlefeBoxes &boxes = tessParams.slefeBoxes[u][v];

					vec3 &lower = slefe.bounds[Slefe::LOWER].points[u][v];
					vec3 &upper = slefe.bounds[Slefe::UPPER].points[u][v];

					vec3 center = (lower + upper) / vec3(2.0);
					vec3 halfSize = (upper - lower) / vec3(2.0);

					boxes.worldAxisBox.min = center - halfSize;
					boxes.worldAxisBox.max = center + halfSize;

					vec3 worldBoxVertices[8];
					GetAABBVertices(boxes.worldAxisBox, worldBoxVertices);

					vec3 &screenMin = boxes.screenAxisBox.min;
					vec3 &screenMax = boxes.screenAxisBox.max;

					screenMin = vec3(FLT_MAX);
					screenMax = vec3(FLT_MIN);

					for (GLuint i = 0; i < 8; ++i)
					{
						vec4 worldVertex = vec4(worldBoxVertices[i], 1);
						vec4 clipVertex = projectionMatrix * modelViewMatrix * worldVertex;
						vec2 normVertex = vec2(clipVertex[0], clipVertex[1]) / vec2(clipVertex.w);
						vec3 winVertex = vec3(halfWindowSize + normVertex * halfWindowSize, 0);

						screenMin.x = glm::min(screenMin.x, winVertex.x);
						screenMin.y = glm::min(screenMin.y, winVertex.y);
						screenMin.z = glm::min(screenMin.z, winVertex.z);
						screenMax.x = glm::max(screenMax.x, winVertex.x);
						screenMax.y = glm::max(screenMax.y, winVertex.y);
						screenMin.z = glm::max(screenMax.z, winVertex.z);
					}

					boxes.maxScreenEdge = glm::max(screenMax.x - screenMin.x, screenMax.y - screenMin.y);
				}
		}
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

		static bool showSlefeBoxes = true;
		bool showSlefeNodes = ImGui::CollapsingHeader("iPASS", ImGuiTreeNodeFlags_DefaultOpen);
		if (showSlefeNodes)
        {
            ImGui::Checkbox("Show slefe tiles", &showSlefeTiles);
			ImGui::Checkbox("Show slefe boxes", &showSlefeBoxes);
        }

		struct TessParams patchTessParams[NumTeapotPatches];
		ComputeTessParams(patchTessParams);
		RenderPatches();

		if (showControlPoints)
			RenderControlPoints(showPatches);
		if (showControlMeshes)
			RenderControlMeshes();
		if (showSlefeTiles)
			RenderSlefeTiles(showSlefeNodes);
		if (showSlefeBoxes)
			RenderSlefeBoxes(patchTessParams);

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
