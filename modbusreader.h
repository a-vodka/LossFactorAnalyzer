#ifndef MODBUSREADER_H
#define MODBUSREADER_H

#pragma once

#include <QObject>
#include <QModbusRtuSerialClient>
#include <QTimer>
#include <QThread>
#include <vector>

class ModbusReader : public QObject {
    Q_OBJECT

public:
    explicit ModbusReader(QObject *parent = nullptr);
    ~ModbusReader();

    void start(const QString &port, int baudRate, int dataBits, const QString &parity,
               float stopBits, const QString &flowControl, int device1, int device2);

    void stop();
    bool isWorking() const;

    void startRecording();
    void stopRecording();
    void clearData();

    const std::vector<float> &device1Data() const;
    const std::vector<float> &device2Data() const;

    bool device1ReadSuccess() const;
    bool device2ReadSuccess() const;

signals:
    void dataReady(int deviceId, float value);
    void errorOccurred(const QString &error);

private slots:
    void readNextDevice();

private:
    QModbusClient *modbus = nullptr;
    QTimer *pollTimer = nullptr;
    bool active = false;
    bool recording = false;

    int currentDeviceIndex = 0;
    QVector<int> deviceIds;

    std::vector<float> data1;
    std::vector<float> data2;
    bool status1 = false;
    bool status2 = false;

    float convertToFloat(const QModbusDataUnit &unit) const;
};


#endif // MODBUSREADER_H
