#include "spiform.h"
#include "ui_spiform.h"

SpiForm::SpiForm(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SpiForm)
{
    ui->setupUi(this);
}

SpiForm::~SpiForm()
{
    delete ui;
}
