#include "SineSweepGenerator.h"
#include <QtMath>
#include <cstring>

SineSweepGenerator::SineSweepGenerator(float startFreq, float endFreq, float durationSec,
                                       const QAudioFormat &format, QObject *parent)
    : QIODevice(parent),
    startFreq(startFreq), endFreq(endFreq), duration(durationSec),
    format(format)
{
    sampleRate = format.sampleRate();
    totalSamples = duration * sampleRate;
}

void SineSweepGenerator::start() {
    currentSample = 0;
    open(QIODevice::ReadOnly);
}

void SineSweepGenerator::stop() {
    close();
}

qint64 SineSweepGenerator::bytesAvailable() const {
    return (totalSamples - currentSample) * format.bytesPerSample();
}

qint64 SineSweepGenerator::readData(char *data, qint64 maxlen) {
    if (currentSample >= totalSamples)
        return 0;

    int bytesPerSample = format.bytesPerSample();
    int numSamples = bytesPerSample != 0 ? maxlen / bytesPerSample : 0;
    qint64 samplesToWrite = qMin((qint64)numSamples, totalSamples - currentSample);

    for (qint64 i = 0; i < samplesToWrite; ++i, ++currentSample) {
        float t = static_cast<float>(currentSample) / sampleRate;
        float k = (endFreq - startFreq) / duration;
        float freq = startFreq + k * t;
        float value = 0.5f * qSin(2 * M_PI * freq * t);

        qint16 sample = static_cast<qint16>(value * 32767);
        std::memcpy(&data[i * 2], &sample, sizeof(qint16));  // native-endian
    }

    return samplesToWrite * bytesPerSample;
}
