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

#include "header_generated.h"
#include "feature_generated.h"

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
    GeometryType mGeometryType;
    QgsFields mFields;
    QgsCoordinateReferenceSystem mCrs;

    const QgsFgbProvider *mProvider;

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

    bool readFid( QgsFeature &feature );
    QgsLineString *readLineString(const double *xy, uint32_t xyLength, uint32_t offset = 0);
    QgsPolygon *readPolygon(const double *xy, uint32_t xyLength, const Vector<uint32_t> *ends);
    QgsAbstractGeometry* readGeometry(const Feature* geometry);

    bool mFetchedFid = false;

    uint64_t mFeaturePos = 0;
    uint64_t mIndexPos = 0;
    std::vector<uint64_t> mIndices;

    QgsCoordinateTransform mTransform;
    QgsRectangle mFilterRect;
};

#endif // QGSFGBFEATUREITERATOR_H
