#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "trichrome_controller.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SoftLoaf Trichrome");
    app.setOrganizationName("SoftLoaf");

    softloaf::trichrome::desktop::TrichromeController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("trichromeController", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("SoftLoafTrichrome", "Main");

    return app.exec();
}
