#pragma once

#ifdef USE_GLEW
	#include <GL/glew.h>
#elif defined(USE_GL3W)
	#include <GL/gl3w.h>
#else
    #error Missing GL extension loader
#endif // USE_GL3W

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>
#include <memory>
#include <sstream>
#include "imgui_raii.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if 0
static std::ostream &
operator<<(std::ostream &os, Error &error)
{
	if (error.SourceFile())
		os << error.SourceFile() << ':';

	if (error.SourceLine())
		os << error.SourceLine() << ':';

	os << " error: ";

	if (error.SourceFunc())
		os << "In " << error.SourceFunc() << "(), ";

	if (error.GLFunc())
	{
		if (error.GLLib())
			os << error.GLLib();
		os << error.GLFunc();
	}

	os << '[';
	if (error.EnumParam() || error.EnumParamName())
	{
		os << "enum: ";
		if (error.EnumParamName())
			os << error.EnumParamName();
		else
			os << "(0x" << std::hex << error.EnumParam() << ')';
	}

#if 0
	if(error.BindTarget() || error.TargetName())
	{
		errstr	<< "Binding point: ";
		if(error.TargetName())
		{
			errstr	<< "'"
			          << error.TargetName()
			          << "'";
		}
		else
		{
			errstr	<< "(0x"
			          << std::hex
			          << error.BindTarget()
			          << ")";
		}
		errstr	<< std::endl;
	}

	if(error.ObjectTypeName() || error.ObjectType())
	{
		errstr	<< "Object type: ";
		if(error.ObjectTypeName())
		{
			errstr	<< "'"
			          << error.ObjectTypeName()
			          << "'";
		}
		else
		{
			errstr	<< "(0x"
			          << std::hex
			          << error.ObjectType()
			          << ")";
		}
		errstr	<< std::endl;
	}

	if((!error.ObjectDesc().empty()) || (error.ObjectName() >= 0))
	{
		errstr	<< "Object: ";
		if(!error.ObjectDesc().empty())
		{
			errstr	<< "'"
			          << error.ObjectDesc()
			          << "'";
		}
		else
		{
			errstr	<< "("
			          << error.ObjectName()
			          << ")";
		}
		errstr	<< std::endl;
	}

	if(error.SubjectTypeName() || error.SubjectType())
	{
		errstr	<< "Subject type: ";
		if(error.SubjectTypeName())
		{
			errstr	<< "'"
			          << error.SubjectTypeName()
			          << "'";
		}
		else
		{
			errstr	<< "(0x"
			          << std::hex
			          << error.SubjectType()
			          << ")";
		}
		errstr	<< std::endl;
	}

	if((!error.SubjectDesc().empty()) || (error.SubjectName() >= 0))
	{
		errstr	<< "Subject: ";
		if(!error.SubjectDesc().empty())
		{
			errstr	<< "'"
			          << error.SubjectDesc()
			          << "'";
		}
		else
		{
			errstr	<< "("
			          << error.SubjectName()
			          << ")";
		}
		errstr	<< std::endl;
	}
#endif // 0

	if (error.Identifier())
		os << "identifier: " << error.Identifier();

#if 0
	if(error.Index() >= 0)
	{
		errstr	<< "Index: ("
		          << error.Index()
		          << ")"
		          << std::endl;
	}

	if(error.Value() != 0)
	{
		errstr	<< "Value: ("
		          << error.Value()
		          << ")"
		          << std::endl;
	}

	if(error.Limit() != 0)
	{
		errstr	<< "Limit: ("
		          << error.Limit()
		          << ")"
		          << std::endl;
	}
#endif // 0
	os << "]: " << error.what();

	if (!error.Log().empty())
	{
		os << ": " << std::endl << error.Log();
	}
	return os;
}
#endif // 0

class GLFWApp
{
protected:
	static void
	Error(int /*error*/, const char *description)
	{
		std::clog << "glfw error: " << description << std::endl;
	}

public:
	GLFWApp()
	{
		glfwSetErrorCallback(Error);
		if (!glfwInit())
			throw std::runtime_error("glfwInit() failed");

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		#ifdef __APPLE__
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		#endif // __APPLE__
	}

