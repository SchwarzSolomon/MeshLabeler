/**
 * @file mainwindow.h
 * @brief 主窗口类定义
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include "meshlabeler.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 主窗口类
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainWindow();

    /**
     * @brief 加载配置文件
     */
    void loadConfig();

    /**
     * @brief 保存配置文件
     */
    void saveConfig();

private slots:
    /**
     * @brief 输入文件按钮点击事件
     */
    void on_inputFile_btn_clicked();

    /**
     * @brief 输出文件按钮点击事件
     */
    void on_outPut_btn_clicked();

    /**
     * @brief 标签值改变事件
     */
    void onLabelChanged(int newLabel);

    /**
     * @brief SpinBox值改变事件
     */
    void on_spinBox_valueChanged(int arg1);

    /**
     * @brief 错误处理槽
     */
    void onError(const QString& errorMessage);

    /**
     * @brief 网格加载完成槽
     */
    void onMeshLoaded(const QString& filename);

    /**
     * @brief 执行自动保存
     */
    void performAutoSave();

private:
    Ui::MainWindow *ui;                ///< UI对象
    QString m_appPath;                 ///< 程序路径
    QString m_lastOpenPath;            ///< 最后打开文件的路径
    QSettings *m_config;               ///< 配置对象
    MeshLabeler *m_labeler;            ///< 标注器对象
    QTimer *m_autoSaveTimer;           ///< 自动保存定时器
};

#endif // MAINWINDOW_H
