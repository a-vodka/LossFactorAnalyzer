#ifndef MODBUSREADER_H
#define MODBUSREADER_H

#pragma once

#include <QObject>
#include <QModbusRtuSerialClient>
#include <QTimer>
#include <QThread>
#include <vector>

enum params_list { AMP, FREQ, DIST };

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

    // Accessors for last values
    float lastValue(int deviceIndex, int paramIndex) const;

    bool device1ReadSuccess() const;
    bool device2ReadSuccess() const;

    const std::vector<float>& device1Data(int paramIndex) const { return data1_param[paramIndex]; }
    const std::vector<float>& device2Data(int paramIndex) const { return data2_param[paramIndex]; }

    void setSimulationMode(bool enabled);
    void generateFakeData();

signals:
    void dataReady(int deviceId, int paramIndex, float value);
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

    std::vector<float> data1_param[3]; // 3 parameters for device 1
    std::vector<float> data2_param[3]; // 3 parameters for device 2


    bool status1 = false;
    bool status2 = false;

    float convertToFloat(const QModbusDataUnit &unit) const;

    // Latest values for each parameter for each device
    float lastValues[2][3] = {{0.0f}}; // 2 devices x 3 parameters

    bool simulationMode;

    QElapsedTimer simTimer;

 };


#endif // MODBUSREADER_H
