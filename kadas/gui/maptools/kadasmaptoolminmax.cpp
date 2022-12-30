/***************************************************************************
    kadasmaptoolminmax.cpp
    ------------------------
    copyright            : (C) 2022 by Sandro Mani
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

#include <gdal.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QToolButton>

#include <qgis/qgsgeometry.h>
#include <qgis/qgsgeometrycollection.h>
#include <qgis/qgsmapcanvas.h>
#include <qgis/qgsmapmouseevent.h>
#include <qgis/qgsrasterlayer.h>
#include <qgis/qgssettings.h>

#include <kadas/core/kadas.h>
#include <kadas/analysis/kadasninecellfilter.h>
#include <kadas/gui/mapitems/kadascircleitem.h>
#include <kadas/gui/mapitems/kadaspolygonitem.h>
#include <kadas/gui/mapitems/kadasrectangleitem.h>
#include <kadas/gui/mapitems/kadassymbolitem.h>
#include <kadas/gui/maptools/kadasmaptoolminmax.h>
#include <kadas/gui/kadasfeaturepicker.h>
#include <kadas/gui/kadasmapcanvasitemmanager.h>


KadasMapToolMinMax::KadasMapToolMinMax( QgsMapCanvas *mapCanvas )
  : KadasMapToolCreateItem( mapCanvas, itemFactory( mapCanvas, FilterRect ) )
{
  setCursor( Qt::ArrowCursor );
  setToolLabel( tr( "Compute min/max" ) );
  setUndoRedoVisible( false );
  connect( this, &KadasMapToolCreateItem::partFinished, this, &KadasMapToolMinMax::drawFinished );

  QWidget *filterTypeWidget = new QWidget();
  filterTypeWidget->setLayout( new QHBoxLayout() );
  filterTypeWidget->layout()->addWidget( new QLabel( tr( "Select area by:" ) ) );
  filterTypeWidget->layout()->setContentsMargins( 0, 0, 0, 0 );

  mFilterTypeCombo = new QComboBox();
  mFilterTypeCombo->addItem( tr( "Rectangle" ), KadasMapToolMinMax::FilterRect );
  mFilterTypeCombo->addItem( tr( "Polygon" ), KadasMapToolMinMax::FilterPoly );
  mFilterTypeCombo->addItem( tr( "Circle" ), KadasMapToolMinMax::FilterCircle );
  mFilterTypeCombo->setCurrentIndex( 0 );
  connect( mFilterTypeCombo, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this]( int )
  {
    setFilterType( static_cast<FilterType>( mFilterTypeCombo->currentData().toInt() ) );
  } );
  filterTypeWidget->layout()->addWidget( mFilterTypeCombo );

  QToolButton *pickButton = new QToolButton();
  pickButton->setIcon( QIcon( ":/kadas/icons/select" ) );
  pickButton->setToolTip( tr( "Pick existing geometry" ) );
  connect( pickButton, &QToolButton::clicked, this, &KadasMapToolMinMax::requestPick );
  filterTypeWidget->layout()->addWidget( pickButton );

  setExtraBottomBarContents( filterTypeWidget );
}

KadasMapToolMinMax::~KadasMapToolMinMax()
{
  if ( mPinMin )
  {
    KadasMapCanvasItemManager::removeItem( mPinMin );
    delete mPinMin.data();
  }
  if ( mPinMax )
  {
    KadasMapCanvasItemManager::removeItem( mPinMax );
    delete mPinMax.data();
  }
}

KadasMapToolCreateItem::ItemFactory KadasMapToolMinMax::itemFactory( const QgsMapCanvas *canvas, FilterType filterType ) const
{
  KadasMapToolCreateItem::ItemFactory factory = nullptr;
  switch ( filterType )
  {
    case FilterRect:
      factory = [ = ] { return new KadasRectangleItem( canvas->mapSettings().destinationCrs() ); };
      break;
    case FilterPoly:
      factory = [ = ] { return new KadasPolygonItem( canvas->mapSettings().destinationCrs() ); };
      break;
    case FilterCircle:
      factory = [ = ] { return new KadasCircleItem( canvas->mapSettings().destinationCrs() ); };
      break;
  }
  return factory;
}

void KadasMapToolMinMax::setFilterType( FilterType filterType )
{
  mFilterType = filterType;
  setItemFactory( itemFactory( mCanvas, mFilterType ) );
  mFilterTypeCombo->blockSignals( true );
  mFilterTypeCombo->setCurrentIndex( mFilterTypeCombo->findData( filterType ) );
  mFilterTypeCombo->blockSignals( false );
}
static inline double pixelToGeoX( double gtrans[6], double px, double py )
{
  return gtrans[0] + px * gtrans[1] + py * gtrans[2];
}

static inline double pixelToGeoY( double gtrans[6], double px, double py )
{
  return gtrans[3] + px * gtrans[4] + py * gtrans[5];
}

void KadasMapToolMinMax::drawFinished()
{
  QString layerid = QgsProject::instance()->readEntry( "Heightmap", "layer" );
  QgsMapLayer *layer = QgsProject::instance()->mapLayer( layerid );
  if ( !layer || layer->type() != QgsMapLayerType::RasterLayer )
  {
    emit messageEmitted( tr( "No heightmap is defined in the project. Right-click a raster layer in the layer tree and select it to be used as heightmap." ), Qgis::Warning );
    clear();
    return;
  }

  QgsCoordinateTransform crst( layer->crs(), mCanvas->mapSettings().destinationCrs(), layer->transformContext() );

  const KadasGeometryItem *item = static_cast<const KadasGeometryItem *>( currentItem() );
  QgsAbstractGeometry *geom = nullptr;
  if ( mFilterType == FilterCircle )
  {
    geom = item->geometry()->segmentize( M_PI / 22.5 );
  }
  else
  {
    geom = item->geometry()->clone();
  }
  geom->transform( crst, Qgis::TransformDirection::Reverse );
  QgsRectangle bbox = geom->boundingBox();
  QPolygonF filterGeom = QgsGeometry( geom ).asQPolygonF();

  GDALDatasetH inputDataset = Kadas::gdalOpenForLayer( static_cast<QgsRasterLayer *>( layer ) );
  double gtrans[6] = {};
  if ( inputDataset == NULL || GDALGetRasterCount( inputDataset ) < 1 || GDALGetGeoTransform( inputDataset, &gtrans[0] ) != CE_None )
  {
    GDALClose( inputDataset );
    clear();
    return;
  }
  GDALRasterBandH band = GDALGetRasterBand( inputDataset, 1 );

  //determine the window
  int rowStart, rowEnd, colStart, colEnd;
  if ( !KadasNineCellFilter::computeWindow( inputDataset, layer->crs(), bbox, layer->crs(), rowStart, rowEnd, colStart, colEnd ) )
  {
    GDALClose( inputDataset );
    clear();
    return;
  }

  double pixValues[256 * 256] = {};
  double valMin = std::numeric_limits<double>::max();
  double valMax = 0;
  int xMin = -1, xMax = -1;
  int yMin = -1, yMax = -1;
  // Read in 256x256 chunks
  for ( int x = colStart; x <= colEnd; x += 256 )
  {
    for ( int y = rowStart; y <= rowEnd; y += 256 )
    {
      if ( CE_None == GDALRasterIO( band, GF_Read, x, y, 256, 256, &pixValues[0], 256, 256, GDT_Float64, 0, 0 ) )
      {
        int maxx = std::min( 256, colEnd - x + 1 );
        int maxy = std::min( 256, rowEnd - y + 1 );
        #pragma omp parallel for schedule(static)
        for ( int bx = 0; bx < maxx; ++bx )
        {
          double localValMin = std::numeric_limits<double>::max();
          double localValMax = 0;
          int localxMin = -1, localxMax = -1;
          int localyMin = -1, localyMax = -1;

          for ( int by = 0; by < maxy; ++by )
          {
            double px = pixelToGeoX( gtrans, x + bx, y + by );
            double py = pixelToGeoY( gtrans, x + bx, y + by );
            if ( mFilterType != FilterRect && !filterGeom.containsPoint( QPointF( px, py ), Qt::WindingFill ) )
            {
              continue;
            }

            double val = pixValues[by * 256 + bx];
            if ( val < localValMin )
            {
              localValMin = val;
              localxMin = px;
              localyMin = py;
            }
            if ( val > localValMax )
            {
              localValMax = val;
              localxMax = px;
              localyMax = py;
            }
          }

          #pragma omp critical
          {
            if ( localValMin < valMin )
            {
              valMin = localValMin;
              xMin = localxMin;
              yMin = localyMin;
            }
            if ( localValMax > valMax )
            {
              valMax = localValMax;
              xMax = localxMax;
              yMax = localyMax;
            }
          }
        }
      }
    }
  }

  QgsPointXY pMin = crst.transform( xMin, yMin );
  QgsPointXY pMax = crst.transform( xMax, yMax );

  if ( !mPinMin )
  {
    mPinMin = new KadasSymbolItem( mCanvas->mapSettings().destinationCrs() );
    mPinMin->setup( ":/kadas/icons/tri_up", 0.5, 0 );
    KadasMapCanvasItemManager::addItem( mPinMin );
  }
  mPinMin->setPosition( KadasItemPos::fromPoint( pMin ) );

  if ( !mPinMax )
  {
    mPinMax = new KadasSymbolItem( mCanvas->mapSettings().destinationCrs() );
    mPinMax->setup( ":/kadas/icons/tri_down", 0.5, 1.0 );
    KadasMapCanvasItemManager::addItem( mPinMax );
  }
  mPinMax->setPosition( KadasItemPos::fromPoint( pMax ) );

  clear();
}

void KadasMapToolMinMax::requestPick()
{
  mPickFeature = true;
  setCursor( QCursor( Qt::CrossCursor ) );
}

void KadasMapToolMinMax::canvasPressEvent( QgsMapMouseEvent *e )
{
  if ( !mPickFeature )
  {
    KadasMapToolCreateItem::canvasPressEvent( e );
  }
}

void KadasMapToolMinMax::canvasMoveEvent( QgsMapMouseEvent *e )
{
  if ( !mPickFeature )
  {
    KadasMapToolCreateItem::canvasMoveEvent( e );
  }
}

void KadasMapToolMinMax::canvasReleaseEvent( QgsMapMouseEvent *e )
{
  if ( !mPickFeature )
  {
    KadasMapToolCreateItem::canvasReleaseEvent( e );
  }
  else
  {
    KadasFeaturePicker::PickResult pickResult = KadasFeaturePicker::pick( mCanvas, toMapCoordinates( e->pos() ), QgsWkbTypes::PolygonGeometry );
    if ( pickResult.geom && pickResult.geom->partCount() == 1 )
    {
      QgsAbstractGeometry *geom = dynamic_cast<QgsGeometryCollection *>( pickResult.geom ) ? static_cast<QgsGeometryCollection *>( pickResult.geom )->geometryN( 0 ) : pickResult.geom;
      if ( QgsWkbTypes::flatType( geom->wkbType() ) == QgsWkbTypes::CurvePolygon )
      {
        setFilterType( FilterCircle );
      }
      else
      {
        setFilterType( FilterPoly );
      }
      addPartFromGeometry( *pickResult.geom, pickResult.crs );
    }
    mPickFeature = false;
    setCursor( Qt::ArrowCursor );
  }
}
