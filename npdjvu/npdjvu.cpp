//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjView4
//C- Copyright (c) 2009-  Leon Bottou
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C-  ------------------------------------------------------------------

// $Id$

#include "config.h"
#include <locale.h>
#include <stdio.h>
#include <string.h>

// Qt includes
#include <QtGlobal>
#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QRegExp>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QVariant>
#include <QWidget>
#include <QX11EmbedWidget>

// Djview includes
#include "qdjview.h"
#include "qdjvu.h"

// Npdjvu includes
#include "npdjvu.h"
#include "qtbrowserplugin.h"
#include "qtbrowserplugin_p.h"


/* ------------------------------------------------------------- */


class QtNPForwarder: public QObject 
{
  Q_OBJECT
public:
  QtNPForwarder(QtNPInstance *instance, QObject *parent = 0);
public slots:
  void showStatus(QString message);
  void getUrl(QUrl url, QString target);
  void onChange();
private:
  QtNPInstance *instance;
};


struct QtNPStream {
  QUrl          url;
  QtNPInstance *instance;
  int           streamid;
  bool          started;
  bool          checked;
  bool          closed;
  QtNPStream(int streamid, QUrl url, QtNPInstance *instance);
  ~QtNPStream();
};


class QtNPDocument : public QDjVuDocument
{
  Q_OBJECT
public: 
  QtNPDocument(QtNPInstance *instance);
  virtual void newstream(int streamid, QString name, QUrl url);
private:
  QtNPInstance *instance;
};


/* ------------------------------------------------------------- */
/* Setup application */

static QApplication *theApp = 0;
static QDjVuContext *djvuContext = 0;
static bool verbose = true;

static void 
qtMessageHandler(QtMsgType type, const char *msg)
{
  switch (type) 
    {
    case QtFatalMsg:
      fprintf(stderr,"djview fatal error: %s\n", msg);
      abort();
    case QtCriticalMsg:
      fprintf(stderr,"djview critical error: %s\n", msg);
      break;
    case QtWarningMsg:
      if (verbose)
        fprintf(stderr,"djview: %s\n", msg);
      break;
    default:
      if (verbose)
        fprintf(stderr,"%s\n", msg);
      break;
    }
}


#ifdef Q_WS_X11
static bool x11EventFilter(void *message, long *)
{
  // We define a low level handler because we need to make
  // the window active before calling the QX11EmbedWidget event filter.
  XEvent *event = reinterpret_cast<XEvent*>(message);
  if (event->type == ButtonPress)
    {
      QWidget *w = QWidget::find(event->xbutton.window);
      if (w && qobject_cast<QX11EmbedWidget*>(w->window()))
        {
          QApplication *app = (QApplication*)QCoreApplication::instance();
          app->setActiveWindow(w->window());
        }
    }
  return false;
}
#endif

static void
addDirectory(QStringList &dirs, QString path)
{
  QString dirname = QDir::cleanPath(path);
  if (! dirs.contains(dirname))
    dirs << dirname;
}


