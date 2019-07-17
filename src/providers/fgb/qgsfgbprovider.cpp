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

const QString QgsFgbProvider::TEXT_PROVIDER_KEY = QStringLiteral( "fgb" );
const QString QgsFgbProvider::TEXT_PROVIDER_DESCRIPTION = QStringLiteral( "FlatGeobuf provider" );

uint8_t magicbytes[] = { 0x66, 0x67, 0x62, 0x00, 0x66, 0x67, 0x62, 0x00 };

QgsFgbProvider::QgsFgbProvider( const QString &uri, const ProviderOptions &options )
  : QgsVectorDataProvider( uri, options )
{
  // we always use UTF-8
  setEncoding( QStringLiteral( "utf8" ) );

  mFileName = uri;

  QFile file( uri );
  if ( !file.open( QIODevice::ReadOnly ) ) {
    QgsLogger::warning( QObject::tr( "Couldn't open the data source: %1" ).arg( uri ) );
    return;
  }

  auto dataStream = new QDataStream(&file);
  char bytes[8];
  dataStream->readRawData(bytes, 8);
  if (memcmp(bytes, magicbytes, sizeof(magicbytes))) {
    QgsLogger::warning( QObject::tr( "%1 does not appear to be a FlatGeobuf file" ).arg( uri ) );
    return;
  }
  QgsDebugMsg("Magic bytes verified");

  uint32_t headerSize;
  dataStream->readRawData((char*) &headerSize, 4);
  QgsDebugMsg(QString("Header size is %1").arg(headerSize));

  char* headerBuf = new char[headerSize];
  dataStream->readRawData(headerBuf, headerSize);
  QgsDebugMsg("Header read");
  auto header = flatbuffers::GetRoot<FlatGeobuf::Header>(headerBuf);
  mFeatureCount = header->features_count();
  mGeometryType = header->geometry_type();
  mEnvelope = std::vector<double>(header->envelope()->data(), header->envelope()->data() + 4);
  mWkbType = toWkbType(mGeometryType);
  QgsDebugMsg("Header parsed");

  uint64_t treeSize = 0;
  if (header->index_node_size() == 16) {
    treeSize = PackedRTree::size(mFeatureCount);
    char *treeBuf = new char[treeSize];
    QgsDebugMsg(QString("Index size is %1").arg(treeSize));
    dataStream->readRawData(treeBuf, treeSize);
    mTree = new PackedRTree(treeBuf, mFeatureCount);
    QgsDebugMsg("Index parsed");
  }

  mFeatureOffsets = new uint64_t[mFeatureCount];
  dataStream->readRawData((char *) mFeatureOffsets, mFeatureCount * 8);
  QgsDebugMsg(QString("Feature offsets read (size %1)").arg(mFeatureCount * 8));

  mFeatureOffset = 8 + headerSize + 4 + treeSize + mFeatureCount * 8;

  delete dataStream;
  file.close();
  delete[] headerBuf;

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
  return QgsRectangle(mEnvelope[0], mEnvelope[1], mEnvelope[2], mEnvelope[3]);
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
  return TEXT_PROVIDER_KEY;
} // QgsFgbProvider::name()


QString QgsFgbProvider::description() const
{
  return TEXT_PROVIDER_KEY;
} // QgsFgbProvider::description()


QVariantMap QgsFgbProviderMetadata::decodeUri( const QString &uri )
{
  QVariantMap components;
  components.insert( QStringLiteral( "path" ), QUrl( uri ).toLocalFile() );
  return components;
}


QgsCoordinateReferenceSystem QgsFgbProvider::crs() const
{
  return QgsCoordinateReferenceSystem( GEOSRID, QgsCoordinateReferenceSystem::PostgisCrsId ); // use WGS84
}


QgsDataProvider *QgsFgbProviderMetadata::createProvider( const QString &uri, const QgsDataProvider::ProviderOptions &options )
{
  return new QgsFgbProvider( uri, options );
}


QGISEXTERN QgsProviderMetadata *providerMetadataFactory()
{
  return new QgsFgbProviderMetadata();
}


QgsFgbProviderMetadata::QgsFgbProviderMetadata():
  QgsProviderMetadata( QgsFgbProvider::TEXT_PROVIDER_KEY, QgsFgbProvider::TEXT_PROVIDER_DESCRIPTION )
{
}


#ifdef HAVE_GUI

class QgsFgbSourceSelectProvider : public QgsSourceSelectProvider
{
  public:

    QString providerKey() const override { return QStringLiteral( "fgb" ); }
    QString text() const override { return QObject::tr( "FlatGeobuf" ); }
    int ordering() const override { return QgsSourceSelectProvider::OrderLocalProvider + 20; }
    QIcon icon() const override { return QgsApplication::getThemeIcon( QStringLiteral( "/mActionAddFgbLayer.svg" ) ); }
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

class QgsFgbProviderGuiMetadata: public QgsProviderGuiMetadata
{
  public:
    QgsFgbProviderGuiMetadata()
      : QgsProviderGuiMetadata( QgsFgbProvider::TEXT_PROVIDER_KEY )
    {
    }

    QList<QgsSourceSelectProvider *> sourceSelectProviders() override
    {
      QList<QgsSourceSelectProvider *> providers;
      providers << new QgsFgbSourceSelectProvider;
      return providers;
    }
};


QGISEXTERN QgsProviderGuiMetadata *providerGuiMetadataFactory()
{
  return new QgsFgbProviderGuiMetadata();
}

#endif
