#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include "modbusconfigdialog.h"
#include "ledindicator.h"

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
    void on_pushButton_clicked();
    void on_actionAbout_triggered();
    void on_actionCOM_Port_Settings_triggered();

private:
    Ui::MainWindow *ui;
    modbusconfigdialog *dlg;
    QProgressBar *progressBar;

    LedIndicator *sensor1Indicator;
    LedIndicator *sensor2Indicator;
};
#endif // MAINWINDOW_H
