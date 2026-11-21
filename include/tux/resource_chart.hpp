// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <utility>
class ResourceHistoryChart final : public QWidget {
public:
    ResourceHistoryChart(QString title, QString unit, double fixedMaximum, QWidget *parent = nullptr)
        : QWidget(parent), title_(std::move(title)), unit_(std::move(unit)), fixedMaximum_(fixedMaximum)
    {
        setMinimumSize(260, 150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    int sampleCount() const {return samples_.size();}
    void clear() {samples_.clear(); update();}
    void addSample(double value)
    {
        samples_.append(qMax(0.0, value));
        if (samples_.size() > 90) samples_.remove(0, samples_.size() - 90);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRect area = rect().adjusted(1, 1, -1, -1);
        painter.setPen(palette().color(QPalette::Mid));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(area, 8, 8);

        const double current = samples_.isEmpty() ? 0.0 : samples_.back();
        painter.setPen(palette().color(QPalette::Text));
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(QRect(14, 9, width() - 28, 24), Qt::AlignLeft | Qt::AlignVCenter, title_);
        QFont valueFont = painter.font();
        valueFont.setBold(false);
        painter.setFont(valueFont);
        const QString value = samples_.isEmpty() ? QStringLiteral("Waiting for sample") : QStringLiteral("%1 %2").arg(current, 0, 'f', unit_ == QStringLiteral("%") ? 0 : 1).arg(unit_);
        painter.drawText(QRect(14, 9, width() - 28, 24), Qt::AlignRight | Qt::AlignVCenter, value);

        const QRectF plot = QRectF(area).adjusted(14, 38, -14, -13);
        const QColor gridColor = palette().color(QPalette::Mid);
        painter.setPen(QPen(gridColor, 1, Qt::DotLine));
        for (int i = 0; i <= 3; ++i) {
            const qreal y = plot.top() + plot.height() * i / 3.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        if (samples_.size() < 2 || plot.width() <= 0 || plot.height() <= 0) return;
        double maximum = fixedMaximum_;
        if (maximum <= 0.0) {
            maximum = 1.0;
            for (double sample : samples_) maximum = qMax(maximum, sample);
            maximum *= 1.2;
        }
        QPainterPath line;
        for (qsizetype i = 0; i < samples_.size(); ++i) {
            const qreal x = plot.left() + plot.width() * static_cast<qreal>(i) / static_cast<qreal>(samples_.size() - 1);
            const qreal y = plot.bottom() - plot.height() * qMin(samples_.at(i), maximum) / maximum;
            if (i == 0) line.moveTo(x, y); else line.lineTo(x, y);
        }
        QPainterPath fill = line;
        fill.lineTo(plot.bottomRight());
        fill.lineTo(plot.bottomLeft());
        fill.closeSubpath();
        QColor accent = palette().color(QPalette::Highlight);
        QColor translucent = accent;
        translucent.setAlpha(42);
        painter.fillPath(fill, translucent);
        painter.setPen(QPen(accent, 2));
        painter.drawPath(line);
    }

private:
    QString title_;
    QString unit_;
    double fixedMaximum_ = 0.0;
    QVector<double> samples_;
};

