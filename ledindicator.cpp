#include "ledindicator.h"
#include <QPainter>

LedIndicator::LedIndicator(QWidget *parent) : QWidget(parent) {
    setFixedSize(16, 16);
}

void LedIndicator::setState(LedIndicator::State state) {
    if (state != currentState) {
        currentState = state;
        update(); // repaint
    }
}

void LedIndicator::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    QColor color;

    switch (currentState) {
    case Green: color = Qt::green; break;
    case Red: color = Qt::red; break;
    default: color = Qt::gray; break;
    }

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QBrush(color));
    painter.setPen(Qt::black);
    painter.drawEllipse(0, 0, width() - 1, height() - 1);
}
