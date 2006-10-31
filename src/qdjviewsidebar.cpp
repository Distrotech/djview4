//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjView4
//C- Copyright (c) 2006  Leon Bottou
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C-  ------------------------------------------------------------------

// $Id$

#include "stdlib.h"
#include "math.h"

#include <QAbstractItemDelegate>
#include <QAbstractListModel>
#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDebug>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemDelegate>
#include <QList>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>

#include <libdjvu/ddjvuapi.h>
#include <libdjvu/miniexp.h>

#include "qdjvu.h"
#include "qdjvuwidget.h"
#include "qdjviewsidebar.h"
#include "qdjview.h"




// ----------------------------------------
// OUTLINE




QDjViewOutline::QDjViewOutline(QDjView *djview)
  : QWidget(djview), 
    djview(djview), 
    loaded(false)
{
  tree = new QTreeWidget(this);
  tree->setColumnCount(1);
  tree->setItemsExpandable(true);
  tree->setUniformRowHeights(true);
  tree->header()->hide();
  tree->header()->setStretchLastSection(true);
  tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree->setSelectionMode(QAbstractItemView::SingleSelection);
  tree->setTextElideMode(Qt::ElideRight);
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(0);
  layout->addWidget(tree);

  connect(tree, SIGNAL(itemActivated(QTreeWidgetItem*, int)),
          this, SLOT(itemActivated(QTreeWidgetItem*)) );
  connect(djview, SIGNAL(documentClosed(QDjVuDocument*)),
          this, SLOT(clear()) );
  connect(djview, SIGNAL(documentOpened(QDjVuDocument*)),
          this, SLOT(clear()) );
  connect(djview, SIGNAL(documentReady(QDjVuDocument*)),
          this, SLOT(refresh()) );
  connect(djview->getDjVuWidget(), SIGNAL(pageChanged(int)),
          this, SLOT(pageChanged(int)) );
  connect(djview->getDjVuWidget(), SIGNAL(layoutChanged()),
          this, SLOT(refresh()) );
  
  setWhatsThis(tr("<html><b>Document outline.</b><br/> "
                  "This panel display the document outline, "
                  "or the page names when the outline is not available, "
                  "Double-click any entry to jump to the selected page."
                  "</html>"));
  
  if (djview->pageNum() > 0)
    refresh();
}


void 
QDjViewOutline::clear()
{
  tree->clear();
  loaded = false;
}

void 
QDjViewOutline::refresh()
{
  QDjVuDocument *doc = djview->getDocument();
  if (doc && !loaded && djview->pageNum()>0)
    {
      miniexp_t outline = doc->getDocumentOutline();
      if (outline == miniexp_dummy)
        return;
      loaded = true;
      if (outline)
        {
          if (!miniexp_consp(outline) ||
              miniexp_car(outline) != miniexp_symbol("bookmarks"))
            {
              QString msg = tr("Outline data is corrupted");
              qWarning((const char*)msg.toLocal8Bit());
            }
          tree->clear();
          QTreeWidgetItem *root = new QTreeWidgetItem();
          fillItems(root, miniexp_cdr(outline));
          QTreeWidgetItem *item = root;
          while (item->childCount()==1 &&
                 item->data(0,Qt::UserRole).toInt() >= 0)
            item = item->child(0);
          while (item->childCount() > 0)
            tree->insertTopLevelItem(tree->topLevelItemCount(),
                                     item->takeChild(0) );
          delete root;
        }
      else
        {
          tree->clear();
          QTreeWidgetItem *root = new QTreeWidgetItem(tree);
          root->setText(0, tr("Pages"));
          root->setFlags(0);
          root->setData(0, Qt::UserRole, -1);
          for (int pageno=0; pageno<djview->pageNum(); pageno++)
            {
              QTreeWidgetItem *item = new QTreeWidgetItem(root);
              QString name = djview->pageName(pageno);
              item->setText(0, tr("Page %1").arg(name));
              item->setData(0, Qt::UserRole, pageno);
              item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
              item->setToolTip(0, tr("Go to page %1").arg(name));
              item->setWhatsThis(0, whatsThis());
            }
          tree->setItemExpanded(root, true);
        }
      pageChanged(djview->getDjVuWidget()->page());
    }
}


