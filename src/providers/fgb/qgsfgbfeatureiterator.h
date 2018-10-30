/***************************************************************************
  qgsfeatureiterator.cpp

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
#ifndef QGSFGBFEATUREITERATOR_H
#define QGSFGBFEATUREITERATOR_H

#include <QFile>
#include <QDataStream>

#include "qgsfeatureiterator.h"
#include "qgsfgbprovider.h"

class QgsFgbProvider;


class QgsFgbFeatureSource : public QgsAbstractFeatureSource
{
  public:
    explicit QgsFgbFeatureSource( const QgsFgbProvider *p );
    ~QgsFgbFeatureSource() override;

    QFile* getFile();
    QDataStream* getDataStream(QFile* file);
    QgsFeatureIterator getFeatures( const QgsFeatureRequest &request ) override;

  private:
    QString mFileName;
    uint32_t mFeatureOffset;
    uint64_t mFeatureCount;
    QgsWkbTypes::Type mWkbType;
    GeometryType mGeometryType;
    QgsFields mFields;
    QgsCoordinateReferenceSystem mCrs;

    friend class QgsFgbFeatureIterator;
};


class QgsFgbFeatureIterator : public QgsAbstractFeatureIteratorFromSource<QgsFgbFeatureSource>
{
  public:
    QgsFgbFeatureIterator( QgsFgbFeatureSource *source, bool ownSource, const QgsFeatureRequest &request );
    ~QgsFgbFeatureIterator() override;

    bool rewind() override;
    bool close() override;

  protected:
    bool fetchFeature( QgsFeature &feature ) override;

  private:
    QFile* mFile = nullptr;
    QDataStream* mDataStream;
    int mC=0;

    bool readFid( QgsFeature &feature );
    QgsAbstractGeometry* toQgsAbstractGeometry(const Geometry* geometry);

    bool mFetchedFid = false;

    uint64_t mFeatureCount;
    uint64_t mFeaturePos = 0;

    QgsCoordinateTransform mTransform;
    QgsRectangle mFilterRect;
};

#endif // QGSFGBFEATUREITERATOR_H
