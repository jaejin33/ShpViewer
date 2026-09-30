#pragma once
#include <afxwin.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "../camera/Camera.h"
#include "../parse/ShpDataset.h"
#include <array>
#include <unordered_map>

struct RecordEdit {
    Vec3 translate{};
    float rotate_y = 0.0f;
    float scale = 1.0f;
};

class CGLView :
    public CWnd
{
    DECLARE_DYNAMIC(CGLView)

    struct DrawRange {
        GLint first;
        GLsizei count;
    };

    struct RecordRange {
        int32_t first_range_index = 0;
        int32_t range_count = 0;
        Vec3 bounds_min;
        Vec3 bounds_max;
    };

    struct FillRange {
        GLint first_index = 0;
        GLint index_count = 0;
        GLint first_vertex = 0;
        GLsizei vertex_count = 0;
    };

    struct ExtrudeRange {
        GLint first_index = 0;
        GLint index_count = 0;
        GLint first_vertex = 0;
        GLsizei vertex_count = 0;
    };

    struct EdgeRange {
        GLint first_vertex = 0;
        GLint vertex_count = 0;
    };

    struct PickResult {
        Vec3 point;                   // 맞은 지점의 월드 좌표
        float t = 0.0f;               // 레이 시작점에서의 거리
        int32_t record_index = -1;    // 맞은 건물 번호. -1이면 지면
        int32_t aabb_pass_count = 0;  // 진단용 - AABB를 통과한 후보 개수
    };

    enum class PickIntent {
        kPreviewOnly,   // 레이, 마커만 갱신
        kSelect,        // 선택까지 갱신
    };

    enum class RecordVisibility : uint8_t {
        kNormal = 0,
        kHidden = 1,
        // kDeleted 는 저장 기능을 붙일 때 추가한다.
    };

public:
    CGLView();
    virtual ~CGLView();

    BOOL InitEGL();
    void Render();
    void Cleanup();
    void SetDataset(const ShpDataset* dataset);
    void SetShowQuadTreeLevels(bool show);
    void SetShowAllNodes(bool show);
    void SetShowAllObjectLevelColors(bool show);
    void SetShowFrustum(bool show);
    void SetShowFill(bool show);
    void SetShowObjectBounds(bool show);
    void SetShow3D(bool show);
    void SetShowEdges(bool show);
    void SetShowObjectOutline(bool show);
    void SetShowTriangulationLines(bool show);
    void SetShowPickRay(bool show);

    void UpdateRecordHeight(int32_t record_index, float new_height);
    void ResetRecordHeight(int32_t record_index);
    void AdjustSelectedRecordHeight(float delta);   // 임시 테스트용
    void ToggleSelectedRecordHidden();
    void RestoreAllRecords();
    void SetSelectedRecordHeight(float height);
    void ResetSelectedRecordHeight();
    void TranslateSelectedRecord(float dx, float dz);
    void ResetSelectedRecordEdit();
    void RotateSelectedRecord(float delta_radians);
    void ScaleSelectedRecord(float factor);

protected:
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    EGLContext m_eglContext = EGL_NO_CONTEXT;
    Camera m_camera;
    bool m_isRotating = false;
    bool m_isPanning = false;
    CPoint m_lastMousePos;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    std::vector<RecordRange> m_recordRanges;
    std::vector<int32_t> m_lastVisibleIndices;   // 직전 프레임에 그린 객체들 (피킹 후보)
    std::vector<RecordVisibility> m_recordVisibility;
    int32_t m_pickedRecordIndex = -1;
    bool isRecordVisible(int32_t record_index) const;

    const ShpDataset* m_pDataset = nullptr;
    GLuint m_shaderProgram = 0;
    GLuint m_vertexBuffer = 0;
    std::vector<DrawRange> m_drawRanges;
    GLuint m_fillVertexBuffer = 0;
    GLuint m_fillIndexBuffer = 0;
    std::vector<FillRange> m_fillRanges;
    GLuint m_extrudeVertexBuffer = 0;
    GLuint m_extrudeIndexBuffer = 0;
    std::vector<ExtrudeRange> m_extrudeRanges;
    GLuint m_edgeVertexBuffer = 0;
    std::vector<EdgeRange> m_edgeRanges;
    std::vector<FillRange> m_fillWireRanges;
    GLuint m_pickRayVertexBuffer = 0;
    GLuint m_pickMarkerVertexBuffer = 0;
    

    bool m_showAllObjectLevelColors = false;
    bool m_showAllNodes = false;
    bool m_showQuadTreeLevels = false;
    bool m_showFill = false;
    bool m_showObjectBounds = false;
    bool m_show3D = true;
    bool m_showEdges = true;
    bool m_showObjectOutline = false;
    bool m_showTriangulationLines = false;
    bool m_showPickRay = false;
    GLuint m_fillWireIndexBuffer = 0;
    GLuint m_nodeBoxVertexBuffer = 0;
    GLuint m_objectBoxVertexBuffer = 0;
    
    Vec3 m_pickHitPoint;
    float m_pickHitDistance = 0.0f;
    bool m_hasPickHit = false;
    bool m_hasPickRay = false;
    Vec3 m_pickRayOrigin;
    Vec3 m_pickRayDirection;
    CPoint m_lButtonDownPos;
    bool m_isPickMarkerVisible = false;

    std::unordered_map<int32_t, float> m_heightOverrides;
    float GetEffectiveHeight(int32_t record_index) const;

    std::unordered_map<int32_t, std::vector<Vec3>> m_editedPoints;

    std::unordered_map<int32_t, RecordEdit> m_edits;

    bool m_showFrustum = false;
    std::array<Vec3, 8> m_frustumCorners{};
    GLuint m_frustumVertexBuffer = 0;
    LARGE_INTEGER m_lastFrameTimestamp{};
    int32_t m_fpsFrameCount = 0;
    double m_fpsAccumulatedSeconds = 0.0f;
    float m_fps = 0.0f;
    float m_aspect = 1.0f;
    Mat4 m_projMatrix = Mat4Identity();

    bool IntersectRayRecord(const Vec3& origin, const Vec3& direction, int32_t record_index, float* out_t) const;
    bool PickWorldPoint(const Vec3& origin, const Vec3& direction, PickResult* out_result) const;
    void ShowPickMarkerAt(const PickResult& result);
    void UpdatePickAt(CPoint point, PickIntent intent);
    void RenderPickMarker();
    void RenderPickRay();
    void UpdateProjection();
    void CaptureFrustumCorners();
    void RenderFrustum();
    void RenderObjectBounds(const std::vector<int32_t>& visible_indices, const std::vector<int32_t>& depths);
    void NotifySelectionChanged();
    bool IsRecordVisible(int32_t record_index) const;

    bool InitShader();
    void BuildDebugGeometry();
    void RenderQuadTreeLevels(const std::vector<NodeDebugInfo>& nodes);
    void ComputePickRay(CPoint point, Vec3* out_origin, Vec3* out_direction) const;

    void RebuildRecordGeometry(int32_t record_index);

    const std::vector<Vec3>& GetEffectivePoints(int32_t record_index) const;
    Vec3 GetRecordPivot(int32_t record_index) const;
    void ApplyEditToRecord(int32_t record_index);
    

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