void 
QDjViewOutline::fillItems(QTreeWidgetItem *root, miniexp_t expr)
{
  while(miniexp_consp(expr))
    {
      miniexp_t s = miniexp_car(expr);
      expr = miniexp_cdr(expr);
      if (miniexp_consp(s) &&
          miniexp_consp(miniexp_cdr(s)) &&
          miniexp_stringp(miniexp_car(s)) &&
          miniexp_stringp(miniexp_cadr(s)) )
        {
          // fill item
          const char *name = miniexp_to_str(miniexp_car(s));
          const char *page = miniexp_to_str(miniexp_cadr(s));
          QTreeWidgetItem *item = new QTreeWidgetItem(root);
          item->setText(0, QString::fromUtf8(name));
          int pageno = -1;
          if (page[0]=='#' && page[1]!='+' && page[1]!='-')
            pageno = djview->pageNumber(QString::fromUtf8(page));
          item->setData(0, Qt::UserRole, pageno);
          item->setFlags(0);
          item->setWhatsThis(0, whatsThis());
          if (pageno >= 0)
            {
              QString name = djview->pageName(pageno);
              item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
              item->setToolTip(0, tr("Go to page %1").arg(name));
            }
          // recurse
          fillItems(item, miniexp_cddr(s));
        }
    }
}


void 
QDjViewOutline::pageChanged(int pageno)
{
  int fp = -1;
  QTreeWidgetItem *fi = 0;
  // find current selection
  QList<QTreeWidgetItem*> sel = tree->selectedItems();
  QTreeWidgetItem *si = 0;
  if (sel.size() == 1)
    si = sel[0];
  // current selection has priority
  if (si)
    searchItem(si, pageno, fi, fp);
  // search
  for (int i=0; i<tree->topLevelItemCount(); i++)
    searchItem(tree->topLevelItem(i), pageno, fi, fp);
  // select
  if (si && fi && si != fi)
    tree->setItemSelected(si, false);
  if (fi && si != fi)
    {
      tree->setCurrentItem(fi);
      tree->setItemSelected(fi, true);
      tree->scrollToItem(fi);
    }
}


void 
QDjViewOutline::searchItem(QTreeWidgetItem *item, int pageno, 
                           QTreeWidgetItem *&fi, int &fp)
{
  int page = item->data(0, Qt::UserRole).toInt();
  if (page>=0 && page<=pageno && page>fp)
    {
      fi = item;
      fp = page;
    }
  for (int i=0; i<item->childCount(); i++)
    searchItem(item->child(i), pageno, fi, fp);
}


void 
QDjViewOutline::itemActivated(QTreeWidgetItem *item)
{
  int pageno = item->data(0, Qt::UserRole).toInt();
  if (pageno >= 0 && pageno < djview->pageNum())
    djview->goToPage(pageno);
}








// ----------------------------------------
// THUMBNAILS


class QDjViewThumbnails::Model : public QAbstractListModel
{
  Q_OBJECT
public:
  ~Model();
  Model(QDjViewThumbnails*);
  virtual QModelIndex index(int row, 
                            int column = 0, 
                            const QModelIndex &p = QModelIndex()) const;
  virtual int rowCount(const QModelIndex &parent) const;
  virtual QVariant data(const QModelIndex &index, int role) const;
  virtual int flags(QModelIndex &index) const;
  int getSize() { return size; }
  int getSmart() { return smart; }
public slots:
  void setSize(int);
  void setSmart(bool);
  void scheduleRefresh();
protected slots:
  void documentClosed(QDjVuDocument *doc);
  void documentReady(QDjVuDocument *doc);
  void thumbnail(int);
  void refresh();
private:
  QDjView *djview;
  QDjViewThumbnails *widget;
  QStringList names;
  ddjvu_format_t *format;
  QIcon icon;
  int size;
  bool smart;
  bool refreshScheduled;
  int  pageInProgress;
  QIcon makeIcon(int pageno) const;
  QSize makeHint(int pageno) const;
};


class QDjViewThumbnails::View : public QListView
{
  Q_OBJECT
public:
  View(QDjViewThumbnails *widget);
protected:
  QStyleOptionViewItem viewOptions() const;
private:
  QDjViewThumbnails *widget;
  QDjView *djview;
};


