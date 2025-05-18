#include "mainwindow.h"

#include <QApplication>
#include <QSplashScreen>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //set ico
    QApplication::setWindowIcon(QIcon(":/images/icon-ico.ico"));

    // Create splash screen
    QPixmap pixmap(":/images/logo.jpg");  // Or use "splash.png" if not in resources
    QPixmap scaledPixmap = pixmap.scaled(600, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QSplashScreen splash(scaledPixmap);
    splash.show();

    // Simulate loading or initialization
    QTimer::singleShot(1500, &splash, &QSplashScreen::close); // Close after 2 seconds

    MainWindow mainWin;
    QTimer::singleShot(1500, &mainWin, [&mainWin]() {
        mainWin.show();
    });


    //MainWindow w;
    //w.show();
    return a.exec();
}
