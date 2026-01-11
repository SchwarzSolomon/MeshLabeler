/**
 * @file meshlabeler.cpp
 * @brief MeshLabeler 核心类的实现
 */

#include "meshlabeler.h"

#include <QTimer>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>

#include <queue>
#include <unordered_set>
#include <algorithm>

#include <vtkSTLReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCellPicker.h>
#include <vtkSphereSource.h>
#include <vtkFeatureEdges.h>
#include <vtkFloatArray.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkNamedColors.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkAutoInit.h>

VTK_MODULE_INIT(vtkRenderingOpenGL2)
VTK_MODULE_INIT(vtkInteractionStyle)
VTK_MODULE_INIT(vtkRenderingFreeType)

// ==================== PaintCommand 实现 ====================

PaintCommand::PaintCommand(vtkSmartPointer<vtkPolyData> polyData,
                           const std::vector<int>& cellIds,
                           int newLabel)
    : m_polyData(polyData)
    , m_cellIds(cellIds)
    , m_newLabel(newLabel)
{
    // 保存旧标签值
    m_oldLabels.reserve(cellIds.size());
    for (int cellId : cellIds) {
        int oldLabel = static_cast<int>(
            m_polyData->GetCellData()->GetScalars()->GetTuple1(cellId));
        m_oldLabels.push_back(oldLabel);
    }
}

void PaintCommand::execute()
{
    for (size_t i = 0; i < m_cellIds.size(); ++i) {
        m_polyData->GetCellData()->GetScalars()->SetTuple1(m_cellIds[i], m_newLabel);
    }
    m_polyData->GetCellData()->Modified();
}

void PaintCommand::undo()
{
    for (size_t i = 0; i < m_cellIds.size(); ++i) {
        m_polyData->GetCellData()->GetScalars()->SetTuple1(m_cellIds[i], m_oldLabels[i]);
    }
    m_polyData->GetCellData()->Modified();
}

QString PaintCommand::description() const
{
    return QString("Paint %1 cells with label %2")
        .arg(m_cellIds.size())
        .arg(m_newLabel);
}

// ==================== 自定义交互样式 ====================

class DesignInteractorStyle : public vtkInteractorStyleTrackballCamera
{
public:
    static DesignInteractorStyle* New() { return new DesignInteractorStyle; }
    vtkTypeMacro(DesignInteractorStyle, vtkInteractorStyleTrackballCamera);

    DesignInteractorStyle() {}
    virtual ~DesignInteractorStyle() {}

    virtual void OnLeftButtonDown() override {}
    virtual void OnLeftButtonUp() override {}
    virtual void OnRightButtonDown() override { this->StartRotate(); }
    virtual void OnRightButtonUp() override {
        this->vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
    }
    virtual void OnMouseMove() override {
        this->vtkInteractorStyleTrackballCamera::OnMouseMove();
    }
    virtual void OnMouseWheelForward() override {
        if (!this->Interactor->GetControlKey() && !this->Interactor->GetShiftKey()) {
            this->vtkInteractorStyleTrackballCamera::OnMouseWheelForward();
        }
    }
    virtual void OnMouseWheelBackward() override {
        if (!this->Interactor->GetControlKey() && !this->Interactor->GetShiftKey()) {
            this->vtkInteractorStyleTrackballCamera::OnMouseWheelBackward();
        }
    }
};

// ==================== 回调函数前向声明 ====================

void LeftButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                             void* clientData, void* callData);
void LeftButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                               void* clientData, void* callData);
void RightButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                              void* clientData, void* callData);
void RightButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                                void* clientData, void* callData);
void KeyPressCallback(vtkObject* caller, long unsigned int eventId,
                      void* clientData, void* callData);
void MouseMoveCallback(vtkObject* caller, long unsigned int eventId,
                       void* clientData, void* callData);
void MouseWheelForwardCallback(vtkObject* caller, long unsigned int eventId,
                               void* clientData, void* callData);
void MouseWheelBackwardCallback(vtkObject* caller, long unsigned int eventId,
                                void* clientData, void* callData);

// ==================== MeshLabeler 实现 ====================

