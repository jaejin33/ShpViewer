#pragma once
#include <string>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>

namespace Render {

    // GLSL 셰이더 프로그램 래퍼 클래스
    // 정점(Vertex) 셰이더와 프래그먼트(Fragment) 셰이더를 컴파일하고 링크한다.
    class Shader {
    private:
        GLuint m_rendererID; // GPU에 등록된 셰이더 프로그램 핸들 번호

        // 개별 셰이더(Vertex/Fragment)를 컴파일하는 내부 헬퍼 함수
        GLuint CompileShader(GLuint type, const std::string& source);

    public:
        // 소스 코드 문자열을 받아서 셰이더 프로그램을 바로 컴파일 및 링크
        Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
        ~Shader();

        // 셰이더 활성화 / 비활성화
        void Bind() const;
        void Unbind() const;

        // GPU 유니폼 변수 설정 (카메라 행렬, 색상 등)
        void SetUniformMat4f(const std::string& name, const glm::mat4& matrix);
        void SetUniform4f(const std::string& name, float r, float g, float b, float a);

        // 셰이더 프로그램 핸들 반환 (glUseProgram, glGetUniformLocation 등에 사용)
        GLuint GetID() const { return m_rendererID; }
    };

}
