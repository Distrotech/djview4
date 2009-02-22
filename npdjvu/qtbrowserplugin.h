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

// Replacement for trolltech's main file

#include <QtGlobal>
#include <QVariant>
#include <QObject>
#include <QWidget>

#include <QtGlobal>

#include "qtnpapi.h"

extern "C" bool qtns_event(QtNPInstance*, NPEvent*);
extern "C" void qtns_initialize(QtNPInstance* This);
extern "C" void qtns_destroy(QtNPInstance* This);
extern "C" void qtns_shutdown(void);
extern "C" void qtns_embed(QtNPInstance *This);
extern "C" void qtns_setGeometry(QtNPInstance *This, const QRect &rect, const QRect &);

