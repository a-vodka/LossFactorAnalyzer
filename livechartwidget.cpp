#include "livechartwidget.h"

LiveChartWidget::LiveChartWidget(ModbusReader* reader, int deviceIndex, QWidget *parent)
    : QWidget(parent), reader(reader), deviceIndex(deviceIndex) {

    series = new QScatterSeries();
    //series->setMarkerSize(8);

    chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();

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
    const auto &xData = (deviceIndex == 0) ? reader->device1Data(1) : reader->device2Data(1); // param 1 as X
    const auto &yData = (deviceIndex == 0) ? reader->device1Data(0) : reader->device2Data(0); // param 0 as Y

    int count = std::min(xData.size(), yData.size());
    if (count == 0)
        return;
    series->clear();

    for (int i = 0; i < count; ++i)
        series->append(xData[i], yData[i]);

    qreal minY = *std::min_element(yData.begin(), yData.end());
    qreal maxY = *std::max_element(yData.begin(), yData.end());

    chart->axes(Qt::Horizontal).first()->setRange(*std::min_element(xData.begin(), xData.end()),
                                                  *std::max_element(xData.begin(), xData.end()));
    chart->axes(Qt::Vertical).first()->setRange(*std::min_element(yData.begin(), yData.end()),
                                                *std::max_element(yData.begin(), yData.end()));
}


void LiveChartWidget::addVerticalLine(QChart* chart, qreal x, qreal minY, qreal maxY, QColor color, int thickness) {
    QLineSeries* line = new QLineSeries();
    line->append(x, minY);
    line->append(x, maxY);

    QPen pen(color);
    pen.setWidth(thickness);
    line->setPen(pen);

    chart->addSeries(line);

    // Attach to existing axes
    line->attachAxis(chart->axes(Qt::Horizontal).first());
    line->attachAxis(chart->axes(Qt::Vertical).first());
}
