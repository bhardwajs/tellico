/***************************************************************************
    Copyright (C) 2004-2009 Robby Stephenson <robby@periapsis.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 ***************************************************************************/

#include "dbusinterface.h"
#include "../controller.h"
#include "../tellico_kernel.h"
#include "../document.h"
#include "../collection.h"
#include "../fieldformat.h"
#include "../translators/bibtexhandler.h"
#include "../mainwindow.h"

#include <QDBusConnection>

using Tellico::ApplicationInterface;
using Tellico::CollectionInterface;

ApplicationInterface::ApplicationInterface(Tellico::MainWindow* parent_) : QObject(parent_), m_mainWindow(parent_) {
  QDBusConnection::sessionBus().registerObject(QLatin1String("/Tellico"), this, QDBusConnection::ExportScriptableSlots);
}

Tellico::Import::Action ApplicationInterface::actionType(const QString& actionName) {
  QString name = actionName.toLower();
  if(name == QLatin1String("append")) {
    return Import::Append;
  } else if(name == QLatin1String("merge")) {
    return Import::Merge;
  }
  return Import::Replace;
}

QList<int> ApplicationInterface::selectedEntries() const {
  QList<int> ids;
  foreach(Data::EntryPtr entry, Controller::self()->selectedEntries()) {
    ids << entry->id();
  }
  return ids;
}

QList<int> ApplicationInterface::filteredEntries() const {
  QList<int> ids;
  foreach(Data::EntryPtr entry, Controller::self()->visibleEntries()) {
    ids << entry->id();
  }
  return ids;
}

void ApplicationInterface::openFile(const QString& file) {
  return m_mainWindow->openFile(file);
}

void ApplicationInterface::setFilter(const QString& text) {
  return m_mainWindow->setFilter(text);
}

bool ApplicationInterface::showEntry(int id)  {
  return m_mainWindow->showEntry(id);
}

bool ApplicationInterface::importFile(Tellico::Import::Format format, const KUrl& url, Tellico::Import::Action action) {
  return m_mainWindow->importFile(format, url, action);
}

bool ApplicationInterface::exportCollection(Tellico::Export::Format format, const KUrl& url) {
  return m_mainWindow->exportCollection(format, url);
}

CollectionInterface::CollectionInterface(QObject* parent_) : QObject(parent_) {
  QDBusConnection::sessionBus().registerObject(QLatin1String("/Collections"), this, QDBusConnection::ExportScriptableSlots);
}

int CollectionInterface::addEntry() {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return -1;
  }
  Data::EntryPtr entry(new Data::Entry(coll));
  Kernel::self()->addEntries(Data::EntryList() << entry, false);
  return entry->id();
}

bool CollectionInterface::removeEntry(int id_) {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return false;
  }
  Data::EntryPtr entry = coll->entryById(id_);
  if(!entry) {
    return false;
  }
  Kernel::self()->removeEntries(Data::EntryList() << entry);
  return !coll->entryById(id_);
}

QStringList CollectionInterface::allValues(const QString& fieldName_) const {
  QStringList results;
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return results;
  }
  Data::FieldPtr field = coll->fieldByName(fieldName_);
  if(!field) {
    field = coll->fieldByTitle(fieldName_);
  }
  if(!field) {
    return results;
  }
  Data::EntryList entries = Controller::self()->selectedEntries();
  foreach(Data::EntryPtr entry, entries) {
    results += entry->fields(field, false);
  }
  return results;
}

QStringList CollectionInterface::entryValues(int id_, const QString& fieldName_) const {
  QStringList results;
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return results;
  }
  Data::FieldPtr field = coll->fieldByName(fieldName_);
  if(!field) {
    field = coll->fieldByTitle(fieldName_);
  }
  if(!field) {
    return results;
  }
  Data::EntryPtr entry = coll->entryById(id_);
  if(entry) {
    results += entry->fields(field, false);
  }
  return results;
}

QStringList CollectionInterface::selectedBibtexKeys() const {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll || coll->type() != Data::Collection::Bibtex) {
    return QStringList();
  }
  return BibtexHandler::bibtexKeys(Controller::self()->selectedEntries());
}

QString CollectionInterface::entryBibtexKey(int id_) const {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll || coll->type() != Data::Collection::Bibtex) {
    return QString();
  }
  Data::EntryPtr entry = coll->entryById(id_);
  if(!entry) {
    return QString();
  }
  return BibtexHandler::bibtexKeys(Data::EntryList() << entry).first();
}

bool CollectionInterface::setEntryValue(int id_, const QString& fieldName_, const QString& value_) {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return false;
  }
  Data::EntryPtr entry = coll->entryById(id_);
  if(!entry) {
    return false;
  }
  Data::EntryPtr oldEntry(new Data::Entry(*entry));
  if(!entry->setField(fieldName_, value_)) {
    return false;
  }
  Kernel::self()->modifyEntries(Data::EntryList() << oldEntry, Data::EntryList() << entry);
  return true;
}

bool CollectionInterface::addEntryValue(int id_, const QString& fieldName_, const QString& value_) {
  Data::CollPtr coll = Data::Document::self()->collection();
  if(!coll) {
    return false;
  }
  Data::EntryPtr entry = coll->entryById(id_);
  if(!entry) {
    return false;
  }
  Data::EntryPtr oldEntry(new Data::Entry(*entry));
  QStringList values = entry->fields(fieldName_, false);
  QStringList newValues = values;
  newValues << value_;
  if(!entry->setField(fieldName_, newValues.join(FieldFormat::delimiterString()))) {
    return false;
  }
  Kernel::self()->modifyEntries(Data::EntryList() << oldEntry, Data::EntryList() << entry);
  return true;
}

#include "dbusinterface.moc"
