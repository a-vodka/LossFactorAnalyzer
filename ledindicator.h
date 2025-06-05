#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <QWidget>

class LedIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit LedIndicator(QWidget *parent = nullptr);

    enum State {
        Off,
        Green,
        Red,
        Yellow
    };

    void setState(State state);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    State currentState = Off;
};

#endif // LEDINDICATOR_H