MeshLabeler::MeshLabeler(QObject* parent)
    : QObject(parent)
    , m_currentLabel(0)
    , m_editMode(EditMode::Brush)
    , m_brushRadius(DEFAULT_BRUSH_RADIUS)
    , m_isMousePressed(false)
    , m_renderPending(false)
{
    // 初始化 VTK 对象
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_lookupTable = vtkSmartPointer<vtkLookupTable>::New();
    m_sphereActor = vtkSmartPointer<vtkActor>::New();

    // 初始化颜色查找表
    initializeLookupTable();

    // 创建回调命令
    m_leftButtonPressCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_leftButtonReleaseCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_rightButtonPressCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_rightButtonReleaseCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_keyPressCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_mouseMoveCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_mouseWheelForwardCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_mouseWheelBackwardCallback = vtkSmartPointer<vtkCallbackCommand>::New();

    // 设置回调函数和客户端数据
    m_leftButtonPressCallback->SetCallback(::LeftButtonPressCallback);
    m_leftButtonPressCallback->SetClientData(this);

    m_leftButtonReleaseCallback->SetCallback(::LeftButtonReleaseCallback);
    m_leftButtonReleaseCallback->SetClientData(this);

    m_rightButtonPressCallback->SetCallback(::RightButtonPressCallback);
    m_rightButtonPressCallback->SetClientData(this);

    m_rightButtonReleaseCallback->SetCallback(::RightButtonReleaseCallback);
    m_rightButtonReleaseCallback->SetClientData(this);

    m_keyPressCallback->SetCallback(::KeyPressCallback);
    m_keyPressCallback->SetClientData(this);

    m_mouseMoveCallback->SetCallback(::MouseMoveCallback);
    m_mouseMoveCallback->SetClientData(this);

    m_mouseWheelForwardCallback->SetCallback(::MouseWheelForwardCallback);
    m_mouseWheelForwardCallback->SetClientData(this);

    m_mouseWheelBackwardCallback->SetCallback(::MouseWheelBackwardCallback);
    m_mouseWheelBackwardCallback->SetClientData(this);

    qDebug() << "MeshLabeler initialized";
}

MeshLabeler::~MeshLabeler()
{
    qDebug() << "MeshLabeler destroyed";
}

void MeshLabeler::initializeLookupTable()
{
    m_lookupTable->SetNumberOfTableValues(MAX_LABELS);
    m_lookupTable->Build();

    // 设置标签0为白色
    m_lookupTable->SetTableValue(0, 1.0, 1.0, 1.0, 1.0);

    // 其他标签使用默认颜色
    // 可以在这里自定义每个标签的颜色
}

void MeshLabeler::initializeCellData()
{
    if (!m_polyData) {
        qWarning() << "Cannot initialize cell data: polyData is null";
        return;
    }

    vtkNew<vtkFloatArray> cellData;
    for (int i = 0; i < m_polyData->GetNumberOfCells(); ++i) {
        cellData->InsertTuple1(i, 0);
    }

    m_polyData->GetCellData()->SetScalars(cellData);
    m_polyData->BuildLinks();

    qDebug() << "Initialized" << m_polyData->GetNumberOfCells() << "cells";
}

void MeshLabeler::createFeatureEdges()
{
    if (!m_polyData) {
        return;
    }

    vtkNew<vtkFeatureEdges> featureEdges;
    featureEdges->SetInputData(m_polyData);
    featureEdges->BoundaryEdgesOff();
    featureEdges->FeatureEdgesOn();
    featureEdges->SetFeatureAngle(20);
    featureEdges->ManifoldEdgesOff();
    featureEdges->NonManifoldEdgesOff();
    featureEdges->ColoringOff();
    featureEdges->Update();

    vtkNew<vtkPolyDataMapper> edgeMapper;
    edgeMapper->SetInputConnection(featureEdges->GetOutputPort());

    m_edgeActor = vtkSmartPointer<vtkActor>::New();
    m_edgeActor->SetMapper(edgeMapper);

    vtkNew<vtkNamedColors> colors;
    m_edgeActor->GetProperty()->SetColor(colors->GetColor3d("Red").GetData());
    m_edgeActor->GetProperty()->SetLineWidth(3.0);
    m_edgeActor->GetProperty()->SetRenderLinesAsTubes(0.5);
    m_edgeActor->PickableOff();

    if (m_renderer) {
        m_renderer->AddActor(m_edgeActor);
    }
}

