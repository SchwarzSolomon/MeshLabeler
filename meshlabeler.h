/**
 * @file meshlabeler.h
 * @brief 3D Mesh Labeling Tool - Core labeling engine
 * @author MeshLabeler Project
 * @date 2026-01-11
 */

#ifndef MESHLABELER_H
#define MESHLABELER_H

#include <QString>
#include <QObject>
#include <memory>
#include <vector>
#include <stack>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkLookupTable.h>
#include <vtkCallbackCommand.h>

/**
 * @brief 编辑模式枚举
 */
enum class EditMode {
    Brush = 0,   ///< 画刷模式：使用球形区域进行区域标注
    Single = 1   ///< 单点模式：单个三角形面片标注
};

/**
 * @brief 标注操作命令基类（用于撤销/重做）
 */
class LabelCommand {
public:
    virtual ~LabelCommand() = default;

    /**
     * @brief 执行命令
     */
    virtual void execute() = 0;

    /**
     * @brief 撤销命令
     */
    virtual void undo() = 0;

    /**
     * @brief 获取命令描述
     */
    virtual QString description() const = 0;
};

/**
 * @brief 绘制命令（用于撤销/重做）
 */
class PaintCommand : public LabelCommand {
public:
    PaintCommand(vtkSmartPointer<vtkPolyData> polyData,
                 const std::vector<int>& cellIds,
                 int newLabel);

    void execute() override;
    void undo() override;
    QString description() const override;

private:
    vtkSmartPointer<vtkPolyData> m_polyData;
    std::vector<int> m_cellIds;      ///< 受影响的单元ID列表
    std::vector<int> m_oldLabels;    ///< 旧标签值
    int m_newLabel;                   ///< 新标签值
};

/**
 * @brief MeshLabeler 核心类
 *
 * 负责3D网格的加载、显示、标注和保存。
 * 支持两种标注模式：画刷模式和单点模式。
 * 提供撤销/重做、自动保存等功能。
 */
class MeshLabeler : public QObject {
    Q_OBJECT

public:
    // ==================== 常量定义 ====================
    static constexpr int MAX_LABELS = 20;                    ///< 最大标签数量
    static constexpr double DEFAULT_BRUSH_RADIUS = 2.5;      ///< 默认画刷半径
    static constexpr double BRUSH_RADIUS_STEP = 0.15;        ///< 画刷半径调整步长
    static constexpr double MIN_BRUSH_RADIUS = 0.15;         ///< 最小画刷半径
    static constexpr int RENDER_THROTTLE_MS = 16;            ///< 渲染节流时间 (60fps)
    static constexpr int AUTO_SAVE_INTERVAL_MS = 300000;     ///< 自动保存间隔 (5分钟)