QDjViewThumbnails::QDjViewThumbnails(QDjView *djview)
  : QWidget(djview),
    djview(djview)
{
  model = new Model(this);
  selection = new QItemSelectionModel(model);
  view = new View(this);
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(0);
  layout->addWidget(view);
  
  connect(djview->getDjVuWidget(), SIGNAL(pageChanged(int)),
          this, SLOT(pageChanged(int)) );
  connect(view, SIGNAL(activated(const QModelIndex&)),
          this, SLOT(activated(const QModelIndex&)) );

  menu = new QMenu(this);
  QActionGroup *group = new QActionGroup(this);
  QAction *action;

  action = menu->addAction(tr("Tiny","thumbnail menu"));
  connect(action,SIGNAL(triggered()),this,SLOT(setSize()) );
  action->setCheckable(true);
  action->setActionGroup(group);
  action->setData(32);
  action = menu->addAction(tr("Small","thumbnail menu"));
  connect(action,SIGNAL(triggered()),this,SLOT(setSize()) );
  action->setCheckable(true);  
  action->setActionGroup(group);
  action->setData(64);
  action = menu->addAction(tr("Medium","thumbnail menu"));
  connect(action,SIGNAL(triggered()),this,SLOT(setSize()) );
  action->setCheckable(true);
  action->setActionGroup(group);
  action->setData(96);
  action = menu->addAction(tr("Large","thumbnail menu"));
  connect(action,SIGNAL(triggered()),this,SLOT(setSize()) );
  action->setCheckable(true);
  action->setActionGroup(group);
  action->setData(160);
  menu->addSeparator();
  action = menu->addAction(tr("Smart","thumbnail menu"));
  connect(action,SIGNAL(toggled(bool)),this,SLOT(setSmart(bool)) );
  action->setCheckable(true);
  action->setData(true);
  updateActions();

#if Q_WS_MAC
  QString mc = tr("Control Left Mouse Button");
#else
  QString mc = tr("Right Mouse Button");
#endif
  setWhatsThis(tr("<html><b>Document thumbnails.</b><br/> "
                  "This panel display thumbnails for the document pages. "
                  "Double click a thumbnail to jump to the selected page. "
                  "%1 to change the thumbnail size or the refresh mode. "
                  "The smart refresh mode only computes thumbnails "
                  "when the page data is present (displayed or cached.)"
                  "</html>").arg(mc) );
}


void 
QDjViewThumbnails::updateActions(void)
{
  QAction *action;
  int size = model->getSize();
  bool smart = model->getSmart();
  foreach(action, menu->actions())
    {
      QVariant data = action->data();
      if (data.type() == QVariant::Bool)
        action->setChecked(smart);
      else
        action->setChecked(data.toInt() == size);
    }
}


void 
QDjViewThumbnails::pageChanged(int pageno)
{
  if (pageno>=0 && pageno<djview->pageNum())
    {
      QModelIndex mi = model->index(pageno);
      if (! selection->isSelected(mi))
        selection->select(mi, QItemSelectionModel::ClearAndSelect);
      view->scrollTo(mi);
    }
}


void 
QDjViewThumbnails::activated(const QModelIndex &index)
{
  if (index.isValid())
    {
      int pageno = index.row();
      if (pageno>=0 && pageno<djview->pageNum())
        djview->goToPage(pageno);
    }
}


int 
QDjViewThumbnails::size()
{
  return model->getSize();
}


void 
QDjViewThumbnails::setSize(int size)
{
  model->setSize(size);
  updateActions();
}


void 
QDjViewThumbnails::setSize()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action)
    setSize(action->data().toInt());
}


bool 
QDjViewThumbnails::smart()
{
  return model->getSmart();
}


void 
QDjViewThumbnails::setSmart(bool smart)
{
  model->setSmart(smart);
  updateActions();
}

void 
QDjViewThumbnails::contextMenuEvent(QContextMenuEvent *event)
{
  menu->exec(event->globalPos());
  event->accept();
}


// ----------------------------------------
// THUMBNAILS MODEL


QDjViewThumbnails::Model::~Model()
{
  if (format)
    ddjvu_format_release(format);
}


