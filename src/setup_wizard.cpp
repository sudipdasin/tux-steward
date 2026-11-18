// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/setup_wizard.hpp"
#include "tux/setup.hpp"
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QVariant>
#include <utility>
namespace tux {
SetupWizard::SetupWizard(QString source,QString home,QWidget *parent):QWizard(parent),source_(std::move(source)),home_(std::move(home)) {
    setWindowTitle("Tux Steward Setup"); setMinimumSize(700,440); setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnLastPage);
    auto *choice=new QWizardPage; choice->setTitle("Install or remove Tux Steward");
    auto *layout=new QVBoxLayout(choice);
    auto *intro=new QLabel("Install the archive for your account, without administrator access. Qt runtime libraries must already be installed. System DEB packages remain managed by your package manager.");
    intro->setWordWrap(true); layout->addWidget(intro);
    auto *install=new QRadioButton("Install for my account"); install->setObjectName("setupInstall"); install->setChecked(true);
    auto *remove=new QRadioButton("Remove my user installation"); remove->setObjectName("setupRemove");
    layout->addWidget(install);layout->addWidget(remove);addPage(choice);
    auto *review=new QWizardPage;review->setTitle("Review this operation");layout=new QVBoxLayout(review);
    auto *details=new QLabel("Programs are copied into a dedicated user-data directory and separate user menu entries are created. Existing installations are never overwritten. Removal checks file hashes and refuses modified files. Personal configuration and unrelated files are retained.\n\nFor an upgrade, remove the unchanged user installation first, then install the new archive. Interrupted operations may require running Remove again.");
    details->setWordWrap(true);layout->addWidget(details);
    auto *target=new QLabel("Installation directory: "+setup::installRoot(home_));target->setWordWrap(true);target->setTextFormat(Qt::PlainText);layout->addWidget(target);
    auto *hint=new QLabel("Check the confirmation box before continuing. No changes are made until then.");hint->setWordWrap(true);layout->addWidget(hint);
    auto *confirm=new QCheckBox("I have reviewed the operation and want to continue");confirm->setObjectName("setupConsent");layout->addWidget(confirm);addPage(review);
    auto *done=new QWizardPage;done->setTitle("Operation result");done->setFinalPage(true);layout=new QVBoxLayout(done);
    auto *result=new QLabel;result->setObjectName("setupResult");result->setWordWrap(true);result->setTextFormat(Qt::PlainText);layout->addWidget(result);addPage(done);
    setButtonText(QWizard::NextButton,"Continue");setButtonText(QWizard::FinishButton,"Close");
}
bool SetupWizard::validateCurrentPage() {
    if (currentId()!=1) return true;
    auto *result=findChild<QLabel *>("setupResult");
    if (!findChild<QCheckBox *>("setupConsent")->isChecked()) { result->setText("No changes made: confirmation is required."); return false; }
    const auto outcome=findChild<QRadioButton *>("setupInstall")->isChecked()?setup::install(source_,home_):setup::remove(home_);
    result->setText((outcome.ok?"Completed\n\n":"Not completed\n\n")+outcome.message);
    setProperty("operationSucceeded",outcome.ok);
    return true;
}
}
