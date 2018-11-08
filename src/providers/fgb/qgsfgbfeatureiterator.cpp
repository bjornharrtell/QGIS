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

QgsFgbFeatureIterator::QgsFgbFeatureIterator(QgsFgbFeatureSource *source, bool ownSource, const QgsFeatureRequest &request)
  : QgsAbstractFeatureIteratorFromSource<QgsFgbFeatureSource>(source, ownSource, request)
{
  if (mRequest.destinationCrs().isValid() && mRequest.destinationCrs() != mSource->mCrs)
    mTransform = QgsCoordinateTransform( mSource->mCrs, mRequest.destinationCrs(), mRequest.transformContext());

  try
  {
    mFilterRect = filterRectToSourceCrs( mTransform );
  }
  catch (QgsCsException &)
  {
    // can't reproject mFilterRect
    close();
    return;
  }

  rewind();
}

QgsFgbFeatureIterator::~QgsFgbFeatureIterator()
{
  QgsDebugMsg("QgsFgbFeatureIterator destruction");
  close();
}

bool QgsFgbFeatureIterator::rewind()
{
  QgsDebugMsg("Rewind requested");

  if (mClosed)
    return false;

  if (mRequest.filterType() == QgsFeatureRequest::FilterFid)
    mFetchedFid = false;
  else if (mFile != nullptr)
    mDataStream->device()->seek(mSource->mFeatureOffset);

  mFeaturePos = 0;
  mIndexPos = 0;

  return true;
}

bool QgsFgbFeatureIterator::close()
{
  if (mClosed)
    return false;

  iteratorClosed();

  if (mFile != nullptr) {
    delete mDataStream;
    mFile->close();
    delete mFile;
    mFile = nullptr;
  }

  mFeaturePos = 0;
  mIndexPos = 0;
  mClosed = true;
  return true;
}

