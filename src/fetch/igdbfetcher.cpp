/***************************************************************************
    Copyright (C) 2017 Robby Stephenson <robby@periapsis.org>
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

#include "igdbfetcher.h"
#include "../collections/gamecollection.h"
#include "../images/imagefactory.h"
#include "../core/filehandler.h"
#include "../utils/guiproxy.h"
#include "../utils/string_utils.h"
#include "../tellico_debug.h"

#include <KLocalizedString>
#include <KConfigGroup>
#include <KJob>
#include <KJobUiDelegate>
#include <KJobWidgets/KJobWidgets>
#include <KIO/StoredTransferJob>

#include <QUrl>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QGridLayout>
#include <QTextCodec>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>

namespace {
  static const int IGDB_MAX_RETURNS_TOTAL = 20;
  static const char* IGDB_API_URL = "https://igdbcom-internet-game-database-v1.p.mashape.com/";
  static const char* IGDB_API_KEY = "Ger6nO0EnKmsh7FCyUPa3GMdeYM5p1sfrPjjsnLYoHdDf19CGG";
}

using namespace Tellico;
using Tellico::Fetch::IGDBFetcher;

IGDBFetcher::IGDBFetcher(QObject* parent_)
    : Fetcher(parent_), m_started(false), m_apiKey(QLatin1String(IGDB_API_KEY)) {
  //  setLimit(IGDB_MAX_RETURNS_TOTAL);
  if(m_genreHash.isEmpty()) {
    populateHashes();
  }
}

IGDBFetcher::~IGDBFetcher() {
}

QString IGDBFetcher::source() const {
  return m_name.isEmpty() ? defaultName() : m_name;
}

QString IGDBFetcher::attribution() const {
  return i18n("This information was freely provided by <a href=\"http://igdb.com\">IGDB.com</a>.");
}

bool IGDBFetcher::canSearch(FetchKey k) const {
  return k == Keyword;
}

bool IGDBFetcher::canFetch(int type) const {
  return type == Data::Collection::Game;
}

void IGDBFetcher::readConfigHook(const KConfigGroup& config_) {
  QString k = config_.readEntry("API Key", IGDB_API_KEY);
  if(!k.isEmpty()) {
    m_apiKey = k;
  }
}

void IGDBFetcher::search() {
  continueSearch();
}

void IGDBFetcher::continueSearch() {
  m_started = true;

  if(m_apiKey.isEmpty()) {
    myDebug() << "empty API key";
    message(i18n("An access key is required to use this data source.")
            + QLatin1Char(' ') +
            i18n("Those values must be entered in the data source settings."), MessageHandler::Error);
    stop();
    return;
  }

  QUrl u(QString::fromLatin1(IGDB_API_URL));
  u.setPath(u.path() + QLatin1String("games/"));
  QUrlQuery q;
  switch(request().key) {
    case Keyword:
      q.addQueryItem(QLatin1String("search"), request().value);
      break;

    default:
      myWarning() << "key not recognized:" << request().key;
      stop();
      return;
  }
//  q.addQueryItem(QLatin1String("fields"), QLatin1String("id,name"));
  q.addQueryItem(QLatin1String("fields"), QLatin1String("*"));
  q.addQueryItem(QLatin1String("limit"), QString::number(IGDB_MAX_RETURNS_TOTAL));
  u.setQuery(q);
//  myDebug() << u;

  m_job = igdbJob(u, m_apiKey);
  connect(m_job, SIGNAL(result(KJob*)), SLOT(slotComplete(KJob*)));
}

void IGDBFetcher::stop() {
  if(!m_started) {
    return;
  }
  if(m_job) {
    m_job->kill();
    m_job = nullptr;
  }
  m_started = false;
  emit signalDone(this);
}

Tellico::Data::EntryPtr IGDBFetcher::fetchEntryHook(uint uid_) {
  if(!m_entries.contains(uid_)) {
    myDebug() << "no entry ptr";
    return Data::EntryPtr();
  }

  Data::EntryPtr entry = m_entries.value(uid_);

  QStringList publishers;
  // grab the publisher data
  if(entry->field(QLatin1String("publisher")).isEmpty()) {
    foreach(const QString& pid, FieldFormat::splitValue(entry->field(QLatin1String("pub-id")))) {
      const QString publisher = companyName(pid);
      if(!publisher.isEmpty()) {
        publishers << publisher;
      }
    }
  }
  entry->setField(QLatin1String("publisher"), publishers.join(FieldFormat::delimiterString()));

  QStringList developers;
  // grab the developer data
  if(entry->field(QLatin1String("developer")).isEmpty()) {
    foreach(const QString& did, FieldFormat::splitValue(entry->field(QLatin1String("dev-id")))) {
      const QString developer = companyName(did);
      if(!developer.isEmpty()) {
        developers << developer;
      }
    }
  }
  entry->setField(QLatin1String("developer"), developers.join(FieldFormat::delimiterString()));

  // image might still be a URL
  const QString image_id = entry->field(QLatin1String("cover"));
  if(image_id.contains(QLatin1Char('/'))) {
    const QString id = ImageFactory::addImage(QUrl::fromUserInput(image_id), true /* quiet */);
    if(id.isEmpty()) {
      message(i18n("The cover image could not be loaded."), MessageHandler::Warning);
    }
    // empty image ID is ok
    entry->setField(QLatin1String("cover"), id);
  }

  // clear the placeholder fields
  entry->setField(QLatin1String("pub-id"), QString());
  entry->setField(QLatin1String("dev-id"), QString());
  return entry;
}

