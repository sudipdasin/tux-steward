// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWizard>
namespace tux {
class SetupWizard final : public QWizard {
public:
    SetupWizard(QString sourcePrefix,QString dataHome,QWidget *parent=nullptr);
    bool validateCurrentPage() override;
private:
    QString source_,home_;
};
}