	~GLFWApp() { glfwTerminate(); }

	void
	CheckGLErrors(const std::string &desc)
	{
	    GLenum error;
	    while ((error = glGetError()) != GL_NO_ERROR)
		{
	        std::ostringstream oss;
	        oss << "GL error during " << desc << ' ' << (void *)(uintptr_t)error;
	        throw std::runtime_error(oss.str());
		}
	}
};

class GLFWWindowedApp : public GLFWApp
{
protected:
	std::unique_ptr<GLFWwindow, void (*)(GLFWwindow *)> window;
	bool useImGuiInput = true;
	bool renderImGui = true;
	bool showImGuiDemo = false;

	static void
	OnResize(GLFWwindow *window, int width, int height)
	{
		auto *app = static_cast<GLFWWindowedApp *>(glfwGetWindowUserPointer(window));
		app->OnResize(width, height);
	}

	static void
	OnKey(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		auto *app = static_cast<GLFWWindowedApp *>(glfwGetWindowUserPointer(window));
		if (ImGui::GetIO().WantCaptureKeyboard)
			ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
		else
			app->OnKey(key, scancode, action, mods);
	}

public:
	GLFWWindowedApp(const std::string &title, int width = 1024, int height = 768)
			: window(glfwCreateWindow(width, height, title.c_str(), NULL, NULL), glfwDestroyWindow)
	{
		GLFWwindow *win = window.get();
		if (!win)
			throw std::runtime_error("Could not create GLFW window");

		glfwSetWindowUserPointer(win, this);
		glfwSetWindowSizeCallback(win, OnResize);
		if (!useImGuiInput)
			glfwSetKeyCallback(win, OnKey);
		glfwMakeContextCurrent(win);
		glfwSwapInterval(1);

#ifdef USE_GL3W
		int err = gl3wInit();
		if (err)
			throw std::runtime_error("gl3wInit() failed, error " + std::to_string(err));
#elif defined(USE_GLEW)
		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			auto description = std::string("glewInit() failed: ") + (const char *)glewGetErrorString(err);
			throw std::runtime_error(description);
		}
#endif // USE_GLEW

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(win, useImGuiInput);
		#ifdef __APPLE__
			ImGui_ImplOpenGL3_Init("#version 410 core");
		#else
			ImGui_ImplOpenGL3_Init("#version 130");
		#endif // __APPLE__

		ImGui::StyleColorsLight(); // {Light,Dark,Classic}
	}

	~GLFWWindowedApp()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	virtual void OnResize(int width, int height) { glViewport(0, 0, width, height); }

	virtual void OnKey(int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/) { }

	virtual void Render(double time) = 0;

	void
	Run()
	{
		GLFWwindow *win = window.get();

		int width, height;
		glfwGetWindowSize(win, &width, &height);
		OnResize(width, height);

		while (!glfwWindowShouldClose(win))
		{
			glfwPollEvents();

			if (ImGui::IsKeyPressed(GLFW_KEY_GRAVE_ACCENT, false))
				showImGuiDemo = !showImGuiDemo;

			if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				glfwSetWindowShouldClose(window.get(), 1);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			if (showImGuiDemo)
				ImGui::ShowDemoWindow(&showImGuiDemo);

			try
			{
				Render(glfwGetTime());
			}
			catch (std::runtime_error &error)
			{
				std::clog << "Exception during Render(): " << error.what() << std::endl;

				ImGui::EndFrame();

				throw;
			}

			if (renderImGui)
			{
				ImGui::Render();
				glfwMakeContextCurrent(win);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}
			else
				ImGui::EndFrame();

			glfwMakeContextCurrent(win);
			glfwSwapBuffers(win);
		}
	}
};

template<typename AppT>
static int
GLFWAppMain()
{
	try
	{
		AppT app;
		app.Run();
		return 0;
	}
	catch (std::runtime_error &error)
	{
		std::clog << error.what() << std::endl;
		return 1;
	}
}
