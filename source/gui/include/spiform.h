#ifndef SPIFORM_H
#define SPIFORM_H

#include <QWidget>

namespace Ui {
class SpiForm;
}

class SpiForm : public QWidget {
    Q_OBJECT

public:
    explicit SpiForm(QWidget* parent = nullptr);
    ~SpiForm();

private:
    Ui::SpiForm* ui;
};

#endif // SPIFORM_H
