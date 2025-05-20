#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include "modbusconfigdialog.h"
#include "ledindicator.h"
#include "generator.h"
#include <QAudioSink>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void updateProgress(int value);

private slots:

    void on_actionAbout_triggered();
    void on_actionCOM_Port_Settings_triggered();
    void on_actionAudio_Settings_triggered();

    void on_start_btn_clicked();

private:
    Ui::MainWindow *ui;
    modbusconfigdialog *dlg;
    QProgressBar *progressBar;

    LedIndicator *sensor1Indicator;
    LedIndicator *sensor2Indicator;

    QScopedPointer<Generator> m_generator;
    QScopedPointer<QAudioSink> m_audioOutput;

    QTimer *m_progressTimer;
    bool is_generator_works = false;
    void stop_generation();
    void start_generation();
private slots:
    void updateProgressBar();
};
#endif // MAINWINDOW_H
