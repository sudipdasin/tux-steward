// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
namespace syspulse {
QString precisionStyle() {
    return QStringLiteral(R"QSS(
QMainWindow,QDialog { background:#F7F8FA; color:#202124; }
QWidget { font-family:"DejaVu Sans","Sans Serif"; font-size:13px; color:#202124; }
QWidget#sidebar { background:#FFFFFF; border-right:1px solid #D5DAE3; }
QWidget#utilityBar { background:#FFFFFF; border-bottom:1px solid #D5DAE3; }
QLabel { background:transparent; }
QLabel#brand { font-size:28px; font-weight:700; color:#17191C; }
QLabel#brandEdition,QLabel#muted,QLabel#intro,QLabel#fileStatus { color:#667085; }
QLabel#navGroup { color:#667085; font-size:11px; font-weight:700; padding:9px 6px 2px; }
QLabel#pageTitle { font-size:27px; font-weight:700; color:#17191C; }
QLabel#sectionTitle { font-size:17px; font-weight:700; }
QLabel#metricValue { font-size:25px; font-weight:700; }
QLabel#statusFooter { color:#667085; font-size:11px; padding:6px 12px; }
QFrame#panel,QWidget#panel,QFrame#metricCard { background:#FFFFFF; border:1px solid #D5DAE3; border-radius:10px; }
QFrame#analysisHero { background:#FFFFFF; border:1px solid #D5DAE3; border-top:4px solid #C8102E; border-radius:10px; }
QPushButton,QToolButton { background:#FFFFFF; border:1px solid #C9CED8; border-radius:6px; padding:8px 12px; min-height:18px; }
QPushButton:hover,QToolButton:hover { background:#F1F3F7; border-color:#98A2B3; }
QPushButton:pressed { background:#E9ECF2; }
QPushButton:focus,QToolButton:focus,QLineEdit:focus,QComboBox:focus { border:2px solid #C8102E; }
QPushButton:disabled { background:#F1F2F4; color:#9198A4; border-color:#E2E5EA; }
QPushButton[primary="true"],QPushButton#primaryAction { background:#C8102E; border-color:#C8102E; color:white; font-weight:600; }
QPushButton[primary="true"]:hover,QPushButton#primaryAction:hover { background:#AB0E27; }
QPushButton#dangerAction { color:#A80D27; border:1px solid #DCA1AC; }
QPushButton#navButton { background:transparent; border:0; text-align:left; padding:8px 11px; min-height:16px; font-size:13px; }
QPushButton#navButton:hover { background:#F5F6F8; }
QPushButton#navButton:checked { background:#FBE9ED; color:#B30C28; font-weight:700; }
QPushButton#navButton:focus { border:1px solid #C8102E; }
QPushButton#metricLink { text-align:left; border:0; background:transparent; font-size:12px; color:#667085; }
QLineEdit,QComboBox,QSpinBox { background:#FFFFFF; border:1px solid #C9CED8; border-radius:6px; padding:7px 9px; min-height:18px; selection-background-color:#F6C5CF; selection-color:#202124; }
QComboBox::drop-down { border:0; width:24px; }
QTableView,QTreeView,QListView,QPlainTextEdit,QTextBrowser { background:#FFFFFF; alternate-background-color:#F8F9FB; border:1px solid #D5DAE3; border-radius:6px; selection-background-color:#FBE9ED; selection-color:#202124; gridline-color:#E3E6EC; }
QTableView::item,QTreeView::item { padding:7px 5px; border:0; }
QTableView::item:selected,QTreeView::item:selected { background:#FBE9ED; color:#202124; }
QHeaderView::section { background:#F5F6F8; color:#333844; font-weight:600; padding:9px 7px; border:0; border-bottom:1px solid #D5DAE3; }
QPlainTextEdit { padding:8px; font-family:"DejaVu Sans Mono",monospace; font-size:12px; }
QTabWidget::pane { border:0; background:transparent; }
QTabBar::tab { background:#FFFFFF; color:#475467; border:1px solid #D5DAE3; border-radius:5px; padding:8px 12px; margin:0 4px 6px 0; }
QTabBar::tab:selected { background:#C8102E; border-color:#C8102E; color:white; }
QTabBar::tab:hover:!selected { background:#FBE9ED; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:#F1F3F6; width:10px; margin:0; }
QScrollBar::handle:vertical { background:#B9C0CD; border-radius:5px; min-height:25px; }
QScrollBar:horizontal { background:#F1F3F6; height:10px; margin:0; }
QScrollBar::handle:horizontal { background:#B9C0CD; border-radius:5px; min-width:25px; }
QScrollBar::add-line,QScrollBar::sub-line { height:0; width:0; }
QSplitter::handle { background:#E7EAF0; }
QSplitter::handle:horizontal { width:3px; }
QSplitter::handle:vertical { height:3px; }
QProgressBar { border:1px solid #D5DAE3; border-radius:5px; background:#EEF0F4; text-align:center; }
QProgressBar::chunk { background:#C8102E; border-radius:4px; }
QCheckBox { spacing:7px; }
QCheckBox::indicator { width:16px; height:16px; }
QToolTip { color:#FFFFFF; background:#303744; border:0; padding:6px; }
QMenu { background:#FFFFFF; border:1px solid #D5DAE3; padding:5px; }
QMenu::item { padding:7px 20px; }
QMenu::item:selected { background:#FBE9ED; }
)QSS");
}
void applyPrecisionTheme() {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window,QColor("#F7F8FA"));
    palette.setColor(QPalette::WindowText,QColor("#202124"));
    palette.setColor(QPalette::Base,Qt::white);
    palette.setColor(QPalette::AlternateBase,QColor("#F8F9FB"));
    palette.setColor(QPalette::Text,QColor("#202124"));
    palette.setColor(QPalette::Button,Qt::white);
    palette.setColor(QPalette::ButtonText,QColor("#202124"));
    palette.setColor(QPalette::Mid,QColor("#D5DAE3"));
    palette.setColor(QPalette::Highlight,QColor("#C8102E"));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::Link,QColor("#B30C28"));
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor("#9198A4"));
    palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor("#9198A4"));
    QApplication::setPalette(palette);
    qApp->setStyleSheet(precisionStyle());
}
}
