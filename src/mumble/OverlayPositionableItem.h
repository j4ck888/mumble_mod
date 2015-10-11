#ifndef MUMBLE_MUMBLE_OVERLAYPOSITIONABLEITEM_H
#define MUMBLE_MUMBLE_OVERLAYPOSITIONABLEITEM_H

#if QT_VERSION >= 0x050000
# include <QtWidgets/QGraphicsItem>
#else
# include <QtGui/QGraphicsItem>
#endif

class OverlayPositionableItem : public QObject, public QGraphicsPixmapItem {
	Q_OBJECT
	Q_DISABLE_COPY(OverlayPositionableItem);
public:
	OverlayPositionableItem(QRectF *posPtr, const bool isPositionable=false);
	virtual ~OverlayPositionableItem();
	void updateRender();
	void setItemVisible(const bool &visible);
private:
	const bool m_isPositionEditable;
	/// Float value between 0 and 1 where 0,0 is top left, and 1,1 is bottom right
	QRectF *m_position;
	QGraphicsEllipseItem *m_qgeiHandle;
	void createPositioningHandle();
	bool sceneEventFilter(QGraphicsItem *, QEvent *) Q_DECL_OVERRIDE;
private slots:
	void onMove();
};

#endif
