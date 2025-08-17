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
                         int device1, int device2, int generatorId) {

    qDebug() <<"Device addr 1:" <<device1;
    qDebug() <<"Device addr 2:" <<device2;
    qDebug() << "Generator Address:" << generatorId;

    deviceIds = {device1, device2, generatorId};

    // Example: if simulation is enabled, skip Modbus init
    if (simulationMode) {
        qDebug() << "Starting in simulation mode.";

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

    modbus->setTimeout(300);
    modbus->setNumberOfRetries(1);

    if (!modbus->connectDevice()) {
        emit errorOccurred("Failed to connect to device.");
        return;
    }


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

    // Step 1: Read Sensor 1
    readSensorData(deviceIds[0], 0);

    // Step 2: Read Sensor 2
    readSensorData(deviceIds[1], 1);

    // Step 3: Read Generator
    readGeneratorData(deviceIds[2]);

/*
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
*/
}

void ModbusReader::readSensorData(int deviceId, int devIdx)
{
    // Read vibration + gap in one request (2 floats = 4 registers)
    QModbusDataUnit req(QModbusDataUnit::InputRegisters,
                        RegSensorFlags,
                        8);

    if (auto *reply = modbus->sendReadRequest(req, deviceId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [=]() {
                if (reply->error() == QModbusDevice::NoError) {
                    quint32 flags = convertToUint32(reply->result());
                    float vibration = convertToFloat(reply->result(), 2);               // first 2 registers
                    float gap = convertToFloat(reply->result(), 4);                  // next 2 registers

                    if (devIdx == 1)
                    {
                        m_ready_to_record = flags & 0x0008;
                        m_generation_finished = flags & 0x0010;
                        qDebug() << "flags:" << flags;
                        qDebug() << "m_ready_to_record:" << m_ready_to_record
                                 << "m_generation_finished:" << m_generation_finished;
                    }

                    lastValues[devIdx][AMP] = vibration;
                    lastValues[devIdx][DIST] = gap;

                    if (recording && m_ready_to_record) {
                        if (devIdx == 0) {
                            data1_param[AMP].push_back(vibration);
                            data1_param[DIST].push_back(gap);
                        } else {
                            data2_param[AMP].push_back(vibration);
                            data2_param[DIST].push_back(gap);
                        }
                    }

                    emit dataReady(deviceId, AMP, vibration);
                    emit dataReady(deviceId, DIST, gap);

                    if (devIdx == 0) status1 = true;
                    else status2 = true;
                } else {
                    emit errorOccurred(reply->errorString());
                    if (devIdx == 0) status1 = false;
                    else status2 = false;
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();
        }
    } else {
        emit errorOccurred("Failed to send Modbus request (Sensor).");
        if (devIdx == 0) status1 = false;
        else status2 = false;
    }
}

void ModbusReader::readGeneratorData(int generatorId)
{
    // Read cycles (uint32) + current frequency (float32) in one request = 4 registers
    QModbusDataUnit req(QModbusDataUnit::InputRegisters,
                        RegCycles,
                        4);

    if (auto *reply = modbus->sendReadRequest(req, generatorId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, [=]() {
                if (reply->error() == QModbusDevice::NoError) {
                    quint32 cycles = convertToUint32(reply->result());        // first 2 registers
                    float curFreq   = convertToFloat(reply->result(), 2);     // next 2 registers

                    // Here you can store or emit these values as needed
                    qDebug() << "Generator cycles:" << cycles
                             << "Current freq:" << curFreq;

                    lastValues[0][FREQ] = curFreq;
                    lastValues[1][FREQ] = curFreq;

                    if (recording && m_ready_to_record) {
                            data1_param[FREQ].push_back(curFreq);
                            data2_param[FREQ].push_back(curFreq);
                    }


                } else {
                    emit errorOccurred(reply->errorString());
                }
                reply->deleteLater();
            });
        } else {
            reply->deleteLater();
        }
    } else {
        emit errorOccurred("Failed to send Modbus request (Generator).");
    }
}



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
    for (int devIdx = 0; devIdx < 2; ++devIdx) {
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

float ModbusReader::convertToFloat(const QModbusDataUnit &unit) const {
    if (unit.valueCount() < 2) return -1.0f;

    quint16 high = unit.value(1);
    quint16 low = unit.value(0);
    quint32 raw = (high << 16) | low;
    float result;
    memcpy(&result, &raw, sizeof(float));
    return result;
}


void ModbusReader::startSweep(float amplitudePercent,
                              float startFreq,
                              float endFreq,
                              float sweepSpeedHzMin,
                              quint32 cycles,
                              SweepDirection direction)
{

    m_start_freq = startFreq;
    m_end_freq = endFreq;

    int generatorId = 0;
    if (deviceIds.size() == 3)
        generatorId = deviceIds[2];
    else
        return;

    // 1. Stop generator first
    writeHoldingRegisters(generatorId, RegMode, uint32ToRegisters(ModeStop));

    // 2. Set amplitude
    writeHoldingRegisters(generatorId, RegAmplitudePercent, floatToRegisters(amplitudePercent));

    // 3. Set start and end frequencies
    writeHoldingRegisters(generatorId, RegStartFreq, floatToRegisters(startFreq));
    writeHoldingRegisters(generatorId, RegEndFreq, floatToRegisters(endFreq));

    // 4. Set sweep speed (Hz/min)
    writeHoldingRegisters(generatorId, RegSweepSpeed, floatToRegisters(sweepSpeedHzMin));

    // 5. Set number of cycles
    writeHoldingRegisters(generatorId, RegCycles, uint32ToRegisters(cycles));

    // 6. Set sweep direction
    writeHoldingRegisters(generatorId, RegDirection, uint32ToRegisters(direction));

    // 7. Start sweep mode (Mode 3 = Hz/min)
    writeHoldingRegisters(generatorId, RegMode, uint32ToRegisters(ModeSweepHzPerMin));
}

void ModbusReader::stopSweep()
{
    int generatorId = 0;
    if (deviceIds.size() == 3)
        generatorId = deviceIds[2]; // get Generator ID
    else
        return;

    // Stop generator
    writeHoldingRegisters(generatorId, RegMode, uint32ToRegisters(ModeStop));
}


void ModbusReader::writeHoldingRegisters(int deviceId, quint16 startAddress, const QVector<quint16> &values) {
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, values.size());

    for (int i = 0; i < values.size(); ++i)
        writeUnit.setValue(i, values[i]);

    if (auto *reply = modbus->sendWriteRequest(writeUnit, deviceId)) {
        connect(reply, &QModbusReply::finished, this, [reply]() {
            if (reply->error() != QModbusDevice::NoError) {
                qWarning() << "Write error:" << reply->errorString();
            }
            reply->deleteLater();
        });
    } else {
        qWarning() << "Failed to send Modbus write request.";
    }
}



float ModbusReader::lastValue(int deviceIndex, int paramIndex) const {
    if (deviceIndex < 0 || deviceIndex > 1 || paramIndex < 0 || paramIndex > 2)
        return 0.0f;
    return lastValues[deviceIndex][paramIndex];
}


QVector<quint16> ModbusReader::floatToRegisters(float value) {
    quint32 raw;
    memcpy(&raw, &value, sizeof(float));
    quint16 low = raw & 0xFFFF;
    quint16 high = (raw >> 16) & 0xFFFF;
    return {low, high};
}

QVector<quint16> ModbusReader::uint32ToRegisters(quint32 value) {
    quint16 low = value & 0xFFFF;
    quint16 high = (value >> 16) & 0xFFFF;
    return {low, high};
}

// Overload convertToFloat for offset reading
float ModbusReader::convertToFloat(const QModbusDataUnit &unit, int offset) const
{
    if (unit.valueCount() < offset + 2) return -1.0f;

    quint16 high = unit.value(offset + 1);
    quint16 low = unit.value(offset);
    quint32 raw = (high << 16) | low;
    float result;
    memcpy(&result, &raw, sizeof(float));
    return result;
}

quint32 ModbusReader::convertToUint32(const QModbusDataUnit &unit, int offset) const
{
    if (unit.valueCount() < offset + 2) return 0;
    quint16 high = unit.value(offset + 1);
    quint16 low  = unit.value(offset);
    return (high << 16) | low;
}

quint32 ModbusReader::convertToUint32(const QModbusDataUnit &unit) const
{
    return convertToUint32(unit, 0);
}


int ModbusReader::getProgress()
{
    if (m_generation_finished)
        return 100.0;

    //if (fabs(m_end_freq - m_start_freq) < 1)
    //    return 0;

    float x = this->lastValues[0][FREQ];
    if ( x < m_start_freq || x > m_end_freq )
        return 0;

    int res = 100.0 / (m_end_freq - m_start_freq) * (x - m_start_freq);
    return res;
}
