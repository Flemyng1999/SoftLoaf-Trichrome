#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <string>

#include "obs_log.hpp"
#include "trichrome_image_provider.hpp"
#include "trichrome_controller.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SoftLoaf Trichrome");
    app.setOrganizationName("SoftLoaf");

    using namespace softloaf::trichrome::desktop;
    ObsLog("app.startup", {{"event", "begin"}});

    auto* image_provider = new TrichromeImageProvider();
    TrichromeController controller(image_provider);

    QQmlApplicationEngine engine;
    engine.addImageProvider("trichrome", image_provider);
    engine.rootContext()->setContextProperty("trichromeController", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    ObsLog("app.startup", {{"event", "qml_load_start"}});
    engine.loadFromModule("SoftLoafTrichrome", "Main");

    const int exit_code = app.exec();
    ObsLog("app.startup", {{"event", "app_exec_return"},
                           {"exit_code", std::to_string(exit_code)}});
    return exit_code;
}
