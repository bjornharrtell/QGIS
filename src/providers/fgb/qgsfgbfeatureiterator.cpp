/***************************************************************************
  qgsfgbfeatureiterator.cpp

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

#include <limits>
#include <cstring>


QgsFGBFeatureIterator::QgsFGBFeatureIterator( QgsFGBFeatureSource *source, bool ownSource, const QgsFeatureRequest &request )
  : QgsAbstractFeatureIteratorFromSource<QgsFGBFeatureSource>( source, ownSource, request )
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

QgsFGBFeatureIterator::~QgsFGBFeatureIterator()
{
  close();
}

bool QgsFGBFeatureIterator::rewind()
{
  if ( mClosed )
    return false;

  if ( mRequest.filterType() == QgsFeatureRequest::FilterFid )
  {
    mFetchedFid = false;
  }
  else
  {
    // TODO: reset iterator
  }

  return true;
}

bool QgsFGBFeatureIterator::close()
{
  if ( mClosed )
    return false;

  iteratorClosed();

  mClosed = true;
  return true;
}

bool QgsFGBFeatureIterator::fetchFeature( QgsFeature &feature )
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

  // TODO: impl

  close();
  return false;
}

bool QgsFGBFeatureIterator::readFid( QgsFeature &feature )
{
  if ( mFetchedFid )
    return false;

  mFetchedFid = true;
  QgsFeatureId fid = mRequest.filterFid();



  return false;
}

// ------------

QgsFGBFeatureSource::QgsFGBFeatureSource( const QgsFGBProvider *p )
  : mFileName( p->mFileName )
  , mFields( p->attributeFields )
  , mCrs( p->crs() )
{

}

QgsFGBFeatureSource::~QgsFGBFeatureSource()
{
}

QgsFeatureIterator QgsFGBFeatureSource::getFeatures( const QgsFeatureRequest &request )
{
  return QgsFeatureIterator( new QgsFGBFeatureIterator( this, false, request ) );
}
