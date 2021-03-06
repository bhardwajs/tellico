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

#include "kinofetcher.h"
#include "../utils/guiproxy.h"
#include "../utils/string_utils.h"
#include "../collections/bookcollection.h"
#include "../collections/videocollection.h"
#include "../entry.h"
#include "../fieldformat.h"
#include "../core/filehandler.h"
#include "../images/imagefactory.h"
#include "../tellico_debug.h"

#include <KLocalizedString>
#include <KConfig>
#include <KIO/Job>
#include <KIO/JobUiDelegate>
#include <KJobWidgets/KJobWidgets>

#include <QRegularExpression>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QUrlQuery>

namespace {
  static const char* KINO_BASE_URL = "http://www.kino.de/se/";
}

using namespace Tellico;
using Tellico::Fetch::KinoFetcher;

KinoFetcher::KinoFetcher(QObject* parent_)
    : Fetcher(parent_), m_started(false) {
}

KinoFetcher::~KinoFetcher() {
}

QString KinoFetcher::source() const {
  return m_name.isEmpty() ? defaultName() : m_name;
}

bool KinoFetcher::canFetch(int type) const {
  return type == Data::Collection::Video;
}

void KinoFetcher::readConfigHook(const KConfigGroup& config_) {
  Q_UNUSED(config_);
}

void KinoFetcher::search() {
  m_started = true;
  m_matches.clear();

  QUrl u(QString::fromLatin1(KINO_BASE_URL));
  QUrlQuery q;
  q.addQueryItem(QLatin1String("sp_search_filter"), QLatin1String("movie"));

  switch(request().key) {
    case Title:
      u.setPath(u.path() + request().value + QLatin1Char('/'));
      break;

    default:
      myWarning() << "key not recognized: " << request().key;
      stop();
      return;
  }
  u.setQuery(q);
//  myDebug() << "url:" << u;

  m_job = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
  KJobWidgets::setWindow(m_job, GUI::Proxy::widget());
  connect(m_job, SIGNAL(result(KJob*)),
          SLOT(slotComplete(KJob*)));
}

