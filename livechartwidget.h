#ifndef LIVECHARTWIDGET_H
#define LIVECHARTWIDGET_H

#include <QWidget>
#include <QtCharts>
#include <QTimer>
#include "modbusreader.h"  // needed to access your vectors

//QT_CHARTS_USE_NAMESPACE

class LiveChartWidget : public QWidget {
    Q_OBJECT

public:
    LiveChartWidget(ModbusReader* reader, QWidget *parent = nullptr);
    QImage getScreenShot();

    double getPeakFreq() {return peakFreq;}
    double getdeltaF() {return deltaF;}
    double getlossFactor() { return lossFactor;}
    bool is_success() {return m_is_success; }
    double getpeakAmplitude() {return peakAmplitude; }
    double getthreshold() { return threshold; }
    double getf1() {return f1;}
    double getf2() {return f2;}

    std::vector<float> getXData()  { return reader ? reader->device1Data(FREQ) : std::vector<float>(0); }
    std::vector<float> getYData1() { return reader ? reader->device1Data(AMP)  : std::vector<float>(0); }
    std::vector<float> getYData2() { return reader ? reader->device2Data(AMP) : std::vector<float>(0); }
    std::vector<float> getYData()  { return divideVectors(getYData1(), getYData2()); }


    void setFreqInterval(qreal start_freq, qreal end_freq);
    void useApproximation(bool isUse);
private slots:
    void updateChart();

private:
    QChart *chart;
    QLineSeries *series;
    QChartView *chartView;
    QTimer *updateTimer;
    ModbusReader* reader;
    void addVerticalLine(QChart* chart, qreal x, qreal minY, qreal maxY, QColor color = Qt::red, int thickness = 3);
    std::vector<float> divideVectors(const std::vector<float>& a, const std::vector<float>& b);
    bool computeLossFactorOberst(const std::vector<float>& xData, const std::vector<float>& yData);
    float peakFreq;
    float deltaF;
    float lossFactor;
    float peakAmplitude;
    float threshold;
    float f1, f2;
    bool m_is_success;
    bool m_use_approximation;
    float start_freq = 0, end_freq = 1000;
    QList<QLineSeries*> verticalLines;

    void saveVectorsToCSV(const QString& filePath,
                                           const std::vector<float>& vec1,
                                           const std::vector<float>& vec2,
                                           const std::vector<float>& vec3,
                                           const std::vector<float>& vec4);

    void removeVerticalLines();

    bool fitData(const std::vector<float>& frequencies,
                 const std::vector<float>& amplitudes,
                 std::vector<float>& fitX,
                 std::vector<float>& fitY);

    QXYSeries* createSeries(const QVector<float>& x, const QVector<float>& y,
                            const QString& name, const QPen& pen = QPen(Qt::SolidLine), bool isLineSeris = true);
    QChartView* createResonanceChart(
        const std::vector<float>& frequencies,
        const std::vector<float>& amplitudes,
        const std::vector<float>& new_freq,
        const std::vector<float>& fitted,
        float half_power,
        int first_idx,
        int last_idx,
        float f_max_amp);

    QValueAxis *axisX;
    QValueAxis *axisY;
    void refreshChart(const std::vector<float>& x,
                      const std::vector<float>& y,
                      const std::vector<float>& fitX = {},
                      const std::vector<float>& fitY = {},
                      const bool status = false);
    bool calculateHalfPowerBandwidth(const std::vector<float>& freq,
                                     const std::vector<float>& amp,
                                     float &f1, float &f2, float &lossFactor);

};

#endif // LIVECHARTWIDGET_H
