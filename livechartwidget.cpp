#include <QFile>
#include <QTextStream>
#include <QString>

#include "skewed_lorentzian_fit.hpp"
#include "livechartwidget.h"

LiveChartWidget::LiveChartWidget(ModbusReader* reader, QWidget *parent)
    : QWidget(parent), chart(new QChart()),
    chartView(new QChartView(chart)), reader(reader),
    axisX(new QValueAxis()), axisY(new QValueAxis())
{
    chart->legend()->hide();
    chartView->setRenderHint(QPainter::Antialiasing);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    axisX->setTitleText("Frequency [Hz]");
    axisY->setTitleText("Amplitude");

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(chartView);

    axisX->setRange(1, 10);
    axisY->setRange(0, 1);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &LiveChartWidget::updateChart);
    updateTimer->start(500);
}

void LiveChartWidget::setFreqInterval(qreal start_freq, qreal end_freq){
    this->start_freq = start_freq;
    this->end_freq = end_freq;
    axisX->setRange(start_freq, end_freq);
}


void LiveChartWidget::saveVectorsToCSV(const QString& filePath,
                      const std::vector<float>& vec1,
                      const std::vector<float>& vec2,
                      const std::vector<float>& vec3,
                      const std::vector<float>& vec4)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing:" << file.errorString();
        return;
    }

    QTextStream out(&file);

    // Get the maximum size of the vectors
    size_t maxSize = std::max({vec1.size(), vec2.size(), vec3.size(), vec4.size()});

    // Write each row
    for (size_t i = 0; i < maxSize; ++i) {
        QStringList row;
        row << (i < vec1.size() ? QString::number(vec1[i]) : "");
        row << (i < vec2.size() ? QString::number(vec2[i]) : "");
        row << (i < vec3.size() ? QString::number(vec3[i]) : "");
        row << (i < vec4.size() ? QString::number(vec4[i]) : "");
        out << row.join(";") << "\n";
    }

    file.close();
    qDebug() << "Data saved to" << filePath;
}

bool LiveChartWidget::fitData(const std::vector<float>& frequencies,
                              const std::vector<float>& amplitudes,
                              std::vector<float>& fitX,
                              std::vector<float>& fitY)
{
    if (frequencies.size() < 3 || amplitudes.size() < 3) return false;

    // Initial guesses
    float A0 = *std::max_element(amplitudes.begin(), amplitudes.end());
    auto itmax = std::max_element(amplitudes.begin(), amplitudes.end());
    size_t idx_max = std::distance(amplitudes.begin(), itmax);
    float f0 = frequencies[idx_max];
    float eta = 0.05, alpha = 0.0;
    float offset = *std::min_element(amplitudes.begin(), amplitudes.end());

    // Fit curve (first pass)
    fit_skewed_lorentzian_basic(frequencies, amplitudes, A0, f0, eta, alpha, offset);

    // Residual filtering
    std::vector<float> residuals(frequencies.size());
    for (size_t i = 0; i < frequencies.size(); ++i)
        residuals[i] = amplitudes[i] - skewed_lorentzian(frequencies[i], A0, f0, eta, alpha, offset);

    float res_mean = mean(residuals);
    float res_std  = stddev(residuals, res_mean);
    float threshold = 2.0f * res_std;

    std::vector<float> xf, yf;
    for (size_t i = 0; i < frequencies.size(); ++i) {
        if (std::abs(residuals[i]) < threshold) {
            xf.push_back(frequencies[i]);
            yf.push_back(amplitudes[i]);
        }
    }
    if (xf.size() < 3) { xf = frequencies; yf = amplitudes; }

    // Refit on filtered data
    fit_skewed_lorentzian_basic(xf, yf, A0, f0, eta, alpha, offset);

    // Generate dense fitted curve
    fitX = linspace(frequencies.front(), frequencies.back(), 150);
    fitY.resize(fitX.size());
    for (size_t i = 0; i < fitX.size(); ++i)
        fitY[i] = skewed_lorentzian(fitX[i], A0, f0, eta, alpha, offset);

    return true;
}

// Helper to create a line series from two vectors
QXYSeries* LiveChartWidget::createSeries(const QVector<float>& x, const QVector<float>& y,
                          const QString& name, const QPen& pen, bool isLineSeries)
{
    QXYSeries* series;
    if (isLineSeries)
        series = new QLineSeries();
    else
        series = new QScatterSeries();

    series->setName(name);
    series->setPen(pen);
    for (int i = 0; i < x.size() && i < y.size(); ++i) {
        series->append(x[i], y[i]);
    }
    return series;
}

void LiveChartWidget::updateChart() {
    if (!reader) return;

    // Snapshot data
    auto freq = reader->device1Data(FREQ);
    auto amp1 = reader->device1Data(AMP);
    auto amp2 = reader->device2Data(AMP);
    auto amp = divideVectors(amp1, amp2);

    if (freq.size() < 3 || amp.size() < 3) return;

    // Filter by frequency range
    std::vector<float> xf, yf;
    for (size_t i = 0; i < freq.size(); ++i) {
        if (freq[i] >= start_freq && freq[i] <= end_freq) {
            xf.push_back(freq[i]);
            yf.push_back(amp[i]);
        }
    }
    if (xf.size() < 3) return;

    std::vector<float> fitX, fitY;
    bool ok = false;

    if (m_use_approximation) {
        if (!fitData(xf, yf, fitX, fitY)) return;
        ok = calculateHalfPowerBandwidth(fitX, fitY, f1, f2, lossFactor);
        refreshChart(xf, yf, fitX, fitY, ok);
    } else {
        ok = calculateHalfPowerBandwidth(xf, yf, f1, f2, lossFactor);
        refreshChart(xf, yf, {}, {}, ok);
    }

    m_is_success = ok;
}


