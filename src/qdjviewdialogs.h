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

#ifndef QDJVIEWDIALOGS_H
#define QDJVIEWDIALOGS_H

#include <QObject>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>

#include "qdjview.h"


// ----------- QDJVIEWERRORDIALOG


class QDjViewErrorDialog : public QDialog
{
  Q_OBJECT
public:
  ~QDjViewErrorDialog();
  QDjViewErrorDialog(QWidget *parent);
  void prepare(QMessageBox::Icon icon, QString caption);
public slots:
  void error(QString message, QString filename, int lineno);
  void okay(void);
private:
  void compose(void);
  struct Private;
  Private *d;
};




// ----------- QDJVIEWINFODIALOG


class QDjViewInfoDialog : public QDialog
{
  Q_OBJECT
public:
  ~QDjViewInfoDialog();
  QDjViewInfoDialog(QDjView *parent);
public slots:
  void refresh();
  void setPage(int pageno);
  void setFile(int fileno);
protected slots:
  void prevFile(void);
  void nextFile(void);
  void jumpToSelectedPage(void);
  void documentClosed(void);
private:
  void fillFileCombo();
  void fillDocLabel();
  void fillDocTable();
  void fillDocRow(int);
  struct Private;
  Private *d;
};





// ----------- QDJVIEWMETADIALOG


class QDjViewMetaDialog : public QDialog
{
  Q_OBJECT
public:
  ~QDjViewMetaDialog();
  QDjViewMetaDialog(QDjView *parent);
public slots:
  void refresh();
  void prevPage(void);
  void nextPage(void);
  void setPage(int pageno);
protected slots:
  void jumpToSelectedPage(void);
  void documentClosed(void);
  void copy(void);
private:
  struct Private;
  Private *d;
};








#endif

/* -------------------------------------------------------------
   Local Variables:
   c++-font-lock-extra-types: ( "\\sw+_t" "[A-Z]\\sw*[a-z]\\sw*" )
   End:
   ------------------------------------------------------------- */
