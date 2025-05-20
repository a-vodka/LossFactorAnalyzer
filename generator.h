// generator.h
#ifndef GENERATOR_H
#define GENERATOR_H

#include <QIODevice>
#include <QAudioFormat>
#include <QByteArray>

class Generator : public QIODevice
{
    Q_OBJECT

public:
    Generator(const QAudioFormat &format, qint64 durationUs, double startFreq, double endFreq);

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;
    qint64 size() const override { return m_buffer.size(); }

    double getProgress() const;

private:
    void generateData(const QAudioFormat &format);

    qint64 m_pos = 0;
    QByteArray m_buffer;

    qint64 m_durationUs = 0;
    double m_startFreq = 0.0;
    double m_endFreq = 0.0;
    int m_totalSamples = 0;
};

#endif // GENERATOR_H
