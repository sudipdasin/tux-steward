// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/schedule_page.hpp"
#include "syspulse/schedule.hpp"
#include "tux/async_command.hpp"
#include "tux/trusted_command.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
namespace syspulse {
struct SchedulePage::Impl {
    SchedulePage* q;QComboBox *preset,*cadence,*weekday;QSpinBox *hour,*minute;QCheckBox* ac;QLabel* status;
    QVector<QPushButton*> buttons;bool busy=false;QString observed="Automation not checked";QVector<QStringList> steps;bool changing=false;bool requestedEnabled=false;
    Impl(SchedulePage* page):q(page){
        q->setObjectName("diagnosticSchedule");auto* layout=new QVBoxLayout(q);auto* note=new QLabel("Schedule only the fixed read-only Health Check presets as your normal user. Reports are saved locally as owner-only summaries. Persistent timers catch up missed runs at user-manager startup; nothing is enabled until you review and confirm.");note->setWordWrap(true);layout->addWidget(note);
        auto* form=new QFormLayout;preset=new QComboBox;preset->setObjectName("schedulePreset");for(const auto& s:QStringList{"balanced","storage","logs","all"})preset->addItem(s,s);
        cadence=new QComboBox;cadence->setObjectName("scheduleCadence");for(const auto& s:QStringList{"hourly","daily","weekly"})cadence->addItem(s,s);cadence->setCurrentIndex(1);
        hour=new QSpinBox;hour->setObjectName("scheduleHour");hour->setRange(0,23);hour->setValue(10);minute=new QSpinBox;minute->setObjectName("scheduleMinute");minute->setRange(0,59);
        weekday=new QComboBox;weekday->setObjectName("scheduleWeekday");weekday->addItems({"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"});
        ac=new QCheckBox("Run only on AC power (when reported by the system)");ac->setObjectName("scheduleACOnly");form->addRow("Diagnostic preset",preset);form->addRow("Cadence",cadence);form->addRow("Hour (local time)",hour);form->addRow("Minute",minute);form->addRow("Weekday",weekday);form->addRow(ac);layout->addLayout(form);
        status=new QLabel("Status not checked. Enabling requires a working systemd user manager.");status->setObjectName("scheduleStatus");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
        auto* row=new QHBoxLayout;
        auto add=[&](const QString& name,const char* id,std::function<void()> fn){auto* b=new QPushButton(QString(name).replace("&","&&"));b->setObjectName(id);buttons.append(b);row->addWidget(b);QObject::connect(b,&QPushButton::clicked,q,[fn=std::move(fn)]{fn();});return b;};
        add("Preview plan…","previewSchedule",[this]{preview();});add("Review save & enable…","enableSchedule",[this]{enable();})->setProperty("primary",true);add("Review disable…","disableSchedule",[this]{disable();});add("Check actual status","refreshSchedule",[this]{query();});layout->addLayout(row);layout->addStretch();
        try{schedule::Plan saved;if(schedule::load(schedule::dataDirectory(),saved)){preset->setCurrentText(saved.preset);cadence->setCurrentText(saved.cadence);hour->setValue(saved.hour);minute->setValue(saved.minute);weekday->setCurrentIndex(saved.weekday-1);ac->setChecked(saved.acOnly);status->setText("Saved plan loaded. Check actual status before assuming the timer is active.");}}
        catch(const std::exception& e){status->setText(e.what());}
        auto fields=[this]{hour->setEnabled(!busy&&cadence->currentText()!="hourly");weekday->setEnabled(!busy&&cadence->currentText()=="weekly");};QObject::connect(cadence,qOverload<int>(&QComboBox::currentIndexChanged),q,[fields]{fields();});fields();
    }
    schedule::Plan plan()const{return {preset->currentData().toString(),cadence->currentData().toString(),hour->value(),minute->value(),weekday->currentIndex()+1,ac->isChecked()};}
    bool admitted()const{return !busy&&(!q->canExecute||q->canExecute());}
    void setBusy(bool value){busy=value;for(auto* b:buttons)b->setEnabled(!busy);preset->setEnabled(!busy);cadence->setEnabled(!busy);minute->setEnabled(!busy);ac->setEnabled(!busy);hour->setEnabled(!busy&&cadence->currentText()!="hourly");weekday->setEnabled(!busy&&cadence->currentText()=="weekly");if(q->onBusyChanged)q->onBusyChanged(busy);}
    void preview(){QDialog d(q);d.setWindowTitle("Read-only diagnostic schedule plan");auto* l=new QVBoxLayout(&d);auto* t=new QPlainTextEdit(schedule::service(plan())+"\n"+schedule::timer(plan()));t->setReadOnly(true);l->addWidget(t);auto* b=new QDialogButtonBox(QDialogButtonBox::Close);QObject::connect(b,&QDialogButtonBox::rejected,&d,&QDialog::reject);l->addWidget(b);d.resize(680,480);d.exec();}
    void enable(){if(!admitted())return;if(QMessageBox::question(q,"Review diagnostic automation",QString("Enable %1 read-only checks at %2? This creates/updates your SysPulse user timer and local redacted reports. It never runs cleanup or administrator commands. Missed runs may execute at user-manager startup.").arg(plan().preset,schedule::calendar(plan())),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes||!admitted())return;
        if(tux::trustedSystemExecutable("syspulse-diagnostic").isEmpty()){status->setText("The installed diagnostic helper is missing or untrusted. Install the complete native package first.");return;}
        try{schedule::save(schedule::configDirectory(),schedule::dataDirectory(),plan());}catch(const std::exception& e){status->setText(e.what());return;}
        changing=true;requestedEnabled=true;steps={{"--user","daemon-reload"},{"--user","enable","--now","syspulse-diagnostics.timer"},{"--user","restart","syspulse-diagnostics.timer"}};setBusy(true);next();
    }
    void disable(){if(!admitted())return;if(QMessageBox::question(q,"Review disable", "Disable and stop only the SysPulse diagnostic timer? Saved reports and the plan will remain.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes||!admitted())return;changing=true;requestedEnabled=false;steps={{"--user","disable","--now","syspulse-diagnostics.timer"}};setBusy(true);next();}
    void next(){
        const auto executable=tux::trustedSystemExecutable("systemctl");if(executable.isEmpty()){status->setText("Trusted systemctl is unavailable. Saved configuration remains; no active state was confirmed.");record(1);observed="Automation state unknown";setBusy(false);return;}
        if(steps.isEmpty()){query(true);return;}const auto args=steps.takeFirst();status->setText("Applying the reviewed user-timer operation…");
        AsyncCommand::runAction(q,executable,args,16000,[this](const AsyncCommandResult& result){if(!result.ok()){status->setText("The timer operation failed or was incomplete. Saved configuration was retained; check actual status before retrying.");record(result.exitCode?result.exitCode:1);observed="Automation state unknown";setBusy(false);return;}next();});
    }
    void record(int code){if(changing&&q->onResult){tux::Result r;r.action="Diagnostic schedule";r.code=code;r.output=status->text().toUtf8();q->onResult(r);}changing=false;}
    void query(bool after=false){if(!after&&!admitted())return;const auto executable=tux::trustedSystemExecutable("systemctl");if(executable.isEmpty()){status->setText("systemd user-manager status is unavailable.");observed="Automation state unknown";if(after)record(1);setBusy(false);return;}if(!after){changing=false;setBusy(true);}
        AsyncCommand::run(q,executable,{"--user","show","syspulse-diagnostics.timer","--property=LoadState,ActiveState,UnitFileState,NextElapseUSecRealtime","--no-pager"},6000,16000,[this,after](const AsyncCommandResult& result){
            if(!result.ok()){status->setText("User-manager status is unavailable. No enabled/disabled state was confirmed.");observed="Automation state unknown";if(after)record(1);setBusy(false);return;}
            QMap<QString,QString> fields;for(const auto& line:QString::fromUtf8(result.output).split('\n')){const auto at=line.indexOf('=');if(at>0)fields[line.left(at)]=line.mid(at+1);}
            const auto active=fields.value("ActiveState","unknown"),enabled=fields.value("UnitFileState","not configured");observed=QString("Diagnostics: %1 / %2").arg(active,enabled);
            status->setText(QString("Runtime: %1\nAt user-manager startup: %2\nNext scheduled run: %3").arg(active,enabled,fields.value("NextElapseUSecRealtime","Unavailable")));
            if(after) {
                const bool confirmed=requestedEnabled?(active=="active"&&enabled=="enabled"):(active=="inactive"&&(enabled=="disabled"||fields.value("LoadState")=="not-found"));
                if(!confirmed)status->setText(status->text()+"\nThe requested state was not confirmed. Review the timer before retrying.");
                record(confirmed?0:1);
            }
            setBusy(false);
        });
    }
};
SchedulePage::SchedulePage(QWidget* parent):QWidget(parent),d(std::make_unique<Impl>(this)){}
SchedulePage::~SchedulePage()=default;
bool SchedulePage::isBusy()const{return d->busy;}
QString SchedulePage::automationSummary()const{return d->observed;}
}