void KinoFetcher::stop() {
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

void KinoFetcher::slotComplete(KJob*) {
  if(m_job->error()) {
    m_job->uiDelegate()->showErrorMessage();
    stop();
    return;
  }

  const QByteArray data = m_job->data();
  if(data.isEmpty()) {
    myDebug() << "no data";
    stop();
    return;
  }

  // since the fetch is done, don't worry about holding the job pointer
  m_job = nullptr;

  QString s = Tellico::decodeHTML(data);
#if 0
  myWarning() << "Remove debug from kinofetcher.cpp";
  QFile f(QLatin1String("/tmp/test.html"));
  if(f.open(QIODevice::WriteOnly)) {
    QTextStream t(&f);
    t.setCodec("UTF-8");
    t << s;
  }
  f.close();
#endif

  QRegularExpression linkRx(QLatin1String("<a .+?movie-link.+?href=\"(.+?)\".*>(.+?)</"));
  QRegularExpression dateSpanRx(QLatin1String("<span .+?movie-startdate.+?>(.+?)</span"));
  // spans could have multiple values, all which have their own span elements. assume all on one line, so check against ending newline
  QRegularExpression genreSpanRx(QLatin1String("<span .+?movie-genre.+?>(.+?)\\n"));
  QRegularExpression castSpanRx(QLatin1String("<span .+?movie-cast.+?>(.+?)\\n"));
  QRegularExpression directorSpanRx(QLatin1String("<span .+?movie-director.+?>(.+?)\\n"));
  QRegularExpression dateRx(QLatin1String("\\d{2}\\.\\d{2}\\.(\\d{4})"));
  QRegularExpression yearEndRx(QLatin1String("(\\d{4})/?$"));
  QRegularExpression tagRx(QLatin1String("<.+?>"));

  QRegularExpressionMatchIterator i = linkRx.globalMatch(s);
  while(i.hasNext()) {
    QRegularExpressionMatch match = i.next();
    const QString u = match.captured(1);
    if(u.isEmpty()) {
      continue;
    }
    Data::CollPtr coll(new Data::VideoCollection(true));
    Data::EntryPtr entry(new Data::Entry(coll));
    coll->addEntries(entry);

    entry->setField(QLatin1String("title"), match.captured(2));

    QString y;
    QRegularExpressionMatch dateMatch = dateSpanRx.match(s, match.capturedEnd());
    if(dateMatch.hasMatch()) {
      y = dateRx.match(dateMatch.captured(1)).captured(1);
    } else {
      // see if year is embedded in url
      y = yearEndRx.match(u).captured(1);
    }
    entry->setField(QLatin1String("year"), y);

    QRegularExpressionMatch genreMatch = genreSpanRx.match(s, match.capturedEnd());
    if(genreMatch.hasMatch()) {
      QString g = genreMatch.captured(1).remove(tagRx).remove(QLatin1String("Genre: "));
      QStringList genres = g.split(QRegularExpression(QLatin1String("\\s*,\\s*")));
      entry->setField(QLatin1String("genre"), genres.join(FieldFormat::delimiterString()));
    }

    QRegularExpressionMatch directorMatch = directorSpanRx.match(s, match.capturedEnd());
    if(directorMatch.hasMatch()) {
      QString d = directorMatch.captured(1).remove(tagRx).remove(QLatin1String("Von: "));
      QStringList directors = d.split(QRegularExpression(QLatin1String("\\s*(,|\\sund\\s)\\s*")));
      entry->setField(QLatin1String("director"), directors.join(FieldFormat::delimiterString()));
    }

    QRegularExpressionMatch castMatch = castSpanRx.match(s, match.capturedEnd());
    if(castMatch.hasMatch()) {
      QString c = castMatch.captured(1).remove(tagRx).remove(QLatin1String("Mit: ")).remove(QLatin1String(" und weiteren"));
      QStringList cast = c.split(QRegularExpression(QLatin1String("\\s*,\\s*")));
      entry->setField(QLatin1String("cast"), cast.join(FieldFormat::rowDelimiterString()));
    }

    FetchResult* r = new FetchResult(Fetcher::Ptr(this), entry);
    QUrl url = QUrl(QString::fromLatin1(KINO_BASE_URL)).resolved(QUrl(u));
    m_matches.insert(r->uid, url);
    m_entries.insert(r->uid, entry);
    // don't emit signal until after putting url in matches hash
    emit signalResultFound(r);
  }

  stop();
}

Tellico::Data::EntryPtr KinoFetcher::fetchEntryHook(uint uid_) {
  if(!m_entries.contains(uid_)) {
    myWarning() << "no entry in hash";
    return Data::EntryPtr();
  }

  Data::EntryPtr entry = m_entries[uid_];
  // if the url is not in the hash, the entry has already been fully populated
  if(!m_matches.contains(uid_)) {
    return entry;
  }

  QString results = Tellico::decodeHTML(FileHandler::readTextFile(m_matches[uid_], true, true));
  if(results.isEmpty()) {
    myDebug() << "no text results from" << m_matches[uid_];
    return entry;
  }

#if 0
  myWarning() << "Remove debug from kinofetcher.cpp";
  QFile f(QLatin1String("/tmp/test2.html"));
  if(f.open(QIODevice::WriteOnly)) {
    QTextStream t(&f);
    t.setCodec("UTF-8");
    t << results;
  }
  f.close();
#endif

  parseEntry(entry, results);
  // remove url to signal the entry is fully populated
  m_matches.remove(uid_);
  return entry;
}

void KinoFetcher::parseEntry(Data::EntryPtr entry, const QString& str_) {
  QRegularExpression tagRx(QLatin1String("<.+?>"));

  QRegularExpression nationalityRx(QLatin1String("<dt>Produktionsland</dt><dd>(.*?)</dd>"));
  QRegularExpressionMatch nationalityMatch = nationalityRx.match(str_);
  if(nationalityMatch.hasMatch()) {
    QString n = nationalityMatch.captured(1).remove(tagRx);
    entry->setField(QLatin1String("nationality"), n);
  }

  QRegularExpression lengthRx(QLatin1String("<dt.*?>Dauer</dt><dd.*?>(.*?)</dd>"));
  QRegularExpressionMatch lengthMatch = lengthRx.match(str_);
  if(lengthMatch.hasMatch()) {
    QString l = lengthMatch.captured(1).remove(tagRx).remove(QLatin1String(" Min"));
    entry->setField(QLatin1String("running-time"), l);
  }

  QRegularExpression certRx(QLatin1String("<dt>FSK</dt><dd>(.*?)</dd>"));
  QRegularExpressionMatch certMatch = certRx.match(str_);
  if(certMatch.hasMatch()) {
    // need to translate? Let's just add FSK ratings to the allowed values
    QStringList allowed = entry->collection()->hasField(QLatin1String("certification")) ?
                          entry->collection()->fieldByName(QLatin1String("certification"))->allowed() :
                          QStringList();
    if(!allowed.contains(QLatin1String("FSK 0 (DE)"))) {
      allowed << QLatin1String("FSK 0 (DE)")
              << QLatin1String("FSK 6 (DE)")
              << QLatin1String("FSK 12 (DE)")
              << QLatin1String("FSK 16 (DE)")
              << QLatin1String("FSK 18 (DE)");
      entry->collection()->fieldByName(QLatin1String("certification"))->setAllowed(allowed);
    }
    QString c = certMatch.captured(1).remove(tagRx);
    if(c == QLatin1String("ab 0")) {
      c = QLatin1String("FSK 0 (DE)");
    } else if(c == QLatin1String("ab 6")) {
      c = QLatin1String("FSK 6 (DE)");
    } else if(c == QLatin1String("ab 12")) {
      c = QLatin1String("FSK 12 (DE)");
    } else if(c == QLatin1String("ab 16")) {
      c = QLatin1String("FSK 16 (DE)");
    } else if(c == QLatin1String("ab 18")) {
      c = QLatin1String("FSK 18 (DE)");
    }
    entry->setField(QLatin1String("certification"), c);
  }

  QRegularExpression studioRx(QLatin1String("<dt>Filmverleih</dt><dd>(.*?)</dd>"));
  QRegularExpressionMatch studioMatch = studioRx.match(str_);
  if(studioMatch.hasMatch()) {
    QString s = studioMatch.captured(1).remove(tagRx).remove(QLatin1String(" Min"));
    entry->setField(QLatin1String("studio"), s);
  }

  QRegularExpression plotRx(QLatin1String("<div class=\"js-teaser movie-plot-teaser\"></div>(.*?)<(/section|h2)>"),
                                          QRegularExpression::DotMatchesEverythingOption);
  QRegularExpressionMatch plotMatch = plotRx.match(str_);
  if(plotMatch.hasMatch()) {
    QString plot;
    QRegularExpression pRx(QLatin1String("<p>.*?</p>"));
    QRegularExpressionMatchIterator i = pRx.globalMatch(plotMatch.captured(1));
    while(i.hasNext()) {
      plot += i.next().captured(0);
    }
    entry->setField(QLatin1String("plot"), plot);
  }

  QRegularExpression divMetaRx(QLatin1String("<div class=\"movie-meta\".*?>(.+?)</div>"),
                               QRegularExpression::DotMatchesEverythingOption);
  QRegularExpressionMatch divMetaMatch = divMetaRx.match(str_);
  if(divMetaMatch.hasMatch()) {
    QRegularExpression coverRx(QString::fromLatin1("<img.+?src=\"(.+?)\".+?%1 Poster.*?/>").arg(entry->field(QLatin1String("title"))));
    QRegularExpressionMatch coverMatch = coverRx.match(divMetaMatch.captured(1));
    const QString id = ImageFactory::addImage(QUrl::fromUserInput(coverMatch.captured(1)), true /* quiet */);
    if(id.isEmpty()) {
      message(i18n("The cover image could not be loaded."), MessageHandler::Warning);
    }
    // empty image ID is ok
    entry->setField(QLatin1String("cover"), id);
  }
}

Tellico::Fetch::FetchRequest KinoFetcher::updateRequest(Data::EntryPtr entry_) {
  QString t = entry_->field(QLatin1String("title"));
  if(!t.isEmpty()) {
    return FetchRequest(Fetch::Title, t);
  }
  return FetchRequest();
}

Tellico::Fetch::ConfigWidget* KinoFetcher::configWidget(QWidget* parent_) const {
  return new KinoFetcher::ConfigWidget(parent_, this);
}

QString KinoFetcher::defaultName() {
  return QLatin1String("kino.de");
}

QString KinoFetcher::defaultIcon() {
  return favIcon("https://www.kino.de");
}

//static
Tellico::StringHash KinoFetcher::allOptionalFields() {
  StringHash hash;
  hash[QLatin1String("distributor")]     = i18n("Distributor");
  hash[QLatin1String("episodes")]        = i18n("Episodes");
  hash[QLatin1String("origtitle")]       = i18n("Original Title");
  hash[QLatin1String("alttitle")]        = i18n("Alternative Titles");
  hash[QLatin1String("animenfo-rating")] = i18n("AnimeNfo Rating");
  hash[QLatin1String("animenfo")]        = i18n("AnimeNfo Link");
  return hash;
}

KinoFetcher::ConfigWidget::ConfigWidget(QWidget* parent_, const KinoFetcher* fetcher_)
    : Fetch::ConfigWidget(parent_) {
  QVBoxLayout* l = new QVBoxLayout(optionsWidget());
  l->addWidget(new QLabel(i18n("This source has no options."), optionsWidget()));
  l->addStretch();

  // now add additional fields widget
  addFieldsWidget(KinoFetcher::allOptionalFields(), fetcher_ ? fetcher_->optionalFields() : QStringList());
}

QString KinoFetcher::ConfigWidget::preferredName() const {
  return KinoFetcher::defaultName();
}