Tellico::Fetch::FetchRequest IGDBFetcher::updateRequest(Data::EntryPtr entry_) {
  QString title = entry_->field(QLatin1String("title"));
  if(!title.isEmpty()) {
    return FetchRequest(Keyword, title);
  }
  return FetchRequest();
}

void IGDBFetcher::slotComplete(KJob* job_) {
  KIO::StoredTransferJob* job = static_cast<KIO::StoredTransferJob*>(job_);

  if(job->error()) {
    job->uiDelegate()->showErrorMessage();
    stop();
    return;
  }

  const QByteArray data = job->data();
  if(data.isEmpty()) {
    myDebug() << "no data";
    stop();
    return;
  }
  // see bug 319662. If fetcher is cancelled, job is killed
  // if the pointer is retained, it gets double-deleted
  m_job = nullptr;

#if 0
  myWarning() << "Remove debug from igdbfetcher.cpp";
  QFile file(QString::fromLatin1("/tmp/test.json"));
  if(file.open(QIODevice::WriteOnly)) {
    QTextStream t(&file);
    t.setCodec("UTF-8");
    t << data;
  }
  file.close();
#endif

  Data::CollPtr coll(new Data::GameCollection(true));
  if(optionalFields().contains(QLatin1String("pegi"))) {
    QStringList pegi = QString::fromLatin1("PEGI 3, PEGI 7, PEGI 12, PEGI 16, PEGI 18")
                                    .split(QRegExp(QLatin1String("\\s*,\\s*")), QString::SkipEmptyParts);
    Data::FieldPtr field(new Data::Field(QLatin1String("pegi"), i18n("PEGI Rating"), pegi));
    field->setFlags(Data::Field::AllowGrouped);
    field->setCategory(i18n("General"));
    coll->addField(field);
  }
  if(optionalFields().contains(QLatin1String("igdb"))) {
    Data::FieldPtr field(new Data::Field(QLatin1String("igdb"), i18n("IGDB Link"), Data::Field::URL));
    field->setCategory(i18n("General"));
    coll->addField(field);
  }
  // placeholder for publisher id, to be removed later
  Data::FieldPtr f(new Data::Field(QLatin1String("pub-id"), QString(), Data::Field::Number));
  f->setFlags(Data::Field::AllowMultiple);
  coll->addField(f);
  // placeholder for developer id, to be removed later
  f = new Data::Field(QLatin1String("dev-id"), QString(), Data::Field::Number);
  f->setFlags(Data::Field::AllowMultiple);
  coll->addField(f);

  QJsonDocument doc = QJsonDocument::fromJson(data);
  foreach(const QVariant& result, doc.array().toVariantList()) {
    QVariantMap resultMap = result.toMap();
    Data::EntryPtr entry(new Data::Entry(coll));
    populateEntry(entry, resultMap);

    FetchResult* r = new FetchResult(Fetcher::Ptr(this), entry);
    m_entries.insert(r->uid, entry);
    emit signalResultFound(r);
  }

  stop();
}

