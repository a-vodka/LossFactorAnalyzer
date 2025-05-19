#include "modbusreader.h"
#include "qvariant.h"
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QSerialPort>
#include <QDebug>

ModbusReader::ModbusReader(QObject *parent) : QObject(parent) {
    modbus = new QModbusRtuSerialClient(this);
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, &ModbusReader::readNextDevice);
}

ModbusReader::~ModbusReader() {
    stop();
}

void ModbusReader::start(const QString &port, int baudRate, int dataBits,
                         const QString &parity, float stopBits,
                         const QString &flowControl,
                         int device1, int device2) {
    if (modbus->state() != QModbusDevice::UnconnectedState)
        modbus->disconnectDevice();

    QSerialPort::Parity p = QSerialPort::NoParity;
    if (parity == "Even") p = QSerialPort::EvenParity;
    else if (parity == "Odd") p = QSerialPort::OddParity;

    QSerialPort::StopBits sb = QSerialPort::OneStop;
    if (stopBits == 1.5f) sb = QSerialPort::OneAndHalfStop;
    else if (stopBits == 2.0f) sb = QSerialPort::TwoStop;

    QSerialPort::FlowControl fc = QSerialPort::NoFlowControl;
    if (flowControl == "RTS/CTS") fc = QSerialPort::HardwareControl;
    else if (flowControl == "XON/XOFF") fc = QSerialPort::SoftwareControl;

    modbus->setConnectionParameter(QModbusDevice::SerialPortNameParameter, port);
    modbus->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, baudRate);
    modbus->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, dataBits);
    modbus->setConnectionParameter(QModbusDevice::SerialParityParameter, p);
    modbus->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, sb);
   // modbus->setConnectionParameter(QModbusDevice::SerialFlowControlParameter, fc);
    Q_UNUSED(fc);

    modbus->setTimeout(250);
    modbus->setNumberOfRetries(1);

    if (!modbus->connectDevice()) {
        emit errorOccurred("Failed to connect to device.");
        return;
    }

    deviceIds = {device1, device2};
    currentDeviceIndex = 0;
    active = true;
    recording = false;
    pollTimer->start(250);
}

void ModbusReader::stop() {
    active = false;
    pollTimer->stop();
    if (modbus && modbus->state() != QModbusDevice::UnconnectedState)
        modbus->disconnectDevice();
}

bool ModbusReader::isWorking() const {
    return active && modbus && modbus->state() == QModbusDevice::ConnectedState;
}

void ModbusReader::startRecording() {
    recording = true;
}

void ModbusReader::stopRecording() {
    recording = false;
}

void ModbusReader::clearData() {
    data1.clear();
    data2.clear();
}

const std::vector<float> &ModbusReader::device1Data() const {
    return data1;
}

const std::vector<float> &ModbusReader::device2Data() const {
    return data2;
}

bool ModbusReader::device1ReadSuccess() const {
    return status1;
}

bool ModbusReader::device2ReadSuccess() const {
    return status2;
}

void ModbusReader::readNextDevice() {
    if (!isWorking()) return;
    int deviceId = deviceIds[currentDeviceIndex];
    QModbusDataUnit req(QModbusDataUnit::InputRegisters, 0x00CE, 2); // float

    if (auto *reply = modbus->sendReadRequest(req, deviceId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [=]() {
                if (reply->error() == QModbusDevice::NoError) {

                    float value = convertToFloat(reply->result());
                    emit dataReady(deviceId, value);

                    if (recording) {
                        if (deviceId == deviceIds[0])
                            data1.push_back(value);
                        else if (deviceId == deviceIds[1])
                            data2.push_back(value);
                    }

                    if (deviceId == deviceIds[0]) status1 = true;
                    else if (deviceId == deviceIds[1]) status2 = true;

                } else {
                    emit errorOccurred(reply->errorString());

                    if (deviceId == deviceIds[0]) status1 = false;
                    else if (deviceId == deviceIds[1]) status2 = false;
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();
        }
    } else {
        qDebug() << "Error";
        emit errorOccurred("Failed to send Modbus request.");
        if (deviceId == deviceIds[0]) status1 = false;
        else if (deviceId == deviceIds[1]) status2 = false;
    }

    currentDeviceIndex = (currentDeviceIndex + 1) % deviceIds.size();
}

float ModbusReader::convertToFloat(const QModbusDataUnit &unit) const {
    if (unit.valueCount() < 2) return 0.0f;

    quint16 high = unit.value(1);
    quint16 low = unit.value(0);
    quint32 raw = (high << 16) | low;
    float result;
    memcpy(&result, &raw, sizeof(float));
    return result;
}
