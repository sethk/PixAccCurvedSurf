#include "GLFWApp.hh"
#include "../Data/Teapot.h"
#include <istream>
#include <fstream>
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
using glm::vec3;
using glm::mat4;
using glm::value_ptr;

class PixAccCurvedSurf : public GLFWWindowedApp
{
    enum {BUFFER_ARRAY, BUFFER_ELEMENTS, NUM_BUFFERS};
    GLuint buffers[NUM_BUFFERS];
    GLuint program;
    vec3 modelCentroid;
    GLint modelViewMatrixLocation;
    GLint projectionMatrixLocation;

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

public:
    PixAccCurvedSurf() : GLFWWindowedApp("PixAccCurvedSurf")
    {
        GLuint vertexArrayObject;
        glGenVertexArrays(1, &vertexArrayObject);
        glBindVertexArray(vertexArrayObject);

        glGenBuffers(NUM_BUFFERS, buffers);

        glBindBuffer(GL_ARRAY_BUFFER, buffers[BUFFER_ARRAY]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(TeapotVertices), TeapotVertices, GL_STATIC_DRAW);

        modelCentroid = vec3(0);
        for (GLuint i = 0; i < NumTeapotVertices; ++i)
            modelCentroid+= vec3(TeapotVertices[i][0], TeapotVertices[i][1], TeapotVertices[i][2]);
        modelCentroid/= NumTeapotVertices;

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[BUFFER_ELEMENTS]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(TeapotIndices), TeapotIndices, GL_STATIC_DRAW);

        glPatchParameteri(GL_PATCH_VERTICES, NumTeapotVerticesPerPatch);

        program = glCreateProgram();

        LoadShader(GL_VERTEX_SHADER, "Debug.vert");
        LoadShader(GL_FRAGMENT_SHADER, "Debug.frag");

        glLinkProgram(program);
        CheckShaderStatus(program, GL_LINK_STATUS, "glLinkProgram()", glGetProgramiv, glGetProgramInfoLog);

        glUseProgram(program);

        GLint positionLocation = GetAttribLocation("Position");
        glEnableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

        projectionMatrixLocation = GetUniformLocation("ProjectionMatrix");
        modelViewMatrixLocation = GetUniformLocation("ModelViewMatrix");

        glClearColor(0.4, 0.4, 0.4, 1);

        CheckGLErrors("PixAccCurvedSurf()");
    }

    virtual void
    Render(double /*time*/)
    {
        ImWindow gui("Controls");

        glClear(GL_COLOR_BUFFER_BIT);

        static vec3 modelPos;
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat3("Position", value_ptr(modelPos), 0.01, -1, 1, "%.2f");
        }

        static float distance = 1;
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

        mat4 modelViewMatrix(1);
        //modelViewMatrix = glm::translate(modelViewMatrix, modelCentroid);
        modelViewMatrix = glm::translate(modelViewMatrix, modelPos - vec3(0, 0, distance));
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[1]), vec3(1, 0, 0));
        modelViewMatrix = glm::rotate(modelViewMatrix, glm::radians(cameraParams[0]), vec3(0, 1, 0));
        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, value_ptr(modelViewMatrix));

        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);
        mat4 projectionMatrix;
        if (perspective)
            projectionMatrix = glm::perspective(glm::radians(fov), float(width) / float(height), 0.1f, 100.0f);
        else
            projectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, value_ptr(projectionMatrix));

        glPointSize(5);
        glDrawElements(GL_POINTS, NumTeapotVertices, GL_UNSIGNED_INT, 0);

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