bool MeshLabeler::loadSTL(const QString& filename)
{
    if (filename.isEmpty()) {
        emit errorOccurred("文件名为空");
        return false;
    }

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        emit errorOccurred(QString("文件不存在: %1").arg(filename));
        return false;
    }

    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(filename.toLocal8Bit().data());
    reader->Update();

    if (!reader->GetOutput() || reader->GetOutput()->GetNumberOfPoints() == 0) {
        emit errorOccurred(QString("无法加载STL文件: %1").arg(filename));
        return false;
    }

    // 清除旧数据
    if (m_renderer) {
        m_renderer->RemoveAllViewProps();
    }
    clearHistory();

    // 加载新网格
    m_polyData = reader->GetOutput();
    m_currentFileName = filename;

    // 初始化
    initializeCellData();
    createFeatureEdges();

    // 创建mapper和actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_polyData);
    mapper->SetScalarRange(0, MAX_LABELS - 1);
    mapper->SetLookupTable(m_lookupTable);
    mapper->Update();

    m_polyDataActor = vtkSmartPointer<vtkActor>::New();
    m_polyDataActor->SetMapper(mapper);
    m_polyDataActor->GetProperty()->SetOpacity(1.0);
    m_polyDataActor->GetProperty()->EdgeVisibilityOff();

    if (m_renderer) {
        m_renderer->AddActor(m_polyDataActor);

        vtkNew<vtkNamedColors> colors;
        m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    }

    emit meshLoaded(filename);
    requestRender();

    qDebug() << "Loaded STL file:" << filename;
    qDebug() << "Points:" << m_polyData->GetNumberOfPoints();
    qDebug() << "Cells:" << m_polyData->GetNumberOfCells();

    return true;
}

bool MeshLabeler::loadVTP(const QString& filename)
{
    if (filename.isEmpty()) {
        emit errorOccurred("文件名为空");
        return false;
    }

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        emit errorOccurred(QString("文件不存在: %1").arg(filename));
        return false;
    }

    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(filename.toLocal8Bit().data());
    reader->Update();

    if (!reader->GetOutput() || reader->GetOutput()->GetNumberOfPoints() == 0) {
        emit errorOccurred(QString("无法加载VTP文件: %1").arg(filename));
        return false;
    }

    // 清除旧数据
    if (m_renderer) {
        m_renderer->RemoveAllViewProps();
    }
    clearHistory();

    // 加载新网格
    m_polyData = reader->GetOutput();
    m_currentFileName = filename;

    // 检查是否有标签数据
    if (!m_polyData->GetCellData()->GetScalars()) {
        qDebug() << "No label data found, initializing...";
        initializeCellData();
    } else {
        m_polyData->BuildLinks();
        qDebug() << "Loaded existing label data";
    }

    createFeatureEdges();

    // 创建mapper和actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_polyData);
    mapper->SetScalarRange(0, MAX_LABELS - 1);
    mapper->SetLookupTable(m_lookupTable);
    mapper->Update();

    m_polyDataActor = vtkSmartPointer<vtkActor>::New();
    m_polyDataActor->SetMapper(mapper);
    m_polyDataActor->GetProperty()->SetOpacity(1.0);
    m_polyDataActor->GetProperty()->EdgeVisibilityOff();

    if (m_renderer) {
        m_renderer->AddActor(m_polyDataActor);

        vtkNew<vtkNamedColors> colors;
        m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    }

    emit meshLoaded(filename);
    requestRender();

    qDebug() << "Loaded VTP file:" << filename;
    qDebug() << "Points:" << m_polyData->GetNumberOfPoints();
    qDebug() << "Cells:" << m_polyData->GetNumberOfCells();

    return true;
}

bool MeshLabeler::saveVTP(const QString& filename)
{
    if (!m_polyData) {
        emit errorOccurred("没有可保存的网格数据");
        return false;
    }

    if (filename.isEmpty()) {
        emit errorOccurred("文件名为空");
        return false;
    }

    // 设置标签名称
    m_polyData->GetCellData()->GetScalars()->SetName("Label");
    m_polyData->GetCellData()->GetScalars()->Modified();

    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetInputData(m_polyData);
    writer->SetFileName(filename.toLocal8Bit().data());
    writer->SetDataModeToAscii();

    int result = writer->Write();
    if (result == 0) {
        emit errorOccurred(QString("保存VTP文件失败: %1").arg(filename));
        return false;
    }

    m_currentFileName = filename;
    qDebug() << "Saved VTP file:" << filename;

    return true;
}

