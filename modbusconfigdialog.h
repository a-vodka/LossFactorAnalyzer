#ifndef MODBUSCONFIGDIALOG_H
#define MODBUSCONFIGDIALOG_H

#include <QDialog>

namespace Ui {
class modbusconfigdialog;
}

class modbusconfigdialog : public QDialog
{
    Q_OBJECT

public:
    explicit modbusconfigdialog(QWidget *parent = nullptr);
    ~modbusconfigdialog();

    QString port() const;
    int baudRate() const;
    int dataBits() const;
    QString parity() const;
    float stopBits() const;
    QString flowControl() const;
    int device1Address() const;
    int device2Address() const;
public slots:
    void accept();

private:
    Ui::modbusconfigdialog *ui;

     void populatePortList();
};

#endif // MODBUSCONFIGDIALOG_H
