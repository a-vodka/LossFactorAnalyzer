#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLineSeries>
#include <aboutdialog.h>
#include "modbusconfigdialog.h"
#include "modbusreader.h"
#include <QTimer>
#include "livechartwidget.h"

// for export into xslx file
#include "xlsxdocument.h"
#include "xlsxworkbook.h"
using namespace QXlsx;

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

    reader = new ModbusReader;

    reader->setSimulationMode(false);
    reader->start(dlg->port(), dlg->baudRate(), dlg->dataBits(), dlg->parity(),
                  dlg->stopBits(), dlg->flowControl(), dlg->device1Address(), dlg->device2Address(), dlg->generatorAddress());

    connect(reader, &ModbusReader::dataReady, this, [](int deviceId, int paramIndex, float value) {

        if (paramIndex == FREQ || paramIndex == AMP)
        qDebug() << "Device" << deviceId << "paramIndex:"<< paramIndex << "Value:" << value;

    });

    connect(reader, &ModbusReader::errorOccurred, this, [](const QString &err) {
        qWarning() << "Modbus error:" << err;
    });

    QTimer* statusTimer = new QTimer(this);
    statusTimer->start(1000);
    connect(statusTimer, &QTimer::timeout, this, [this, status1, status2](){
        bool ok1 = this->reader->device1ReadSuccess();
        bool ok2 = this->reader->device2ReadSuccess();

        float amp1 =  this->reader->lastValue(0, AMP),
              freq1 = this->reader->lastValue(0, FREQ),
              dist1 = this->reader->lastValue(0, DIST);

        float amp2 =  this->reader->lastValue(1, AMP),
              freq2 = this->reader->lastValue(1, FREQ),
              dist2 = this->reader->lastValue(1, DIST);


        LedIndicator::State l1 = ok1 ? LedIndicator::Green : LedIndicator::Red;
        LedIndicator::State l2 = ok2 ? LedIndicator::Green : LedIndicator::Red;

        bool y_cond1 = amp1 == 0 && freq1 == 0 && dist1 == 0 && ok1;
        bool y_cond2 = amp2 == 0 && freq2 == 0 && dist2 == 0 && ok2;

        l1 = y_cond1 ? LedIndicator::Yellow : l1;
        l2 = y_cond2 ? LedIndicator::Yellow : l2;

        sensor1Indicator->setState(l1);
        sensor2Indicator->setState(l2);

        QString frmt = "Amp = %1 | Freq = %2 | Dist = %3";
        QString s1 = QString("Sens 1: " + frmt).arg(amp1 / 1e3, 4, 'f' , 2).arg(freq1, 3, 'f', 1).arg(dist1 / 1e3, 3, 'f' , 1);
        QString s2 = QString("Sens 2: " + frmt).arg(amp2 / 1e3, 4, 'f' , 2).arg(freq2, 3, 'f', 1).arg(dist2 / 1e3, 3, 'f' , 1);
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
    this->chart = new LiveChartWidget(reader, this);

    // Check if group box already has a layout
    if (!ui->graph_box->layout()) {
        ui->graph_box->setLayout(new QVBoxLayout());
    }
    ui->graph_box->layout()->addWidget(chart);

    // load last experiment parameters
    QSettings settings;
    ui->start_freq->setText(settings.value("startFreq", 15).toString());
    ui->end_freq->setText(settings.value("endFreq", 35).toString());
    ui->duration->setText(settings.value("duration", 10).toString());


    auto setBackgroundToFormColor = [](QLineEdit* lineEdit) {
        QPalette palette = lineEdit->palette();
        palette.setColor(QPalette::Base, lineEdit->parentWidget()->palette().color(QPalette::Window));
        lineEdit->setPalette(palette);
    };

    setBackgroundToFormColor(ui->loss_factor);
    setBackgroundToFormColor(ui->peak_width);
    setBackgroundToFormColor(ui->rs_freq);
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

void MainWindow::on_start_btn_clicked()
{
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

    ui->start_btn->setText("Start");
    reader->stopSweep();
    reader->stopRecording();

}

void MainWindow::start_generation()
{
    is_generator_works = true;
    float startFreq = ui->start_freq->text().toFloat();   // Hz
    float endFreq   = ui->end_freq->text().toFloat();     // Hz
    float duration  = ui->duration->text().toFloat();     // seconds

    this->chart->setFreqInterval(startFreq, endFreq);

    QSettings settings;

    settings.setValue("startFreq", startFreq);
    settings.setValue("endFreq", endFreq);
    settings.setValue("duration", duration);

    m_progressTimer->start(250);

    ui->start_btn->setText("Stop");

    reader->clearData();
    float sweep_speed = fabs(endFreq - startFreq) / (duration / 60.0f);
    int generatorVolume = settings.value("modbus/generatorVolume").toInt();
    reader->startSweep(
        generatorVolume,                      // Amplitude %
        startFreq,                            // Start frequency Hz
        endFreq,                              // End frequency Hz
        sweep_speed,                          // Sweep speed Hz/min
        1,                                    // Cycles
        ModbusReader::SweepFminToFmax         // Direction
        );

    reader->startRecording();
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionAudio_Settings_triggered()
{

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
        qDebug() << "Device 1 Address:" << dlg.device1Address();
        qDebug() << "Device 2 Address:" << dlg.device2Address();
        qDebug() << "Generator Address:" << dlg.generatorAddress();
        qDebug() << "Generator Volume:" << dlg.generatorVolume();
    }
}

void MainWindow::updateProgressBar()
{
    if (this->chart->is_success())
    {
        float peak_freq = chart->getPeakFreq();
        float peak_width = chart->getdeltaF();
        float loss_factor = chart->getlossFactor();

        ui->peak_width->setText(QString::number(peak_width));
        ui->rs_freq->setText(QString::number(peak_freq));
        ui->loss_factor->setText(QString::number(loss_factor));

    }
    else
    {
        ui->peak_width->setText("");
        ui->rs_freq->setText("");
        ui->loss_factor->setText("");
    }


    int progress = this->reader->getProgress();
    progressBar->setValue(progress);

    if (progress == 100)
    {
        qDebug() << "Done";
        is_generator_works = false;
        ui->start_btn->setText("Start");
        m_progressTimer->stop();
        //this->stop_generation();
    }
}


void MainWindow::on_export_btn_clicked()
{
    if (!this->chart->is_success())
        return;

    QSettings settings;

    // Load last directory or use home if not set
    QString lastDir = settings.value("lastExportDir", QDir::homePath()).toString();

    // Suggest filename with current date
    QString defaultFileName = "Report_" + QDate::currentDate().toString("yyyyMMdd") + ".xlsx";

    // Open dialog with suggested filename and last directory
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Save Excel",
        QDir(lastDir).filePath(defaultFileName),
        "Excel Files (*.xlsx)"
        );

    if (!savePath.isEmpty())
    {
        // Save the directory for next time
        QFileInfo fi(savePath);
        settings.setValue("lastExportDir", fi.absolutePath());

        // Load the template
        Document xlsx(":/report_template.xlsx");
        auto *wb = xlsx.workbook();
        auto *sheet = wb->sheet(0);
        if (!sheet) {
            QMessageBox::warning(this, "Export Failed", "Sheet 'Results' not found in template.");
            return;
        }

        // Fill named cells
        xlsx.write("B6", chart->getPeakFreq());
        xlsx.write("B7", chart->getpeakAmplitude());
        xlsx.write("B8", chart->getthreshold());
        xlsx.write("B9", chart->getf1());
        xlsx.write("B10", chart->getf2());
        xlsx.write("B11", chart->getdeltaF());
        xlsx.write("B12", chart->getlossFactor());

        QImage img = chart->getScreenShot();

        xlsx.insertImage(14, 0, img);

        xlsx.selectSheet("Raw data");

        auto write_data = [&xlsx](std::vector<float> data, QString s) {

             for (size_t i = 0; i < data.size(); i++)
             {
                 QString addr = s + QString::number(i + 2);
                 xlsx.write(addr, data[i]);
             }

        };

        write_data(chart->getXData(), "A");
        write_data(chart->getYData1(), "B");
        write_data(chart->getYData2(), "C");
        write_data(chart->getYData(), "D");

        xlsx.selectSheet("Report");

        // Save to new file
        if (!xlsx.saveAs(savePath)) {
            QMessageBox::warning(this, "Export Failed", "Failed to save file.");
            return;
        }

        // Open the file with the default Excel application
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    }

}


void MainWindow::on_approximation_check_box_checkStateChanged(const Qt::CheckState &arg1)
{
    if (this->chart)
        this->chart->useApproximation(arg1 == Qt::CheckState::Checked);
}

