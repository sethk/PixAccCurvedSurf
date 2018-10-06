#include "GLFWApp.hh"
#include "../Data/Teapot.h"
#include <istream>
#include <fstream>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using std::runtime_error;
using std::array;
using std::string;
using std::ifstream;
using std::ostringstream;
using std::clog;
using std::endl;
using std::vector;
using glm::vec3;
using glm::mat4;
using glm::value_ptr;

class PixAccCurvedSurf : public GLFWWindowedApp
{
    enum {VERTEX_ARRAY_TEAPOT, VERTEX_ARRAY_DEBUG, NUM_VERTEX_ARRAYS};
    GLuint vertexArrayObjects[NUM_VERTEX_ARRAYS];
    enum {BUFFER_CONTROL_POINTS, BUFFER_CONTROL_POINT_INDICES, BUFFER_DEBUG_INDICES, NUM_BUFFERS};
    GLuint buffers[NUM_BUFFERS];
    GLuint program;
    vec3 modelCentroid;
    GLint modelViewMatrixLocation;
    GLint projectionMatrixLocation;
    bool showControlPoints = true;
    bool showControlMeshes = true;
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
	RenderDebugPrimitives(GLenum type, GLfloat r, GLfloat g, GLfloat b, const vector<GLuint> &indices)
	{
		GLint colorLocation = GetUniformLocation("Color");
		glUniform4f(colorLocation, r, g, b, 1);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_DEBUG_INDICES]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STREAM_DRAW);

		glDrawElements(type, indices.size(), GL_UNSIGNED_INT, 0);
	}

    void
    RenderDebug(void)
    {
        glBindVertexArray(vertexArrayObjects[VERTEX_ARRAY_DEBUG]);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINTS]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_CONTROL_POINT_INDICES]);

        GLint positionLocation = GetAttribLocation("Position");
        glEnableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);


        if (showControlMeshes)
		{
			vector<GLuint> indices;

			for (GLint i = patchRange[0]; i < patchRange[0] + patchRange[1]; ++i)
			{
				for (GLuint j = 0; j < 4; ++j)
				{
					for (GLuint k = 0; k < 3; ++k)
					{
						indices.push_back(TeapotIndices[i][j][k]);
						indices.push_back(TeapotIndices[i][j][k + 1]);
						indices.push_back(TeapotIndices[i][k][j]);
						indices.push_back(TeapotIndices[i][k + 1][j]);
					}
				}
			}

			RenderDebugPrimitives(GL_LINES, 0.6, 0.6, 0.6, indices);
		}

        if (showControlPoints)
		{
			vector<GLuint> anchorIndices;
			vector<GLuint> controlIndices;

			for (GLint i = patchRange[0]; i < patchRange[0] + patchRange[1]; ++i)
			{
				for (GLuint j = 0; j < 4; ++j)
					for (GLuint k = 0; k < 4; ++k)
					{
						if ((j == 0 || j == 3) && (k == 0 || k == 3))
							anchorIndices.push_back(TeapotIndices[i][j][k]);
						else
							controlIndices.push_back(TeapotIndices[i][j][k]);
					}
			}

			glPointSize(5);

			RenderDebugPrimitives(GL_POINTS, 1, 1, 1, anchorIndices);
			RenderDebugPrimitives(GL_POINTS, 1, 0, 0, controlIndices);
		}

        CheckGLErrors("RenderDebug()");
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

        CheckGLErrors("PixAccCurvedSurf()");
    }

    virtual void
    Render(double /*time*/)
    {
        ImWindow gui("Controls", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        glClear(GL_COLOR_BUFFER_BIT);

        static vec3 modelPos;
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Position", value_ptr(modelPos), 0.01, -1, 1, "%.2f");
            if (ImGui::DragInt2("Draw patches", patchRange, 0.2, 0, NumTeapotPatches))
            {
                patchRange[0] = std::min(std::max(patchRange[0], 0), NumTeapotPatches);
                patchRange[1] = std::max(std::min(patchRange[1], NumTeapotPatches - patchRange[0]), 1);
            }
            ImGui::Checkbox("Show control points", &showControlPoints);
            ImGui::Checkbox("Show control meshes", &showControlMeshes);
        }

        static float distance = 5;
        static float cameraParams[2];
        static bool perspective = true;
        static float fov = 70;
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Distance", &distance, 0.01, 0.01, 10.0);
            ImGui::DragFloat2("Azi./Elev.", cameraParams, 1, -180, 180, "%.0f deg");
            ImGui::Checkbox("Perspective", &perspective);
            if (perspective)
                ImGui::DragFloat("FOV", &fov, 1, 30, 120);
        }

		ImGuiIO &io = ImGui::GetIO();
		if (!io.WantCaptureMouse && ImGui::IsMouseDown(0))
		{
			ImVec2 delta = ImGui::GetMouseDragDelta(0, 0);
			cameraParams[0]+= delta.x;
			cameraParams[1]+= delta.y;
			ImGui::ResetMouseDragDelta(0);
		}

        mat4 modelViewMatrix(1);
        modelViewMatrix = glm::translate(modelViewMatrix, -vec3(0, 0, distance));
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[1]), vec3(1, 0, 0));
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[0]), vec3(0, 1, 0));
        modelViewMatrix = glm::translate(modelViewMatrix, modelPos - modelCentroid);
        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, value_ptr(modelViewMatrix));

        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);
        mat4 projectionMatrix;
        if (perspective)
            projectionMatrix = glm::perspective(glm::radians(fov), float(width) / float(height), 0.1f, 100.0f);
        else
            projectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, value_ptr(projectionMatrix));

        RenderDebug();

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