void IGDBFetcher::populateEntry(Data::EntryPtr entry_, const QVariantMap& resultMap_) {
  entry_->setField(QLatin1String("title"), mapValue(resultMap_, "name"));
  entry_->setField(QLatin1String("description"), mapValue(resultMap_, "summary"));
  entry_->setField(QLatin1String("certification"), m_esrbHash.value(mapValue(resultMap_, "esrb", "rating")));
  entry_->setField(QLatin1String("pub-id"), mapValue(resultMap_, "publishers"));
  entry_->setField(QLatin1String("dev-id"), mapValue(resultMap_, "developers"));

  QString cover = mapValue(resultMap_, "cover", "url");
  if(cover.startsWith(QLatin1Char('/'))) {
    cover.prepend(QLatin1String("https:"));
  }
  entry_->setField(QLatin1String("cover"), cover);

  QVariantList genreIDs = resultMap_.value(QLatin1String("genres")).toList();
  QStringList genres;
  foreach(const QVariant& id, genreIDs) {
    QString g = m_genreHash.value(id.toInt());
    if(!g.isEmpty()) {
      genres << g;
    }
  }
  entry_->setField(QLatin1String("genre"), genres.join(FieldFormat::delimiterString()));

  QVariantList releases = resultMap_.value(QLatin1String("release_dates")).toList();
  if(!releases.isEmpty()) {
    QVariantMap releaseMap = releases.at(0).toMap();
    // for now just grab the year of the first release
    entry_->setField(QLatin1String("year"), mapValue(releaseMap, "y"));
    const QString platform = m_platformHash.value(releaseMap.value(QLatin1String("platform")).toInt());
    if(platform == QLatin1String("Nintendo Entertainment System (NES)")) {
      entry_->setField(QLatin1String("platform"), i18n("Nintendo"));
    } else if(platform == QLatin1String("Nintendo PlayStation")) {
      entry_->setField(QLatin1String("platform"), i18n("PlayStation"));
    } else if(platform == QLatin1String("PlayStation 2")) {
      entry_->setField(QLatin1String("platform"), i18n("PlayStation2"));
    } else if(platform == QLatin1String("PlayStation 3")) {
      entry_->setField(QLatin1String("platform"), i18n("PlayStation3"));
    } else if(platform == QLatin1String("PlayStation 4")) {
      entry_->setField(QLatin1String("platform"), i18n("PlayStation4"));
    } else if(platform == QLatin1String("PlayStation Portable")) {
      entry_->setField(QLatin1String("platform"), i18nc("PlayStation Portable", "PSP"));
    } else if(platform == QLatin1String("Wii")) {
      entry_->setField(QLatin1String("platform"), i18n("Nintendo Wii"));
    } else if(platform == QLatin1String("Nintendo GameCube")) {
      entry_->setField(QLatin1String("platform"), i18n("GameCube"));
    } else if(platform == QLatin1String("PC (Microsoft Windows)")) {
      entry_->setField(QLatin1String("platform"), i18nc("Windows Platform", "Windows"));
    } else if(platform == QLatin1String("Mac")) {
      entry_->setField(QLatin1String("platform"), i18n("Mac OS"));
    } else {
      // TODO all the other platform translations
      // also make the assumption that if the platform name isn't already in the allowed list, it should be added
      Data::FieldPtr f = entry_->collection()->fieldByName(QLatin1String("platform"));
      if(f && !f->allowed().contains(platform)) {
        f->setAllowed(QStringList(f->allowed()) << platform);
      }
      entry_->setField(QLatin1String("platform"), platform);
    }
  }

  if(optionalFields().contains(QLatin1String("pegi"))) {
    entry_->setField(QLatin1String("pegi"), m_pegiHash.value(mapValue(resultMap_, "pegi", "rating")));
  }

  if(optionalFields().contains(QLatin1String("igdb"))) {
    entry_->setField(QLatin1String("igdb"), mapValue(resultMap_, "url"));
  }
}

