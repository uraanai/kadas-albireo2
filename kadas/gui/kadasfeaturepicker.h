/***************************************************************************
    kadasfeaturepicker.h
    --------------------
    copyright            : (C) 2019 by Sandro Mani
    email                : smani at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KADASFEATUREPICKER_H
#define KADASFEATUREPICKER_H

#include <QRectF>

#include <qgis/qgsfeature.h>
#include <qgis/qgspallabeling.h>
#include <qgis/qgswkbtypes.h>

#include <kadas/gui/kadas_gui.h>
#include <kadas/gui/kadasitemlayer.h>

class QgsMapLayer;
class QgsMapCanvas;
class KadasItemLayer;

class KADAS_GUI_EXPORT KadasFeaturePicker
{
  public:
    class PickResult
    {
      public:
        bool isEmpty() const { return layer == nullptr; }

        QgsMapLayer *layer = nullptr;
        const QgsAbstractGeometry *geom = nullptr;
        QgsCoordinateReferenceSystem crs;
        QgsFeature feature;
        KadasItemLayer::ItemId itemId = KadasItemLayer::ITEM_ID_NULL;
    };

    static PickResult pick( const QgsMapCanvas *canvas, const QgsPointXY &mapPos, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::UnknownGeometry );
#ifndef SIP_RUN
    [[deprecated( "Use variant without canvasPos instead" )]]
#endif
    static PickResult pick( const QgsMapCanvas *canvas, const QPoint &canvasPos, const QgsPointXY &mapPos, QgsWkbTypes::GeometryType geomType );

  private:
    static PickResult pickItemLayer( KadasItemLayer *layer, const QgsMapCanvas *canvas, const KadasMapPos &mapPos );
    static PickResult pickVectorLayer( QgsVectorLayer *vlayer, const QgsMapCanvas *canvas, const QgsPointXY &mapPos, QgsWkbTypes::GeometryType geomType );
};

#endif // KADASFEATUREPICKER_H

