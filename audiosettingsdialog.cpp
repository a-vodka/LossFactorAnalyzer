#include "audiosettingsdialog.h"
#include <QMediaDevices>
#include "qaudiodevice.h"
#include "ui_audiosettingsdialog.h"

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AudioSettingsDialog)
{
    ui->setupUi(this);

    m_devices = QMediaDevices().audioOutputs();
    for (const auto &device : std::as_const(m_devices))
    {
        ui->m_deviceBox->addItem(device.description(), device.id());
    }

    ui->m_volumeSlider->setRange(0, 100);


    this->loadSettings();

}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}


void AudioSettingsDialog::loadSettings()
{
    QString deviceId = m_settings.value("audio/deviceId").toString();
    int volume = m_settings.value("audio/volume", 50).toInt();

    for (int i = 0; i < ui->m_deviceBox->count(); ++i) {
        if (ui->m_deviceBox->itemData(i).toString() == deviceId) {
            ui->m_deviceBox->setCurrentIndex(i);
            break;
        }
    }
    ui->m_volumeSlider->setValue(volume);
}

void AudioSettingsDialog::saveSettings()
{
    QString deviceId = ui->m_deviceBox->currentData().toString();
    int volume = ui->m_volumeSlider->value();

    m_settings.setValue("audio/deviceId", deviceId);
    m_settings.setValue("audio/volume", volume);
}

void AudioSettingsDialog::accept()
{
    qDebug() << "onAccept";
    saveSettings();
    QDialog::accept();
}

QString AudioSettingsDialog::selectedDeviceId() const
{
    return ui->m_deviceBox->currentData().toString();
}

int AudioSettingsDialog::selectedVolume() const
{
    return ui->m_volumeSlider->value();
}
