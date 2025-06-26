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

    qDebug() <<"Device addr 1:" <<device1;
    qDebug() <<"Device addr 2:" <<device2;

    // Example: if simulation is enabled, skip Modbus init
    if (simulationMode) {
        qDebug() << "Starting in simulation mode.";
        deviceIds = {device1, device2};
        currentDeviceIndex = 0;
        active = true;
        recording = false;
        simTimer.start(); // Start fake clock
        pollTimer->start(200);
        return;
    }

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
    pollTimer->start(300);
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
    simTimer.restart();

}

void ModbusReader::stopRecording() {
    recording = false;
}

void ModbusReader::clearData() {
    for (int i = 0; i < 3; ++i) {
        data1_param[i].clear();
        data2_param[i].clear();
    }
}


bool ModbusReader::device1ReadSuccess() const {
    return status1;
}

bool ModbusReader::device2ReadSuccess() const {
    return status2;
}


void ModbusReader::readNextDevice() {
    if (simulationMode) {
        generateFakeData();
        return;
    }

    if (!isWorking()) return;

    static int currentParamIndex;
    // 0x00CE
    // 210 230 202
    QVector<quint16> baseAddressesRecording  = { 0x00D2, 0x00E6 };  // e.g. just main amplitude and freq
    QVector<quint16> baseAddressesIdle = { 0x00D2, 0x00E6, 0x00CA }; // full set

    int deviceId = deviceIds[currentDeviceIndex];
    const QVector<quint16>& activeAddresses = recording ? baseAddressesRecording : baseAddressesIdle;

    if (currentParamIndex >= activeAddresses.size()) {
        currentParamIndex = 0;
        currentDeviceIndex = (currentDeviceIndex + 1) % deviceIds.size();
        return;
    }

    quint16 address = activeAddresses[currentParamIndex];
    QModbusDataUnit req(QModbusDataUnit::InputRegisters, address, 2); // float = 2 registers

    if (auto *reply = modbus->sendReadRequest(req, deviceId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [=]() {
                if (reply->error() == QModbusDevice::NoError) {
                    float value = convertToFloat(reply->result());

                    int devIdx = (deviceId == deviceIds[0]) ? 0 : 1;
                    lastValues[devIdx][currentParamIndex] = value;


                    if (recording) {
                        if (devIdx == 0) data1_param[currentParamIndex].push_back(value);
                        if (devIdx == 1) data2_param[currentParamIndex].push_back(value);

                        // emit dataReady(deviceId, currentParamIndex, value);
                    }

                    if (devIdx == 0) status1 = true;
                    else if (devIdx == 1) status2 = true;
                } else {
                    emit errorOccurred(reply->errorString());
                    if (deviceId == deviceIds[0]) status1 = false;
                    else if (deviceId == deviceIds[1]) status2 = false;
                }

                reply->deleteLater();
                currentParamIndex++;
            });
        } else {
            reply->deleteLater();
            currentParamIndex++;
        }
    } else {
        emit errorOccurred("Failed to send Modbus request.");
        if (deviceId == deviceIds[0]) status1 = false;
        else if (deviceId == deviceIds[1]) status2 = false;

        currentParamIndex++;
    }
}



/* old version
 *
 *
void ModbusReader::readNextDevice(){

    if (simulationMode) {
        generateFakeData();
        return;
    }

    if (!isWorking()) return;

    int deviceId = deviceIds[currentDeviceIndex];
    QVector<quint16> baseAddresses = {0x00CE, 0x00E6, 0x00CA}; // Adjust these if needed

    for (int i = 0; i < 3; ++i) {
        QModbusDataUnit req(QModbusDataUnit::InputRegisters, baseAddresses[i], 2); // 2 registers for float

        if (auto *reply = modbus->sendReadRequest(req, deviceId)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, [=]() {
                    if (reply->error() == QModbusDevice::NoError) {
                        float value = convertToFloat(reply->result());

                        // Store last value
                        int devIdx = (deviceId == deviceIds[0]) ? 0 : 1;
                        lastValues[devIdx][i] = value;

                        // Emit signal
                        emit dataReady(deviceId, i, value);

                        if (recording) {
                            if (devIdx == 0) data1_param[i].push_back(value);
                            else if (devIdx == 1) data2_param[i].push_back(value);
                        }

                        if (devIdx == 0) status1 = true;
                        else if (devIdx == 1) status2 = true;

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
            qDebug() << "Failed to send Modbus request.";
            emit errorOccurred("Failed to send Modbus request.");
            if (deviceId == deviceIds[0]) status1 = false;
            else if (deviceId == deviceIds[1]) status2 = false;
        }
        // QTest::qWait(250);
    }

    currentDeviceIndex = (currentDeviceIndex + 1) % deviceIds.size();
}

*/

void ModbusReader::setSimulationMode(bool enabled) {
    simulationMode = enabled;
}


void ModbusReader::generateFakeData() {
    qint64 ms = simTimer.elapsed();
    float t = ms / 1000.0f; // seconds

    float omega = (t + 7.0)* 2.0 * M_PI;
    float zeta = 0.15f;    // damping ratio
    float omega_n = 2.0f * M_PI * 17.0f; // natural frequency ~63Hz
    float betta = omega / omega_n;

    float x = 1.0 / std::sqrt(std::pow((1 - betta * betta), 2) + std::pow(2 * zeta * betta, 2));
    float data[2][3] = {
                         {x*1e3f , float(omega / 2.0 / M_PI), 2.9e3f},
                         {1e3f , float(omega / 2.0 / M_PI), 2.8e3f}
                        };
    for (int devIdx = 0; devIdx < deviceIds.size(); ++devIdx) {
        for (int i = 0; i < 3; ++i) {

            float value = data[devIdx][i];
            lastValues[devIdx][i] = value;
            emit dataReady(deviceIds[devIdx], i, value);


            if (recording) {
                if (devIdx == 0) data1_param[i].push_back(value);
                else if (devIdx == 1) data2_param[i].push_back(value);
            }

            if (devIdx == 0) status1 = true;
            else if (devIdx == 1) status2 = true;
        }
    }

    currentDeviceIndex = (currentDeviceIndex + 1) % deviceIds.size();
}

/*
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
*/
float ModbusReader::convertToFloat(const QModbusDataUnit &unit) const {
    if (unit.valueCount() < 2) return -1.0f;

    quint16 high = unit.value(1);
    quint16 low = unit.value(0);
    quint32 raw = (high << 16) | low;
    float result;
    memcpy(&result, &raw, sizeof(float));
    return result;
}


float ModbusReader::lastValue(int deviceIndex, int paramIndex) const {
    if (deviceIndex < 0 || deviceIndex > 1 || paramIndex < 0 || paramIndex > 2)
        return 0.0f;
    return lastValues[deviceIndex][paramIndex];
}
