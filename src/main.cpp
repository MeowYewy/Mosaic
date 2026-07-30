#include "appcontroller.h"
#include "appsettings.h"
#include "pipelinediag.h"
#include "previewimageprovider.h"
#include "updatechecker.h"
#include "redemptionclient.h"
#include "filedropbridge.h"

#include "filepicker.h"
#include "pdfappcontroller.h"
#include "pdfthumbprovider.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--diag")) {
        const QString report = runPipelineSelfTest();
        fprintf(stderr, "%s\n", report.toLocal8Bit().constData());
        const bool ok = report.contains(QStringLiteral("RESULT: OK"));
        return ok ? 0 : 2;
    }

    app.setOrganizationName(QStringLiteral("TechG"));
    app.setOrganizationDomain(QStringLiteral("techg.local"));
    app.setApplicationName(QStringLiteral("Mosaic"));
    app.setApplicationVersion(QStringLiteral("0.2.1"));

    QIcon appIcon(QStringLiteral(":/qt/qml/ProjectO/resources/app-icon.ico"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/qt/qml/ProjectO/resources/app-icon.png"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral(":/qt/qml/ProjectO/resources/logo.svg"));
    app.setWindowIcon(appIcon);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    auto *maskPreviewProvider = new PreviewImageProvider();
    auto *pdfThumbProvider = new PdfThumbProvider();
    AppSettings settings;
    UpdateChecker updateChecker;
    RedemptionClient redemptionClient(&settings);
    FileDropBridge fileDropBridge;
    FilePicker filePicker(&settings);
    PdfAppController pdfController(pdfThumbProvider, &settings, &filePicker);
    AppController maskController(maskPreviewProvider, &settings, &filePicker);

    QObject::connect(
        &fileDropBridge, &FileDropBridge::filesDropped, &app,
        [&settings, &pdfController, &maskController](const QStringList &paths) {
            if (paths.isEmpty())
                return;
            if (settings.maskMode()) {
                QVariantList list;
                list.reserve(paths.size());
                for (const QString &path : paths)
                    list.append(path);
                maskController.addFilesFromList(list);
            } else if (!pdfController.maskedPreview()) {
                pdfController.addFiles(paths);
            }
        });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("preview"), maskPreviewProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("AppSettings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("PdfController"), &pdfController);
    engine.rootContext()->setContextProperty(QStringLiteral("AppController"), &maskController);
    engine.rootContext()->setContextProperty(QStringLiteral("FilePicker"), &filePicker);
    engine.rootContext()->setContextProperty(QStringLiteral("UpdateChecker"), &updateChecker);
    engine.rootContext()->setContextProperty(QStringLiteral("RedemptionClient"), &redemptionClient);
    engine.rootContext()->setContextProperty(QStringLiteral("FileDropBridge"), &fileDropBridge);

    QStringList qmlErrors;
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [&qmlErrors](const QList<QQmlError> &warnings) {
                         for (const QQmlError &err : warnings)
                             qmlErrors << err.toString();
                     });

    const auto showLoadError = [](const QString &detail) {
        QMessageBox::critical(nullptr, QStringLiteral("Mosaic"),
                              QStringLiteral("Failed to load UI / 界面加载失败\n\n%1").arg(detail));
    };

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [&]() {
            showLoadError(qmlErrors.isEmpty() ? QStringLiteral("objectCreationFailed")
                                              : qmlErrors.join(QLatin1Char('\n')));
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule(QStringLiteral("ProjectO"), QStringLiteral("Main"));

    if (!engine.rootObjects().isEmpty()) {
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            window->setIcon(appIcon);
            fileDropBridge.attachWindow(window);
        }
        return app.exec();
    }

    showLoadError(qmlErrors.isEmpty() ? QStringLiteral("No QML window was created.")
                                      : qmlErrors.join(QLatin1Char('\n')));
    return -1;
}
