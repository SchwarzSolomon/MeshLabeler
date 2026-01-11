/**
 * @file mainwindow.cpp
 * @brief 主窗口类的实现
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QDebug>
#include <QTextCodec>

#pragma execution_character_set("utf-8")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_labeler(nullptr)
    , m_autoSaveTimer(nullptr)
{
    ui->setupUi(this);

    m_appPath = QCoreApplication::applicationDirPath();

    // 初始化配置
    m_config = new QSettings(m_appPath + "/config.ini", QSettings::IniFormat, this);
    m_config->setIniCodec(QTextCodec::codecForName("UTF-8"));

    loadConfig();

    // 创建 MeshLabeler 实例
    m_labeler = new MeshLabeler(this);

    // 设置渲染窗口
    m_labeler->setupRenderer(ui->qvtkWidget->GetRenderWindow());
    m_labeler->initializeCallbacks();

    // 连接信号和槽
    connect(m_labeler, &MeshLabeler::currentLabelChanged,
            this, &MainWindow::onLabelChanged);
    connect(m_labeler, &MeshLabeler::errorOccurred,
            this, &MainWindow::onError);
    connect(m_labeler, &MeshLabeler::meshLoaded,
            this, &MainWindow::onMeshLoaded);

    // 设置自动保存定时器
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(MeshLabeler::AUTO_SAVE_INTERVAL_MS);
    connect(m_autoSaveTimer, &QTimer::timeout,
            this, &MainWindow::performAutoSave);
    m_autoSaveTimer->start();

    qDebug() << "MainWindow initialized";
}

MainWindow::~MainWindow()
{
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
    }

    delete m_config;
    delete ui;

    qDebug() << "MainWindow destroyed";
}

void MainWindow::loadConfig()
{
    m_config->beginGroup("path");
    QString inputFileName = m_config->value("INPUT_FILE_NAME").toString();
    QString outputFileName = m_config->value("OUTPUT_FILE_NAME").toString();
    m_lastOpenPath = m_config->value("LAST_OPEN_PATH").toString();
    m_config->endGroup();

    // 尝试自动加载上次的文件
    if (!inputFileName.isEmpty() && QFileInfo::exists(inputFileName)) {
        if (m_labeler) {
            // 根据文件扩展名选择加载方法
            if (inputFileName.endsWith(".vtp", Qt::CaseInsensitive)) {
                m_labeler->loadVTP(inputFileName);
            } else {
                m_labeler->loadSTL(inputFileName);
            }
            ui->fileName_label->setText(inputFileName);
        }
    }
}

void MainWindow::saveConfig()
{
    m_config->clear();
    m_config->beginGroup("path");

    if (m_labeler && m_labeler->isMeshLoaded()) {
        m_config->setValue("INPUT_FILE_NAME", ui->fileName_label->text());
    }
    m_config->setValue("LAST_OPEN_PATH", m_lastOpenPath);

    m_config->endGroup();
    m_config->sync();
}

void MainWindow::on_inputFile_btn_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择网格文件"),
        m_lastOpenPath,
        "Mesh Files(*.stl *.vtp *.ply *.obj);;STL Files(*.stl);;VTP Files(*.vtp);;All Files(*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    // 根据文件扩展名选择加载方法
    bool success = false;
    if (fileName.endsWith(".vtp", Qt::CaseInsensitive)) {
        success = m_labeler->loadVTP(fileName);
    } else if (fileName.endsWith(".stl", Qt::CaseInsensitive)) {
        success = m_labeler->loadSTL(fileName);
    } else {
        // 默认尝试 STL
        success = m_labeler->loadSTL(fileName);
    }

    if (success) {
        m_lastOpenPath = QFileInfo(fileName).dir().path();
        ui->fileName_label->setText(fileName);
        saveConfig();
        qDebug() << "Loaded file:" << fileName;
    }
}

void MainWindow::on_outPut_btn_clicked()
{
    if (!m_labeler || !m_labeler->isMeshLoaded()) {
        QMessageBox::warning(this, tr("警告"), tr("没有可保存的网格数据"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("保存标注文件"),
        m_lastOpenPath,
        "VTP Files(*.vtp)");

    if (fileName.isEmpty()) {
        return;
    }

    // 确保文件扩展名
    if (!fileName.endsWith(".vtp", Qt::CaseInsensitive)) {
        fileName += ".vtp";
    }

    bool success = m_labeler->saveVTP(fileName);

    if (success) {
        m_lastOpenPath = QFileInfo(fileName).dir().path();
        saveConfig();
        QMessageBox::information(this, tr("成功"), tr("文件已保存: %1").arg(fileName));
        qDebug() << "Saved file:" << fileName;
    }
}

void MainWindow::onLabelChanged(int newLabel)
{
    // 更新 SpinBox 显示当前标签
    if (ui->spinBox->value() != newLabel) {
        ui->spinBox->setValue(newLabel);
    }
}

void MainWindow::on_spinBox_valueChanged(int arg1)
{
    if (m_labeler && m_labeler->getCurrentLabel() != arg1) {
        m_labeler->setCurrentLabel(arg1);
    }
}

void MainWindow::onError(const QString& errorMessage)
{
    QMessageBox::critical(this, tr("错误"), errorMessage);
    qCritical() << "Error:" << errorMessage;
}

void MainWindow::onMeshLoaded(const QString& filename)
{
    ui->fileName_label->setText(filename);
    qDebug() << "Mesh loaded:" << filename;

    // 显示统计信息
    if (m_labeler) {
        int cellCount = m_labeler->getCellCount();
        auto stats = m_labeler->getLabelStatistics();

        qDebug() << "Total cells:" << cellCount;
        for (int i = 0; i < stats.size(); ++i) {
            if (stats[i] > 0) {
                qDebug() << "Label" << i << ":" << stats[i] << "cells";
            }
        }
    }
}

void MainWindow::performAutoSave()
{
    if (m_labeler && m_labeler->isMeshLoaded()) {
        m_labeler->performAutoSave();
    }
}