std::vector<float> LiveChartWidget::divideVectors(const std::vector<float>& a, const std::vector<float>& b) {
    size_t minSize = std::min(a.size(), b.size());
    std::vector<float> result(minSize);

    for (size_t i = 0; i < minSize; ++i) {
        float ai = i < a.size() ? a[i] : 0.0;  // or any default
        float bi = i < b.size() ? b[i] : 1.0;  // avoid zero!

        result[i] = bi != 0 ? ai / bi : 0.0;
    }

    return result;
}

QImage LiveChartWidget::getScreenShot()
{
    // Render chart to pixmap
    QPixmap pixmap(chartView->size());
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);

    chartView->render(&painter);

    painter.end();

    // Convert to QByteArray (PNG)
    QByteArray imageBytes;
    QBuffer buffer(&imageBytes);
    buffer.open(QIODevice::WriteOnly);

    pixmap.toImage().save(&buffer, "PNG");

    // Insert into sheet
    QImage img = QImage::fromData(imageBytes, "PNG");
    return img;

}

void  LiveChartWidget::useApproximation(bool isUse)
{
    m_use_approximation = isUse;
    this->updateChart();
}


// Unified half-power calculation (used by both fit and raw modes)
bool LiveChartWidget::calculateHalfPowerBandwidth(
    const std::vector<float>& freq,
    const std::vector<float>& amp,
    float &f1, float &f2, float &lossFactor)
{
    if (freq.size() < 3 || amp.size() < 3) return false;

    // Find peak
    auto maxIt = std::max_element(amp.begin(), amp.end());
    float peakAmp = *maxIt;
    int idx = std::distance(amp.begin(), maxIt);
    float peakFreq = freq[idx];
    float threshold = peakAmp / std::sqrt(2.0);

    f1 = -1; f2 = -1;

    // Lower crossing
    for (int i = idx; i > 0; --i) {
        if (amp[i] > threshold && amp[i - 1] <= threshold) {
            float t = (threshold - amp[i]) / (amp[i - 1] - amp[i]);
            f1 = freq[i] + t * (freq[i - 1] - freq[i]);
            break;
        }
    }

    // Upper crossing
    for (int i = idx; i < (int)freq.size() - 1; ++i) {
        if (amp[i] > threshold && amp[i + 1] <= threshold) {
            float t = (threshold - amp[i]) / (amp[i + 1] - amp[i]);
            f2 = freq[i] + t * (freq[i + 1] - freq[i]);
            break;
        }
    }

    if (f1 < 0 || f2 < 0) return false;

    lossFactor = (f2 - f1) / peakFreq;
    this->peakFreq = peakFreq;
    this->lossFactor = lossFactor;
    this->f1 = f1;
    this->f2 = f2;
    this->peakAmplitude = peakAmp;
    this->threshold = threshold;
    this->deltaF = f2 - f1;

    return true;
}


// Safe chart refresh: clears series/axes properly
void LiveChartWidget::refreshChart(const std::vector<float>& x,
                                   const std::vector<float>& y,
                                   const std::vector<float>& fitX,
                                   const std::vector<float>& fitY,
                                   const bool status)
{
    chart->removeAllSeries();

    auto addSeries = [&] (QVector<float> x, QVector<float> y, QString name, QPen pen, bool isLineSeries=true)
    {
        auto new_series = createSeries(x, y, name, pen, isLineSeries);
        chart->addSeries(new_series);
        new_series->attachAxis(axisX);
        new_series->attachAxis(axisY);
        return new_series;
    };

    addSeries(QVector<float>(x.begin(), x.end()),
              QVector<float>(y.begin(), y.end()), "Raw", QPen(QColorConstants::Blue, 2), !m_use_approximation)->setMarkerSize(5);

    if (!fitX.empty() && !fitY.empty()) {
        addSeries(QVector<float>(fitX.begin(), fitX.end()),
                  QVector<float>(fitY.begin(), fitY.end()), "Fit", QPen(QColorConstants::Blue, 2));

    }

    float y_min = 0;
    float y_max = ceil( *std::max_element(y.begin(), y.end() ));

    if (status)
    {

        // Half-power line
        QVector<float> hp_x = { this->f1, this->f2 };
        QVector<float> hp_y = { this->threshold,  this->threshold };

        // Vertical markers
        QVector<float> vxf1 = { this->f1, this->f1 };
        QVector<float> vxf2 = { this->f2, this->f2 };
        QVector<float> vxf = { this->peakFreq, this->peakFreq };
        QVector<float> vyf = { y_min, y_max };

        addSeries(hp_x, hp_y, "Threshold", QPen(QColorConstants::Green));
        addSeries(vxf1, vyf, "f1", QPen(QColorConstants::Red));
        addSeries(vxf2, vyf, "f2", QPen(QColorConstants::Red));
        addSeries(vxf, vyf, "f_peak", QPen(QColorConstants::Gray));
    }

    axisX->setRange(start_freq, end_freq);
    axisY->setRange(y_min, y_max);
}
