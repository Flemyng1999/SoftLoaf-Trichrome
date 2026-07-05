#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>

#include <string>

#include "obs_log.hpp"
#include "trichrome_image_provider.hpp"
#include "trichrome_controller.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SoftLoaf Trichrome");
    app.setOrganizationName("SoftLoaf");
    QQuickStyle::setStyle("Basic");

    using namespace softloaf::trichrome::desktop;
    ObsLog("app.startup", {{"event", "begin"}});

    auto* image_provider = new TrichromeImageProvider();
    TrichromeController controller(image_provider);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &controller, [&controller]() { controller.shutdown(); });

    QQmlApplicationEngine engine;
    engine.addImageProvider("trichrome", image_provider);
    engine.rootContext()->setContextProperty("trichromeController", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    ObsLog("app.startup", {{"event", "qml_load_start"}});
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     &app, [](const QList<QQmlError>& warnings) {
        for (const QQmlError& warning : warnings)
            ObsLog("qml.warning", {{"message", warning.toString().toStdString()}});
    });
    engine.loadFromModule("SoftLoafTrichrome", "Main");
    ObsLog("app.startup", {{"event", "qml_load_return"},
                           {"root_count", std::to_string(engine.rootObjects().size())}});

    const int exit_code = app.exec();
    ObsLog("app.startup", {{"event", "app_exec_return"},
                           {"exit_code", std::to_string(exit_code)}});
    return exit_code;
}
