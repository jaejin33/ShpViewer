#include "pch.h"
#include "Shader.h"
#include <iostream>
#include <vector>

namespace Render {

    Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
        // 1. 빈 셰이더 프로그램 객체 생성
        m_rendererID = glCreateProgram();

        // 2. Vertex / Fragment 셰이더 개별 컴파일
        GLuint vs = CompileShader(GL_VERTEX_SHADER,   vertexSrc);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        // 3. 두 셰이더를 하나의 프로그램으로 링크(Link)
        glAttachShader(m_rendererID, vs);
        glAttachShader(m_rendererID, fs);
        glLinkProgram(m_rendererID);
        glValidateProgram(m_rendererID);

        // 4. 링크 완료 후 개별 셰이더 객체는 불필요 -> 메모리 해제
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    // 소멸자: GPU 셰이더 프로그램 삭제
    Shader::~Shader() {
        glDeleteProgram(m_rendererID);
    }

    // 셰이더 소스 코드를 컴파일하고 핸들을 반환한다.
    // 컴파일 실패 시 오류 메시지를 TRACE로 출력하고 0을 반환한다.
    GLuint Shader::CompileShader(GLuint type, const std::string& source) {
        GLuint id = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);

        // 컴파일 성공 여부 확인 (개발 중 오류 조기 발견에 중요)
        int result;
        glGetShaderiv(id, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            MessageBoxA(NULL, "Shader compile failed! Check TRACE output.", "Shader Error", MB_OK);
            int length;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> message(length);
            glGetShaderInfoLog(id, length, &length, &message[0]);

            // 컴파일 오류 로그를 Visual Studio 출력 창에 출력
            TRACE(_T("Shader Compile Error: %S\n"), message.data());
            glDeleteShader(id);
            return 0;
        }
        return id;
    }

    // 셰이더 프로그램 활성화 (렌더링 전 호출)
    void Shader::Bind() const { glUseProgram(m_rendererID); }

    // 셰이더 프로그램 비활성화
    void Shader::Unbind() const { glUseProgram(0); }

    // 4x4 행렬 유니폼 설정 (MVP 행렬 전달에 사용)
    void Shader::SetUniformMat4f(const std::string& name, const glm::mat4& matrix) {
        GLint location = glGetUniformLocation(m_rendererID, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, &matrix[0][0]);
    }

    // 4-성분 벡터 유니폼 설정 (색상 전달에 사용)
    void Shader::SetUniform4f(const std::string& name, float r, float g, float b, float a) {
        GLint location = glGetUniformLocation(m_rendererID, name.c_str());
        glUniform4f(location, r, g, b, a);
    }

}