QString IGDBFetcher::companyName(const QString& companyId_) const {
  if(m_companyHash.contains(companyId_)) {
    return m_companyHash.value(companyId_);
  }
  QUrl u(QString::fromLatin1(IGDB_API_URL));
  u.setPath(u.path() + QLatin1String("companies/") + companyId_);

  QUrlQuery q;
  q.addQueryItem(QLatin1String("fields"), QLatin1String("*"));

  u.setQuery(q);

  QPointer<KIO::StoredTransferJob> job = igdbJob(u, m_apiKey);
  if(!job->exec()) {
    myDebug() << job->errorString() << u;
    return QString();
  }
  const QByteArray data = job->data();
  if(data.isEmpty()) {
    myDebug() << "no data for" << u;
    return QString();
  }
#if 0
  myWarning() << "Remove company debug from igdbfetcher.cpp";
  QFile file(QString::fromLatin1("/tmp/igdb-company.json"));
  if(file.open(QIODevice::WriteOnly)) {
    QTextStream t(&file);
    t.setCodec("UTF-8");
    t << data;
  }
  file.close();
#endif

  QJsonDocument doc = QJsonDocument::fromJson(data);
  const QString company = mapValue(doc.array().toVariantList().at(0).toMap(), "name");
  m_companyHash.insert(companyId_, company);
  return company;
}

Tellico::Fetch::ConfigWidget* IGDBFetcher::configWidget(QWidget* parent_) const {
  return new IGDBFetcher::ConfigWidget(parent_, this);
}


