#include "generator.h"

#include <QAudioDevice>
#include <QDebug>
#include <QVBoxLayout>
#include <QtMath>

Generator::Generator(const QAudioFormat &format, qint64 durationUs, double startFreq, double endFreq)
    : m_durationUs(durationUs), m_startFreq(startFreq), m_endFreq(endFreq)
{
    QVector<QPair<double, double>> ampTable = {
        {     0.0,  1.0},
        {   100.0,  1.0},
        {   500.0,  1.0},
        {  1000.0,  1.0},
        {  2000.0,  1.0},
        {  4000.0,  1.0},
        { 20000.0,  1.0},
    };
    this->setAmplitudeTable(ampTable);

    if (format.isValid())
        generateData(format);
}

void Generator::start()
{
    open(QIODevice::ReadOnly);
}

void Generator::stop()
{
    m_pos = 0;
    close();
}


void Generator::setAmplitudeTable(const QVector<QPair<double, double>> &table)
{
    m_amplitudeTable = table;
    std::sort(m_amplitudeTable.begin(), m_amplitudeTable.end()); // ensure sorted by frequency
}

double Generator::interpolateAmplitude(double freq) const
{
    if (m_amplitudeTable.isEmpty())
        return 1.0;

    // Clamp
    if (freq <= m_amplitudeTable.first().first)
        return m_amplitudeTable.first().second;
    if (freq >= m_amplitudeTable.last().first)
        return m_amplitudeTable.last().second;

    // Linear interpolation
    for (int i = 1; i < m_amplitudeTable.size(); ++i) {
        if (freq < m_amplitudeTable[i].first) {
            double f1 = m_amplitudeTable[i - 1].first;
            double f2 = m_amplitudeTable[i].first;
            double a1 = m_amplitudeTable[i - 1].second;
            double a2 = m_amplitudeTable[i].second;
            return a1 + (a2 - a1) * (freq - f1) / (f2 - f1);
        }
    }

    return 1.0;
}

void Generator::generateData(const QAudioFormat &format)
{
    const int channelBytes = format.bytesPerSample();
    const int sampleBytes = format.channelCount() * channelBytes;
    qint64 length = format.bytesForDuration(m_durationUs);
    Q_ASSERT(length % sampleBytes == 0);

    int sampleRate = format.sampleRate();
    int totalSamples = length / sampleBytes;
    m_totalSamples = totalSamples;

    m_buffer.resize(length);
    unsigned char *ptr = reinterpret_cast<unsigned char *>(m_buffer.data());

    const double fadeDurationSec = 0.500; // 500 ms for both fade-in and fade-out
    int fadeSamples = static_cast<int>(fadeDurationSec * sampleRate);


    for (int i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double duration = static_cast<double>(m_durationUs) / 1e6;
        double phase = 2 * M_PI * (m_startFreq * t + (m_endFreq - m_startFreq) * t * t / (2 * duration));

        double instFreq = m_startFreq + (m_endFreq - m_startFreq) * t / duration;
        double amplitudeFactor = interpolateAmplitude(instFreq);
        double x = qSin(phase) * amplitudeFactor;

        // Apply fade-in
        double fadeInFactor = (i < fadeSamples) ? static_cast<double>(i) / fadeSamples : 1.0;

        // Apply fade-out
        double fadeOutFactor = (i > totalSamples - fadeSamples) ?
                                   static_cast<double>(totalSamples - i) / fadeSamples : 1.0;

        // Combine both
        double amplitude = qMin(fadeInFactor, fadeOutFactor);
        x *= amplitude;

        for (int c = 0; c < format.channelCount(); ++c) {
            switch (format.sampleFormat()) {
            case QAudioFormat::UInt8:
                *reinterpret_cast<quint8 *>(ptr) = static_cast<quint8>((1.0 + x) / 2 * 255);
                break;
            case QAudioFormat::Int16:
                *reinterpret_cast<qint16 *>(ptr) = static_cast<qint16>(x * 32767);
                break;
            case QAudioFormat::Int32:
                *reinterpret_cast<qint32 *>(ptr) = static_cast<qint32>(x * std::numeric_limits<qint32>::max());
                break;
            case QAudioFormat::Float:
                *reinterpret_cast<float *>(ptr) = x;
                break;
            default:
                break;
            }
            ptr += channelBytes;
        }
    }
}

double Generator::getProgress() const
{
    return m_totalSamples == 0 ? 0.0 : static_cast<double>(m_pos) / m_buffer.size();
}

qint64 Generator::readData(char *data, qint64 len)
{
    qint64 total = 0;
    if (!m_buffer.isEmpty()) {
        while (len - total > 0) {
            const qint64 chunk = qMin((m_buffer.size() - m_pos), len - total);
            memcpy(data + total, m_buffer.constData() + m_pos, chunk);
            m_pos = (m_pos + chunk);
            if (m_pos >= m_buffer.size()) break; // stop at end
            total += chunk;
        }
    }
    return total;
}

qint64 Generator::writeData(const char *, qint64)
{
    return 0;
}

qint64 Generator::bytesAvailable() const
{
    return m_buffer.size() - m_pos + QIODevice::bytesAvailable();
}
