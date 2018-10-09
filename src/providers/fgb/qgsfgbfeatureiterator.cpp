/***************************************************************************
  QgsFgbFeatureIterator.cpp

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

#include "qgsfgbfeatureiterator.h"
#include "qgsfgbprovider.h"

#include "qgsapplication.h"
#include "qgsgeometry.h"
#include "qgslinestring.h"
#include "qgspolygon.h"
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgsexception.h"

#include "flatgeobuf_generated.h"

#include <limits>
#include <cstring>

using namespace flatbuffers;
using namespace FlatGeobuf;

QgsFgbFeatureIterator::QgsFgbFeatureIterator( QgsFgbFeatureSource *source, bool ownSource, const QgsFeatureRequest &request )
  : QgsAbstractFeatureIteratorFromSource<QgsFgbFeatureSource>( source, ownSource, request )
{
  if ( mRequest.destinationCrs().isValid() && mRequest.destinationCrs() != mSource->mCrs )
  {
    mTransform = QgsCoordinateTransform( mSource->mCrs, mRequest.destinationCrs(), mRequest.transformContext() );
  }
  try
  {
    mFilterRect = filterRectToSourceCrs( mTransform );
  }
  catch ( QgsCsException & )
  {
    // can't reproject mFilterRect
    close();
    return;
  }

  rewind();
}

QgsFgbFeatureIterator::~QgsFgbFeatureIterator()
{
  close();
}

bool QgsFgbFeatureIterator::rewind()
{
  if ( mClosed )
    return false;

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    mFetchedFid = false;
  }
  else
  {
    mFile = nullptr;
  }

  return true;
}

bool QgsFgbFeatureIterator::close()
{
  if ( mClosed )
    return false;

  iteratorClosed();

  if (mFile) {
    delete mDataStream;
    mFile->close();
    delete mFile;
  }

  mClosed = true;
  return true;
}

bool QgsFgbFeatureIterator::fetchFeature( QgsFeature &feature )
{
  feature.setValid( false );

  if ( mClosed )
    return false;

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    bool res = readFid( feature );
    close();
    if ( res )
      geometryToDestinationCrs( feature, mTransform );
    return res;
  }

  if ( !mFile )
  {
    QgsLogger::debug("FGB: Opening file");
    mFile = mSource->getFile();
    if ( !mFile->open( QIODevice::ReadOnly ) )
    {
      mFile = nullptr;
      QgsLogger::warning( QObject::tr( "Couldn't open the data source" ) );
      return false;
    }
    mDataStream = mSource->getDataStream(mFile);
  }

  uint32_t featureSize;
  mDataStream->readRawData((char*) &featureSize, 4);
  char* featureBuf = new char[featureSize];
  mDataStream->readRawData(featureBuf, featureSize);
  auto f = GetRoot<Feature>(featureBuf);
  auto geometry = f->geometry();
  auto qgsAbstractGeometry = toQgsAbstractGeometry(geometry);
  delete featureBuf;

  QgsGeometry qgsGeometry(qgsAbstractGeometry);
  feature.setGeometry(qgsGeometry);

  if ( mDataStream->atEnd() )
  {
    QgsLogger::debug("At end, closing iterator");
    close();
    return false;
  }

  return true;
}

QgsAbstractGeometry* QgsFgbFeatureIterator::toQgsAbstractGeometry(const Geometry* geometry)
{
  auto coords = geometry->coords()->data();
  auto coordsLength = geometry->coords()->Length();
  //auto lengths = geometry->lengths();
  //auto lengthsLength = lengths->Length();
  switch (mSource->mGeometryType) {
    case GeometryType::Point:
    {
      auto point = new QgsPoint(QgsWkbTypes::Point, coords[0], coords[1]);
      return point;
    }
    case GeometryType::Polygon:
    {
      auto ringLengths = geometry->ring_lengths();
      if (ringLengths != nullptr) {
        auto ringLengthsLength = ringLengths->Length();
        if (ringLengthsLength > 0)
          coordsLength = ringLengths->Get(0);
      }
      auto dimLength = coordsLength / 2;
      auto x = QVector<double>(dimLength);
      auto xd = x.data();
      auto y = QVector<double>(dimLength);
      auto yd = y.data();
      auto z = QVector<double>();
      auto m = QVector<double>();
      unsigned int c = 0;
      for (std::vector<double>::size_type i = 0; i < coordsLength; i = i + 2) {
        xd[c] = coords[i];
        yd[c] = coords[i+1];
        c++;
      }
      auto exteriorRing = new QgsLineString(x, y, z, m, false);
      auto polygon = new QgsPolygon();
      polygon->setExteriorRing(exteriorRing);
      return polygon;
    }
    default:
    {
      QgsLogger::warning( "Unknown geometry type");
      return nullptr;
    }
  }
}

bool QgsFgbFeatureIterator::readFid( QgsFeature &feature )
{
  if ( mFetchedFid )
    return false;

  mFetchedFid = true;
  QgsFeatureId fid = mRequest.filterFid();

  return false;
}

QgsFgbFeatureSource::QgsFgbFeatureSource( const QgsFgbProvider *p )
  : mFileName( p->mFileName )
  , mFeatureOffset( p->mFeatureOffset )
  , mWkbType( p->mWkbType )
  , mGeometryType( p->mGeometryType )
  , mFields( p->attributeFields )
  , mCrs( p->crs() )
{

}

QgsFgbFeatureSource::~QgsFgbFeatureSource()
{
}

QFile* QgsFgbFeatureSource::getFile()
{
  return new QFile(mFileName);
}

QDataStream* QgsFgbFeatureSource::getDataStream(QFile* file)
{
  auto dataStream = new QDataStream(file);
  dataStream->skipRawData(mFeatureOffset);
  return dataStream;
}

QgsFeatureIterator QgsFgbFeatureSource::getFeatures( const QgsFeatureRequest &request )
{
  return QgsFeatureIterator( new QgsFgbFeatureIterator( this, false, request ) );
}