// Use member hash for certain field names for now.
// Don't expect IGDB values to change. This avoids exponentially multiplying the number of API calls
void IGDBFetcher::populateHashes() {
  m_genreHash.insert(2,  QLatin1String("Point-and-click"));
  m_genreHash.insert(4,  QLatin1String("Fighting"));
  m_genreHash.insert(5,  QLatin1String("Shooter"));
  m_genreHash.insert(7,  QLatin1String("Music"));
  m_genreHash.insert(8,  QLatin1String("Platform"));
  m_genreHash.insert(9,  QLatin1String("Puzzle"));
  m_genreHash.insert(10, QLatin1String("Racing"));
  m_genreHash.insert(11, QLatin1String("Real Time Strategy (RTS)"));
  m_genreHash.insert(12, QLatin1String("Role-playing (RPG)"));
  m_genreHash.insert(13, QLatin1String("Simulator"));
  m_genreHash.insert(14, QLatin1String("Sport"));
  m_genreHash.insert(15, QLatin1String("Strategy"));
  m_genreHash.insert(16, QLatin1String("Turn-based strategy (TBS)"));
  m_genreHash.insert(24, QLatin1String("Tactical"));
  m_genreHash.insert(25, QLatin1String("Hack and slash/Beat 'em up"));
  m_genreHash.insert(26, QLatin1String("Quiz/Trivia"));
  m_genreHash.insert(30, QLatin1String("Pinball"));
  m_genreHash.insert(31, QLatin1String("Adventure"));
  m_genreHash.insert(32, QLatin1String("Indie"));
  m_genreHash.insert(33, QLatin1String("Arcade"));

  m_platformHash.insert(3, QLatin1String("Linux"));
  m_platformHash.insert(4, QLatin1String("Nintendo 64"));
  m_platformHash.insert(5, QLatin1String("Wii"));
  m_platformHash.insert(6, QLatin1String("PC (Microsoft Windows)"));
  m_platformHash.insert(7, QLatin1String("PlayStation"));
  m_platformHash.insert(8, QLatin1String("PlayStation 2"));
  m_platformHash.insert(9, QLatin1String("PlayStation 3"));
  m_platformHash.insert(11, QLatin1String("Xbox"));
  m_platformHash.insert(12, QLatin1String("Xbox 360"));
  m_platformHash.insert(13, QLatin1String("PC DOS"));
  m_platformHash.insert(14, QLatin1String("Mac"));
  m_platformHash.insert(15, QLatin1String("Commodore C64/128"));
  m_platformHash.insert(16, QLatin1String("Amiga"));
  m_platformHash.insert(18, QLatin1String("Nintendo Entertainment System (NES)"));
  m_platformHash.insert(19, QLatin1String("Super Nintendo Entertainment System (SNES)"));
  m_platformHash.insert(20, QLatin1String("Nintendo DS"));
  m_platformHash.insert(21, QLatin1String("Nintendo GameCube"));
  m_platformHash.insert(22, QLatin1String("Game Boy Color"));
  m_platformHash.insert(23, QLatin1String("Dreamcast"));
  m_platformHash.insert(24, QLatin1String("Game Boy Advance"));
  m_platformHash.insert(25, QLatin1String("Amstrad CPC"));
  m_platformHash.insert(26, QLatin1String("ZX Spectrum"));
  m_platformHash.insert(27, QLatin1String("MSX"));
  m_platformHash.insert(29, QLatin1String("Sega Mega Drive/Genesis"));
  m_platformHash.insert(30, QLatin1String("Sega 32X"));
  m_platformHash.insert(32, QLatin1String("Sega Saturn"));
  m_platformHash.insert(33, QLatin1String("Game Boy"));
  m_platformHash.insert(34, QLatin1String("Android"));
  m_platformHash.insert(35, QLatin1String("Sega Game Gear"));
  m_platformHash.insert(36, QLatin1String("Xbox Live Arcade"));
  m_platformHash.insert(37, QLatin1String("Nintendo 3DS"));
  m_platformHash.insert(38, QLatin1String("PlayStation Portable"));
  m_platformHash.insert(39, QLatin1String("iOS"));
  m_platformHash.insert(41, QLatin1String("Wii U"));
  m_platformHash.insert(42, QLatin1String("N-Gage"));
  m_platformHash.insert(44, QLatin1String("Tapwave Zodiac"));
  m_platformHash.insert(45, QLatin1String("PlayStation Network"));
  m_platformHash.insert(46, QLatin1String("PlayStation Vita"));
  m_platformHash.insert(47, QLatin1String("Virtual Console (Nintendo)"));
  m_platformHash.insert(48, QLatin1String("PlayStation 4"));
  m_platformHash.insert(49, QLatin1String("Xbox One"));
  m_platformHash.insert(50, QLatin1String("3DO Interactive Multiplayer"));
  m_platformHash.insert(51, QLatin1String("Family Computer Disk System"));
  m_platformHash.insert(52, QLatin1String("Arcade"));
  m_platformHash.insert(53, QLatin1String("MSX2"));
  m_platformHash.insert(55, QLatin1String("Mobile"));
  m_platformHash.insert(56, QLatin1String("WiiWare"));
  m_platformHash.insert(57, QLatin1String("WonderSwan"));
  m_platformHash.insert(58, QLatin1String("Super Famicom"));
  m_platformHash.insert(59, QLatin1String("Atari 2600"));
  m_platformHash.insert(60, QLatin1String("Atari 7800"));
  m_platformHash.insert(61, QLatin1String("Atari Lynx"));
  m_platformHash.insert(62, QLatin1String("Atari Jaguar"));
  m_platformHash.insert(63, QLatin1String("Atari ST/STE"));
  m_platformHash.insert(64, QLatin1String("Sega Master System"));
  m_platformHash.insert(65, QLatin1String("Atari 8-bit"));
  m_platformHash.insert(66, QLatin1String("Atari 5200"));
  m_platformHash.insert(67, QLatin1String("Intellivision"));
  m_platformHash.insert(68, QLatin1String("ColecoVision"));
  m_platformHash.insert(69, QLatin1String("BBC Microcomputer System"));
  m_platformHash.insert(70, QLatin1String("Vectrex"));
  m_platformHash.insert(71, QLatin1String("Commodore VIC-20"));
  m_platformHash.insert(72, QLatin1String("Ouya"));
  m_platformHash.insert(73, QLatin1String("BlackBerry OS"));
  m_platformHash.insert(74, QLatin1String("Windows Phone"));
  m_platformHash.insert(75, QLatin1String("Apple II"));
  m_platformHash.insert(77, QLatin1String("Sharp X1"));
  m_platformHash.insert(78, QLatin1String("Sega CD"));
  m_platformHash.insert(79, QLatin1String("Neo Geo MVS"));
  m_platformHash.insert(80, QLatin1String("Neo Geo AES"));
  m_platformHash.insert(82, QLatin1String("Web browser"));
  m_platformHash.insert(84, QLatin1String("SG-1000"));
  m_platformHash.insert(85, QLatin1String("Donner Model 30"));
  m_platformHash.insert(86, QLatin1String("TurboGrafx-16/PC Engine"));
  m_platformHash.insert(87, QLatin1String("Virtual Boy"));
  m_platformHash.insert(88, QLatin1String("Odyssey"));
  m_platformHash.insert(89, QLatin1String("Microvision"));
  m_platformHash.insert(90, QLatin1String("Commodore PET"));
  m_platformHash.insert(91, QLatin1String("Bally Astrocade"));
  m_platformHash.insert(92, QLatin1String("SteamOS"));
  m_platformHash.insert(93, QLatin1String("Commodore 16"));
  m_platformHash.insert(94, QLatin1String("Commodore Plus/4"));
  m_platformHash.insert(95, QLatin1String("PDP-1"));
  m_platformHash.insert(96, QLatin1String("PDP-10"));
  m_platformHash.insert(97, QLatin1String("PDP-8"));
  m_platformHash.insert(98, QLatin1String("DEC GT40"));
  m_platformHash.insert(99, QLatin1String("Family Computer"));
  m_platformHash.insert(100, QLatin1String("Analogue electronics"));
  m_platformHash.insert(101, QLatin1String("Ferranti Nimrod Computer"));
  m_platformHash.insert(102, QLatin1String("EDSAC"));
  m_platformHash.insert(103, QLatin1String("PDP-7"));
  m_platformHash.insert(104, QLatin1String("HP 2100"));
  m_platformHash.insert(105, QLatin1String("HP 3000"));
  m_platformHash.insert(106, QLatin1String("SDS Sigma 7"));
  m_platformHash.insert(107, QLatin1String("Call-A-Computer time-shared mainframe computer system"));
  m_platformHash.insert(108, QLatin1String("PDP-11"));
  m_platformHash.insert(109, QLatin1String("CDC Cyber 70"));
  m_platformHash.insert(110, QLatin1String("PLATO"));
  m_platformHash.insert(111, QLatin1String("Imlac PDS-1"));
  m_platformHash.insert(112, QLatin1String("Microcomputer"));
  m_platformHash.insert(113, QLatin1String("OnLive Game System"));
  m_platformHash.insert(114, QLatin1String("Amiga CD32"));
  m_platformHash.insert(115, QLatin1String("Apple IIGS"));
  m_platformHash.insert(116, QLatin1String("Acorn Archimedes"));
  m_platformHash.insert(117, QLatin1String("Philips CD-i"));
  m_platformHash.insert(118, QLatin1String("FM Towns"));
  m_platformHash.insert(119, QLatin1String("Neo Geo Pocket"));
  m_platformHash.insert(120, QLatin1String("Neo Geo Pocket Color"));
  m_platformHash.insert(121, QLatin1String("Sharp X68000"));
  m_platformHash.insert(122, QLatin1String("Nuon"));
  m_platformHash.insert(123, QLatin1String("WonderSwan Color"));
  m_platformHash.insert(124, QLatin1String("SwanCrystal"));
  m_platformHash.insert(125, QLatin1String("PC-8801"));
  m_platformHash.insert(126, QLatin1String("TRS-80"));
  m_platformHash.insert(127, QLatin1String("Fairchild Channel F"));
  m_platformHash.insert(128, QLatin1String("PC Engine SuperGrafx"));
  m_platformHash.insert(129, QLatin1String("Texas Instruments TI-99"));
  m_platformHash.insert(130, QLatin1String("Nintendo Switch"));
  m_platformHash.insert(131, QLatin1String("Nintendo PlayStation"));
  m_platformHash.insert(132, QLatin1String("Amazon Fire TV"));
  m_platformHash.insert(133, QLatin1String("Philips Videopac G7000"));
  m_platformHash.insert(134, QLatin1String("Acorn Electron"));
  m_platformHash.insert(135, QLatin1String("Hyper Neo Geo 64"));
  m_platformHash.insert(136, QLatin1String("Neo Geo CD"));

  // cheat by grabbing i18n values from default collection
  Data::CollPtr c(new Data::GameCollection(true));
  QStringList esrb = c->fieldByName(QLatin1String("certification"))->allowed();
  Q_ASSERT(esrb.size() == 8);
  while(esrb.size() < 8) {
    esrb << QString();
  }
  m_esrbHash.insert(QLatin1String("1"), esrb.at(7));
  m_esrbHash.insert(QLatin1String("2"), esrb.at(6));
  m_esrbHash.insert(QLatin1String("3"), esrb.at(5));
  m_esrbHash.insert(QLatin1String("4"), esrb.at(4));
  m_esrbHash.insert(QLatin1String("5"), esrb.at(3));
  m_esrbHash.insert(QLatin1String("6"), esrb.at(2));
  m_esrbHash.insert(QLatin1String("7"), esrb.at(1));

  m_pegiHash.insert(QLatin1String("1"), QLatin1String("PEGI 3"));
  m_pegiHash.insert(QLatin1String("2"), QLatin1String("PEGI 7"));
  m_pegiHash.insert(QLatin1String("3"), QLatin1String("PEGI 12"));
  m_pegiHash.insert(QLatin1String("4"), QLatin1String("PEGI 16"));
  m_pegiHash.insert(QLatin1String("5"), QLatin1String("PEGI 18"));
}

