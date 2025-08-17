#include <QFile>
#include <QTextStream>
#include <QString>

#include "skewed_lorentzian_fit.hpp"
#include "livechartwidget.h"

LiveChartWidget::LiveChartWidget(ModbusReader* reader, QWidget *parent)
    : QWidget(parent), reader(reader)
{
    //series = new QScatterSeries();
    //series->setMarkerSize(8);
    series = new QLineSeries();

    chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();

    chart->axes(Qt::Horizontal).first()->setTitleText("Frequency, Hz");
    chart->axes(Qt::Vertical).first()->setTitleText("Amplitude Ratio");

    // Disable the legend
    chart->legend()->hide();

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(chartView);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &LiveChartWidget::updateChart);
    updateTimer->start(500);  // Update every 500 ms
}

void LiveChartWidget::setFreqInterval(qreal start_freq, qreal end_freq){
    this->start_freq = start_freq;
    this->end_freq = end_freq;
    chart->axes(Qt::Horizontal).first()->setRange(start_freq, end_freq);
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


void LiveChartWidget::fitData(std::vector<float> frequencies, std::vector<float> amplitudes)
{
    if (frequencies.empty()) {
        // synthetic example (replace with your actual data load)
        frequencies = linspace(7.0, 30.0, 40);
        for (size_t i = 0; i < frequencies.size(); ++i)
            amplitudes.push_back(skewed_lorentzian(frequencies[i], 1.0, 20.0, 0.11, 0.02, 0.0));
    }

    // --- Initial guesses (like Python)
    float A0 = *std::max_element(amplitudes.begin(), amplitudes.end());
    auto itmax = std::max_element(amplitudes.begin(), amplitudes.end());
    size_t idx_max = std::distance(amplitudes.begin(), itmax);
    float f0 = frequencies[idx_max];
    float eta = 0.05;
    float alpha = 0.0;
    float offset = *std::min_element(amplitudes.begin(), amplitudes.end());

    // --- First fit (all data)
    fit_skewed_lorentzian_basic(frequencies, amplitudes, A0, f0, eta, alpha, offset);

    // --- Compute residuals and filter outliers (|res| < 2*std)
    std::vector<float> residuals(frequencies.size());
    for (size_t i = 0; i < frequencies.size(); ++i)
        residuals[i] = amplitudes[i] - skewed_lorentzian(frequencies[i], A0, f0, eta, alpha, offset);

    float res_mean = mean(residuals);
    float res_std  = stddev(residuals, res_mean);
    float threshold = 2.0 * res_std;

    std::vector<float> xf, yf;
    for (size_t i = 0; i < frequencies.size(); ++i) {
        if (std::abs(residuals[i]) < threshold) {
            xf.push_back(frequencies[i]);
            yf.push_back(amplitudes[i]);
        }
    }

    // If filtering removed everything (edge case), keep all
    if (xf.size() < 3) { xf = frequencies; yf = amplitudes; }

    // --- Refit on filtered data with previous params as initial guess
    fit_skewed_lorentzian_basic(xf, yf, A0, f0, eta, alpha, offset);

    // --- Goodness of fit (R^2) computed on filtered points
    float ss_res = 0.0, ss_tot = 0.0;
    float mean_y = mean(yf);
    for (size_t i = 0; i < xf.size(); ++i) {
        float yfitted = skewed_lorentzian(xf[i], A0, f0, eta, alpha, offset);
        float diff = yf[i] - yfitted;
        ss_res += diff*diff;
        float dt = yf[i] - mean_y;
        ss_tot += dt*dt;
    }
    float r_squared = (ss_tot > 0.0) ? 1.0 - ss_res/ss_tot : 1.0;

    // --- Make dense fitted curve and compute half-power loss factor (Oberst)
    auto new_freq = linspace(frequencies.front(), frequencies.back(), 150);
    std::vector<float> fitted(new_freq.size());
    for (size_t i = 0; i < new_freq.size(); ++i)
        fitted[i] = skewed_lorentzian(new_freq[i], A0, f0, eta, alpha, offset);

    float A_peak = *std::max_element(fitted.begin(), fitted.end());
    float half_power = A_peak / std::sqrt(2.0);

    // find contiguous region where fitted >= half_power
    size_t first_idx = 0, last_idx = fitted.size() - 1;
    for (size_t i = 0; i < fitted.size(); ++i) {
        if (fitted[i] >= half_power) { first_idx = i; break; }
    }
    for (size_t i = fitted.size(); i-- > 0;) {
        if (fitted[i] >= half_power) { last_idx = i; break; }
    }
    float delta_f = new_freq[last_idx] - new_freq[first_idx];
    float eta_half_power = delta_f / f0;
    float f_max_amp = new_freq[std::distance(fitted.begin(), std::max_element(fitted.begin(), fitted.end()))];

    // --- Print results
    qDebug() << "Fitted params (A0, f0, eta, alpha, offset):\n";
    qDebug() << A0 << ", " << f0 << ", " << eta << ", " << alpha << ", " << offset << "\n";
    qDebug() << "R^2 (on filtered data): " << r_squared << "\n";
    qDebug() << "Loss factor from fit: eta = " << eta << "\n";
    qDebug() << "Loss factor by half-power: eta ≈ " << eta_half_power << "\n";

}

void LiveChartWidget::updateChart() {
    const auto xData = reader->device1Data(FREQ);
    const auto yD1 = reader->device1Data(AMP);
    const auto yD2 = reader->device2Data(AMP);
    const auto yData = divideVectors(yD1, yD2);

    removeVerticalLines();


    const int trimCount = 2;
    int count = std::min(xData.size(), yData.size());
    if (count < trimCount + 2)
        return;  // Not enough data


    //saveVectorsToCSV("output.csv", xData, yD1, yD2, yData); // method for debug only

    // qDebug() << xData.size() << yData.size() << " << xData size";

    auto xTrimmed = std::vector<float>(xData.begin() + trimCount, xData.begin() + (count - 0 * trimCount) );
    auto yTrimmed = std::vector<float>(yData.begin() + trimCount, yData.begin() + (count - 0 * trimCount) );

    //qDebug() << xTrimmed.size() << yTrimmed.size() << " << xTrimmed size";

    std::vector<float> xFiltered, yFiltered;
    for (size_t i = 0; i < xTrimmed.size(); ++i) {
        if (xTrimmed[i] >= start_freq && xTrimmed[i] <= end_freq) {
            xFiltered.push_back(xTrimmed[i]);
            yFiltered.push_back(yTrimmed[i]);
        }
    }
    // qDebug() << xFiltered.size() << yFiltered.size() << " << xFiltered size";
    series->clear();

    for (size_t i = 0; i < xFiltered.size(); ++i)
    {
        series->append(xFiltered[i], yFiltered[i]);
    }

    if (xFiltered.size() < 3)
        return; // Not enough data


    qreal min_y = *std::min_element(yFiltered.begin(), yFiltered.end()),
          max_y = *std::max_element(yFiltered.begin(), yFiltered.end());

    //qDebug() << min_y << max_y;

    min_y = std::floor(min_y);
    max_y = std::ceil(max_y);

    chart->axes(Qt::Vertical).first()->setRange(min_y, max_y);

    if (m_use_approximation)
        fitData(xFiltered, yFiltered);
    else
        computeLossFactorOberst(xFiltered, yFiltered);
}


void LiveChartWidget::addVerticalLine(QChart* chart, qreal x, qreal minY, qreal maxY, QColor color, int thickness) {
    QLineSeries* line = new QLineSeries();
    line->append(x, minY);
    line->append(x, maxY);

    QPen pen(color);
    pen.setWidth(thickness);
    line->setPen(pen);

    chart->addSeries(line);
    line->attachAxis(chart->axes(Qt::Horizontal).first());
    line->attachAxis(chart->axes(Qt::Vertical).first());

    verticalLines.append(line); // Track it
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

void LiveChartWidget::computeLossFactorOberst(const std::vector<float>& xData, const std::vector<float>& yData)
{
    //const auto &xData = (deviceIndex == 0) ? reader->device1Data(FREQ) : reader->device2Data(FREQ); // frequency
    //const auto &yData = (deviceIndex == 0) ? reader->device1Data(AMP) : reader->device2Data(AMP); // amplitude
    m_is_success = false;
    int count = std::min(xData.size(), yData.size());
    if (count < 3)
        return;

    // Step 1: Find peak amplitude and its frequency
    auto maxIt = std::max_element(yData.begin(), yData.end());
    this->peakAmplitude = *maxIt;
    int peakIndex = std::distance(yData.begin(), maxIt);
    this->peakFreq = xData[peakIndex];
    this->threshold = peakAmplitude / std::sqrt(2.0);

    // qDebug() << "Max amp:" << peakAmplitude << "peakIndex: " <<peakIndex << "peakFreq" << peakFreq << "threshold:" << threshold;

    // Step 2: Search for lower and upper frequencies where amplitude crosses threshold
    f1 = -1, f2 = -1;

    // Lower side
    for (int i = peakIndex; i > 0; --i) {
        if (yData[i] > threshold && yData[i - 1] <= threshold) {
            // Linear interpolation for better accuracy
            qreal t = (threshold - yData[i]) / (yData[i - 1] - yData[i]);
            f1 = xData[i] + t * (xData[i - 1] - xData[i]);
            break;
        }
    }
    // qDebug() << "F1 " << f1;
    // Upper side
    for (int i = peakIndex; i < count - 1; ++i) {
        if (yData[i] > threshold && yData[i + 1] <= threshold) {
            qreal t = (threshold - yData[i]) / (yData[i + 1] - yData[i]);
            f2 = xData[i] + t * (xData[i + 1] - xData[i]);
            break;
        }
    }

    // qDebug() << "F2 " << f2;

    if (f1 > 0 && f2 > 0) {
        this->deltaF = f2 - f1;
        this->lossFactor = deltaF / peakFreq;

        // qDebug() << "Oberst Loss Factor (η):" << lossFactor;

        // Show vertical lines on chart
        qreal minY = *std::min_element(yData.begin(), yData.end());
        qreal maxY = *std::max_element(yData.begin(), yData.end());

        // Show new vertical lines
        addVerticalLine(chart, f1, minY, maxY, Qt::blue, 1);
        addVerticalLine(chart, f2, minY, maxY, Qt::blue, 1);
        addVerticalLine(chart, peakFreq, minY, maxY, Qt::red, 2);
        m_is_success = true;
    } else {
        // qDebug() << "Unable to find both threshold crossings.";
    }
}

void LiveChartWidget::removeVerticalLines() {
    for (QLineSeries* line : std::as_const(verticalLines)) {
        chart->removeSeries(line);
        delete line;
    }
    verticalLines.clear();
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
