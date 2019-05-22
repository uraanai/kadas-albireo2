/***************************************************************************
    kadasmaptoolcreateitem.h
    ------------------------
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

#ifndef KADASMAPTOOLCREATEITEM_H
#define KADASMAPTOOLCREATEITEM_H

#include <qgis/qgsmaptool.h>

#include <kadas/gui/kadas_gui.h>
#include <kadas/core/kadasstatehistory.h>
#include <kadas/core/mapitems/kadasmapitem.h>

class KadasFloatingInputWidget;
class KadasFloatingInputWidgetField;
class KadasItemLayer;

class KADAS_GUI_EXPORT KadasMapToolCreateItem : public QgsMapTool
{
  Q_OBJECT
public:
  typedef std::function<KadasMapItem*()> ItemFactory;


  KadasMapToolCreateItem(QgsMapCanvas* canvas, ItemFactory itemFactory, KadasItemLayer* layer);
  ~KadasMapToolCreateItem();

  void activate() override;
  void deactivate() override;

  void canvasPressEvent( QgsMapMouseEvent* e ) override;
  void canvasMoveEvent( QgsMapMouseEvent* e ) override;
  void canvasReleaseEvent( QgsMapMouseEvent* e ) override;
  void keyPressEvent( QKeyEvent *e ) override;

  KadasMapItem* currentItem() const{ return mItem; }

signals:
  void startedCreatingItem(KadasMapItem* item);
  void finishedCreatingItem(KadasMapItem* item);

private:
  ItemFactory mItemFactory = nullptr;
  KadasMapItem* mItem = nullptr;
  KadasItemLayer* mLayer = nullptr;

  KadasStateHistory* mStateHistory = nullptr;
  KadasFloatingInputWidget* mInputWidget = nullptr;
  bool mIgnoreNextMoveEvent = false;

  void createItem();
  void addPoint(const QgsPointXY& mapPos);
  void startItem(const QgsPointXY &pos);
  void startItem(const QList<double>& attributes);
  void finishItem();
  void commitItem();
  void cleanup();
  void reset();
  QList<double> collectAttributeValues() const;

private slots:
  void inputChanged();
  void acceptInput();
  void stateChanged(KadasStateHistory::State *state);

};

#endif // KADASMAPTOOLCREATEITEM_H