QString IGDBFetcher::defaultName() {
  return i18n("Internet Game Database (IGDB.com)");
}

QString IGDBFetcher::defaultIcon() {
  return favIcon("http://www.igdb.com");
}

Tellico::StringHash IGDBFetcher::allOptionalFields() {
  StringHash hash;
  hash[QLatin1String("pegi")] = i18n("PEGI Rating");
  hash[QLatin1String("igdb")] = i18n("IGDB Link");
  return hash;
}

IGDBFetcher::ConfigWidget::ConfigWidget(QWidget* parent_, const IGDBFetcher* fetcher_)
    : Fetch::ConfigWidget(parent_) {
  QGridLayout* l = new QGridLayout(optionsWidget());
  l->setSpacing(4);
  l->setColumnStretch(1, 10);

  int row = -1;

  QLabel* al = new QLabel(i18n("Registration is required for accessing the %1 data source. "
                               "If you agree to the terms and conditions, <a href='%2'>sign "
                               "up for an account</a>, and enter your information below.",
                                IGDBFetcher::defaultName(),
                                QLatin1String("http://igdb.github.io/api/about/welcome/")),
                          optionsWidget());
  al->setOpenExternalLinks(true);
  al->setWordWrap(true);
  ++row;
  l->addWidget(al, row, 0, 1, 2);
  // richtext gets weird with size
  al->setMinimumWidth(al->sizeHint().width());

  QLabel* label = new QLabel(i18n("Access key: "), optionsWidget());
  l->addWidget(label, ++row, 0);

  m_apiKeyEdit = new QLineEdit(optionsWidget());
  connect(m_apiKeyEdit, SIGNAL(textChanged(const QString&)), SLOT(slotSetModified()));
  l->addWidget(m_apiKeyEdit, row, 1);
  QString w = i18n("The default Tellico key may be used, but searching may fail due to reaching access limits.");
  label->setWhatsThis(w);
  m_apiKeyEdit->setWhatsThis(w);
  label->setBuddy(m_apiKeyEdit);

  l->setRowStretch(++row, 10);

  // now add additional fields widget
  addFieldsWidget(IGDBFetcher::allOptionalFields(), fetcher_ ? fetcher_->optionalFields() : QStringList());

  if(fetcher_) {
    // only show the key if it is not the default Tellico one...
    // that way the user is prompted to apply for their own
    if(fetcher_->m_apiKey != QLatin1String(IGDB_API_KEY)) {
      m_apiKeyEdit->setText(fetcher_->m_apiKey);
    }
  }
}

void IGDBFetcher::ConfigWidget::saveConfigHook(KConfigGroup& config_) {
  const QString apiKey = m_apiKeyEdit->text().trimmed();
  if(!apiKey.isEmpty()) {
    config_.writeEntry("API Key", apiKey);
  }
}

QString IGDBFetcher::ConfigWidget::preferredName() const {
  return IGDBFetcher::defaultName();
}

QPointer<KIO::StoredTransferJob> IGDBFetcher::igdbJob(const QUrl& url_, const QString& apiKey_) {
  QPointer<KIO::StoredTransferJob> job = KIO::storedGet(url_, KIO::NoReload, KIO::HideProgressInfo);
  job->addMetaData(QLatin1String("customHTTPHeader"), QLatin1String("X-Mashape-Key: ") + apiKey_);
  job->addMetaData(QLatin1String("accept"), QLatin1String("application/json"));
  KJobWidgets::setWindow(job, GUI::Proxy::widget());
  return job;
}
