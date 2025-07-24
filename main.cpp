#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon("/usr/share/icons/YTDLPFrontend.png"));
    MainWindow window;
    window.show();
    return app.exec();
}