    // ==================== 构造/析构 ====================
    /**
     * @brief 构造函数
     * @param parent Qt父对象
     */
    explicit MeshLabeler(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MeshLabeler();

    // ==================== 文件操作 ====================
    /**
     * @brief 加载STL网格文件
     * @param filename 文件路径
     * @return 成功返回true，失败返回false
     */
    bool loadSTL(const QString& filename);

    /**
     * @brief 加载VTP网格文件（带标注数据）
     * @param filename 文件路径
     * @return 成功返回true，失败返回false
     */
    bool loadVTP(const QString& filename);

    /**
     * @brief 保存VTP文件
     * @param filename 文件路径
     * @return 成功返回true，失败返回false
     */
    bool saveVTP(const QString& filename);

    /**
     * @brief 保存到临时文件（自动保存）
     * @return 成功返回true，失败返回false
     */
    bool saveToTempFile();

    // ==================== 渲染设置 ====================
    /**
     * @brief 设置VTK渲染窗口
     * @param renderWindow VTK渲染窗口指针
     */
    void setupRenderer(vtkRenderWindow* renderWindow);

    /**
     * @brief 初始化交互回调
     */
    void initializeCallbacks();

    /**
     * @brief 更新渲染
     */
    void render();

    /**
     * @brief 请求渲染（带节流）
     */
    void requestRender();

    // ==================== 标注操作 ====================
    /**
     * @brief 设置当前标签
     * @param label 标签值 (0-19)
     */
    void setCurrentLabel(int label);

    /**
     * @brief 获取当前标签
     * @return 当前标签值
     */
    int getCurrentLabel() const { return m_currentLabel; }

    /**
     * @brief 设置编辑模式
     * @param mode 编辑模式（画刷/单点）
     */
    void setEditMode(EditMode mode);

    /**
     * @brief 获取编辑模式
     * @return 当前编辑模式
     */
    EditMode getEditMode() const { return m_editMode; }

    /**
     * @brief 设置画刷半径
     * @param radius 半径值
     */
    void setBrushRadius(double radius);

    /**
     * @brief 获取画刷半径
     * @return 当前画刷半径
     */
    double getBrushRadius() const { return m_brushRadius; }

    /**
     * @brief 增加画刷半径
     */
    void increaseBrushRadius();

    /**
     * @brief 减小画刷半径
     */
    void decreaseBrushRadius();

    // ==================== 撤销/重做 ====================
    /**
     * @brief 撤销上一步操作
     */
    void undo();

    /**
     * @brief 重做操作
     */
    void redo();

    /**
     * @brief 是否可以撤销
     */
    bool canUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief 是否可以重做
     */
    bool canRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief 清空撤销/重做历史
     */
    void clearHistory();

    // ==================== 查询接口 ====================
    /**
     * @brief 获取网格单元数量
     * @return 单元数量
     */
    int getCellCount() const;

    /**
     * @brief 获取指定单元的标签
     * @param cellId 单元ID
     * @return 标签值
     */
    int getCellLabel(int cellId) const;

    /**
     * @brief 获取每个标签的统计信息
     * @return 标签统计 (标签ID -> 单元数量)
     */
    std::vector<int> getLabelStatistics() const;

    /**
     * @brief 检查网格是否已加载
     */
    bool isMeshLoaded() const { return m_polyData != nullptr; }

    // ==================== 内部访问器（用于回调） ====================
    vtkRenderer* getRenderer() { return m_renderer.Get(); }
    vtkRenderWindow* getRenderWindow() { return m_renderWindow.Get(); }
    vtkPolyData* getPolyData() { return m_polyData.Get(); }
    vtkActor* getPolyDataActor() { return m_polyDataActor.Get(); }
    vtkActor* getSphereActor() { return m_sphereActor.Get(); }
    vtkLookupTable* getLookupTable() { return m_lookupTable.Get(); }

    bool isMousePressed() const { return m_isMousePressed; }
    void setMousePressed(bool pressed) { m_isMousePressed = pressed; }

signals:
    /**
     * @brief 标签改变信号
     * @param newLabel 新标签值
     */
    void currentLabelChanged(int newLabel);

    /**
     * @brief 编辑模式改变信号
     * @param newMode 新编辑模式
     */
    void editModeChanged(EditMode newMode);

    /**
     * @brief 网格加载完成信号
     * @param filename 文件名
     */
    void meshLoaded(const QString& filename);

    /**
     * @brief 渲染需要更新信号
     */
    void renderNeeded();

    /**
     * @brief 撤销/重做状态改变信号
     */
    void historyChanged();

    /**
     * @brief 错误信号
     * @param errorMessage 错误消息
     */
    void errorOccurred(const QString& errorMessage);

public slots:
    /**
     * @brief 执行自动保存
     */
    void performAutoSave();

private:
    // ==================== 私有方法 ====================
    /**
     * @brief 初始化颜色查找表
     */
    void initializeLookupTable();

    /**
     * @brief 初始化单元数据
     */
    void initializeCellData();

    /**
     * @brief 创建特征边缘
     */
    void createFeatureEdges();

    /**
     * @brief 使用BFS算法在球形区域内标注
     * @param position 球心位置
     * @param startCellId 起始单元ID
     * @return 受影响的单元ID列表
     */
    std::vector<int> labelWithBFS(double* position, int startCellId);

    /**
     * @brief 检查单元是否在球体内
     * @param position 球心位置
     * @param cellId 单元ID
     * @return 在球内返回true
     */
    bool isCellInSphere(double* position, int cellId) const;

    /**
     * @brief 标注单个单元
     * @param cellId 单元ID
     * @param label 标签值
     */
    void labelCell(int cellId, int label);

    /**
     * @brief 标注多个单元
     * @param cellIds 单元ID列表
     * @param label 标签值
     */
    void labelCells(const std::vector<int>& cellIds, int label);

    /**
     * @brief 更新画刷球体显示
     * @param position 球心位置
     */
    void updateBrushSphere(double* position);

    /**
     * @brief 添加命令到历史栈
     * @param command 命令对象
     */
    void addCommand(std::shared_ptr<LabelCommand> command);

    // ==================== 回调函数（友元） ====================
    friend void LeftButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                                       void* clientData, void* callData);
    friend void LeftButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                                         void* clientData, void* callData);
    friend void RightButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                                        void* clientData, void* callData);
    friend void RightButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                                          void* clientData, void* callData);
    friend void KeyPressCallback(vtkObject* caller, long unsigned int eventId,
                                void* clientData, void* callData);
    friend void MouseMoveCallback(vtkObject* caller, long unsigned int eventId,
                                 void* clientData, void* callData);
    friend void MouseWheelForwardCallback(vtkObject* caller, long unsigned int eventId,
                                         void* clientData, void* callData);
    friend void MouseWheelBackwardCallback(vtkObject* caller, long unsigned int eventId,
                                          void* clientData, void* callData);

    // ==================== 成员变量 ====================
    // VTK 对象
    vtkSmartPointer<vtkPolyData> m_polyData;              ///< 网格数据
    vtkSmartPointer<vtkActor> m_polyDataActor;            ///< 网格Actor
    vtkSmartPointer<vtkActor> m_sphereActor;              ///< 画刷球体Actor
    vtkSmartPointer<vtkActor> m_edgeActor;                ///< 特征边缘Actor
    vtkSmartPointer<vtkRenderer> m_renderer;              ///< 渲染器
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;      ///< 渲染窗口
    vtkSmartPointer<vtkLookupTable> m_lookupTable;        ///< 颜色查找表

    // 回调命令
    vtkSmartPointer<vtkCallbackCommand> m_leftButtonPressCallback;
    vtkSmartPointer<vtkCallbackCommand> m_leftButtonReleaseCallback;
    vtkSmartPointer<vtkCallbackCommand> m_rightButtonPressCallback;
    vtkSmartPointer<vtkCallbackCommand> m_rightButtonReleaseCallback;
    vtkSmartPointer<vtkCallbackCommand> m_keyPressCallback;
    vtkSmartPointer<vtkCallbackCommand> m_mouseMoveCallback;
    vtkSmartPointer<vtkCallbackCommand> m_mouseWheelForwardCallback;
    vtkSmartPointer<vtkCallbackCommand> m_mouseWheelBackwardCallback;

    // 状态变量
    int m_currentLabel;                ///< 当前标签 (0-19)
    EditMode m_editMode;               ///< 编辑模式
    double m_brushRadius;              ///< 画刷半径
    bool m_isMousePressed;             ///< 鼠标是否按下

    // 文件信息
    QString m_currentFileName;         ///< 当前文件名
    QString m_tempFileName;            ///< 临时文件名

    // 撤销/重做
    std::stack<std::shared_ptr<LabelCommand>> m_undoStack;  ///< 撤销栈
    std::stack<std::shared_ptr<LabelCommand>> m_redoStack;  ///< 重做栈
    static constexpr int MAX_HISTORY_SIZE = 100;            ///< 最大历史记录数

    // 渲染节流
    bool m_renderPending;              ///< 是否有待处理的渲染请求
};

#endif // MESHLABELER_H
