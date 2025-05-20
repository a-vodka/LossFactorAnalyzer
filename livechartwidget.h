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
    LiveChartWidget(ModbusReader* reader, int deviceIndex, QWidget *parent = nullptr);

private slots:
    void updateChart();

private:
    QChart *chart;
    QScatterSeries *series;
    QChartView *chartView;
    QTimer *updateTimer;
    ModbusReader* reader;
    int deviceIndex;
    void addVerticalLine(QChart* chart, qreal x, qreal minY, qreal maxY, QColor color = Qt::red, int thickness = 3);
};

#endif // LIVECHARTWIDGET_H