bool QgsFgbFeatureIterator::fetchFeature( QgsFeature &feature )
{
  feature.setValid( false );

  if (mClosed)
    return false;

  if (mRequest.filterType() == QgsFeatureRequest::FilterFid) {
    bool res = readFid( feature );
    close();
    if (res)
      geometryToDestinationCrs( feature, mTransform );
    return res;
  }

  if (mFile == nullptr) {
    QgsDebugMsg("Opening file due to iteration start");
    mFile = mSource->getFile();
    if ( !mFile->open( QIODevice::ReadOnly ) ) {
      mFile = nullptr;
      QgsLogger::warning( QObject::tr( "Couldn't open the data source" ) );
      return false;
    }
    mDataStream = mSource->getDataStream(mFile);
  }

  if (mIndexPos == 0) {
    QgsDebugMsg(QString("Index search for %1").arg(mFilterRect.toString()));
    mIndices = mSource->mProvider->mTree->search(
      mFilterRect.xMinimum(),
      mFilterRect.yMinimum(),
      mFilterRect.xMaximum(),
      mFilterRect.yMaximum());
    QgsDebugMsg(QString("Got %1 indices in tree search").arg(mIndices.size()));
    if (mIndices.size() == 0)
      return false;
  }

  auto i = mIndices[mIndexPos];

  //QgsDebugMsg(QString("mIndices[mIndexPos] %1 ").arg(mIndices[mIndexPos]));
  //auto featureOffset = mSource->mProvider->mFeatureOffsets[i];
  auto featureOffset = mSource->mProvider->mFeatureOffsets[mSource->mProvider->mTree->getIndex(i)];
  //QgsDebugMsg(QString("featureOffset %1 ").arg(featureOffset));
  //QgsDebugMsg(QString("featureOffset 0 %1 ").arg(mSource->mProvider->mFeatureOffsets[0]));
  //QgsDebugMsg(QString("featureOffset 1 %1 ").arg(mSource->mProvider->mFeatureOffsets[1]));
  //QgsDebugMsg(QString("mSource->mFeatureOffset %1 ").arg(mSource->mFeatureOffset));
  //QgsDebugMsg(QString("device pos %1 ").arg(mDataStream->device()->pos()));
  auto res = mDataStream->device()->seek(mSource->mFeatureOffset + featureOffset);

  if (!res) {
    QgsDebugMsg(QString("Unexpected seek failure (offset %1)").arg(mSource->mFeatureOffset + featureOffset));
    return false;
  }

  uint32_t featureSize;
  mDataStream->readRawData((char*) &featureSize, 4);
  char *featureBuf = new char[featureSize];
  mDataStream->readRawData(featureBuf, featureSize);
  const uint8_t * vBuf = const_cast<const uint8_t *>(reinterpret_cast<uint8_t *>(featureBuf));

  // TODO: only on debug build
  Verifier v(vBuf, featureSize);
  auto ok = VerifyFeatureBuffer(v);
  if (!ok) {
    QgsDebugMsg(QString("VerifyFeatureBuffer says not ok"));
    QgsDebugMsg(QString("featureOffset: %1").arg(featureOffset));
    QgsDebugMsg(QString("mIndexPos: %1").arg(mIndexPos));
    QgsDebugMsg(QString("featureSize: %1").arg(featureSize));
    close();
    return false;
  }

  auto f = GetRoot<Feature>(featureBuf);
  auto geometry = f->geometry();
  auto qgsAbstractGeometry = toQgsAbstractGeometry(geometry);
  delete featureBuf;

  QgsGeometry qgsGeometry(qgsAbstractGeometry);
  feature.setGeometry(qgsGeometry);

  Rect rect {
    mFilterRect.xMinimum(),
    mFilterRect.yMinimum(),
    mFilterRect.xMaximum(),
    mFilterRect.yMaximum()
  };

  // TODO: only on debug build
  if (!rect.intersects(mSource->mProvider->mTree->getRect(mSource->mProvider->mTree->getIndex(i)))) {
    QgsDebugMsg(QString("!mFilterRect.intersects getRects"));
    QgsDebugMsg(QString("featureOffset: %1").arg(featureOffset));
    QgsDebugMsg(QString("mIndexPos: %1").arg(mIndexPos));
    QgsDebugMsg(QString("i: %1").arg(i));
    QgsDebugMsg(QString("featureSize: %1").arg(featureSize));
    close();
    return false;
  }
  // TODO: only on debug build
  if (!mFilterRect.intersects(qgsGeometry.boundingBox())) {
    QgsDebugMsg(QString("!mFilterRect.intersects"));
    QgsDebugMsg(QString("featureOffset: %1").arg(featureOffset));
    QgsDebugMsg(QString("mIndexPos: %1").arg(mIndexPos));
    QgsDebugMsg(QString("featureSize: %1").arg(featureSize));
    close();
    return false;
  }

  /*if (mFeaturePos >= mSource->mFeatureCount-1 || mDataStream->atEnd() ) {
    QgsLogger::debug("At end, closing iterator");
    close();
    return false;
  }
  mFeaturePos++;*/

  if (++mIndexPos >= mIndices.size()) {
    QgsDebugMsg(QString("Iteration end at mIndexPos %1").arg(mIndexPos));
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
    case GeometryType::Point: {
      auto point = new QgsPoint(coords[0], coords[1]);
      return point;
    }
    case GeometryType::Polygon: {
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
    default: {
      QgsLogger::warning("Unknown geometry type");
      return nullptr;
    }
  }
}

bool QgsFgbFeatureIterator::readFid( QgsFeature &feature )
{
  if (mFetchedFid)
    return false;

  mFetchedFid = true;
  QgsFeatureId fid = mRequest.filterFid();

  return false;
}

QgsFgbFeatureSource::QgsFgbFeatureSource( const QgsFgbProvider *p )
  : mFileName( p->mFileName )
  , mFeatureOffset( p->mFeatureOffset )
  , mFeatureCount( p->mFeatureCount )
  , mWkbType( p->mWkbType )
  , mGeometryType( p->mGeometryType )
  , mFields( p->attributeFields )
  , mCrs( p->crs() )
  , mProvider (p)
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
  return QgsFeatureIterator(new QgsFgbFeatureIterator(this, false, request));
}
