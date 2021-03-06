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

#undef QT_NO_CAST_FROM_ASCII

#include "videogamegeekfetchertest.h"

#include "../fetch/execexternalfetcher.h"
#include "../fetch/videogamegeekfetcher.h"
#include "../collections/gamecollection.h"
#include "../collectionfactory.h"
#include "../entry.h"
#include "../images/imagefactory.h"
#include "../utils/datafileregistry.h"

#include <KConfigGroup>

#include <QTest>

QTEST_GUILESS_MAIN( VideoGameGeekFetcherTest )

VideoGameGeekFetcherTest::VideoGameGeekFetcherTest() : AbstractFetcherTest() {
}

void VideoGameGeekFetcherTest::initTestCase() {
  Tellico::RegisterCollection<Tellico::Data::GameCollection> registerVGG(Tellico::Data::Collection::Game, "game");
  Tellico::ImageFactory::init();
  Tellico::DataFileRegistry::self()->addDataLocation(QFINDTESTDATA("../../xslt/boardgamegeek2tellico.xsl"));
}

void VideoGameGeekFetcherTest::testTitle() {
  Tellico::Fetch::FetchRequest request(Tellico::Data::Collection::Game, Tellico::Fetch::Title,
                                       QLatin1String("Mass Effect 3: Citadel"));
  Tellico::Fetch::Fetcher::Ptr fetcher(new Tellico::Fetch::VideoGameGeekFetcher(this));

  Tellico::Data::EntryList results = DO_FETCH1(fetcher, request, 1);

  QCOMPARE(results.size(), 1);

  Tellico::Data::EntryPtr entry = results.at(0);
  QCOMPARE(entry->collection()->type(), Tellico::Data::Collection::Game);
  QCOMPARE(entry->field(QLatin1String("title")), QLatin1String("Mass Effect 3: Citadel"));
  QCOMPARE(entry->field(QLatin1String("developer")), QLatin1String("BioWare"));
  QCOMPARE(entry->field(QLatin1String("publisher")), QLatin1String("Electronic Arts Inc. (EA)"));
  QCOMPARE(entry->field(QLatin1String("year")), QLatin1String("2013"));
//  QCOMPARE(entry->field(QLatin1String("platform")), QLatin1String("PlayStation3"));
  QCOMPARE(set(entry, "genre"), set("Action RPG; Shooter"));
  // the cover image got removed for some reason
//  QVERIFY(!entry->field(QLatin1String("cover")).isEmpty());
  QVERIFY(!entry->field(QLatin1String("description")).isEmpty());
}
