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
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgsexception.h"

#include "flatgeobuf_generated.h"

#include <limits>
#include <cstring>


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
    mDataStream = new QDataStream(mFile);
    mDataStream->skipRawData(60);
  }

  uint32_t featureSize;
  mDataStream->readRawData((char*) &featureSize, 4);
  char* featureBuf = new char[featureSize];
  mDataStream->readRawData(featureBuf, featureSize);

  auto f = flatbuffers::GetRoot<FlatGeobuf::Feature>(featureBuf);
  auto geometry = f->geometry();
  auto x = geometry->coords()->Get(0);
  auto y = geometry->coords()->Get(1);

  delete featureBuf;

  //auto x = ((double) rand() / (RAND_MAX)) + 1;
  //auto y = ((double) rand() / (RAND_MAX)) + 1;
  auto p = new QgsPoint(QgsWkbTypes::Point, x, y);
  QgsGeometry g(p);
  feature.setGeometry(g);

  if ( mDataStream->atEnd() )
  {
    QgsLogger::debug("At end, closing iterator");
    close();
    return false;
  }

  return true;
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

QgsFeatureIterator QgsFgbFeatureSource::getFeatures( const QgsFeatureRequest &request )
{
  return QgsFeatureIterator( new QgsFgbFeatureIterator( this, false, request ) );
}