QDjViewThumbnails::Model::Model(QDjViewThumbnails *widget)
  : QAbstractListModel(widget),
    djview(widget->djview), 
    widget(widget), 
    format(0),
    size(0),
    smart(true),
    refreshScheduled(false),
    pageInProgress(-1)
{
  // create format
  unsigned int masks[4] = { 0xff0000, 0xff00, 0xff, 0xff000000 };
  format = ddjvu_format_create(DDJVU_FORMAT_RGBMASK32, 3, masks);
  ddjvu_format_set_row_order(format, true);
  ddjvu_format_set_y_direction(format, true);
  ddjvu_format_set_ditherbits(format, QPixmap::defaultDepth());
  // set size
  setSize(64);
  // connect
  connect(djview, SIGNAL(documentClosed(QDjVuDocument*)),
          this, SLOT(documentClosed(QDjVuDocument*)) );
  connect(djview, SIGNAL(documentReady(QDjVuDocument*)),
          this, SLOT(documentReady(QDjVuDocument*)) );
  // update
  if (djview->pageNum() > 0)
    documentReady(djview->getDocument());
}


QModelIndex 
QDjViewThumbnails::Model::index(int row, int, const QModelIndex&) const
{
  return createIndex(row, 0);
}


void 
QDjViewThumbnails::Model::documentClosed(QDjVuDocument *doc)
{
  names.clear();
  disconnect(doc, 0, this, 0);
  emit layoutChanged();
}


void 
QDjViewThumbnails::Model::documentReady(QDjVuDocument *doc)
{
  names.clear();
  int pagenum = djview->pageNum();
  for (int pageno=0; pageno<pagenum; pageno++)
    names << djview->pageName(pageno);
  connect(doc, SIGNAL(thumbnail(int)),
          this, SLOT(thumbnail(int)) );
  connect(doc, SIGNAL(pageinfo()),
          this, SLOT(scheduleRefresh()) );
  connect(doc, SIGNAL(idle()),
          this, SLOT(scheduleRefresh()) );
  emit layoutChanged();
  widget->pageChanged(djview->getDjVuWidget()->page());
}


void 
QDjViewThumbnails::Model::thumbnail(int pageno)
{
  QModelIndex mi = index(pageno);
  emit dataChanged(mi, mi);
  scheduleRefresh();
}


void
QDjViewThumbnails::Model::scheduleRefresh()
{
  if (! refreshScheduled)
    QTimer::singleShot(0, this, SLOT(refresh()));
  refreshScheduled = true;
}


void
QDjViewThumbnails::Model::refresh()
{
  QDjVuDocument *doc = djview->getDocument();
  ddjvu_status_t status;
  refreshScheduled = false;
  if (doc && pageInProgress >= 0)
    {
      status = ddjvu_thumbnail_status(*doc, pageInProgress, 0);
      if (status >= DDJVU_JOB_OK)
        pageInProgress = -1;
    }
  if (doc && pageInProgress < 0)
    {
      QRect dr = widget->view->rect();
      for (int i=0; i<names.size(); i++)
        {
          QModelIndex mi = index(i);
          if (dr.intersects(widget->view->visualRect(mi)))
            {
              status = ddjvu_thumbnail_status(*doc, i, 0);
              if (status == DDJVU_JOB_NOTSTARTED)
                {
                  if (smart && !ddjvu_document_check_pagedata(*doc, i))
                    continue;
                  status = ddjvu_thumbnail_status(*doc, i, 1);
                  if (status == DDJVU_JOB_STARTED)
                    {
                      pageInProgress = i;
                      break;
                    }
                }
            }
        }
    }
}


void 
QDjViewThumbnails::Model::setSmart(bool b)
{
  if (b != smart)
    {
      smart = b;
      scheduleRefresh();
    }
}


void 
QDjViewThumbnails::Model::setSize(int newSize)
{
  newSize = qBound(16, newSize, 256);
  if (newSize != size)
    {
      size = newSize;
      QPixmap pixmap(size,size);
      pixmap.fill();
      QPainter painter;
      int s8 = size/8;
      if (s8 >= 1)
        {
          QPolygon poly;
          poly << QPoint(s8,0)
               << QPoint(size-2*s8,0) 
               << QPoint(size-s8-1,s8) 
               << QPoint(size-s8-1,size-1) 
               << QPoint(s8,size-1);
          QPainter painter(&pixmap);
          painter.setBrush(Qt::NoBrush);
          painter.setPen(Qt::darkGray);
          painter.drawPolygon(poly);
        }
      icon = QIcon(pixmap);
    }
  emit layoutChanged();
}


