

#include <iostream>
#include <fstream>
#include <sstream>
#include "PTShader.h"

namespace PathTraceAlg
{
    Shader::Shader(const ShaderInclude::ShaderSource& sourceObj, GLenum shaderType)
    {
        object = glCreateShader(shaderType);
        const GLchar* src = (const GLchar*)sourceObj.src.c_str();
        glShaderSource(object, 1, &src, 0);
        glCompileShader(object);
        GLint success = 0;
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (success == GL_FALSE)
        {
            std::string msg;
            GLint logSize = 0;
            glGetShaderiv(object, GL_INFO_LOG_LENGTH, &logSize);
            char* info = new char[logSize + 1];
            glGetShaderInfoLog(object, logSize, NULL, info);
            msg += sourceObj.path;
            msg += "\n";
            msg += info;
            delete[] info;
            glDeleteShader(object);
            object = 0;
            printf("compilation error %s\n", msg.c_str());
            throw std::runtime_error(msg.c_str());
        }
    }

    GLuint Shader::getObject() const
    {
        return object;
    }
}