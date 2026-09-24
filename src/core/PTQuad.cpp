

#include "PTQuad.h"
#include "PTProgram.h"

namespace PathTraceAlg
{
    Quad::Quad()
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        //Vertex data
        float vertices[] =
        {
            -1.0f, 1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));

        glBindVertexArray(0);
    }

    void Quad::Draw(Program* shader)
    {
        shader->Use();
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        shader->StopUsing();
    }
}

/*作用：后处理的"画布"
职责：
- 渲染覆盖整个屏幕的四边形
- 传递纹理坐标到片段着色器
- 简化全屏后处理实现

技术细节：
├── VAO/VBO管理
├── 固定顶点数据（NDC坐标 + UV）
└── 简单的Draw()方法*/