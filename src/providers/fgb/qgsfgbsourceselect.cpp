/***************************************************************************
  qgsfgbsourceselect.cpp - QgsFgbSourceSelect

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
#include "qgsfgbsourceselect.h"
#include "qgsvectorlayer.h"
#include "qgsquerybuilder.h"
#include "qgssettings.h"

#include <QMessageBox>

QgsFgbSourceSelect::QgsFgbSourceSelect(QWidget *parent, Qt::WindowFlags fl, QgsProviderRegistry::WidgetMode theWidgetMode )
  : QgsAbstractDataSourceWidget( parent, fl, theWidgetMode )
{
  setupUi( this );

  setupButtons( buttonBox );

  QgsSettings settings;

  connect( mFileWidget, &QgsFileWidget::fileChanged, this, [ = ]( const QString & path )
  {
    mPath = path;
    emit enableButtons( ! mPath.isEmpty() );
  } );
}

QgsFgbSourceSelect::~QgsFgbSourceSelect()
{
  QgsSettings settings;
}

void QgsFgbSourceSelect::addButtonClicked()
{
  if (mPath.isEmpty()) {
      QMessageBox::information( this,
                                tr( "Add vector layer" ),
                                tr( "No path selected." ) );
      return;
  }
  else {
    emit addVectorLayer(mPath, NULL, QStringLiteral( "fgb" ) );
  }
}

void QgsFgbSourceSelect::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/opening_data.html#FlatGeobuf-layer" ) );
}

