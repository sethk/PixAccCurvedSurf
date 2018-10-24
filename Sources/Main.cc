#include "GLFWApp.hh"
#include "ShaderProgram.hh"
#include "AnimationCurve.hh"
#include "../Data/Teapot.h"
#include <istream>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <SubLiME.h>
#include <QuickHull.hpp>

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
static const GLuint maxSlefeDivs = 9;
static const float minCameraZ = 0.1f;
static const float maxCameraZ = 100.0f;

struct Slefe
{
    enum {LOWER, UPPER, NUM_BOUNDS};
    struct Bounds
    {
        vec3 points[maxSlefeDivs + 1][maxSlefeDivs + 1];
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

// For debugging slefe tiles:
typedef quickhull::QuickHull<float> QuickHull;
typedef quickhull::HalfEdgeMesh<float, size_t> HalfEdgeMesh;

class PixAccCurvedSurf : public GLFWWindowedApp
{
	// Model data
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
    vec3 modelCentroid;
    GLint patchRange[2] = {0, NumTeapotPatches};

	// Shaders
	unique_ptr<ShaderProgram> mainProgram;
    ShaderProgram debugProgram;

	// Camera
	mat4 modelViewMatrix;
	mat4 projectionMatrix;
	GLint numSampleBuffers;
	bool useMultiSampling = true;

	// Tessellation
	enum {TESS_IPASS, TESS_UNIFORM};
	int tessMode = TESS_IPASS;
	int uniformLevel = 11;
	Slefe slefes[NumTeapotPatches];
	bool slefesChanged = true;
	bool slefeBoxesChanged;
	bool slefeTilesChanged;
	GLuint numSlefeDivs = 3;
	bool fracTessLevels = true;
	SlefeBox patchSlefeBoxes[NumTeapotPatches][maxSlefeDivs + 1][maxSlefeDivs + 1];
	vec3 slefeBoxVertices[NumTeapotPatches][maxSlefeDivs + 1][maxSlefeDivs + 1][8];
	vector<vec3> slefeTileVertices;
	vector<GLuint> slefeTileIndices;
	GLuint patchSlefeTileIndices[NumTeapotPatches][2]; // first index, last index

	// Scene
	vec3 backgroundColor = vec3(24 / 255.0);
	float ambientIntensity = 0.1;
	vec3 lightPosition = vec3(10);
	float lightIntensity = 1;

	// Material
	bool twoSided = true;
	bool showNormals = false;
	vec3 diffuseColor1 = vec3(0.5, 0, 0);
	vec3 diffuseColor2 = vec3(0);
	GLuint texture;
	bool useTexture = true;
	GLint textureRepeat = 5;
	const GLenum minFilters[6] =
	{
		GL_NEAREST,
		GL_LINEAR,
		GL_NEAREST_MIPMAP_NEAREST,
		GL_LINEAR_MIPMAP_NEAREST,
		GL_NEAREST_MIPMAP_LINEAR,
		GL_LINEAR_MIPMAP_LINEAR
	};
	const GLenum magFilters[2] =
	{
		GL_NEAREST,
		GL_LINEAR
	};
	const char * const filterStrings[6] =
	{
		"Nearest",
		"Linear",
		"Nearest mipmap nearest",
		"Linear mipmap nearest",
		"Nearest mipmap linear",
		"Linear mipmap linear"
	};
	int minFilterIndex = 5;
	int magFilterIndex = 1;
	float minTextureLOD = 0;
	float maxTextureLOD = 4.5;
	float shininess = 32;

	// Debug
	bool showStatsCounters = false;
	enum {QUERY_TRIANGLES, QUERY_FRAGMENTS, NUM_QUERIES};
	GLuint queries[NUM_QUERIES];
	bool showModel = true;
	bool showWireframe = false;
	bool showControlPoints = false;
	vec3 anchorPointColor = vec3(1, 0, 0);
	vec3 controlPointColor = vec3(0, 1, 0);
	bool showControlMeshes = false;
	vec3 controlMeshColor = vec3(0.7, 0.7, 0.7);
	bool showSlefeTiles = false;
	vec3 slefeTileColor = vec3(0.936, 1, 0.502);
	bool showSlefeBoxes = false;
	vec3 slefeBoxColor = vec3(0.5, 1, 0.5);
	bool showScreenRects = false;
	vec3 slefeRectColor = vec3(0.5, 0.5, 1);

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
	UpdateTexture()
	{
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilters[minFilterIndex]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilters[magFilterIndex]);

