/***************************************************************************
  QgsFgbProvider.cpp - Data provider for FlatGeobuf files

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
#include <QDataStream>
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


QgsFgbProvider::QgsFgbProvider( const QString &uri, const ProviderOptions &options )
  : QgsVectorDataProvider( uri, options )
{
  // we always use UTF-8
  setEncoding( QStringLiteral( "utf8" ) );

  // TODO: parse the file
  mFileName = uri;

  QFile file( uri );
  if ( !file.open( QIODevice::ReadOnly ) )
  {
    QgsLogger::warning( QObject::tr( "Couldn't open the data source: %1" ).arg( uri ) );
    return;
  }

  auto dataStream = new QDataStream(&file);
  uint32_t headerSize;
  dataStream->readRawData((char*) &headerSize, 4);
  char* headerBuf = new char[headerSize];
  dataStream->readRawData(headerBuf, headerSize);

  auto header = flatbuffers::GetRoot<FlatGeobuf::Header>(headerBuf);
  mFeatureCount = header->features_count();
  mGeometryType = header->geometry_type();
  mWkbType = toWkbType(mGeometryType);
  mFeatureOffset = headerSize + 4;

  delete headerBuf;

  mValid = true;
}

QgsWkbTypes::Type QgsFgbProvider::toWkbType(GeometryType geometryType) {
  switch (geometryType)
  {
    case GeometryType::Point:
      return QgsWkbTypes::Point;
    case GeometryType::LineString:
      return QgsWkbTypes::LineString;
    case GeometryType::Polygon:
      return QgsWkbTypes::Polygon;
    default:
      return QgsWkbTypes::Unknown;
  }
}


QgsFgbProvider::~QgsFgbProvider()
{
}


QgsAbstractFeatureSource *QgsFgbProvider::featureSource() const
{
  return new QgsFgbFeatureSource( this );
}


QString QgsFgbProvider::storageType() const
{
  return tr( "FlatGeobuf file" );
}


// Return the extent of the layer
QgsRectangle QgsFgbProvider::extent() const
{
  // TODO: impl
  return QgsRectangle();
}


/**
 * Returns the feature type
 */
QgsWkbTypes::Type QgsFgbProvider::wkbType() const
{
  return mWkbType;
}


/**
 * Returns the feature count
 */
long QgsFgbProvider::featureCount() const
{
  return mFeatureCount;
}


QgsFields QgsFgbProvider::fields() const
{
  return attributeFields;
}


bool QgsFgbProvider::isValid() const
{
  return mValid;
}


QgsFeatureIterator QgsFgbProvider::getFeatures( const QgsFeatureRequest &request ) const
{
  return QgsFeatureIterator( new QgsFgbFeatureIterator( new QgsFgbFeatureSource( this ), true, request ) );
}


QString QgsFgbProvider::name() const
{
  return FGB_KEY;
} // QgsFgbProvider::name()


QString QgsFgbProvider::description() const
{
  return FGB_DESCRIPTION;
} // QgsFgbProvider::description()


QgsCoordinateReferenceSystem QgsFgbProvider::crs() const
{
  return QgsCoordinateReferenceSystem( GEOSRID, QgsCoordinateReferenceSystem::PostgisCrsId ); // use WGS84
}


/**
 * Class factory to return a pointer to a newly created
 * QgsFgbProvider object
 */
QGISEXTERN QgsFgbProvider *classFactory( const QString *uri, const QgsDataProvider::ProviderOptions &options )
{
  return new QgsFgbProvider( *uri, options );
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
