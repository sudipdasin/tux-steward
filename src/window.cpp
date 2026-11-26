#include "tux/window.hpp"
#include "tux/trusted_command.hpp"
#include "tux/history_dialog.hpp"
#include "tux/report_dialog.hpp"
#include "tux/health_dialog.hpp"
#include <QAction>
#include <QProcess>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QFileDialog>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSplitter>
#include <QShortcut>
#include <QTabWidget>
#include <QTextCodec>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <stdexcept>

namespace tux {
namespace {
QString guide(const QString& group) {
    if (group=="Storage") return "Inspect mounts before repairs. Filesystem repair remains an offline, manual workflow.";
    if (group=="Maintenance") return "Start with previews. Journal pruning removes archived logs permanently; export needed diagnostics first.";
    if (group=="Performance") return "Measure before tuning. Compositor controls are reversible. Hardware-specific profiles are planned.";
    if (group=="Input") return "X11 device inspection. Device toggling with timed recovery is planned; permanent disabling is not available.";
    if (group=="Gaming") return "Check Wine, Lutris and graphics support. Managed game prefixes and installers are planned.";
    if (group=="Schedules") return "Inspect timers or enable the distribution SSD trim timer. Custom cleanup plans are planned.";
    if (group=="Experimental") return "Experimental beta tools that change settings. Real desktop validation is incomplete. Read every command and keep backups before testing.";
    return "Review system health without changing settings. Reports may contain private machine information; review before sharing.";
}
}
Window::Window() {
    setWindowTitle("Tux Steward · Linux System Tools"); resize(1120,820); setMinimumSize(840,620);
    setStyleSheet("QMainWindow,QWidget{background:#101827;color:#e7eef9;font-size:14px;}"
                  "QTabWidget::pane{border:1px solid #2a3c54;} QTabBar::tab{background:#192438;padding:12px;}"
                  "QTabBar::tab:selected{background:#293c54;color:#66dfbf;}"
                  "QPushButton{background:#2b4359;border:0;border-radius:5px;padding:9px 14px;}"
                  "QPushButton:hover{background:#365775;} QPushButton:disabled{color:#8794a7;background:#202c3e;}"
                  "QLineEdit,QPlainTextEdit{background:#0b1220;border:1px solid #2a3c54;padding:8px;}"
                  "QMenuBar,QMenu{background:#192438;} QScrollArea{border:0;}");
    auto* file=menuBar()->addMenu("&File");
    auto* exportAction=file->addAction("Export session report…");
    connect(exportAction,&QAction::triggered,this,[this]{exportReport();});
    file->addSeparator(); connect(file->addAction("Quit"),&QAction::triggered,this,[this]{close();});
    auto* view=menuBar()->addMenu("&View");
    auto* historyAction=view->addAction("Session history…"); historyAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(historyAction,&QAction::triggered,this,[this]{showHistory();});
    auto* healthAction=view->addAction("Health check…");
    connect(healthAction,&QAction::triggered,this,[this]{showHealth();});
    auto* help=menuBar()->addMenu("&Help");
    connect(help->addAction("Setup and removal…"),&QAction::triggered,this,[this]{
        if (!QProcess::startDetached(QCoreApplication::applicationDirPath()+"/tux-setup",{}))
            QMessageBox::warning(this,"Setup unavailable","The setup program could not start. Use the complete archive or reinstall the package.");
    });
    connect(help->addAction("Send feedback…"),&QAction::triggered,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/tux-steward/issues/new/choose"));});
    connect(help->addAction("Contribute…"),&QAction::triggered,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/tux-steward/blob/main/CONTRIBUTING.md"));});
    connect(help->addAction("About & roadmap"),&QAction::triggered,this,[this]{
        QMessageBox::about(this,"Tux Steward",QString("Tux Steward %1\nDeveloped by Sudip Das\nC++17 · Qt Widgets\nGPL-3.0-or-later; no warranty. See LICENSE for terms.\n\nPlanned: managed Wine games, recovery-safe input controls, maintenance plans.").arg(Version));
    });
    auto* center=new QWidget; setCentralWidget(center);
    auto* layout=new QVBoxLayout(center); layout->setContentsMargins(24,16,24,18); layout->setSpacing(12);
    auto* brand=new QLabel("◉  TUX STEWARD"); brand->setStyleSheet("color:#66dfbf;font-size:16px;font-weight:bold;"); layout->addWidget(brand);
    auto* heading=new QLabel("A calmer way to care for Linux."); heading->setStyleSheet("font-size:27px;font-weight:bold;"); layout->addWidget(heading);
    layout->addWidget(new QLabel(QString("Version %1 public beta · C++17 · Native desktop tools").arg(Version)));
    status_=new QLabel("Ready · Choose a tool to review its command"); status_->setObjectName("taskStatus"); status_->setStyleSheet("color:#66dfbf;"); layout->addWidget(status_);
    healthButton_=new QPushButton("Health Check · Review and run diagnostics…"); healthButton_->setObjectName("healthCheck");
    connect(healthButton_,&QPushButton::clicked,this,[this]{showHealth();}); layout->addWidget(healthButton_);
    auto* taskManager=new QPushButton("Task Manager · CPU, RAM, disks and network graphs…");taskManager->setObjectName("openTaskManager");
    connect(taskManager,&QPushButton::clicked,this,[this]{
        if(!QProcess::startDetached(QCoreApplication::applicationDirPath()+"/tux-task-manager",{})) QMessageBox::warning(this,"Task Manager unavailable","Install the complete Tux Steward package to open Task Manager.");
    });layout->addWidget(taskManager);
    auto* searchRow=new QHBoxLayout;
    searchRow->addWidget(new QLabel("Find tools"));
    search_=new QLineEdit; search_->setObjectName("toolSearch"); search_->setPlaceholderText("Search by name, category or description…"); searchRow->addWidget(search_);
    search_->setAccessibleName("Find tools across all tabs");
    search_->setClearButtonEnabled(true);
    auto* clear=new QPushButton("Clear"); searchRow->addWidget(clear); layout->addLayout(searchRow);
    searchStatus_=new QLabel; searchStatus_->setObjectName("searchStatus");
    searchStatus_->setWordWrap(true); layout->addWidget(searchStatus_);
    auto* findShortcut=new QShortcut(QKeySequence::Find,this);
    connect(findShortcut,&QShortcut::activated,this,[this]{search_->setFocus(); search_->selectAll();});
    auto* escapeShortcut=new QShortcut(QKeySequence(Qt::Key_Escape),search_);
    escapeShortcut->setContext(Qt::WidgetShortcut);
    connect(escapeShortcut,&QShortcut::activated,search_,&QLineEdit::clear);
    connect(clear,&QPushButton::clicked,search_,&QLineEdit::clear);
    auto* splitter=new QSplitter(Qt::Vertical); layout->addWidget(splitter,1);
    tabs_=new QTabWidget; tabs_->setObjectName("toolTabs"); splitter->addWidget(tabs_);
    const QStringList groups={"Diagnostics","Storage","Maintenance","Performance","Input","Gaming","Schedules","Experimental"};
    for (const auto& group:groups) {
        auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); auto* page=new QWidget;
        auto* items=new QVBoxLayout(page); items->setContentsMargins(16,14,16,14);
        auto* intro=new QLabel(guide(group)); intro->setWordWrap(true); intro->setStyleSheet("color:#9aacc5;"); items->addWidget(intro);
        for (int i=0;i<tux::actions().size();++i) {
            const auto& tool=tux::actions()[i];
            const auto displayGroup=(tool.group=="Network"||tool.group=="Security")?QString("Diagnostics"):tool.group=="Desktop"?QString("Input"):tool.group;
            if ((group=="Experimental") ? !tool.changes : (tool.changes || displayGroup!=group)) continue;
            auto* row=new QWidget; auto* columns=new QHBoxLayout(row); columns->setContentsMargins(0,6,0,6);
            auto* button=new QPushButton(tool.title); button->setObjectName("tool_"+tool.id); button->setMinimumWidth(230); columns->addWidget(button);
            auto* description=new QVBoxLayout; auto* details=new QLabel(tool.description); details->setWordWrap(true); description->addWidget(details);
            auto* state=new QLabel; state->setWordWrap(true); state->setStyleSheet("color:#9aacc5;font-size:12px;"); description->addWidget(state); columns->addLayout(description,1);
            items->addWidget(row); rows_.append({i,row,button,state});
            connect(button,&QPushButton::clicked,this,[this,i]{review(i);});
        }
        items->addStretch(); scroll->setWidget(page); tabs_->addTab(scroll,group);
    }
    auto* outputPanel=new QWidget; auto* outputLayout=new QVBoxLayout(outputPanel); outputLayout->setContentsMargins(0,10,0,0);
    auto* outputTools=new QHBoxLayout;
    outputTools->addWidget(new QLabel("LIVE OUTPUT · latest 250 KB per task")); outputTools->addStretch();
    historyButton_=new QPushButton("History (0)"); historyButton_->setObjectName("sessionHistory");
    connect(historyButton_,&QPushButton::clicked,this,[this]{showHistory();}); outputTools->addWidget(historyButton_);
    stopButton_=new QPushButton("Stop diagnostic"); stopButton_->setObjectName("stopDiagnostic");
    stopButton_->setEnabled(false);
    stopButton_->setToolTip("Stop a read-only diagnostic. Settings-changing tasks cannot be stopped here.");
    connect(stopButton_,&QPushButton::clicked,this,[this]{
        if(!busy_ || !cancellable_) return;
        cancelRequested_.store(true,std::memory_order_relaxed); stopButton_->setEnabled(false);
        status_->setText("Stopping diagnostic…");
    }); outputTools->addWidget(stopButton_);
    outputLayout->addLayout(outputTools);
    output_=new QPlainTextEdit; output_->setObjectName("sessionOutput"); output_->setReadOnly(true); output_->setMinimumHeight(120);
    output_->setStyleSheet("font-family:monospace;background:#0b1220;"); outputLayout->addWidget(output_); splitter->addWidget(outputPanel); splitter->setSizes({420,180});
    write("Welcome. No maintenance actions run at startup.\nReview commands before running them.\n");
    connect(search_,&QLineEdit::textChanged,this,[this]{filter();});
    timer_=new QTimer(this); timer_->setInterval(50); connect(timer_,&QTimer::timeout,this,[this]{poll();}); timer_->start();
    updateButtons();
    filter();
}
Window::~Window() { if (worker_.joinable()) worker_.join(); }
int Window::toolCount() const { return rows_.size(); }
void Window::filter() {
    QVector<int> counts(tabs_->count(),0);
    int total=0;
    for (const auto& row:rows_) {
        const bool found=matches(tux::actions()[row.index],search_->text());
        row.widget->setVisible(found);
        if (!found) continue;
        ++total;
        for (int i=0;i<tabs_->count();++i)
            if (tabs_->widget(i)->isAncestorOf(row.widget)) { ++counts[i]; break; }
    }
    const bool searching=!search_->text().trimmed().isEmpty();
    const QStringList groups={"Diagnostics","Storage","Maintenance","Performance","Input","Gaming","Schedules","Experimental"};
    for (int i=0;i<tabs_->count();++i)
        tabs_->setTabText(i,searching?QString("%1 (%2)").arg(groups[i]).arg(counts[i]):groups[i]);
    if (searching && total && counts[tabs_->currentIndex()]==0)
        for (int i=0;i<counts.size();++i) if (counts[i]) { tabs_->setCurrentIndex(i); break; }
    searchStatus_->setText(!searching?QString("%1 tools · Ctrl+F to search · Review every command before running").arg(rows_.size()):
        total?QString("%1 matching tool(s) across all tabs · Esc clears search").arg(total):
        "No matching tools. Try a shorter name or clear the search.");
}
void Window::updateButtons() {
    healthButton_->setEnabled(!busy_);
    for (const auto& row:rows_) {
        const auto& tool=tux::actions()[row.index]; const auto problem=unavailable(tool);
        row.button->setEnabled(!busy_ && problem.isEmpty());
        row.state->setText(!problem.isEmpty()?problem:tool.admin?"Administrator authentication required":tool.changes?"Changes settings":"Read only");
        row.button->setToolTip(problem.isEmpty()?tool.description:problem);
    }
}
void Window::review(int index) {
    if (busy_) return;
    const auto tool=tux::actions()[index]; const auto problem=unavailable(tool);
    if (!problem.isEmpty()) { QMessageBox::warning(this,tool.title,problem); return; }
    auto command=tool.command;
    if (tool.admin) {command[0]=trustedSystemExecutable(command.first());command.prepend(trustedSystemExecutable("pkexec"));}
    QMessageBox dialog(QMessageBox::Question,tool.title,tool.description,QMessageBox::Ok|QMessageBox::Cancel,this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setInformativeText("Command:\n"+preview(command)+"\n\n"+(tool.changes?"This action changes your system. Continue?":"Run this diagnostic?"));
    dialog.setDefaultButton(QMessageBox::Cancel);
    if (dialog.exec()!=QMessageBox::Ok) return;
    cancelRequested_.store(false,std::memory_order_relaxed);
    cancellable_=!tool.changes && !tool.admin;
    busy_=true; updateButtons(); stopButton_->setEnabled(cancellable_);
    status_->setText("Running · "+tool.title); write("\n$ "+preview(command)+"\n");
    decoder_.reset(QTextCodec::codecForName("UTF-8")->makeDecoder());
    worker_=std::thread([this,tool,command] {
        Result result;
        try {
            result=execute(tool.id,true,120000,[this](const QByteArray& chunk) {
                std::lock_guard<std::mutex> lock(mutex_); pending_.append(chunk);
                if (pending_.size()>OutputLimit) { pending_=pending_.right(OutputLimit); skipped_=true; }
            },&cancelRequested_);
        } catch (const std::exception& error) {
            result.action=tool.title; result.command=command; result.code=-1; result.output=error.what();
            std::lock_guard<std::mutex> lock(mutex_); pending_.append(result.output);
        }
        std::lock_guard<std::mutex> lock(mutex_); completed_=std::move(result);
    });
}
void Window::poll() {
    QByteArray pending; bool skipped; std::optional<Result> result;
    {
        std::lock_guard<std::mutex> lock(mutex_); pending.swap(pending_); skipped=skipped_; skipped_=false;
        if (completed_) { result=std::move(completed_); completed_.reset(); }
    }
    if (skipped) { decoder_.reset(QTextCodec::codecForName("UTF-8")->makeDecoder()); write("\n[Live preview skipped a burst; report retains the output tail.]\n"); }
    if (!pending.isEmpty()) write(decoder_?decoder_->toUnicode(pending):QString::fromUtf8(pending));
    if (result) {
        if (worker_.joinable()) worker_.join();
        history_.append(*result); if (history_.size()>20) history_.removeFirst();
        historyButton_->setText(QString("History (%1)").arg(history_.size()));
        busy_=false; cancellable_=false; stopButton_->setEnabled(false); updateButtons();
        status_->setText((result->cancelled?"Cancelled · ":result->code==0?"Completed · ":"Needs attention · ")+result->action);
        write(QString("\nExit code: %1 · %2 seconds\n").arg(result->code).arg(result->seconds,0,'f',2));
        if (result->truncated) write("Report retains the latest 250,000 output bytes.\n");
    }
}
void Window::write(const QString& text) {
    auto cursor=output_->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText(text);
    const auto extra=output_->document()->characterCount()-1-OutputLimit;
    if (extra>0) { cursor.movePosition(QTextCursor::Start); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,extra); cursor.removeSelectedText(); }
    output_->moveCursor(QTextCursor::End);
}
void Window::showHealth() {
    if(busy_) return;
    HealthDialog dialog(this); dialog.exec();
    const auto results=dialog.results(); if(results.isEmpty()) return;
    for(const auto& result:results) {history_.append(result); if(history_.size()>20) history_.removeFirst();}
    historyButton_->setText(QString("History (%1)").arg(history_.size()));
    status_->setText(QString("Health check · %1 results recorded · Inspect findings in History").arg(results.size()));
    write(QString("\nHealth Check recorded %1 diagnostic results. Open History or Export session report to review them.\n").arg(results.size()));
}
void Window::showHistory() {
    HistoryDialog dialog(history_,busy_,this);
    if(dialog.exec()!=QDialog::Accepted) return;
    const auto id=dialog.selectedTool();
    for(int i=0;i<tux::actions().size();++i) if(tux::actions()[i].id==id) { review(i); break; }
}
void Window::exportReport() {
    ReportDialog dialog(history_,this);
    dialog.exec();
}
void Window::closeEvent(QCloseEvent* event) {
    if (busy_) { QMessageBox::warning(this,"Task running","Wait for the current task to finish before closing."); event->ignore(); }
    else event->accept();
}
}

