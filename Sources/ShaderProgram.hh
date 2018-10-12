#include <string>
#include <fstream>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

class ShaderProgram
{
	GLuint program;
    GLint modelViewMatrixLocation;
    GLint projectionMatrixLocation;

	void
	CheckShaderStatus(GLuint object,
					  GLenum pname,
					  const std::string &desc,
					  void (*getParam)(GLuint object, GLenum pname, GLint *value),
					  void (*getInfoLog)(GLuint object, GLsizei len, GLsizei *lenp, GLchar *log))
	{
	    GLint status;
	    getParam(object, pname, &status);
	    if (!status)
		{
			GLsizei len;
			getParam(object, GL_INFO_LOG_LENGTH, &len);

			GLchar log[len + 1];
			getInfoLog(object, len, &len, log);
			throw std::runtime_error(desc + " failed " + log);
		}
	}

public:
	ShaderProgram()
	{
		program = glCreateProgram();
	}

	~ShaderProgram()
	{
		glDeleteProgram(program);
	}

    void
    LoadShader(GLenum type, const std::string &path)
    {
        GLuint shader = glCreateShader(type);

		std::ifstream file(path);
        if (!file)
            throw std::runtime_error("Could not open " + path);

		std::ostringstream oss;
        oss << file.rdbuf();

		std::string source = oss.str();
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
			std::clog << "Error while compiling shader " << path << ':' << std::endl;
			std::clog << "<<<<<<<<<" << std::endl << source << std::endl << "<<<<<<<<<" << std::endl;
            throw;
        }

        glAttachShader(program, shader);
    }

	void
	Link()
	{
        glLinkProgram(program);
        CheckShaderStatus(program, GL_LINK_STATUS, "glLinkProgram()", glGetProgramiv, glGetProgramInfoLog);

        projectionMatrixLocation = GetUniformLocation("ProjectionMatrix");
        modelViewMatrixLocation = GetUniformLocation("ModelViewMatrix");
	}

	void
	Use()
	{
		glUseProgram(program);
	}

    GLint
    GetUniformLocation(const char *name, bool required = true)
    {
        GLint location = glGetUniformLocation(program, name);
        if (location == -1 && required)
            throw std::runtime_error(std::string("Uniform location ") + name + " was not found in program");

        return location;
    }

	void
	SetUniform(const char *name, float f)
	{
		GLint location = GetUniformLocation(name);
		glUniform1f(location, f);
	}

	void
	SetUniform(const char *name, const glm::vec3 &v)
	{
		GLint location = GetUniformLocation(name);
		glUniform3fv(location, 1, value_ptr(v));
	}

	void
	SetUniform(const char *name, const glm::vec4 &v)
	{
		GLint location = GetUniformLocation(name);
		glUniform4fv(location, 1, value_ptr(v));
	}

    GLint
    GetAttribLocation(const char *name, bool required = true)
    {
        GLint location = glGetAttribLocation(program, name);
        if (location == -1 && required)
            throw std::runtime_error(std::string("Attribute location ") + name + " was not found in the program");

        return location;
    }

	void
	SetCamera(glm::mat4 &modelView, glm::mat4 &projection)
	{
        glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelView));
        glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(projection));
	}
};