bool MeshLabeler::saveToTempFile()
{
    if (!m_polyData) {
        return false;
    }

    QString tempPath = QFileInfo(m_currentFileName).dir().path();
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    m_tempFileName = QString("%1/autosave_%2.vtp").arg(tempPath).arg(timestamp);

    bool result = saveVTP(m_tempFileName);
    if (result) {
        qDebug() << "Auto-saved to:" << m_tempFileName;
    }

    return result;
}

void MeshLabeler::setupRenderer(vtkRenderWindow* renderWindow)
{
    if (!renderWindow) {
        qWarning() << "RenderWindow is null";
        return;
    }

    m_renderWindow = renderWindow;
    m_renderWindow->AddRenderer(m_renderer);
    m_renderWindow->SetSize(1920, 1080);

    qDebug() << "Renderer setup complete";
}

void MeshLabeler::initializeCallbacks()
{
    if (!m_renderWindow) {
        qWarning() << "Cannot initialize callbacks: RenderWindow is null";
        return;
    }

    vtkRenderWindowInteractor* interactor = m_renderWindow->GetInteractor();
    if (!interactor) {
        qWarning() << "Interactor is null";
        return;
    }

    // 设置交互样式
    vtkNew<DesignInteractorStyle> style;
    interactor->SetInteractorStyle(style);

    // 添加观察者
    interactor->AddObserver(vtkCommand::LeftButtonPressEvent, m_leftButtonPressCallback);
    interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent, m_leftButtonReleaseCallback);
    interactor->AddObserver(vtkCommand::RightButtonPressEvent, m_rightButtonPressCallback);
    interactor->AddObserver(vtkCommand::RightButtonReleaseEvent, m_rightButtonReleaseCallback);
    interactor->AddObserver(vtkCommand::KeyPressEvent, m_keyPressCallback);
    interactor->AddObserver(vtkCommand::MouseMoveEvent, m_mouseMoveCallback);
    interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, m_mouseWheelForwardCallback);
    interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, m_mouseWheelBackwardCallback);

    qDebug() << "Callbacks initialized";
}

void MeshLabeler::render()
{
    if (m_renderWindow) {
        m_renderWindow->Render();
    }
    m_renderPending = false;
}

void MeshLabeler::requestRender()
{
    if (!m_renderPending) {
        m_renderPending = true;
        QTimer::singleShot(RENDER_THROTTLE_MS, this, [this]() {
            render();
        });
    }
}

void MeshLabeler::setCurrentLabel(int label)
{
    if (label < 0 || label >= MAX_LABELS) {
        qWarning() << "Invalid label:" << label;
        return;
    }

    if (m_currentLabel != label) {
        m_currentLabel = label;
        emit currentLabelChanged(label);
        qDebug() << "Current label changed to:" << label;
    }
}

void MeshLabeler::setEditMode(EditMode mode)
{
    if (m_editMode != mode) {
        m_editMode = mode;

        if (m_polyDataActor) {
            if (mode == EditMode::Single) {
                m_polyDataActor->GetProperty()->EdgeVisibilityOn();
                if (m_renderer && m_sphereActor) {
                    m_renderer->RemoveActor(m_sphereActor);
                }
            } else {
                m_polyDataActor->GetProperty()->EdgeVisibilityOff();
            }
        }

        emit editModeChanged(mode);
        requestRender();

        qDebug() << "Edit mode changed to:"
                 << (mode == EditMode::Brush ? "Brush" : "Single");
    }
}

void MeshLabeler::setBrushRadius(double radius)
{
    if (radius >= MIN_BRUSH_RADIUS) {
        m_brushRadius = radius;
        qDebug() << "Brush radius:" << m_brushRadius;
    }
}

void MeshLabeler::increaseBrushRadius()
{
    m_brushRadius += BRUSH_RADIUS_STEP;
    qDebug() << "Brush radius increased to:" << m_brushRadius;
}

