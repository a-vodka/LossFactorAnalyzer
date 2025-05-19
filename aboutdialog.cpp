#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    setWindowTitle("LossFactor Analyzer");
    setFixedSize(size()); // Prevent resizing

    // Optional: Set version dynamically
    ui->labelVersion->setText("Version: " + QCoreApplication::applicationVersion());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
