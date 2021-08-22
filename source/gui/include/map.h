#ifndef MAP_H
#define MAP_H

#include <QWidget>

struct GeodeticPos;

namespace Ui {
class Map;
}

class Map : public QWidget {
    Q_OBJECT

public:
    explicit Map(QWidget* parent = nullptr);
    void onGeodeticPosReceived(GeodeticPos pos);
    ~Map();

private:
    QObject* mapComponent = nullptr;
    Ui::Map* mapUi;
};
#endif // MAP_H
