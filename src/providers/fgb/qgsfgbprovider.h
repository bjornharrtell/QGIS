/***************************************************************************
  QgsFgbProvider.h - Data provider for FlatGeobuf files

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

#ifndef QgsFgbProvider_H
#define QgsFgbProvider_H

#include "qgsvectordataprovider.h"
#include "qgsfields.h"

class QgsFeature;
class QgsField;
class QFile;
class QDomDocument;

class QgsFgbFeatureIterator;

/**
\class QgsFgbProvider
\brief Data provider for FlatGeobuf files
* This provider adds the ability to load FlatGeobuf files as vector layers.
*
*/
class QgsFgbProvider : public QgsVectorDataProvider
{
    Q_OBJECT

  public:
    explicit QgsFgbProvider( const QString &uri, const QgsDataProvider::ProviderOptions &options );
    ~QgsFgbProvider() override;

    /* Functions inherited from QgsVectorDataProvider */
    QgsAbstractFeatureSource *featureSource() const override;
    QString storageType() const override;
    QgsFeatureIterator getFeatures( const QgsFeatureRequest &request ) const override;
    QgsWkbTypes::Type wkbType() const override;
    long featureCount() const override;
    QgsFields fields() const override;

    /* Functions inherited from QgsDataProvider */
    QgsRectangle extent() const override;
    bool isValid() const override;
    QString name() const override;
    QString description() const override;
    QgsCoordinateReferenceSystem crs() const override;

  private:

    //! Fields
    QgsFields attributeFields;

    QString mFileName;

    bool mValid = false;

    friend class QgsFgbFeatureSource;
};

#endif
