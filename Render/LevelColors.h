#pragma once
#include <glm/glm.hpp>

namespace Render {

    // 쿼드트리 깊이(depth) 0~8에 대응하는 색상 테이블을 반환한다.
    // 깊이가 0에 가까울수록 넓은 노드(빨강), 8에 가까울수록 세밀한 리프 노드(흰색).
    // 범위를 벗어난 depth 값은 자동으로 클램핑된다.
    inline glm::vec4 GetLevelColor(int depth) {
        static const glm::vec4 colors[9] = {
            {1.0f, 0.2f, 0.2f, 1.0f},  // 0: 빨강  (루트 레벨)
            {1.0f, 0.6f, 0.0f, 1.0f},  // 1: 주황
            {1.0f, 1.0f, 0.0f, 1.0f},  // 2: 노랑
            {0.2f, 0.9f, 0.2f, 1.0f},  // 3: 초록
            {0.0f, 0.9f, 0.9f, 1.0f},  // 4: 청록
            {0.3f, 0.6f, 1.0f, 1.0f},  // 5: 파랑
            {0.8f, 0.2f, 1.0f, 1.0f},  // 6: 보라
            {1.0f, 0.3f, 0.7f, 1.0f},  // 7: 분홍
            {0.9f, 0.9f, 0.9f, 1.0f},  // 8: 흰색  (리프 레벨)
        };
        if (depth < 0) depth = 0;
        if (depth > 8) depth = 8;
        return colors[depth];
    }

}
