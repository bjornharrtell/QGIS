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

#include "qgsfeatureiterator.h"

#include "qgsfgbprovider.h"

class QgsFGBProvider;


class QgsFGBFeatureSource : public QgsAbstractFeatureSource
{
  public:
    explicit QgsFGBFeatureSource( const QgsFGBProvider *p );
    ~QgsFGBFeatureSource() override;

    QgsFeatureIterator getFeatures( const QgsFeatureRequest &request ) override;

  private:
    QString mFileName;
    QgsFields mFields;
    QgsCoordinateReferenceSystem mCrs;

    friend class QgsFGBFeatureIterator;
};


class QgsFGBFeatureIterator : public QgsAbstractFeatureIteratorFromSource<QgsFGBFeatureSource>
{
  public:
    QgsFGBFeatureIterator( QgsFGBFeatureSource *source, bool ownSource, const QgsFeatureRequest &request );

    ~QgsFGBFeatureIterator() override;

    bool rewind() override;
    bool close() override;

  protected:

    bool fetchFeature( QgsFeature &feature ) override;

  private:

    bool readFid( QgsFeature &feature );

    bool mFetchedFid = false;

    QgsCoordinateTransform mTransform;
    QgsRectangle mFilterRect;
};

#endif // QGSFGBFEATUREITERATOR_H
