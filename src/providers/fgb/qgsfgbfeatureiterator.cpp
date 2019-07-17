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

  /*if (mIndexPos == 0) {
    QgsDebugMsg(QString("Index search for %1").arg(mFilterRect.toString()));
    mIndices = mSource->mProvider->mTree->search(
      mFilterRect.xMinimum(),
      mFilterRect.yMinimum(),
      mFilterRect.xMaximum(),
      mFilterRect.yMaximum());
    QgsDebugMsg(QString("Got %1 indices in tree search").arg(mIndices.size()));
    if (mIndices.size() == 0)
      return false;
  } else if (mIndexPos >= mIndices.size()) {
    QgsDebugMsg(QString("Iteration end at mIndexPos %1").arg(mIndexPos));
    close();
    return false;
  }*/

  //auto i = mIndices[mIndexPos];
  //auto featureOffset = mSource->mProvider->mFeatureOffsets[i];

  /*
  QgsDebugMsg(QString("mIndices[mIndexPos] %1 ").arg(mIndices[mIndexPos]));
  QgsDebugMsg(QString("featureOffset %1 ").arg(featureOffset));
  QgsDebugMsg(QString("featureOffset 0 %1 ").arg(mSource->mProvider->mFeatureOffsets[0]));
  QgsDebugMsg(QString("featureOffset 1 %1 ").arg(mSource->mProvider->mFeatureOffsets[1]));
  QgsDebugMsg(QString("mSource->mFeatureOffset %1 ").arg(mSource->mFeatureOffset));
  QgsDebugMsg(QString("device pos %1 ").arg(mDataStream->device()->pos()));
  */

  //auto res = mDataStream->device()->seek(mSource->mFeatureOffset + featureOffset);
  bool res;
  if (mFeaturePos == 0)
    res = mDataStream->device()->seek(mSource->mFeatureOffset);

  /*if (!res) {
    QgsDebugMsg(QString("Unexpected seek failure (offset %1)").arg(mSource->mFeatureOffset + featureOffset));
    return false;
  }*/

  uint32_t featureSize;
  mDataStream->readRawData((char*) &featureSize, 4);
  char *featureBuf = new char[featureSize];
  mDataStream->readRawData(featureBuf, featureSize);

  // TODO: only on debug build
  /*const uint8_t *vBuf = const_cast<const uint8_t *>(reinterpret_cast<uint8_t *>(featureBuf));
  Verifier v(vBuf, featureSize);
  auto ok = VerifyFeatureBuffer(v);
  if (!ok) {
    QgsDebugMsg(QString("VerifyFeatureBuffer says not ok"));
    QgsDebugMsg(QString("featureOffset: %1").arg(featureOffset));
    QgsDebugMsg(QString("mIndexPos: %1").arg(mIndexPos));
    QgsDebugMsg(QString("featureSize: %1").arg(featureSize));
    close();
    return false;
  }*/

  auto f = GetRoot<Feature>(featureBuf);
  auto qgsAbstractGeometry = readGeometry(f, 2);
  feature.setId(f->fid());

  delete[] featureBuf;
  QgsGeometry qgsGeometry(qgsAbstractGeometry);
  feature.setGeometry(qgsGeometry);

  // TODO: only on debug build
  /*
  Rect rect {
    mFilterRect.xMinimum(),
    mFilterRect.yMinimum(),
    mFilterRect.xMaximum(),
    mFilterRect.yMaximum()
  };
  auto fRect = mSource->mProvider->mTree->getRect(mSource->mProvider->mTree->getIndex(i));
  if (!rect.intersects(fRect)) {
    QgsDebugMsg(QString("Filter rect does not intersect feature rect"));
    QgsRectangle r(fRect.minX, fRect.minY, fRect.maxX, fRect.maxY);
    QgsDebugMsg(QString("mFilterRect rect: %1").arg(mFilterRect.toString()));
    QgsDebugMsg(QString("Index rect: %1").arg(r.toString()));
    QgsDebugMsg(QString("Index rect: %1,%2 : %3,%4").arg(fRect.minX, 0, 'f').arg(fRect.minY, 0, 'f').arg(fRect.maxX, 0, 'f').arg(fRect.maxY, 0, 'f'));
    QgsDebugMsg(QString("Feature rect: %1").arg(qgsGeometry.boundingBox().toString()));
    QgsDebugMsg(QString("Feature geom: %1").arg(qgsGeometry.asPoint().toString()));
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
  */

  if (mFeaturePos >= mSource->mProvider->mFeatureCount-1 || mDataStream->atEnd() ) {
    QgsLogger::debug("At end, closing iterator");
    close();
    return false;
  }
  mFeaturePos++;

  //mIndexPos++;
  return true;
}

QgsLineString *QgsFgbFeatureIterator::readLineString(const double *xy, uint32_t xyLength, uint8_t dimensions, uint32_t offset)
{
  auto dimLength = xyLength >> 1;
  auto x = QVector<double>(dimLength);
  auto xd = x.data();
  auto y = QVector<double>(dimLength);
  auto yd = y.data();
  auto z = QVector<double>();
  auto m = QVector<double>();
  unsigned int c = 0;
  for (std::vector<double>::size_type i = offset; i < offset + xyLength; i = i + 2) {
    xd[c] = xy[i];
    yd[c] = xy[i+1];
    c++;
  }
  auto qgsLineString = new QgsLineString(x, y, z, m, false);
  return qgsLineString;
}

QgsPolygon *QgsFgbFeatureIterator::readPolygon(const double *xy, uint32_t xyLength, const Vector<uint32_t> *ends, uint8_t dimensions)
{
  QVector<QgsCurve *> rings;

  if (ends != nullptr) {
    size_t offset = 0;
    for (size_t i = 0; i < ends->size(); i++) {
      auto end = ends->Get(i) << 1;
      auto ring = readLineString(xy, end, dimensions, offset);
      rings.append(ring);
      offset += end;
    }
  } else {
    auto ring = readLineString(xy, xyLength, dimensions);
    rings.append(ring);
  }

  auto polygon = new QgsPolygon();
  polygon->setExteriorRing(rings.at(0));
  if (rings.size() > 1) {
    const QVector<QgsCurve *> interiorRings = rings.mid(1);
    polygon->setInteriorRings(interiorRings);
  }
  return polygon;
}

QgsAbstractGeometry* QgsFgbFeatureIterator::readGeometry(const Feature* feature, uint8_t dimensions)
{
  auto xy = feature->xy()->data();
  auto xyLength = feature->xy()->Length();
  switch (mSource->mGeometryType) {
    case GeometryType::Point:
      return new QgsPoint(xy[0], xy[1]);
    case GeometryType::LineString:
      return readLineString(xy, xyLength, dimensions);
    case GeometryType::Polygon:
      return readPolygon(xy, xyLength, feature->ends(), dimensions);
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
