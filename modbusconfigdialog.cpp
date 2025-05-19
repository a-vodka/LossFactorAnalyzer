#include "modbusconfigdialog.h"
#include "ui_modbusconfigdialog.h"
#include <QSerialPortInfo>
#include <QSettings>

modbusconfigdialog::modbusconfigdialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::modbusconfigdialog)
{
    ui->setupUi(this);

    setWindowTitle("Modbus COM Port Settings");

    populatePortList();

    ui->comboBaudRate->addItems({"9600", "19200", "38400", "57600", "115200"});
    ui->comboDataBits->addItems({"5", "6", "7", "8"});
    ui->comboParity->addItems({"None", "Even", "Odd"});
    ui->comboStopBits->addItems({"1", "1.5", "2"});
    ui->comboFlowControl->addItems({"None", "RTS/CTS", "XON/XOFF"});

    QSettings settings;

    ui->comboBaudRate->setCurrentText(settings.value("modbus/baudRate", "19200").toString());
    ui->comboDataBits->setCurrentText(settings.value("modbus/dataBits", "8").toString());
    ui->comboParity->setCurrentText(settings.value("modbus/parity", "None").toString());
    ui->comboStopBits->setCurrentText(settings.value("modbus/stopBits", "1").toString());
    ui->comboFlowControl->setCurrentText(settings.value("modbus/flowControl", "None").toString());

    ui->lineDevice1->setText(settings.value("modbus/device1", "246").toString());
    ui->lineDevice2->setText(settings.value("modbus/device2", "247").toString());

    QString savedPort = settings.value("modbus/port").toString();
    int index = ui->comboPort->findText(savedPort);
    if (index != -1) {
        ui->comboPort->setCurrentIndex(index);
    }
}

modbusconfigdialog::~modbusconfigdialog()
{
    delete ui;
}

void modbusconfigdialog::populatePortList() {
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        ui->comboPort->addItem(info.portName());
    }
}

QString modbusconfigdialog::port() const {
    return ui->comboPort->currentText();
}

int modbusconfigdialog::baudRate() const {
    return ui->comboBaudRate->currentText().toInt();
}

int modbusconfigdialog::dataBits() const {
    return ui->comboDataBits->currentText().toInt();
}

QString modbusconfigdialog::parity() const {
    return ui->comboParity->currentText();
}

float modbusconfigdialog::stopBits() const {
    return ui->comboStopBits->currentText().toFloat();
}

QString modbusconfigdialog::flowControl() const {
    return ui->comboFlowControl->currentText();
}

int modbusconfigdialog::device1Address() const {
    return ui->lineDevice1->text().toInt();
}

int modbusconfigdialog::device2Address() const {
    return ui->lineDevice2->text().toInt();
}

void modbusconfigdialog::accept()
{
    QSettings settings;

    settings.setValue("modbus/port", port());
    settings.setValue("modbus/baudRate", baudRate());
    settings.setValue("modbus/dataBits", dataBits());
    settings.setValue("modbus/parity", parity());
    settings.setValue("modbus/stopBits", stopBits());
    settings.setValue("modbus/flowControl", flowControl());
    settings.setValue("modbus/device1", device1Address());
    settings.setValue("modbus/device2", device2Address());

    QDialog::accept(); // Call base class
}
