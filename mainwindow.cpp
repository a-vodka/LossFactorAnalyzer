#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLineSeries>
#include <aboutdialog.h>
#include "modbusconfigdialog.h"
#include "modbusreader.h"

#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QTimer>
#include "sinesweepgenerator.h"

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

    sensor1Indicator = new LedIndicator(this);
    sensor2Indicator = new LedIndicator(this);

    QLabel *label1 = new QLabel("Sensor 1:");
    QLabel *label2 = new QLabel("Sensor 2:");

    statusBar()->addPermanentWidget(label1);
    statusBar()->addPermanentWidget(sensor1Indicator);
    statusBar()->addPermanentWidget(label2);
    statusBar()->addPermanentWidget(sensor2Indicator);


    this->dlg = new modbusconfigdialog(this);
    ModbusReader *reader = new ModbusReader;
    reader->start(dlg->port(), dlg->baudRate(), dlg->dataBits(), dlg->parity(),
                  dlg->stopBits(), dlg->flowControl(), dlg->device1Address(), dlg->device2Address());

    connect(reader, &ModbusReader::dataReady, this, [](int id, float value) {
        qDebug() << "Device" << id << "Value:" << value;
    });

    connect(reader, &ModbusReader::errorOccurred, this, [](const QString &err) {
        qWarning() << "Modbus error:" << err;
    });

    QTimer* statusTimer = new QTimer(this);
    //statusTimer->setInterval(1000);
    statusTimer->start(1000);
    connect(statusTimer, &QTimer::timeout, this, [reader, this](){
        bool ok1 = reader->device1ReadSuccess();
        bool ok2 = reader->device2ReadSuccess();
        sensor1Indicator->setState(ok1 ? LedIndicator::Green : LedIndicator::Red);
        sensor2Indicator->setState(ok2 ? LedIndicator::Green : LedIndicator::Red);
    });

    connect(qApp, &QCoreApplication::aboutToQuit, [=]() {
        reader->stop();
        //thread->quit();
        //thread->wait();
        reader->deleteLater();
        //thread->deleteLater();
    });

   // thread->start();


}

MainWindow::~MainWindow()
{
    delete ui;
    delete progressBar;
    delete dlg;
    delete sensor1Indicator;
    delete sensor2Indicator;
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

    // Setup audio format
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16); // Qt 6

    // Get default output device
    QAudioDevice device = QMediaDevices::defaultAudioOutput();

    if (!device.isFormatSupported(format)) {

        const auto devices = QMediaDevices::audioOutputs();
        for (const QAudioDevice &device : devices)
        {
            QAudioFormat qfm = device.preferredFormat();
            qDebug() << "Device: " << device.description();
            qDebug() << "Sample format: " <<qfm.sampleFormat();
            qDebug() << "Channel config: " <<  qfm.channelConfig();
            qDebug() << "Channel count:" << qfm.channelCount();
            qDebug() << "Channel sanple rate:" << qfm.sampleRate();
        }
        qWarning() << "Audio format not supported by output device.";
        return ;
    }

    // Sweep parameters
    float startFreq = 200.0f;    // Hz
    float endFreq   = 2000.0f;   // Hz
    float duration  = 5.0f;      // seconds

    // Create sweep generator
    auto generator = new SineSweepGenerator(startFreq, endFreq, duration, format);
    auto audioOutput = new QAudioOutput(device, this);

    // Start generation and playback
    generator->start();
   // audioOutput->start();



}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionCOM_Port_Settings_triggered()
{
    modbusconfigdialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        qDebug() << "Port:" << dlg.port();
        qDebug() << "Baud:" << dlg.baudRate();
        qDebug() << "Data bits:" << dlg.dataBits();
        qDebug() << "Parity:" << dlg.parity();
        qDebug() << "Stop bits:" << dlg.stopBits();
        qDebug() << "Flow control:" << dlg.flowControl();
        int addr1 = dlg.device1Address();
        int addr2 = dlg.device2Address();
        qDebug() << "Device 1 Address:" << addr1;
        qDebug() << "Device 2 Address:" << addr2;
    }
}

