/***************************************************************************
  qgsfgbsourceselect.h - QgsFgbSourceSelect

 ---------------------
 begin                : October 2018
 copyright            : (C) 2018 by Björn Harrtell
 email                : bjorn at wololo org
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSFGBSOURCESELECT_H
#define QGSFGBSOURCESELECT_H

#include "ui_qgsfgbsourceselectbase.h"
#include "qgsguiutils.h"
#include "qgshelp.h"
#include "qgsabstractdatasourcewidget.h"

class QgsFgbSourceSelect: public QgsAbstractDataSourceWidget, private Ui::QgsFgbSourceSelectBase
{
    Q_OBJECT

  public:
    QgsFgbSourceSelect( QWidget *parent = nullptr, Qt::WindowFlags fl = QgsGuiUtils::ModalDialogFlags, QgsProviderRegistry::WidgetMode theWidgetMode = QgsProviderRegistry::WidgetMode::None );

    ~QgsFgbSourceSelect() override;

  public slots:

    void showHelp();

  private:
    QString mPath;
};

#endif // QGSFGBSOURCESELECT_H
