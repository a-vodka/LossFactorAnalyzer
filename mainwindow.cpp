#include "mainwindow.h"
#include "audiosettingsdialog.h"
#include "generator.h"
#include "ui_mainwindow.h"
#include <QLineSeries>
#include <aboutdialog.h>
#include "modbusconfigdialog.h"
#include "modbusreader.h"
#include <QAudioOutput>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QTimer>
#include <QAudioSink>
#include "livechartwidget.h"

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

    QLabel *status1 = new QLabel("");
    QLabel *status2 = new QLabel("");

    statusBar()->addPermanentWidget(status1);
    statusBar()->addPermanentWidget(status2);

    statusBar()->addPermanentWidget(label1);
    statusBar()->addPermanentWidget(sensor1Indicator);
    statusBar()->addPermanentWidget(label2);
    statusBar()->addPermanentWidget(sensor2Indicator);


    this->dlg = new modbusconfigdialog(this);
    ModbusReader *reader = new ModbusReader;
    reader->start(dlg->port(), dlg->baudRate(), dlg->dataBits(), dlg->parity(),
                  dlg->stopBits(), dlg->flowControl(), dlg->device1Address(), dlg->device2Address());

    connect(reader, &ModbusReader::dataReady, this, [](int deviceId, int paramIndex, float value) {
        qDebug() << "Device" << deviceId << "paramIndex:"<< paramIndex << "Value:" << value;

    });

    connect(reader, &ModbusReader::errorOccurred, this, [](const QString &err) {
        qWarning() << "Modbus error:" << err;
    });

    QTimer* statusTimer = new QTimer(this);
    statusTimer->start(1000);
    connect(statusTimer, &QTimer::timeout, this, [reader, this, status1, status2](){
        bool ok1 = reader->device1ReadSuccess();
        bool ok2 = reader->device2ReadSuccess();
        sensor1Indicator->setState(ok1 ? LedIndicator::Green : LedIndicator::Red);
        sensor2Indicator->setState(ok2 ? LedIndicator::Green : LedIndicator::Red);
        float amp1 = reader->lastValue(0, AMP),
              freq1 = reader->lastValue(0, FREQ),
              dist1 = reader->lastValue(0, DIST);

        float amp2 = reader->lastValue(1, AMP),
              freq2 = reader->lastValue(1, FREQ),
              dist2 = reader->lastValue(1, DIST);

        QString frmt = "Amp = %1 | Freq = %2 | Dist = %3";
        QString s1 = QString("Sens 1: " + frmt).arg(amp1 / 1e3, 4, 'f' , 2).arg(freq1, 3, 'f', 0).arg(dist1, 3, 'f' , 1);
        QString s2 = QString("Sens 2: " + frmt).arg(amp2 / 1e3, 4, 'f' , 2).arg(freq2, 3, 'f', 0).arg(dist2, 3, 'f' , 1);
        status1->setText(s1);
        status2->setText(s2);
    });

    connect(qApp, &QCoreApplication::aboutToQuit, [=]() {
        reader->stop();
        reader->deleteLater();
    });

    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::updateProgressBar);

    // Assuming `reader` is already started
    LiveChartWidget* chart = new LiveChartWidget(reader, 0); // 0 = device 1

    // Check if group box already has a layout
    if (!ui->graph_box->layout()) {
        ui->graph_box->setLayout(new QVBoxLayout());
    }
    ui->graph_box->layout()->addWidget(chart);
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


QAudioDevice getAudioDeviceById(const QByteArray &deviceId, bool isInput = false)
{
    const QList<QAudioDevice> devices = isInput
                                            ? QMediaDevices::audioInputs()
                                            : QMediaDevices::audioOutputs();

    for (const QAudioDevice &device : devices) {
        if (device.id() == deviceId) {
            qDebug() << "Found audio device with id" << deviceId;
            return device;  // Found the matching device
        }
    }

    qDebug() << "NOT Found audio device. Use Default";

    return isInput ? QMediaDevices(nullptr).defaultAudioInput() : QMediaDevices(nullptr).defaultAudioOutput();  // Return default-constructed (invalid) if not found
}



void MainWindow::on_start_btn_clicked()
{
 /*   QLineSeries *series = new QLineSeries();
    series->append(0, 0);
    series->append(1, 2);
    series->append(2, 1);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle("Chart in Designer");

    ui->chartView->setChart(chart);
*/

    if (is_generator_works)
    {
        stop_generation();
    }
    else
    {
        start_generation();
    }

}

void MainWindow::stop_generation()
{
    is_generator_works = false;
    m_progressTimer->stop();

    m_generator->stop();
    m_audioOutput->stop();
    m_audioOutput->disconnect(this);
    ui->start_btn->setText("Start");
}

void MainWindow::start_generation()
{
    is_generator_works = true;
    float startFreq = ui->start_freq->text().toFloat();   // Hz
    float endFreq   = ui->end_freq->text().toFloat();     // Hz
    float duration  = ui->duration->text().toFloat();     // seconds

    QSettings settings;
    QByteArray device_id = settings.value("audio/deviceId").toByteArray();
    QAudioDevice DeviceInfo = getAudioDeviceById(device_id);

    QAudioFormat format = DeviceInfo.preferredFormat();
    qDebug() << "Device: " << DeviceInfo.description();
    qDebug() << "Sample format: " <<format.sampleFormat();
    qDebug() << "Channel config: " <<  format.channelConfig();
    qDebug() << "Channel count:" << format.channelCount();
    qDebug() << "Channel sample rate:" << format.sampleRate();

    qint64 durationUs = duration * 1000000; // 3 seconds

    int value = settings.value("audio/volume").toInt();
    qreal linearVolume = QAudio::convertVolume(value / qreal(100), QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);

    qDebug() << "Linear volume:" << linearVolume;

    m_generator.reset(new Generator(format, durationUs, startFreq, endFreq));
    m_audioOutput.reset(new QAudioSink(DeviceInfo, format));
    m_audioOutput->setVolume(linearVolume);
    m_generator->start();
    m_audioOutput->start(m_generator.data());

    m_progressTimer->start(250);

    ui->start_btn->setText("Stop");
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionAudio_Settings_triggered()
{
    static AudioSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        QString deviceId = dialog.selectedDeviceId();
        int volume = dialog.selectedVolume();
        qDebug() << "Device id: " <<deviceId;
        qDebug() << "Volume:" << volume;
    }
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

void MainWindow::updateProgressBar()
{
    if (!m_generator)
        return;

    int progress = int(m_generator->getProgress() * 100);
    progressBar->setValue(progress);

    if (progress == 100)
    {
        qDebug() << "Done";
        this->stop_generation();

    }
}


