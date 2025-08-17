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


    enum GeneratorRegister : quint16 {
        RegFrequencyHz        = 0x636C,
        RegAmplitudePercent   = 0x6370,
        RegMode               = 0x6374,
        RegStartFreq          = 0x6378,
        RegEndFreq            = 0x637C,
        RegSweepSpeed         = 0x6380,
        RegPulseDurationMs    = 0x6384,
        RegPauseDurationMs    = 0x6388,
        RegCycles             = 0x638C,
        RegDirection          = 0x63AC,
        RegReadCurFrequency   = 0x6390 // Read-only current frequency actual for sweep mode
    };

    enum GeneratorMode : quint32 {
        ModeStop             = 0,
        ModeSine             = 2,
        ModeSweepHzPerMin    = 4,
        ModeSweepOctPerMin   = 8,
        ModePulse            = 16,
        NotUsed              = 32,
        ModeFixedSine        = 64,
        ModeNoise            = 128

    };

    enum SweepDirection : quint32 {
        SweepFminToFmax      = 0,
        SweepFminFmaxFmin    = 1
    };

    enum SensorRegister : quint16 {
        RegSensorFlags = 0x0248, // uint16
        RegSensorVibration = 0x024C, // float32 588
        RegSensorGap       = 0x0250  // float32
    };

    void startSweep(float amplitudePercent,
                    float startFreq,
                    float endFreq,
                    float sweepSpeedHzMin,
                    quint32 cycles,
                    SweepDirection direction);
    void stopSweep();

    void start(const QString &port, int baudRate, int dataBits, const QString &parity,
               float stopBits, const QString &flowControl, int device1, int device2, int generatorId);

    void stop();
    bool isWorking() const;

    void startRecording();
    void stopRecording();
    void clearData();

    // Accessors for last values
    float lastValue(int deviceIndex, int paramIndex) const;

    bool device1ReadSuccess() const;
    bool device2ReadSuccess() const;

    std::vector<float> device1Data(int paramIndex) const { return data1_param[paramIndex]; }
    std::vector<float> device2Data(int paramIndex) const { return data2_param[paramIndex]; }

    void setSimulationMode(bool enabled);
    void generateFakeData();

    int getProgress();

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

    bool m_ready_to_record = false;
    bool m_generation_finished = false;

    void readGeneratorData(int generatorId);
    void readSensorData(int deviceId, int devIdx);

    float convertToFloat(const QModbusDataUnit &unit) const;
    float convertToFloat(const QModbusDataUnit &unit, int offset) const;
    quint32 convertToUint32(const QModbusDataUnit &unit, int offset) const;
    quint32 convertToUint32(const QModbusDataUnit &unit) const;
    QVector<quint16> floatToRegisters(float value);

    QVector<quint16> uint32ToRegisters(quint32 value);

    void writeHoldingRegisters(int deviceId, quint16 startAddress, const QVector<quint16> &values);
    // Latest values for each parameter for each device
    float lastValues[2][3] = {{0.0f}}; // 2 devices x 3 parameters

    bool simulationMode;

    QElapsedTimer simTimer;

    float m_start_freq, m_end_freq;


 };


#endif // MODBUSREADER_H