static void
setupApplication(QtNPInstance *instance)
{
  // quick
  if (theApp)
    {
      qtns_initialize(instance);
      if (! djvuContext)
        djvuContext = new QDjVuContext;
      return;
    }
  // globals
  QApplication::setOrganizationName(DJVIEW_ORG);
  QApplication::setOrganizationDomain(DJVIEW_DOMAIN);
  QApplication::setApplicationName(DJVIEW_APP);
  qInstallMsgHandler(qtMessageHandler);
  // application
  qtns_initialize(instance);
  if (! djvuContext)
    djvuContext = new QDjVuContext;
  theApp = (QApplication*)QCoreApplication::instance();
#ifdef Q_WS_X11
  theApp->setEventFilter(x11EventFilter);
#endif
  // translators
  QTranslator *qtTrans = new QTranslator(theApp);
  QTranslator *djviewTrans = new QTranslator(theApp);
  // - determine preferred languages
  QStringList langs; 
  QString varLanguage = ::getenv("LANGUAGE");
  if (varLanguage.size())
    langs += varLanguage.toLower().split(":", QString::SkipEmptyParts);
#ifdef LC_MESSAGES
  QString varLcMessages = ::setlocale(LC_MESSAGES, 0);
  if (varLcMessages.size())
    langs += varLcMessages.toLower();
#else
# ifdef LC_ALL
  QString varLcMessages = ::setlocale(LC_ALL, 0);
  if (varLcMessages.size())
    langs += varLcMessages.toLower();
# endif
#endif
#ifdef Q_WS_MAC
  langs += QSettings(".", "globalPreferences")
    .value("AppleLanguages").toStringList();
#endif
  QString qtLocale =  QLocale::system().name();
  if (qtLocale.size())
    langs += qtLocale.toLower();
  // - determine potential directories
  QStringList dirs;
  QDir dir = QApplication::applicationDirPath();
  QString dirPath = dir.canonicalPath();
  addDirectory(dirs, dirPath);
#ifdef DIR_DATADIR
  QString datadir = DIR_DATADIR;
  addDirectory(dirs, datadir + "/djvu/djview4");
  addDirectory(dirs, datadir + "/djview4");
#endif
#ifdef Q_WS_MAC
  addDirectory(dirs, dirPath + "/Resources/$LANG.lproj");
  addDirectory(dirs, dirPath + "/../Resources/$LANG.lproj");
  addDirectory(dirs, dirPath + "/../../Resources/$LANG.lproj");
#endif
  addDirectory(dirs, dirPath + "/share/djvu/djview4");
  addDirectory(dirs, dirPath + "/share/djview4");
  addDirectory(dirs, dirPath + "/../share/djvu/djview4");
  addDirectory(dirs, dirPath + "/../share/djview4");
  addDirectory(dirs, dirPath + "/../../share/djvu/djview4");
  addDirectory(dirs, dirPath + "/../../share/djview4");
  addDirectory(dirs, "/usr/share/djvu/djview4");
  addDirectory(dirs, "/usr/share/djview4");
  addDirectory(dirs, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  // - load translators
  bool qtTransValid = false;
  bool djviewTransValid = false;
  foreach (QString lang, langs)
    {
      foreach (QString dir, dirs)
        {
          dir = dir.replace(QRegExp("\\$LANG(?!\\w)"),lang);
          QDir qdir(dir);
          if (! qtTransValid && qdir.exists())
            qtTransValid = qtTrans->load("qt_" + lang, dir, "_.-");
          if (! djviewTransValid && qdir.exists())
            djviewTransValid= djviewTrans->load("djview_" + lang, dir, "_.-");
        }
      if (lang == "en" || lang.startsWith("en_") || lang == "c")
        break;
    }
  // - install tranlators
  if (qtTransValid)
    theApp->installTranslator(qtTrans);
  if (djviewTransValid)
    theApp->installTranslator(djviewTrans);
}


void 
NPP_Shutdown()
{
  delete djvuContext;
  djvuContext = 0;
}



/* ------------------------------------------------------------- */
/* Forwarder */

QtNPForwarder::QtNPForwarder(QtNPInstance *instance, QObject *parent)
  : QObject(parent), instance(instance)
{
}

void 
QtNPForwarder::showStatus(QString message)
{
  message.replace(QRegExp("\\s"), " ");
  QByteArray utf8 = message.toUtf8();
  NPN_Status(instance->npp, utf8.constData());
}

void 
QtNPForwarder::getUrl(QUrl url, QString target)
{
  if (instance)
    {
      QByteArray bUrl = url.toEncoded();
      QByteArray bTarget = target.toUtf8();
      NPN_GetURL(instance->npp, bUrl.constData(), 
             bTarget.isEmpty() ? 0 : bTarget.constData());
    }
}

void 
QtNPForwarder::onChange()
{
  if (instance && !instance->onchange.type == NPVariant::String)
    {
      NPVariant res;
      NPString *code = &instance->onchange.value.stringValue;
      NPN_Evaluate(instance->npp, instance->npobject, code, &res);
      NPN_ReleaseVariantValue(&res);
    }
}



/* ------------------------------------------------------------- */
/* Stream */


QtNPStream::QtNPStream(int streamid, QUrl url, QtNPInstance *instance)
  : url(url), instance(instance), streamid(streamid), 
    started(false), checked(false), closed(false)
{
  instance->streams.insert(this);
}


QtNPStream::~QtNPStream()
{
  if (instance && instance->streams.contains(this))
    if (instance->document && started && !closed)
      ddjvu_stream_close(*instance->document, streamid, true);
  closed = true;
  started = checked = false;
  instance->streams.remove(this);
}



/* ------------------------------------------------------------- */
/* Document */


QtNPDocument::QtNPDocument(QtNPInstance *instance)
  : QDjVuDocument(true), instance(instance)
{
  QUrl docurl = QDjView::removeDjVuCgiArguments(instance->url);
  setUrl(djvuContext, docurl);
}

void
QtNPDocument::newstream(int streamid, QString, QUrl url)
{
  if (streamid > 0)
    {
      new QtNPStream(streamid, url, instance);
      QString message = tr("Requesting %1.").arg(url.toString());
      instance->forwarder->showStatus(message);
      instance->forwarder->getUrl(url, QString());
    }
}




/* ------------------------------------------------------------- */
/* Open/Close djview */


static void 
openInstance(QtNPInstance *instance)
{
  if (! instance->document && instance->url.isValid())
    {
      instance->document = new QtNPDocument(instance);
      instance->document->ref();
    }
  if (instance->document && instance->djview)
    if (! instance->djview->getDocument())
      {
        instance->djview->open(instance->document, instance->url);
        // restoreData(djview->getDjVuWidget(), instance->savedData);
        instance->djview->show();
      }
}


static void 
closeInstance(QtNPInstance *instance)
{
  if (instance->djview)
    {
      // instance->savedData = saveData(djview->getDjVuWidget());
      instance->djview->close();
      delete instance->djview;
      instance->djview = 0;
      instance->qt.object = 0;
      qtns_destroy(instance);
    }
}



/* ------------------------------------------------------------- */
/* NPP functions */


NPError 
NPP_New(NPMIMEType mime, NPP npp,
        uint16 mode, int16 argc, char* argn[],
        char* argv[], NPSavedData* saved)
{
  Q_UNUSED(mime)
  if (! npp)
    return NPERR_INVALID_INSTANCE_ERROR;
  QtNPInstance *instance = new QtNPInstance;
  if (! instance)
    return NPERR_OUT_OF_MEMORY_ERROR;
  // initialize
  npp->pdata = instance;
  instance->fMode = mode;
  instance->forwarder = 0;
  instance->document = 0;
  instance->djview = 0;
  instance->npobject = 0;
  instance->npp = npp;
  instance->qt.object = 0;
#ifdef Q_WS_MAC
  instance->rootWidget = 0;
#endif
  if (saved && saved->len)
    instance->savedData = QByteArray((const char*)saved->buf, saved->len);
  // arguments
  for (int i = 0; i < argc; i++) 
    {
      QString key = QString::fromUtf8(argn[i]);
      QString val = QString::fromUtf8(argv[i]);
      QString k = key.toLower();
      if (k == "flags")
        instance->args += val.split(QRegExp("\\s+"), QString::SkipEmptyParts);
      else
        instance->args += key + QString("=") + val;
    }    
  fprintf(stderr,"npdjvu: using the QtBrowserPlugin%s\n",
#if defined(Q_WS_X11)
          "/XEmbed"
#elif defined(Q_WS_MAC)
          "/Mac"
#elif defined(Q_WS_Win)
          "/Windows"
#else
          ""
#endif
          );
  return NPERR_NO_ERROR;
}


NPError 
NPP_Destroy(NPP npp, NPSavedData** save)
{
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  if (! instance)
    return NPERR_INVALID_INSTANCE_ERROR;
  closeInstance(instance);
  delete instance->forwarder;
  instance->forwarder = 0;
  if (instance->document)
    instance->document->deref();
  instance->document = 0;
  if (save && !instance->savedData.isEmpty())
    {
      int len = instance->savedData.size();
      void *data = NPN_MemAlloc(len);
      NPSavedData *saved = (NPSavedData*)NPN_MemAlloc(sizeof(NPSavedData));
      if (saved && data)
        {
          memcpy(data, instance->savedData.constData(), len);
          saved->len = len;
          saved->buf = data;
          *save = saved;
        }
    }
  instance->npp->pdata = 0;
  delete instance;
  return NPERR_NO_ERROR;
}


NPError 
NPP_SetWindow(NPP npp, NPWindow* window)
{
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  if (! instance)
    return NPERR_INVALID_INSTANCE_ERROR;
  QRect clipRect;
  if (window)
    {
      clipRect = QRect(window->clipRect.left, window->clipRect.top,
                       window->clipRect.right - window->clipRect.left,
                       window->clipRect.bottom - window->clipRect.top);
      instance->geometry = QRect(window->x, window->y, window->width, window->height);
    }
  // take a shortcut if all that was changed is the geometry
  if (instance->djview && window && 
      instance->window == (QtNPInstance::Widget)window->window) 
    {
      qtns_setGeometry(instance, instance->geometry, clipRect);
      return NPERR_NO_ERROR;
    }
  if (instance->djview)
    closeInstance(instance);
  if (! window)
    {
      instance->window = 0;
      return NPERR_NO_ERROR;
    }
  instance->window = (QtNPInstance::Widget)window->window;
  setupApplication(instance);
  if (! instance->forwarder)
    instance->forwarder = new QtNPForwarder(instance);
  QDjView::ViewerMode mode = (instance->fMode == NP_FULL) 
    ? QDjView::FULLPAGE_PLUGIN : QDjView::EMBEDDED_PLUGIN;
  QDjView *djview = new QDjView(*djvuContext, mode);
  djview->setWindowFlags(djview->windowFlags() & ~Qt::Window);
  djview->setAttribute(Qt::WA_DeleteOnClose, false);
  djview->setObjectName("npdjvu");
  instance->djview = djview;
  instance->qt.widget = djview;
  QObject::connect(djview, SIGNAL(pluginStatusMessage(QString)),
                   instance->forwarder, SLOT(showStatus(QString)) );
  QObject::connect(djview, SIGNAL(pluginGetUrl(QUrl,QString)),
                   instance->forwarder, SLOT(getUrl(QUrl,QString)) );
  QObject::connect(djview, SIGNAL(pluginOnChange()),
                   instance->forwarder, SLOT(onChange()) );
  qtns_embed(instance); 
  QEvent e(QEvent::EmbeddingControl);
  QApplication::sendEvent(instance->qt.widget, &e);
  if (!instance->qt.widget->testAttribute(Qt::WA_PaintOnScreen))
    instance->qt.widget->setAutoFillBackground(true);
  instance->qt.widget->raise();
  qtns_setGeometry(instance, instance->geometry, clipRect);
  while (instance->args.size() > 0)
    djview->parseArgument(instance->args.takeFirst());
  openInstance(instance);
  return NPERR_NO_ERROR;
}


NPError 
NPP_NewStream(NPP npp, NPMIMEType mime, NPStream* stream, 
              NPBool seekable, uint16 *stype)
{
  Q_UNUSED(mime)
  Q_UNUSED(seekable)
  Q_UNUSED(stype)
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  QUrl url = QUrl::fromEncoded(stream->url);
  if (instance)
    {
      if (! url.isValid() )
        {
          qWarning("npdjvu: invalid url '%s'.", stream->url);
          return NPERR_GENERIC_ERROR;
        }
      // first stream
      if (! instance->url.isValid())
        {
          instance->url = url;
          url = QDjView::removeDjVuCgiArguments(url);
          QtNPStream *s = new QtNPStream(0, url, instance);
          s->checked = true;
        }
      // search streams
      QtNPStream *npstream = 0;
      foreach(QtNPStream *s, instance->streams)
        if (!npstream && !s->started)
          if (s->url == url)
            npstream = s;
      // mark started stream
      if (npstream)
        {
          stream->pdata = npstream;
          npstream->started = true;
          if (npstream->streamid == 0)
            openInstance(instance);
        }
    }
  return NPERR_NO_ERROR;
}


NPError 
NPP_DestroyStream(NPP npp, NPStream* stream,
                  NPReason reason)
{
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  QtNPStream *npstream = (stream) ? (QtNPStream*)(stream->pdata) : 0;
  bool okay = (reason == NPRES_DONE);
  if (instance && npstream && instance->streams.contains(npstream))
    {
      if (instance->document && !npstream->closed)
        ddjvu_stream_close(*instance->document, npstream->streamid, !okay);
      npstream->closed = true;
      if (instance->forwarder)
        instance->forwarder->showStatus(QString());
      delete npstream;
    }
  return NPERR_NO_ERROR;
}


int32 
NPP_WriteReady(NPP npp, NPStream* stream)
{
  Q_UNUSED(npp)
  Q_UNUSED(stream)
  return 0x7fffffff;
}

int32 
NPP_Write(NPP npp, NPStream* stream, int32 offset,
          int32 len, void *buffer)
{
  Q_UNUSED(offset)
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  QtNPStream *npstream = (stream) ? (QtNPStream*)(stream->pdata) : 0;
  const char *cdata = (const char*)buffer;
  if (instance && npstream && instance->streams.contains(npstream))
    {
      if (!npstream->checked && len > 0)
        {
          // -- if the stream is not a djvu file, 
          //    this is probably a html error message.
          //    But we need at least 8 bytes to check
          npstream->checked = true;
          if (instance->forwarder && len >= 8)
            {
              if (strncmp(cdata, "FORM", 4) && strncmp(cdata+4, "FORM", 4))
                {
                  instance->forwarder->getUrl(npstream->url, QString("_self")); 
                  return len;
                }
            }
        }
      // pass data
      QtNPDocument *document = instance->document;
      if (document)
        ddjvu_stream_write(*document, npstream->streamid, cdata, len);
    }
  return len;
}


void 
NPP_StreamAsFile(NPP npp, NPStream* stream, const char* fname)
{
  Q_UNUSED(npp)
  Q_UNUSED(stream)
  Q_UNUSED(fname)
}


void 
NPP_Print(NPP npp, NPPrint* platformPrint)
{
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  if (instance && instance->djview)
    {
      instance->djview->print();
      if (platformPrint && platformPrint->mode==NP_FULL)
        platformPrint->print.fullPrint.pluginPrinted=TRUE;
    }
}


int16 
NPP_HandleEvent(NPP npp, NPEvent* event)
{
  QtNPInstance *instance = (npp) ? (QtNPInstance*)(npp->pdata) : 0;
  if (instance)
    return qtns_event(instance, event) ? 1 : 0;
  return NPERR_INVALID_INSTANCE_ERROR;
}


void 
NPP_URLNotify(NPP npp, const char *url, NPReason reason, void *data)
{
  Q_UNUSED(npp)
  Q_UNUSED(url)
  Q_UNUSED(reason)
  Q_UNUSED(data)
}


NPError 
NPP_GetValue(NPP npp, 
             NPPVariable variable, void *value)
{
  Q_UNUSED(npp)
  NPError err = NPERR_GENERIC_ERROR;
  switch (variable)
    {
    case NPPVpluginNameString:
      *((const char **)value) = "DjView-" DJVIEW_VERSION_STR " (npdjvu)";
      err = NPERR_NO_ERROR;
      break;
    case NPPVpluginDescriptionString:
      *((const char **)value) =
        "This is the <a href=\"http://djvu.sourceforge.net\">"
        "DjView-" DJVIEW_VERSION_STR " (npdjvu)</a> version of "
        "the DjVu plugin.<br>"
        "See <a href=\"http://djvu.sourceforge.net\">DjVuLibre</a>.";
      err = NPERR_NO_ERROR;
      break;
    case NPPVpluginNeedsXEmbed:
      *((NPBool*)value) = TRUE;
      err = NPERR_NO_ERROR;
      break;
    case NPPVpluginScriptableNPObject:
      *((NPObject**)value) = 0;
      break;
    default:
      break;
    }
  return err;
}


NPError 
NPP_SetValue(NPP npp, 
             NPPVariable variable, void *value)
{
  Q_UNUSED(npp)
  Q_UNUSED(variable)
  Q_UNUSED(value)
  return NPERR_GENERIC_ERROR;
}


/* ------------------------------------------------------------- */

#ifdef Q_WS_X11

const char*
NP_GetMIMEDescription(void)
{
  return
    "image/x-djvu:djvu,djv:DjVu File;"
    "image/x.djvu::DjVu File;"
    "image/djvu::DjVu File;"
    "image/x-dejavu::DjVu File (obsolete);"
    "image/x-iw44::DjVu File (obsolete);"
    "image/vnd.djvu::DjVu File;" ;
}

NPError
NP_GetValue(void*, NPPVariable aVariable, void *aValue)
{
  return NPP_GetValue(0, aVariable, aValue);
}

#endif


/* ------------------------------------------------------------- */


#include "npdjvu.moc"


/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ( "\\sw+_t" "[A-Z]\\sw*[a-z]\\sw*" "NPP" "QtNP\\sw*")
   End:
   ------------------------------------------------------------- */
