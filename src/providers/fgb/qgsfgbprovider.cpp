/***************************************************************************
  qgsfgbprovider.cpp - Data provider for FlatGeobuf files

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

#include <algorithm>
#include <limits>
#include <cstring>
#include <cmath>

// Changed #include <qapp.h> to <qapplication.h>. Apparently some
// debian distros do not include the qapp.h wrapper and the compilation
// fails. [gsherman]
#include <QApplication>
#include <QFile>
#include <QTextCodec>
#include <QTextStream>
#include <QObject>

#include "qgis.h"
#include "qgsapplication.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsdataprovider.h"
#include "qgsfeature.h"
#include "qgsfields.h"
#include "qgsgeometry.h"
#include "qgslogger.h"
#include "qgsrectangle.h"

#include "qgsfgbfeatureiterator.h"
#include "qgsfgbprovider.h"

#ifdef HAVE_GUI
#include "qgssourceselectprovider.h"
#include "qgsfgbsourceselect.h"
#endif


const QString FGB_KEY = QStringLiteral( "fgb" );

const QString FGB_DESCRIPTION = QObject::tr( "FlatGeobuf format provider" );


QgsFGBProvider::QgsFGBProvider( const QString &uri, const ProviderOptions &options )
  : QgsVectorDataProvider( uri, options )
{
  // we always use UTF-8
  setEncoding( QStringLiteral( "utf8" ) );

  // get the file name and the type parameter from the URI
  int fileNameEnd = uri.indexOf( '?' );
  if ( fileNameEnd == -1 || uri.mid( fileNameEnd + 1, 5 ) != QLatin1String( "type=" ) )
  {
    QgsLogger::warning( tr( "Bad URI - you need to specify the feature type." ) );
    return;
  }
  QString typeStr = uri.mid( fileNameEnd + 6 );

  // TODO: set up the attributes and the geometry type depending on the feature type

  mFileName = uri.left( fileNameEnd );

  // TODO: parse the file

  mValid = true;
}


QgsFGBProvider::~QgsFGBProvider()
{
  // TODO: cleanup
}


QgsAbstractFeatureSource *QgsFGBProvider::featureSource() const
{
  return new QgsFGBFeatureSource( this );
}


QString QgsFGBProvider::storageType() const
{
  return tr( "FlatGeobuf file" );
}


// Return the extent of the layer
QgsRectangle QgsFGBProvider::extent() const
{
  // TODO: impl
  return QgsRectangle();
}


/**
 * Returns the feature type
 */
QgsWkbTypes::Type QgsFGBProvider::wkbType() const
{
  // TODO: impl
  return QgsWkbTypes::Point;
  // return QgsWkbTypes::LineString;
  // return QgsWkbTypes::Unknown;
}


/**
 * Returns the feature count
 */
long QgsFGBProvider::featureCount() const
{
  // TODO: impl
  return 0;
}


QgsFields QgsFGBProvider::fields() const
{
  return attributeFields;
}


bool QgsFGBProvider::isValid() const
{
  return mValid;
}


QgsFeatureIterator QgsFGBProvider::getFeatures( const QgsFeatureRequest &request ) const
{
  return QgsFeatureIterator( new QgsFGBFeatureIterator( new QgsFGBFeatureSource( this ), true, request ) );
}


QString QgsFGBProvider::name() const
{
  return FGB_KEY;
} // QgsFGBProvider::name()


QString QgsFGBProvider::description() const
{
  return FGB_DESCRIPTION;
} // QgsFGBProvider::description()


QgsCoordinateReferenceSystem QgsFGBProvider::crs() const
{
  return QgsCoordinateReferenceSystem( GEOSRID, QgsCoordinateReferenceSystem::PostgisCrsId ); // use WGS84
}


/**
 * Class factory to return a pointer to a newly created
 * QgsFGBProvider object
 */
QGISEXTERN QgsFGBProvider *classFactory( const QString *uri, const QgsDataProvider::ProviderOptions &options )
{
  return new QgsFGBProvider( *uri, options );
}


/**
 * Required key function (used to map the plugin to a data store type)
*/
QGISEXTERN QString providerKey()
{
  return FGB_KEY;
}


/**
 * Required description function
 */
QGISEXTERN QString description()
{
  return FGB_DESCRIPTION;
}


/**
 * Required isProvider function. Used to determine if this shared library
 * is a data provider plugin
 */
QGISEXTERN bool isProvider()
{
  return true;
}



#ifdef HAVE_GUI

class QgsFgbSourceSelectProvider : public QgsSourceSelectProvider
{
  public:

    QString providerKey() const override { return QStringLiteral( "fgb" ); }
    QString text() const override { return QObject::tr( "Vector" ); }
    int ordering() const override { return QgsSourceSelectProvider::OrderLocalProvider + 20; }
    QIcon icon() const override { return QgsApplication::getThemeIcon( QStringLiteral( "/mActionAddVectorLayer.svg" ) ); }
    QgsAbstractDataSourceWidget *createDataSourceWidget( QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::Widget, QgsProviderRegistry::WidgetMode widgetMode = QgsProviderRegistry::WidgetMode::Embedded ) const override
    {
      return new QgsFgbSourceSelect( parent, fl, widgetMode );
    }
};

QGISEXTERN QList<QgsSourceSelectProvider *> *sourceSelectProviders()
{
  QList<QgsSourceSelectProvider *> *providers = new QList<QgsSourceSelectProvider *>();

  *providers
      << new QgsFgbSourceSelectProvider;

  return providers;
}

#endif
