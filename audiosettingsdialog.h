#ifndef AUDIOSETTINGSDIALOG_H
#define AUDIOSETTINGSDIALOG_H

#include <QAudioDevice>
#include <QDialog>
#include <QSettings>

namespace Ui {
class AudioSettingsDialog;
}

class AudioSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);
    ~AudioSettingsDialog();

    void loadSettings();
    QString selectedDeviceId() const;
    int selectedVolume() const;
public slots:
    void accept();
signals:
    void settingsChanged(QString deviceId, int volume);

private slots:
    void saveSettings();


private:
    Ui::AudioSettingsDialog *ui;
    QSettings m_settings;
    QList<QAudioDevice> m_devices;

};

#endif // AUDIOSETTINGSDIALOG_H