void MeshLabeler::decreaseBrushRadius()
{
    if (m_brushRadius > MIN_BRUSH_RADIUS) {
        m_brushRadius -= BRUSH_RADIUS_STEP;
        qDebug() << "Brush radius decreased to:" << m_brushRadius;
    }
}

bool MeshLabeler::isCellInSphere(double* position, int cellId) const
{
    if (!m_polyData || cellId < 0 || cellId >= m_polyData->GetNumberOfCells()) {
        return false;
    }

    vtkCell* cell = m_polyData->GetCell(cellId);
    vtkPoints* points = cell->GetPoints();

    double radiusSquared = m_brushRadius * m_brushRadius;

    for (int i = 0; i < points->GetNumberOfPoints(); ++i) {
        double* pt = points->GetPoint(i);
        double distSquared = vtkMath::Distance2BetweenPoints(position, pt);
        if (distSquared < radiusSquared) {
            return true;
        }
    }

    return false;
}

std::vector<int> MeshLabeler::labelWithBFS(double* position, int startCellId)
{
    std::vector<int> affectedCells;

    if (!m_polyData || startCellId < 0 || startCellId >= m_polyData->GetNumberOfCells()) {
        return affectedCells;
    }

    std::queue<int> queue;
    std::unordered_set<int> visited;

    queue.push(startCellId);
    visited.insert(startCellId);

    while (!queue.empty()) {
        int cellId = queue.front();
        queue.pop();

        // 检查是否在球体内
        if (!isCellInSphere(position, cellId)) {
            continue;
        }

        // 检查是否已经是目标标签
        int currentLabel = static_cast<int>(
            m_polyData->GetCellData()->GetScalars()->GetTuple1(cellId));

        if (currentLabel == m_currentLabel) {
            continue;
        }

        // 标记为受影响的单元
        affectedCells.push_back(cellId);

        // 获取邻居单元
        vtkCell* cell = m_polyData->GetCell(cellId);
        for (int i = 0; i < cell->GetNumberOfPoints(); ++i) {
            vtkIdType pointId = cell->GetPointId(i);

            vtkNew<vtkIdList> cellIds;
            m_polyData->GetPointCells(pointId, cellIds);

            for (int j = 0; j < cellIds->GetNumberOfIds(); ++j) {
                int neighborId = cellIds->GetId(j);

                if (visited.find(neighborId) == visited.end()) {
                    visited.insert(neighborId);
                    queue.push(neighborId);
                }
            }
        }
    }

    return affectedCells;
}

void MeshLabeler::labelCell(int cellId, int label)
{
    if (!m_polyData || cellId < 0 || cellId >= m_polyData->GetNumberOfCells()) {
        return;
    }

    m_polyData->GetCellData()->GetScalars()->SetTuple1(cellId, label);
}

void MeshLabeler::labelCells(const std::vector<int>& cellIds, int label)
{
    for (int cellId : cellIds) {
        labelCell(cellId, label);
    }

    m_polyData->GetCellData()->Modified();
    m_polyData->GetCellData()->GetScalars()->Modified();
}

void MeshLabeler::updateBrushSphere(double* position)
{
    vtkNew<vtkSphereSource> sphere;
    sphere->SetCenter(position);
    sphere->SetRadius(m_brushRadius);
    sphere->SetPhiResolution(36);
    sphere->SetThetaResolution(36);
    sphere->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(sphere->GetOutputPort());

    m_sphereActor->SetMapper(mapper);
    m_sphereActor->GetProperty()->SetOpacity(0.2);
    m_sphereActor->GetProperty()->SetColor(m_lookupTable->GetTableValue(m_currentLabel));
    m_sphereActor->PickableOff();

    if (m_renderer) {
        m_renderer->AddActor(m_sphereActor);
    }
}

void MeshLabeler::addCommand(std::shared_ptr<LabelCommand> command)
{
    // 执行命令
    command->execute();

    // 添加到撤销栈
    m_undoStack.push(command);

    // 清空重做栈
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }

    // 限制历史大小
    if (m_undoStack.size() > MAX_HISTORY_SIZE) {
        // 移除最旧的命令（需要临时栈）
        std::stack<std::shared_ptr<LabelCommand>> tempStack;
        while (m_undoStack.size() > 1) {
            tempStack.push(m_undoStack.top());
            m_undoStack.pop();
        }
        m_undoStack.pop();  // 移除最旧的

        while (!tempStack.empty()) {
            m_undoStack.push(tempStack.top());
            tempStack.pop();
        }
    }

    emit historyChanged();
}