		if (useTexture)
		{
			static const GLuint minCheckerSize = 2;
			static const GLuint maxCheckerLevel = 10;

			for (GLuint checkerLevel = 0; checkerLevel <= maxCheckerLevel; ++checkerLevel)
			{
				GLuint checkerSize = minCheckerSize << (maxCheckerLevel - checkerLevel);

				static vec3 checker[(minCheckerSize << maxCheckerLevel) * (minCheckerSize << maxCheckerLevel)];
				for (GLuint u = 0; u < checkerSize; ++u)
					for (GLuint v = 0; v < checkerSize; ++v)
					{
						bool check = ((u < checkerSize / 2) ^ (v < checkerSize / 2));
						checker[v * checkerSize + u] = (check) ? diffuseColor1 : diffuseColor2;
					}

				glTexImage2D(GL_TEXTURE_2D, checkerLevel, GL_RGB, checkerSize, checkerSize, 0, GL_RGB, GL_FLOAT, checker);
			}

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxCheckerLevel);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, minTextureLOD);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, maxTextureLOD);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_FLOAT, &diffuseColor1);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		}

		CheckGLErrors("UpdateTexture()");
	}

	void
	Set3DCamera(ShaderProgram &program)
	{
		program.SetCamera(modelViewMatrix, projectionMatrix);
	}

	void
	RenderDebugPrimitives(GLenum type,
			const vec3 &color,
			const vector<GLuint> &indices,
			GLuint start = 0, GLint count = -1)
	{
		GLint positionLocation = debugProgram.GetAttribLocation("Position");
		glEnableVertexAttribArray(positionLocation);
		glVertexAttribPointer(positionLocation, threeD, GL_FLOAT, GL_FALSE, 0, 0);

		debugProgram.SetUniform("Color", color);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_DEBUG_INDICES]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STREAM_DRAW);

		if (type == GL_LINES && !useMultiSampling)
		{
			glEnable(GL_LINE_SMOOTH);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
		}
		EnablePolygonOffset();

		assert(start < indices.size());
		if (count == -1)
			count = indices.size() - start;
		else
		{
			assert(count > 0);
			assert(start + count <= indices.size());
		}

		glDrawElements(type, count, GL_UNSIGNED_INT, (GLvoid *)(uintptr_t)(start * sizeof(indices[0])));

		if (type == GL_LINES && !useMultiSampling)
		{
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_BLEND);
		}
		DisablePolygonOffset();

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

		if (showDebugWindow)
		{
			ImGui::ColorEdit3("Anchor point color", value_ptr(anchorPointColor), ImGuiColorEditFlags_NoInputs);
			ImGui::ColorEdit3("Control point color", value_ptr(controlPointColor), ImGuiColorEditFlags_NoInputs);
		}

		glPointSize(5);
		RenderDebugPrimitives(GL_POINTS, anchorPointColor, anchorIndices);
		RenderDebugPrimitives(GL_POINTS, controlPointColor, controlIndices);
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

		if (showDebugWindow)
			ImGui::ColorEdit3("Control mesh color", value_ptr(controlMeshColor), ImGuiColorEditFlags_NoInputs);

		RenderDebugPrimitives(GL_LINES, controlMeshColor, indices);
	}

	void
	ComputeSlefes()
	{
		if (!slefesChanged)
			return;

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

			REAL lower[numSlefeDivs + 1][numSlefeDivs + 1][threeD];
			REAL upper[numSlefeDivs + 1][numSlefeDivs + 1][threeD];

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

		slefesChanged = false;
		slefeBoxesChanged = true;
	}

	void
	ComputeTessLevels(float vertexTessLevels[NumTeapotVertices])
	{
		ComputeSlefeRects();

		bool levelsOpen = false;
		if (showDebugWindow)
			levelsOpen = ImGui::TreeNode("Tess levels");

		for (GLuint i = 0; i < NumTeapotVertices; ++i)
			vertexTessLevels[i] = 0;

		int screenWidth, screenHeight;
		glfwGetWindowSize(window.get(), &screenWidth, &screenHeight);

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			bool patchOpen = (levelsOpen && ImGui::TreeNode((string("Patch ") + std::to_string(patchIndex)).c_str()));

			const SlefeBox (&slefeBoxes)[maxSlefeDivs + 1][maxSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

			float maxScreenEdge = 0;

			for (GLuint u = 0; u < numSlefeDivs; ++u)
				for (GLuint v = 0; v < numSlefeDivs; ++v)
				{
					AABB tileBox = {vec3(INFINITY), vec3(-INFINITY)};

					for (GLuint uOff = 0; uOff < 2; ++uOff)
						for (GLuint vOff = 0; vOff < 2; ++vOff)
							for (GLuint dim = 0; dim < threeD; ++dim)
							{
								tileBox.min[dim] = glm::min(slefeBoxes[u + uOff][v + vOff].screenAxisBox.min[dim],
										tileBox.min[dim]);
								tileBox.max[dim] = glm::max(slefeBoxes[u + uOff][v + vOff].screenAxisBox.max[dim],
										tileBox.max[dim]);
							}

					if (tileBox.min.x > screenWidth || tileBox.min.y > screenHeight || tileBox.min.z > 1 ||
							tileBox.max.x < 0 || tileBox.max.y < 0 || tileBox.max.z < 0)
						continue;

					if (patchOpen)
						ImGui::Text("Tile[%u][%u].maxScreenEdge = %.2f", u, v, slefeBoxes[u][v].maxScreenEdge);

					maxScreenEdge = glm::max(slefeBoxes[u][v].maxScreenEdge, maxScreenEdge);
				}

			float tessLevel = numSlefeDivs * sqrt(maxScreenEdge);
			if (!fracTessLevels)
				tessLevel = ceilf(tessLevel);

			vertexTessLevels[TeapotIndices[patchIndex][0][2]] =
					vertexTessLevels[TeapotIndices[patchIndex][2][3]] =
					vertexTessLevels[TeapotIndices[patchIndex][3][1]] =
					vertexTessLevels[TeapotIndices[patchIndex][1][0]] = tessLevel;

			vertexTessLevels[TeapotIndices[patchIndex][1][1]] = tessLevel;

			if (patchOpen)
			{
				ImGui::Text("Tess level = %.2f", tessLevel);
				ImGui::TreePop();
			}
		}

		if (levelsOpen)
			ImGui::TreePop();
	}

	vec3
	GetHullVertex(const HalfEdgeMesh &mesh, size_t index)
	{
		return vec3(mesh.m_vertices[index].x, mesh.m_vertices[index].y, mesh.m_vertices[index].z);
	}

	vec3
	ComputeFaceNormal(vec3 &a, vec3 &b, vec3 &c)
	{
		return normalize(cross(b - a, c - a));
	}

	void
	ComputeSlefeTiles()
	{
		ComputeSlefeBoxes();

		if (!slefeTilesChanged)
			return;

		slefeTileVertices.clear();
		slefeTileIndices.clear();

		for (GLint patchIndex = 0; patchIndex < NumTeapotPatches; ++patchIndex)
		{
			patchSlefeTileIndices[patchIndex][0] = slefeTileIndices.size();

			for (GLuint udiv = 0; udiv < numSlefeDivs; ++udiv)
				for (GLuint vdiv = 0; vdiv < numSlefeDivs; ++vdiv)
				{
					vec3 tilePoints[2][2][8];

					for (GLuint uOff = 0; uOff < 2; ++uOff)
						for (GLuint vOff = 0; vOff < 2; ++vOff)
							bcopy(slefeBoxVertices[patchIndex][udiv + uOff][vdiv + vOff],
									tilePoints[uOff][vOff],
									sizeof(slefeBoxVertices[patchIndex][udiv + uOff][vdiv + vOff]));

					QuickHull quickHull;
					auto tileMesh = quickHull.getConvexHullAsMesh(value_ptr(tilePoints[0][0][0]),
							2 * 2 * 8,
							true,
							1e-7);

					// It's pretty wasteful not to just build indices into the slefe box vertex list, but this is just
					// for debug display.
					for (auto &edge : tileMesh.m_halfEdges)
					{
						auto &otherHalf = tileMesh.m_halfEdges[edge.m_opp];

						if (edge.m_endVertex > otherHalf.m_endVertex)
							continue;

						vec3 vertex = GetHullVertex(tileMesh, edge.m_endVertex);
						vec3 otherVertex = GetHullVertex(tileMesh, otherHalf.m_endVertex);

						HalfEdgeMesh::HalfEdge &nextEdge = tileMesh.m_halfEdges[edge.m_next];
						vec3 thirdVertex = GetHullVertex(tileMesh, nextEdge.m_endVertex);
						HalfEdgeMesh::HalfEdge &otherNextEdge = tileMesh.m_halfEdges[otherHalf.m_next];
						vec3 otherThirdVertex = GetHullVertex(tileMesh, otherNextEdge.m_endVertex);

						vec3 faceNormal = ComputeFaceNormal(vertex, otherVertex, thirdVertex);
						vec3 otherFaceNormal = ComputeFaceNormal(otherVertex, vertex, otherThirdVertex);

						// If this edge separates two faces on the same plane, don't display it in wireframe
						if (dot(faceNormal, otherFaceNormal) >= 1.0 - 1e-6)
							continue;

						slefeTileIndices.push_back(slefeTileVertices.size());
						slefeTileVertices.push_back(vertex);
						slefeTileIndices.push_back(slefeTileVertices.size());
						slefeTileVertices.push_back(otherVertex);
					}
				}

			patchSlefeTileIndices[patchIndex][1] = slefeTileVertices.size();
		}

		slefeTilesChanged = false;
	}

    void
    RenderSlefeTiles()
    {
		ComputeSlefeTiles();

		bool patchesOpen = (showDebugWindow && ImGui::TreeNode("Patch slefes"));

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
					}
				}
			}

			if (showPatch)
				ImGui::TreePop();
		}

		if (patchesOpen)
			ImGui::TreePop();

		debugProgram.Use();

        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER,
				slefeTileVertices.size() * sizeof(slefeTileVertices[0]),
				slefeTileVertices.data(),
				GL_STREAM_DRAW);

		Set3DCamera(debugProgram);

		if (showDebugWindow)
			ImGui::ColorEdit3("Slefe tile color", value_ptr(slefeTileColor), ImGuiColorEditFlags_NoInputs);

		GLuint start = patchSlefeTileIndices[patchRange[0]][0];
		GLuint stop = patchSlefeTileIndices[patchRange[0] + patchRange[1] - 1][1];
		RenderDebugPrimitives(GL_LINES, slefeTileColor, slefeTileIndices, start, stop - start);
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
	DebugAABB(const char *name, GLuint u, GLuint v, const AABB &box)
	{
		ImGui::Text("%s[%u][%u]: { %.2f %.2f %.2f } - { %.2f %.2f %.2f }",
				name, u, v, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z);
	}

	void
	RenderSlefeBoxes()
	{
		bool slefeNodesOpen = (showDebugWindow && ImGui::TreeNode("Slefe boxes"));

		vector<vec3> boxVertices;
		vector<GLuint> boxIndices;
		vector<GLuint> screenRectIndices;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			bool patchOpen = false;
			if (slefeNodesOpen)
				patchOpen = ImGui::TreeNode((string("Slefe box ") + std::to_string(patchIndex)).c_str());

			const SlefeBox (&slefeBoxes)[maxSlefeDivs + 1][maxSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					const SlefeBox &box = slefeBoxes[u][v];

					if (patchOpen)
					{
						DebugAABB("worldAxisBox", u, v, box.worldAxisBox);
						DebugAABB("screenAxisBox", u, v, box.screenAxisBox);
					}

					if (showSlefeBoxes)
						RenderAABBWireframe(box.worldAxisBox, boxVertices, boxIndices);

					if (showScreenRects)
						RenderAABBWireframe(box.screenAxisBox, boxVertices, screenRectIndices);
				}

			if (patchOpen)
				ImGui::TreePop();
		}

		if (slefeNodesOpen)
			ImGui::TreePop();

		debugProgram.Use();

		glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_TEAPOT]);

		glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_DEBUG_VERTICES]);
		glBufferData(GL_ARRAY_BUFFER, boxVertices.size() * sizeof(boxVertices[0]), boxVertices.data(), GL_STREAM_DRAW);

		if (showSlefeBoxes)
		{
			Set3DCamera(debugProgram);

			if (showDebugWindow)
				ImGui::ColorEdit3("Slefe box color", value_ptr(slefeBoxColor), ImGuiColorEditFlags_NoInputs);

			RenderDebugPrimitives(GL_LINES, slefeBoxColor, boxIndices);
		}

		if (showScreenRects)
		{
			mat4 identity(1);

			int width, height;
			glfwGetWindowSize(window.get(), &width, &height);
			mat4 screenSpace = glm::ortho(0.0f, float(width), 0.0f, float(height), -minCameraZ, -maxCameraZ);

			debugProgram.SetCamera(identity, screenSpace);

			if (showDebugWindow)
				ImGui::ColorEdit3("Slefe rect color", value_ptr(slefeRectColor), ImGuiColorEditFlags_NoInputs);

			RenderDebugPrimitives(GL_LINES, slefeRectColor, screenRectIndices);
		}

		CheckGLErrors("RenderSlefeBoxes()");
	}

	void
	EnablePolygonOffset()
	{
		static float polygonOffset[2] = {0, -30};
		static ImGuiOnceUponAFrame once;
		if (showDebugWindow && once)
			ImGui::DragFloat2("Polygon offset", polygonOffset, 0.01, -1000, 1000);

		glPolygonOffset(polygonOffset[0], polygonOffset[1]);

		glEnable(GL_POLYGON_OFFSET_LINE);
		glEnable(GL_POLYGON_OFFSET_POINT);
	}

	void
	DisablePolygonOffset()
	{
		glDisable(GL_POLYGON_OFFSET_LINE);
		glDisable(GL_POLYGON_OFFSET_POINT);
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

        setenv("SUBLIMEPATH", ".", false);
        InitBounds();

		glGetIntegerv(GL_SAMPLES, &numSampleBuffers);

		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

		glEnable(GL_DEPTH_TEST);

		glGenTextures(1, &texture);
		UpdateTexture();

		glGenQueries(NUM_QUERIES, queries);

        CheckGLErrors("PixAccCurvedSurf()");
    }

	~PixAccCurvedSurf()
	{
		glDeleteTextures(1, &texture);

		glDeleteQueries(NUM_QUERIES, queries);

		CheckGLErrors("~PixAccCurvedSurf()");
	}

	void
	ComputeSlefeBoxes()
	{
		ComputeSlefes();

		if (!slefeBoxesChanged)
			return;

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			Slefe &slefe = slefes[patchIndex];
			SlefeBox (&slefeBoxes)[maxSlefeDivs + 1][maxSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

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

					GetAABBVertices(box.worldAxisBox, slefeBoxVertices[patchIndex][u][v]);
				}
		}

		slefeBoxesChanged = false;
		slefeTilesChanged = true;
	}

	void
	ComputeSlefeRects()
	{
		ComputeSlefeBoxes();

		int width, height;
		glfwGetWindowSize(window.get(), &width, &height);
		vec3 halfWindowSize = vec3(width / 2.0, height / 2.0, 0.5);

		for (GLint patchIndex = patchRange[0]; patchIndex < patchRange[0] + patchRange[1]; ++patchIndex)
		{
			SlefeBox (&slefeBoxes)[maxSlefeDivs + 1][maxSlefeDivs + 1] = patchSlefeBoxes[patchIndex];

			for (GLuint u = 0; u <= numSlefeDivs; ++u)
				for (GLuint v = 0; v <= numSlefeDivs; ++v)
				{
					struct SlefeBox &box = slefeBoxes[u][v];
					vec3 (&worldBoxVertices)[8] = slefeBoxVertices[patchIndex][u][v];

					vec3 &screenMin = box.screenAxisBox.min;
					vec3 &screenMax = box.screenAxisBox.max;

					screenMin = vec3(INFINITY);
					screenMax = vec3(-INFINITY);

					for (GLuint i = 0; i < 8; ++i)
					{
						vec4 worldVertex = vec4(worldBoxVertices[i], 1);
						vec4 clipVertex = projectionMatrix * modelViewMatrix * worldVertex;
						vec3 normVertex = vec3(clipVertex[0], clipVertex[1], clipVertex[2]) / vec3(clipVertex.w);
						vec3 winVertex = halfWindowSize + normVertex * halfWindowSize;

						for (GLuint dim = 0; dim < threeD; ++dim)
						{
							screenMin[dim] = glm::min(screenMin[dim], winVertex[dim]);
							screenMax[dim] = glm::max(screenMax[dim], winVertex[dim]);
						}
					}

					box.maxScreenEdge = glm::max(screenMax.x - screenMin.x, screenMax.y - screenMin.y);
				}
		}
	}

	void
	RenderModel(const float vertexTessLevels[NumTeapotPatches])
	{
		glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_TEAPOT]);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);

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

		if (!twoSided)
			glEnable(GL_CULL_FACE);

		if (!showNormals)
		{
			mainProgram->SetUniform("AmbientIntensity", ambientIntensity);
			mainProgram->SetUniform("TextureRepeat", textureRepeat);
			vec3 worldLightPos = vec3(modelViewMatrix * vec4(lightPosition, 1));
			mainProgram->SetUniform("LightPosition", worldLightPos);
			mainProgram->SetUniform("LightIntensity", lightIntensity);
			mainProgram->SetUniform("Shininess", shininess);
		}

		if (showModel)
		{
			if (showStatsCounters)
			{
				glBeginQuery(GL_PRIMITIVES_GENERATED, queries[QUERY_TRIANGLES]);
				glBeginQuery(GL_SAMPLES_PASSED, queries[QUERY_FRAGMENTS]);
			}

			glDrawElements(GL_PATCHES,
					NumTeapotVerticesPerPatch * patchRange[1],
					GL_UNSIGNED_INT, (void *)(patchRange[0] * sizeof(TeapotIndices[0])));

			if (showStatsCounters)
			{
				glEndQuery(GL_PRIMITIVES_GENERATED);
				glEndQuery(GL_SAMPLES_PASSED);
			}
		}

		if (showWireframe)
		{
			if (showModel)
			{
				glEnable(GL_COLOR_LOGIC_OP);
				glLogicOp(GL_INVERT);

				EnablePolygonOffset();
			}

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			glDrawElements(GL_PATCHES,
					NumTeapotVerticesPerPatch * patchRange[1],
					GL_UNSIGNED_INT, (void *)(patchRange[0] * sizeof(TeapotIndices[0])));

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			if (showModel)
			{
				DisablePolygonOffset();

				glDisable(GL_COLOR_LOGIC_OP);
			}
		}

		if (!twoSided)
			glDisable(GL_CULL_FACE);

		glBindVertexArray(0);
	}

	void
	RenderUI(double time)
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImWindow gui("Controls", NULL, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::Combo("Mode", &tessMode, "iPASS\0Uniform\0\0");
		if (tessMode == TESS_IPASS)
		{
			if (ImGui::CollapsingHeader("iPASS", ImGuiTreeNodeFlags_DefaultOpen))
			{
				static const GLuint step = 1;
				if (ImGui::InputScalar("Divisions", ImGuiDataType_U32, &numSlefeDivs, &step))
				{
					numSlefeDivs = glm::clamp(numSlefeDivs, 2u, maxSlefeDivs);
					slefesChanged = true;
				}
				ImGui::Checkbox("Fractional tessellation", &fracTessLevels);

				ImGui::Checkbox("Show slefe boxes", &showSlefeBoxes);
				ImGui::Checkbox("Show screen-space slefe bounds", &showScreenRects);
				ImGui::Checkbox("Show slefe tiles", &showSlefeTiles);
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

		static float zoomSpeed = 0.01;
		static float panSpeed = 0.2;
		if (showDebugWindow)
		{
			ImGui::DragFloat("Zoom speed", &zoomSpeed, 0.1, 0.001, 2);
			ImGui::DragFloat("Pan speed", &panSpeed, 0.1, 0.001, 2);
		}

		if (!io.WantCaptureKeyboard)
		{
			if (ImGui::IsKeyDown(GLFW_KEY_W))
				cameraOffset[2]-= zoomSpeed;
			else if (ImGui::IsKeyDown(GLFW_KEY_S))
				cameraOffset[2]+= zoomSpeed;
			if (ImGui::IsKeyDown(GLFW_KEY_UP))
				cameraParams[1]-= panSpeed;
			else if (ImGui::IsKeyDown(GLFW_KEY_DOWN))
				cameraParams[1]+= panSpeed;
			if (ImGui::IsKeyDown(GLFW_KEY_LEFT))
				cameraParams[0]-= panSpeed;
			else if (ImGui::IsKeyDown(GLFW_KEY_RIGHT))
				cameraParams[0]+= panSpeed;
		}

		static bool animateAzimuth = false;
		static AnimationCurve azimuthCurve(-110, 40, 0.5);
		static bool animateElevation = false;
		static AnimationCurve elevationCurve(-20, 40, 0.25);
		static bool animateDistance = false;
		static AnimationCurve distanceCurve(2.4, 15.0, 0.5);

        if (ImGui::CollapsingHeader("Camera"))
        {
			ImGui::Checkbox("Anim. dist.", &animateDistance);
			ImGui::SameLine();
			ImGui::Checkbox("Anim. azi.", &animateAzimuth);
			ImGui::SameLine();
			ImGui::Checkbox("Anim. elev.", &animateElevation);

			if (showDebugWindow)
			{
				azimuthCurve.RenderUI("Azimuth");
				elevationCurve.RenderUI("Elevation");
				distanceCurve.RenderUI("Distance");
			}

			ImGui::DragFloat3("X/Y/Z Off.", value_ptr(cameraOffset), -0.01, -10, 10.0);
            ImGui::DragFloat2("Azi./Elev.", cameraParams, 1, -179, 180, "%.0f deg");
            ImGui::Checkbox("Perspective", &perspective);

            if (perspective)
                ImGui::DragFloat("FOV", &fov, 1, 30, 120);
			if (numSampleBuffers > 1)
				ImGui::Checkbox((std::to_string(numSampleBuffers) + "x MSAA").c_str(), &useMultiSampling);
        }

		if (animateDistance)
			cameraOffset[2] = distanceCurve.Sample(time);
		if (animateAzimuth)
			cameraParams[0] = azimuthCurve.Sample(time);
		if (animateElevation)
			cameraParams[1] = elevationCurve.Sample(time);

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
            projectionMatrix = glm::perspective(glm::radians(fov), aspect, minCameraZ, maxCameraZ);
        else
            projectionMatrix = glm::ortho(-3.0f * aspect, 3.0f * aspect, -3.0f, 3.0f, -10.0f, 10.0f);

		if (useMultiSampling)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);

		if (ImGui::IsKeyPressed(GLFW_KEY_1, false))
			showWireframe = !showWireframe;

        static vec3 modelPos = vec3(0);
		static vec3 modelScale = vec3(1);
        if (ImGui::CollapsingHeader("Model"))
        {
            ImGui::DragFloat3("Position", value_ptr(modelPos), 0.01, -10, 10, "%.2f");
			ImGui::DragFloat3("Scale", value_ptr(modelScale), 0.01, -10, 10, "%.2f");
            if (ImGui::DragInt2("Patches", patchRange, 0.2, 0, NumTeapotPatches))
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
		modelViewMatrix = glm::scale(modelViewMatrix, modelScale);

		if (ImGui::CollapsingHeader("Scene"))
		{
			ImGui::ColorEdit3("Background color", value_ptr(backgroundColor), ImGuiColorEditFlags_NoInputs);
			ImGui::SliderFloat("Ambient", &ambientIntensity, 0, 1);
			ImGui::DragFloat3("Light pos.", value_ptr(lightPosition), 0.1, -100, 100);
			ImGui::SliderFloat("Intensity", &lightIntensity, 0, 1);
		}

		if (ImGui::CollapsingHeader("Material"))
		{
			bool materialChanged = false;

			ImGui::Checkbox("Two sided", &twoSided);
			ImGui::SameLine();
			materialChanged|= ImGui::Checkbox("Show normals", &showNormals);

			bool textureChanged = false;
			textureChanged|= ImGui::ColorEdit3("Diffuse color", value_ptr(diffuseColor1), ImGuiColorEditFlags_NoInputs);
			textureChanged|= ImGui::Checkbox("Checker", &useTexture);
			if (useTexture)
			{
				textureChanged|= ImGui::ColorEdit3("Checker color",
						value_ptr(diffuseColor2),
						ImGuiColorEditFlags_NoInputs);

				ImGui::SliderInt("Repeat", &textureRepeat, 1, 25);

				if (showDebugWindow)
				{
					if (ImGui::Combo("Min filter", &minFilterIndex, filterStrings, IM_ARRAYSIZE(minFilters)))
						textureChanged = true;
					if (ImGui::Combo("Mag filter", &magFilterIndex, filterStrings, IM_ARRAYSIZE(magFilters)))
						textureChanged = true;
					if (ImGui::DragFloat("Min LOD", &minTextureLOD, 0.5, -10, 10))
						textureChanged = true;
					if (ImGui::DragFloat("Max LOD", &maxTextureLOD, 0.5, -10, 10))
						textureChanged = true;
				}
			}

			if (textureChanged)
				UpdateTexture();

			ImGui::SliderFloat("Shininess", &shininess, 4, 120);

			if (materialChanged)
				RebuildMainProgram();
		}
	}

	void
	RenderStats()
	{
		ImGuiIO &io = ImGui::GetIO();

		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1, 0));

		static float opacity = 0.75;
		if (showDebugWindow)
			ImGui::DragFloat("Stats opacity", &opacity, 0.01, 0, 1);

		ImGui::SetNextWindowBgAlpha(opacity);

		ImWindow window("Stats", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
		if (window)
		{
			ImGui::Text("%.1f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

			if (ImGui::IsWindowHovered())
			{
				if (showStatsCounters)
				{
					GLint numTriangles;
					glGetQueryObjectiv(queries[QUERY_TRIANGLES], GL_QUERY_RESULT, &numTriangles);
					GLint numFragments;
					glGetQueryObjectiv(queries[QUERY_FRAGMENTS], GL_QUERY_RESULT, &numFragments);
					ImGui::Text("%'i triangles, %'i fragments", numTriangles, numFragments);
				}
				showStatsCounters = showModel;
			}
			else
				showStatsCounters = false;
		}
	}

    virtual void
    Render(double time)
    {
		RenderUI(time);

        glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	    float vertexTessLevels[NumTeapotVertices];
		if (tessMode == TESS_IPASS)
			ComputeTessLevels(vertexTessLevels);
		else
			for (GLuint i = 0; i < NumTeapotVertices; ++i)
				vertexTessLevels[i] = uniformLevel;

		RenderModel(vertexTessLevels);

		if (showControlPoints)
			RenderControlPoints();
		if (showControlMeshes)
			RenderControlMeshes();
		if (tessMode == TESS_IPASS)
		{
			if (showSlefeTiles)
				RenderSlefeTiles();
			if (showSlefeBoxes || showScreenRects)
				RenderSlefeBoxes();
		}

		static bool showStats = true;
		if (showDebugWindow)
			ImGui::Checkbox("Show stats overlay", &showStats);

		if (showStats)
			RenderStats();

        CheckGLErrors("Render()");
    }
};

int
main()
{
    return GLFWAppMain<PixAccCurvedSurf>();
}