QIcon
QDjViewThumbnails::Model::makeIcon(int pageno) const
{
  QDjVuDocument *doc = djview->getDocument();
  if (doc)
    {
      // render thumbnail
      int w = size;
      int h = size;
      QImage img(size, size, QImage::Format_RGB32);
      if (ddjvu_thumbnail_render(*doc, pageno, &w, &h, format, 
                                 img.bytesPerLine(), (char*)img.bits() ) )
        {
          QPixmap pixmap(size,size);
          pixmap.fill();
          QPoint dst((size-w)/2, (size-h)/2);
          QRect src(0,0,w,h);
          QPainter painter;
          painter.begin(&pixmap);
          painter.drawImage(dst, img, src);
          painter.setBrush(Qt::NoBrush);
          painter.setPen(Qt::darkGray);
          painter.drawRect(dst.x(), dst.y(), w-1, h-1);
          painter.end();
          return QIcon(pixmap);
        }
#if QT_VERSION >= 0x40100
      // we know this is a repaint request because size hint
      // requests are taken care of by Qt::SizeHintRole.
      else if (ddjvu_thumbnail_status(*doc,pageno,0)==DDJVU_JOB_NOTSTARTED)
        const_cast<Model*>(this)->scheduleRefresh();
#endif
    }
  return icon;
}


QSize 
QDjViewThumbnails::Model::makeHint(int) const
{
  QFontMetrics metrics(widget->view->font());
  return QSize(size, size+metrics.height());
}


int 
QDjViewThumbnails::Model::rowCount(const QModelIndex &) const
{
  return names.size();
}


QVariant 
QDjViewThumbnails::Model::data(const QModelIndex &index, int role) const
{
  if (index.isValid())
    {
      int pageno = index.row();
      if (pageno>=0 && pageno<names.size())
        {
          switch(role)
            {
            case Qt::DisplayRole: 
            case Qt::ToolTipRole:
              return names[pageno];
            case Qt::DecorationRole:
              return makeIcon(pageno);
            case Qt::WhatsThisRole:
              return widget->whatsThis();
            case Qt::UserRole:
              return pageno;
#if QT_VERSION >= 0x040100
            case Qt::SizeHintRole:
              return makeHint(pageno);
#endif
            default:
              break;
            }
        }
    }
  return QVariant();
}


int 
QDjViewThumbnails::Model::flags(QModelIndex&) const
{
  return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
}



// ----------------------------------------
// THUMBNAILS VIEW


QDjViewThumbnails::View::View(QDjViewThumbnails *widget)
  : QListView(widget), 
    widget(widget), 
    djview(widget->djview)
{
  setModel(widget->model);
  setSelectionModel(widget->selection);
  setDragEnabled(false);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setTextElideMode(Qt::ElideRight);
  setViewMode(QListView::IconMode);
  setFlow(QListView::LeftToRight);
  setWrapping(true);
  setMovement(QListView::Static);
  setResizeMode(QListView::Adjust);
  setLayoutMode(QListView::Batched);
  setSpacing(8);
#if QT_VERSION >= 0x040100
  setUniformItemSizes(true);
#endif
#if QT_VERSION < 0x040100
  // hack to request thumbnail computations.
  connect((QObject*)verticalScrollBar(), SIGNAL(sliderMoved(int)),
          (QObject*)widget->model, SLOT(scheduleRefresh()) );
#endif
}


QStyleOptionViewItem 
QDjViewThumbnails::View::viewOptions() const
{
  int size = widget->model->getSize();
  QStyleOptionViewItem opt = QListView::viewOptions();
  opt.decorationAlignment = Qt::AlignCenter;
  opt.decorationPosition = QStyleOptionViewItem::Top;
  opt.decorationSize = QSize(size, size);
  opt.displayAlignment = Qt::AlignCenter;
  return opt;
}





// ----------------------------------------
// MOC

#include "qdjviewsidebar.moc"




/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ( "\\sw+_t" "[A-Z]\\sw*[a-z]\\sw*" )
   End:
   ------------------------------------------------------------- */