void MeshLabeler::undo()
{
    if (m_undoStack.empty()) {
        qDebug() << "Nothing to undo";
        return;
    }

    auto command = m_undoStack.top();
    m_undoStack.pop();

    command->undo();
    m_redoStack.push(command);

    requestRender();
    emit historyChanged();

    qDebug() << "Undo:" << command->description();
}

void MeshLabeler::redo()
{
    if (m_redoStack.empty()) {
        qDebug() << "Nothing to redo";
        return;
    }

    auto command = m_redoStack.top();
    m_redoStack.pop();

    command->execute();
    m_undoStack.push(command);

    requestRender();
    emit historyChanged();

    qDebug() << "Redo:" << command->description();
}

void MeshLabeler::clearHistory()
{
    while (!m_undoStack.empty()) {
        m_undoStack.pop();
    }
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }

    emit historyChanged();
    qDebug() << "History cleared";
}

int MeshLabeler::getCellCount() const
{
    return m_polyData ? m_polyData->GetNumberOfCells() : 0;
}

int MeshLabeler::getCellLabel(int cellId) const
{
    if (!m_polyData || cellId < 0 || cellId >= m_polyData->GetNumberOfCells()) {
        return -1;
    }

    return static_cast<int>(m_polyData->GetCellData()->GetScalars()->GetTuple1(cellId));
}

std::vector<int> MeshLabeler::getLabelStatistics() const
{
    std::vector<int> stats(MAX_LABELS, 0);

    if (!m_polyData) {
        return stats;
    }

    for (int i = 0; i < m_polyData->GetNumberOfCells(); ++i) {
        int label = static_cast<int>(m_polyData->GetCellData()->GetScalars()->GetTuple1(i));
        if (label >= 0 && label < MAX_LABELS) {
            stats[label]++;
        }
    }

    return stats;
}

void MeshLabeler::performAutoSave()
{
    if (isMeshLoaded()) {
        saveToTempFile();
    }
}

// ==================== 回调函数实现 ====================

void LeftButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                             void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (!labeler || !labeler->getPolyData()) {
        return;
    }

    labeler->setMousePressed(true);

    vtkRenderWindowInteractor* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);
    int* pos = interactor->GetEventPosition();
    interactor->FindPokedRenderer(pos[0], pos[1]);

    vtkNew<vtkCellPicker> picker;
    interactor->SetPicker(picker);
    interactor->GetPicker()->Pick(pos[0], pos[1], 0, labeler->getRenderer());

    double position[3];
    picker->GetPickPosition(position);
    int cellId = picker->GetCellId();

    if (cellId >= 0) {
        if (labeler->getEditMode() == EditMode::Brush) {
            // 画刷模式：使用BFS
            std::vector<int> affectedCells = labeler->labelWithBFS(position, cellId);
            if (!affectedCells.empty()) {
                auto command = std::make_shared<PaintCommand>(
                    labeler->getPolyData(), affectedCells, labeler->getCurrentLabel());
                labeler->labelCells(affectedCells, labeler->getCurrentLabel());
                // 注意：这里不调用 addCommand，因为会重复执行
                // 应该先创建命令但不执行，然后通过 addCommand 执行
            }
        } else {
            // 单点模式
            labeler->labelCell(cellId, labeler->getCurrentLabel());
            labeler->getPolyData()->GetCellData()->Modified();
        }

        labeler->requestRender();
    }
}

void LeftButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                               void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (labeler) {
        labeler->setMousePressed(false);
    }
}

void RightButtonPressCallback(vtkObject* caller, long unsigned int eventId,
                              void* clientData, void* callData)
{
    // 由 DesignInteractorStyle 处理
}

void RightButtonReleaseCallback(vtkObject* caller, long unsigned int eventId,
                                void* clientData, void* callData)
{
    // 由 DesignInteractorStyle 处理
}

