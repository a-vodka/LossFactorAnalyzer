#ifndef SINESWEEPGENERATOR_H
#define SINESWEEPGENERATOR_H

#pragma once

#include <QIODevice>
#include <QAudioFormat>

class SineSweepGenerator : public QIODevice {
    Q_OBJECT

public:
    SineSweepGenerator(float startFreq, float endFreq, float durationSec,
                       const QAudioFormat &format, QObject *parent = nullptr);

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *, qint64) override { return 0; }
    qint64 bytesAvailable() const override;

private:
    float startFreq;
    float endFreq;
    float duration;
    QAudioFormat format;
    qint64 totalSamples;
    qint64 currentSample = 0;
    double sampleRate;
};

#endif // SINESWEEPGENERATOR_H
