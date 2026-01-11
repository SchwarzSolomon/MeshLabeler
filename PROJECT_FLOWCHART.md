# MeshLabeler 项目流程图

## 项目概述
MeshLabeler 是一个基于 Qt 和 VTK 的 3D 网格标注工具,支持 STL/PLY/OBJ 格式的网格文件标注。

## 主要流程图

```mermaid
flowchart TD
    Start([程序启动]) --> InitQt[初始化 Qt 应用]
    InitQt --> DisableVTKWarning[禁用 VTK 警告]
    DisableVTKWarning --> CreateMainWindow[创建主窗口]
    CreateMainWindow --> LoadConfig[加载配置文件 config.ini]
    LoadConfig --> InitVTKWidget[初始化 VTK 渲染窗口]
    InitVTKWidget --> SetupCallbacks[设置事件回调函数]
    SetupCallbacks --> CheckPreviousFile{是否有历史文件?}

    CheckPreviousFile -->|是| AutoLoadMesh[自动加载上次文件]
    CheckPreviousFile -->|否| ShowEmptyWindow[显示空窗口]

    AutoLoadMesh --> DisplayMesh[显示网格模型]
    ShowEmptyWindow --> WaitUserInput[等待用户操作]
    DisplayMesh --> WaitUserInput

    WaitUserInput --> UserAction{用户操作}

    %% 文件输入分支
    UserAction -->|点击输入文件按钮| OpenFileDialog[打开文件对话框]
    OpenFileDialog --> SelectSTL[选择 STL 文件]
    SelectSTL --> LoadSTL[使用 vtkSTLReader 加载]
    LoadSTL --> ExtractFeatures[提取特征边缘]
    ExtractFeatures --> InitCellData[初始化单元数据为 0]
    InitCellData --> SetupLUT[设置颜色查找表 20色]
    SetupLUT --> CreateMapper[创建 PolyDataMapper]
    CreateMapper --> RenderMesh[渲染网格到窗口]
    RenderMesh --> SaveConfigFile[保存配置文件]
    SaveConfigFile --> WaitUserInput

    %% 标注模式选择
    UserAction -->|按键 'r'| BrushMode[切换到画刷模式]
    UserAction -->|按键 's'| SingleMode[切换到单点模式]

    BrushMode --> SetEditMode0[EditMode = 0]
    SetEditMode0 --> HideEdges[隐藏网格边缘]
    HideEdges --> WaitUserInput

    SingleMode --> SetEditMode1[EditMode = 1]
    SetEditMode1 --> ShowEdges[显示网格边缘]
    ShowEdges --> RemoveSphere[移除画刷球体]
    RemoveSphere --> WaitUserInput

    %% 标签选择
    UserAction -->|按数字键 0-9| SetLabel[设置当前标签 PressFlag]
    SetLabel --> UpdateSpinBox[更新 UI SpinBox]
    UpdateSpinBox --> WaitUserInput

    %% 画刷模式标注
    UserAction -->|鼠标移动 EditMode=0| PickPosition[拾取鼠标位置]
    PickPosition --> CreateSphere[创建球体预览]
    CreateSphere --> SetSphereColor[设置球体颜色为当前标签色]
    SetSphereColor --> CheckMousePressed{鼠标是否按下?}

    CheckMousePressed -->|否| DisplaySphereOnly[仅显示球体预览]
    DisplaySphereOnly --> WaitUserInput

    CheckMousePressed -->|是| GetCellID[获取拾取的三角形 ID]
    GetCellID --> BFSPaint[BFS 广度优先搜索标注]

    BFSPaint --> CheckInSphere{三角形顶点在球内?}
    CheckInSphere -->|否| SkipCell[跳过此单元]
    CheckInSphere -->|是| CheckAlreadyLabeled{已是目标标签?}

    CheckAlreadyLabeled -->|是| SkipCell
    CheckAlreadyLabeled -->|否| LabelCell[标注单元为当前标签]

    LabelCell --> GetNeighbors[获取相邻三角形]
    GetNeighbors --> RecursiveBFS[递归 BFS 处理相邻单元]
    RecursiveBFS --> UpdateCellData[更新单元数据]
    UpdateCellData --> RenderUpdate1[更新渲染]
    RenderUpdate1 --> WaitUserInput

    SkipCell --> WaitUserInput

    %% 单点模式标注
    UserAction -->|鼠标移动 EditMode=1<br/>且鼠标按下| PickCell[拾取单个三角形]
    PickCell --> LabelSingleCell[标注单个单元]
    LabelSingleCell --> UpdateScalars[更新 Scalars 数据]
    UpdateScalars --> RenderUpdate2[更新渲染]
    RenderUpdate2 --> WaitUserInput

    %% 鼠标点击
    UserAction -->|左键按下| SetPressFlag[Press = 1]
    SetPressFlag --> PickAndLabel[拾取并标注三角形]
    PickAndLabel --> RenderUpdate3[更新渲染]
    RenderUpdate3 --> WaitUserInput

    UserAction -->|左键释放| ReleasePressFlag[Press = 0]
    ReleasePressFlag --> WaitUserInput

    UserAction -->|右键按下| StartRotate[开始旋转视图]
    StartRotate --> WaitUserInput

    %% 画刷大小调整
    UserAction -->|Ctrl+滚轮前| IncreaseRadius[增加画刷半径 +0.15]
    UserAction -->|Ctrl+滚轮后| DecreaseRadius[减小画刷半径 -0.15]

    IncreaseRadius --> UpdateSphereSize1[更新球体大小显示]
    DecreaseRadius --> UpdateSphereSize2[更新球体大小显示]
    UpdateSphereSize1 --> WaitUserInput
    UpdateSphereSize2 --> WaitUserInput

    %% 保存文件
    UserAction -->|点击输出文件按钮| SaveDialog[打开保存对话框]
    SaveDialog --> SelectVTP[选择 VTP 输出路径]
    SelectVTP --> SetLabelName[设置标签数组名称为 'Label']
    SetLabelName --> WriteVTP[使用 vtkXMLPolyDataWriter 写入]
    WriteVTP --> SaveConfig2[保存配置文件]
    SaveConfig2 --> WaitUserInput

    %% 退出
    UserAction -->|关闭窗口| Cleanup[清理资源]
    Cleanup --> End([程序结束])

    style Start fill:#90EE90
    style End fill:#FFB6C1
    style BFSPaint fill:#FFE4B5
    style RenderMesh fill:#87CEEB
    style WriteVTP fill:#DDA0DD
```