void KeyPressCallback(vtkObject* caller, long unsigned int eventId,
                      void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (!labeler) {
        return;
    }

    vtkRenderWindowInteractor* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);
    char key = interactor->GetKeyCode();

    if (key == 's') {
        // 切换到单点模式
        labeler->setEditMode(EditMode::Single);
    } else if (key == 'r') {
        // 切换到画刷模式
        labeler->setEditMode(EditMode::Brush);
    } else if (key >= '0' && key <= '9') {
        // 设置标签
        int label = key - '0';
        labeler->setCurrentLabel(label);
    } else if (key == 'z' && interactor->GetControlKey()) {
        // Ctrl+Z 撤销
        labeler->undo();
    } else if (key == 'y' && interactor->GetControlKey()) {
        // Ctrl+Y 重做
        labeler->redo();
    }
}

void MouseMoveCallback(vtkObject* caller, long unsigned int eventId,
                       void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (!labeler || !labeler->getPolyData()) {
        return;
    }

    vtkRenderWindowInteractor* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);
    int* pos = interactor->GetEventPosition();
    interactor->FindPokedRenderer(pos[0], pos[1]);

    vtkNew<vtkCellPicker> picker;
    interactor->SetPicker(picker);
    interactor->GetPicker()->Pick(pos[0], pos[1], 0, labeler->getRenderer());

    double position[3];
    picker->GetPickPosition(position);
    int cellId = picker->GetCellId();

    if (cellId == -1) {
        return;
    }

    if (labeler->getEditMode() == EditMode::Brush) {
        // 画刷模式：显示球体预览
        labeler->updateBrushSphere(position);
        labeler->requestRender();

        if (labeler->isMousePressed()) {
            // 鼠标按下时进行标注
            std::vector<int> affectedCells = labeler->labelWithBFS(position, cellId);
            if (!affectedCells.empty()) {
                labeler->labelCells(affectedCells, labeler->getCurrentLabel());
                labeler->requestRender();
            }
        }
    } else if (labeler->getEditMode() == EditMode::Single) {
        // 单点模式
        if (labeler->isMousePressed()) {
            labeler->labelCell(cellId, labeler->getCurrentLabel());
            labeler->getPolyData()->GetCellData()->Modified();
            labeler->requestRender();
        }
    }
}

void MouseWheelForwardCallback(vtkObject* caller, long unsigned int eventId,
                               void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (!labeler || !labeler->getPolyData()) {
        return;
    }

    vtkRenderWindowInteractor* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);

    if (interactor->GetControlKey()) {
        // Ctrl + 滚轮：调整画刷大小
        labeler->increaseBrushRadius();

        // 更新球体显示
        int* pos = interactor->GetEventPosition();
        interactor->FindPokedRenderer(pos[0], pos[1]);

        vtkNew<vtkCellPicker> picker;
        interactor->SetPicker(picker);
        interactor->GetPicker()->Pick(pos[0], pos[1], 0, labeler->getRenderer());

        double position[3];
        picker->GetPickPosition(position);
        int cellId = picker->GetCellId();

        if (cellId >= 0) {
            labeler->updateBrushSphere(position);
            labeler->requestRender();
        }
    }
}

void MouseWheelBackwardCallback(vtkObject* caller, long unsigned int eventId,
                                void* clientData, void* callData)
{
    MeshLabeler* labeler = static_cast<MeshLabeler*>(clientData);
    if (!labeler || !labeler->getPolyData()) {
        return;
    }

    vtkRenderWindowInteractor* interactor = vtkRenderWindowInteractor::SafeDownCast(caller);

    if (interactor->GetControlKey()) {
        // Ctrl + 滚轮：调整画刷大小
        labeler->decreaseBrushRadius();

        // 更新球体显示
        int* pos = interactor->GetEventPosition();
        interactor->FindPokedRenderer(pos[0], pos[1]);

        vtkNew<vtkCellPicker> picker;
        interactor->SetPicker(picker);
        interactor->GetPicker()->Pick(pos[0], pos[1], 0, labeler->getRenderer());

        double position[3];
        picker->GetPickPosition(position);
        int cellId = picker->GetCellId();

        if (cellId >= 0) {
            labeler->updateBrushSphere(position);
            labeler->requestRender();
        }
    }
}
