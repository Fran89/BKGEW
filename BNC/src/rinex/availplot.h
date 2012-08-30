#ifndef AVAILPLOT_H
#define AVAILPLOT_H

#include <QtCore>
#include <qwt_plot.h>
#include <qwt_symbol.h>

class t_availData;

class t_availPlot: public QwtPlot {
 Q_OBJECT

public:
  t_availPlot(QWidget* parent, QMap<QString, t_availData>* availDataMap);

private:
  void addCurve(const QString& name, const QwtSymbol* symbol,
                const QVector<double>& xData, const QVector<double>& yData);
};

#endif