## 数据结构流程

```mermaid
flowchart LR
    STL[STL 文件] --> Reader[vtkSTLReader]
    Reader --> PolyData[vtkPolyData<br/>网格数据]
    PolyData --> CellData[CellData<br/>单元数据]
    CellData --> Scalars[vtkFloatArray<br/>标签值 0-19]

    Scalars --> LUT[vtkLookupTable<br/>颜色查找表 20色]
    LUT --> Mapper[vtkPolyDataMapper]
    Mapper --> Actor[vtkActor]
    Actor --> Renderer[vtkRenderer]
    Renderer --> RenderWindow[vtkRenderWindow]
    RenderWindow --> QVTKWidget[QVTKWidget<br/>Qt 显示窗口]

    PolyData --> Writer[vtkXMLPolyDataWriter]
    Writer --> VTP[VTP 文件<br/>带标签数据]

    style STL fill:#FFE4B5
    style VTP fill:#DDA0DD
```

## 用户交互模式

```mermaid
stateDiagram-v2
    [*] --> 画刷模式

    画刷模式 --> 单点模式: 按 's' 键
    单点模式 --> 画刷模式: 按 'r' 键

    state 画刷模式 {
        [*] --> 预览球体
        预览球体 --> 区域标注: 左键按下+移动
        区域标注 --> 预览球体: 左键释放
    }

    state 单点模式 {
        [*] --> 显示网格边缘
        显示网格边缘 --> 单元标注: 左键按下+移动
        单元标注 --> 显示网格边缘: 左键释放
    }

    画刷模式 --> 调整画刷: Ctrl+滚轮
    调整画刷 --> 画刷模式

    画刷模式 --> 切换标签: 数字键 0-9
    单点模式 --> 切换标签: 数字键 0-9
    切换标签 --> 画刷模式
    切换标签 --> 单点模式
```

## 核心算法：BFS 区域标注

```mermaid
flowchart TD
    Start([BFS 开始]) --> CheckInSphere{顶点在球体内?}
    CheckInSphere -->|否| Return1([返回])
    CheckInSphere -->|是| CheckLabeled{已是目标标签?}
    CheckLabeled -->|是| Return2([返回])
    CheckLabeled -->|否| SetLabel[设置单元标签]

    SetLabel --> GetVertex0[获取顶点 0]
    GetVertex0 --> GetVertex1[获取顶点 1]
    GetVertex1 --> GetVertex2[获取顶点 2]

    GetVertex2 --> GetCells0[获取顶点 0 相邻单元]
    GetCells0 --> GetCells1[获取顶点 1 相邻单元]
    GetCells1 --> GetCells2[获取顶点 2 相邻单元]

    GetCells2 --> Loop0[遍历顶点 0 的相邻单元]
    Loop0 --> RecursiveBFS0[递归调用 BFS]
    RecursiveBFS0 --> Loop1[遍历顶点 1 的相邻单元]
    Loop1 --> RecursiveBFS1[递归调用 BFS]
    RecursiveBFS1 --> Loop2[遍历顶点 2 的相邻单元]
    Loop2 --> RecursiveBFS2[递归调用 BFS]
    RecursiveBFS2 --> Return3([返回])

    style Start fill:#90EE90
    style Return1 fill:#FFB6C1
    style Return2 fill:#FFB6C1
    style Return3 fill:#FFB6C1
```

## 配置文件结构

```
[path]
INPUT_FILE_NAME=上次打开的输入文件路径.stl
OUTPUT_FILE_NAME=上次保存的输出文件路径.vtp
LAST_OPEN_PATH=上次访问的目录路径
```

## 快捷键说明

| 按键 | 功能 |
|------|------|
| 0-9 | 选择标签编号 (0-9) |
| s | 切换到单点标注模式 |
| r | 切换到画刷标注模式 |
| 左键 | 标注网格单元 |
| 右键拖拽 | 旋转视图 |
| Ctrl + 滚轮↑ | 增大画刷半径 |
| Ctrl + 滚轮↓ | 减小画刷半径 |
| 滚轮 | 缩放视图 |

## 技术栈

- **UI 框架**: Qt 5/6
- **3D 渲染**: VTK (Visualization Toolkit)
- **支持格式**:
  - 输入: STL, PLY, OBJ
  - 输出: VTP (VTK XML PolyData)
- **构建系统**: qmake (.pro 文件)
