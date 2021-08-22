#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSsl>
#include <map.h>
#include <muondetector_structs.h>
#include <ui_map.h>

Map::Map(QWidget* parent)
    : QWidget(parent)
    , mapUi(new Ui::Map)
{
    QVariantMap parameters;
    mapUi->setupUi(this);
    mapUi->quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QQmlEngine* engine = new QQmlEngine(this);
    QQmlComponent* component = new QQmlComponent(engine, "qrc:/qml/CustomMap.qml");
    mapComponent = component->create();
    mapUi->quickWidget->setContent(component->url(), component, mapComponent);
    mapUi->quickWidget->show();
}

Map::~Map()
{
    delete mapComponent;
    delete mapUi;
}

void Map::onGeodeticPosReceived(GeodeticPos pos)
{
    if (mapComponent == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(mapComponent, "setCircle",
        Q_ARG(QVariant, ((double)pos.lon) * 1e-7),
        Q_ARG(QVariant, ((double)pos.lat) * 1e-7),
        Q_ARG(QVariant, ((double)pos.hAcc) / 1000));
}
