# MeshLabeler

<div align="center">

![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)

**一款基于 VTK 和 Qt 的专业 3D 网格标注工具**

[功能特性](#-功能特性) •
[快速开始](#-快速开始) •
[文档](#-文档) •
[更新日志](#-更新日志)

</div>

---

## 📖 简介

MeshLabeler 是一款用于 3D 网格标注的专业工具，支持 STL、VTP 等格式。通过直观的交互界面和强大的算法，让网格标注工作变得简单高效。

### ✨ 功能特性

- **🎨 双模式标注**
  - 画刷模式：使用 BFS 算法智能标注区域
  - 单点模式：精确控制单个面片

- **📂 文件支持**
  - 导入：STL、VTP 格式
  - 导出：VTP（包含标签数据）
  - 支持继续编辑已标注文件

- **⚡ 性能优化**
  - 修复递归栈溢出问题（改为迭代 BFS）
  - 渲染节流优化（60fps）
  - 支持大型网格（百万面片级别）

- **🔧 编辑功能**
  - 撤销/重做（最多 100 步）
  - 自动保存（每 5 分钟）
  - 20 种可自定义标签

- **🖥️ 用户体验**
  - 实时 3D 可视化
  - 特征边缘显示
  - 完善的错误提示
  - 跨平台支持

---

## 🚀 快速开始

### 系统要求

- **操作系统**: Windows 10/11, Linux, macOS
- **依赖库**:
  - Qt 5.12+
  - VTK 8.2+
  - CMake 3.12+ (构建)

### 构建

#### 使用 CMake (推荐)

```bash
# 克隆仓库
git clone https://github.com/SchwarzSolomon/MeshLabeler.git
cd MeshLabeler

# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
cmake --build . --config Release

# 运行
./bin/MeshLabeler
```

#### 使用 qmake

```bash
# 修改 VTKlib.pri 中的 VTK 路径
# 然后执行：
qmake labelTest.pro
make release
```

### 快速使用

1. **加载网格**：点击"输入文件"按钮，选择 STL 或 VTP 文件
2. **选择标签**：按数字键 `0-9` 选择标签
3. **开始标注**：
   - 画刷模式（`R`）：左键拖动标注区域
   - 单点模式（`S`）：左键点击标注单个面片
4. **保存结果**：点击"输出文件"按钮保存为 VTP

---

## 📚 文档

- **[用户手册](USER_MANUAL.md)** - 详细使用说明
- **[项目流程图](PROJECT_FLOWCHART.md)** - 架构和流程
- **[优化建议](OPTIMIZATION_SUGGESTIONS.md)** - 代码优化文档
- **[任务清单](TODO.md)** - 开发计划

---

## 🎮 快捷键

| 按键 | 功能 |
|------|------|
| `0-9` | 选择标签编号 |
| `R` | 切换到画刷模式 |
| `S` | 切换到单点模式 |
| `Ctrl + Z` | 撤销 |
| `Ctrl + Y` | 重做 |
| `Ctrl + 滚轮` | 调整画刷大小 |
| 左键拖动 | 标注 |
| 右键拖动 | 旋转视图 |
| 滚轮 | 缩放视图 |

---

## 🔄 更新日志

### v2.0.0 (2026-01-11) - 重大更新

**架构重构**
- ✅ 将 `label.cpp` 重构为独立的 `MeshLabeler` 类
- ✅ 消除全局变量，使用正确的封装
- ✅ 完整的 Doxygen 注释

**核心功能**
- ✅ 修复 BFS 递归栈溢出问题（改为迭代实现）
- ✅ 实现撤销/重做功能（命令模式）
- ✅ 支持加载 VTP 文件继续编辑
- ✅ 自动保存功能（每 5 分钟）

**性能优化**
- ✅ 渲染节流（60fps）
- ✅ 球体对象复用
- ✅ 代码去重

**质量提升**
- ✅ 完整的错误处理
- ✅ 枚举类型替换魔法数字
- ✅ 常量定义
- ✅ 统一命名规范

**构建系统**
- ✅ 添加 CMakeLists.txt
- ✅ 跨平台构建支持

**文档**
- ✅ 用户手册
- ✅ 项目流程图
- ✅ 优化建议文档

### v1.0.0 (初始版本)
- 基本的 STL 加载和标注功能
- 画刷和单点两种模式

---

## 🏗️ 技术栈

- **UI 框架**: Qt 5/6
- **3D 渲染**: VTK (Visualization Toolkit)
- **构建系统**: CMake / qmake
- **语言**: C++ 17

### 架构设计

```
┌─────────────────┐
│   MainWindow    │  Qt 主窗口
│   (UI Layer)    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  MeshLabeler    │  核心标注引擎
│  (Core Layer)   │
└────────┬────────┘
         │
    ┌────┴────┐
    ▼         ▼
┌───────┐ ┌───────┐
│  VTK  │ │  Qt   │  依赖库
└───────┘ └───────┘
```

---

## 🤝 贡献

欢迎贡献代码、报告问题或提出建议！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

---

## 📝 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

---

## 👥 作者

- **SchwarzSolomon** - [GitHub](https://github.com/SchwarzSolomon)

---

## 🙏 致谢

- [VTK](https://vtk.org/) - 3D 可视化工具包
- [Qt](https://www.qt.io/) - 跨平台 UI 框架

---

<div align="center">

**如果这个项目对你有帮助，请给个 ⭐️ Star！**

</div>
