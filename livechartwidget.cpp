#include "livechartwidget.h"

LiveChartWidget::LiveChartWidget(ModbusReader* reader, int deviceIndex, QWidget *parent)
    : QWidget(parent), reader(reader), deviceIndex(deviceIndex) {

    //series = new QScatterSeries();
    series = new QLineSeries();
    //series->setMarkerSize(8);

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

void LiveChartWidget::updateChart() {
    const auto &xData = (deviceIndex == 0) ? reader->device1Data(FREQ) : reader->device2Data(FREQ); // param 1 as X
    //const auto &yData = (deviceIndex == 0) ? reader->device1Data(0) : reader->device2Data(0); // param 0 as Y
    const auto yData = divideVectors(reader->device1Data(AMP), reader->device2Data(AMP));
    int count = std::min(xData.size(), yData.size());
    if (count == 0)
        return;
    series->clear();

    for (int i = 0; i < count; ++i)
        series->append(xData[i], yData[i]);

    chart->axes(Qt::Horizontal).first()->setRange(*std::min_element(xData.begin(), xData.end()),
                                                  *std::max_element(xData.begin(), xData.end()));
    chart->axes(Qt::Vertical).first()->setRange(*std::min_element(yData.begin(), yData.end()),
                                                *std::max_element(yData.begin(), yData.end()));
    computeLossFactorOberst(xData, yData);
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
    size_t maxSize = std::max(a.size(), b.size());
    std::vector<float> result(maxSize);

    for (size_t i = 0; i < maxSize; ++i) {
        double ai = i < a.size() ? a[i] : 0.0;  // or any default
        double bi = i < b.size() ? b[i] : 1.0;  // avoid zero!

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

    qDebug() << "Max amp:" << peakAmplitude << "peakIndex: " <<peakIndex << "peakFreq" << peakFreq << "threshold:" << threshold;

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
    qDebug() << "F1 " << f1;
    // Upper side
    for (int i = peakIndex; i < count - 1; ++i) {
        if (yData[i] > threshold && yData[i + 1] <= threshold) {
            qreal t = (threshold - yData[i]) / (yData[i + 1] - yData[i]);
            f2 = xData[i] + t * (xData[i + 1] - xData[i]);
            break;
        }
    }

    qDebug() << "F2 " << f2;

    if (f1 > 0 && f2 > 0) {
        this->deltaF = f2 - f1;
        this->lossFactor = deltaF / peakFreq;

        qDebug() << "Oberst Loss Factor (η):" << lossFactor;

        // Show vertical lines on chart
        qreal minY = *std::min_element(yData.begin(), yData.end());
        qreal maxY = *std::max_element(yData.begin(), yData.end());

        // Remove previous vertical lines
        for (QLineSeries* line : verticalLines) {
            chart->removeSeries(line);
            delete line;
        }
        verticalLines.clear();

        // Show new vertical lines
        addVerticalLine(chart, f1, minY, maxY, Qt::blue, 1);
        addVerticalLine(chart, f2, minY, maxY, Qt::blue, 1);
        addVerticalLine(chart, peakFreq, minY, maxY, Qt::red, 2);
        m_is_success = true;
    } else {
        qDebug() << "Unable to find both threshold crossings.";
    }
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
