#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLineSeries>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);

    // Add to status bar
    ui->statusBar->addPermanentWidget(progressBar);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete progressBar;
}

void MainWindow::updateProgress(int value)
{
    progressBar->setValue(value);
}

void MainWindow::on_pushButton_clicked()
{
    QLineSeries *series = new QLineSeries();
    series->append(0, 0);
    series->append(1, 2);
    series->append(2, 1);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle("Chart in Designer");

    ui->chartView->setChart(chart);
}

