/***************************************************************************
    Copyright (C) 2015 Robby Stephenson <robby@periapsis.org>
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

#include "htmlexportertest.h"

#include "../translators/htmlexporter.h"
#include "../collections/bookcollection.h"
#include "../collectionfactory.h"
#include "../entry.h"
#include "../document.h"
#include "../images/imagefactory.h"
#include "../utils/datafileregistry.h"
#include "../config/tellico_config.h"

#include <QTest>
#include <QRegExp>
#include <QTemporaryDir>

QTEST_GUILESS_MAIN( HtmlExporterTest )

void HtmlExporterTest::initTestCase() {
  Tellico::ImageFactory::init();
  Tellico::RegisterCollection<Tellico::Data::BookCollection> registerBook(Tellico::Data::Collection::Book, "book");
  Tellico::DataFileRegistry::self()->addDataLocation(QFINDTESTDATA("../../xslt/tellico2html.xsl"));
  Tellico::DataFileRegistry::self()->addDataLocation(QFINDTESTDATA("../../xslt/entry-templates/Fancy.xsl"));
  Tellico::DataFileRegistry::self()->addDataLocation(QFINDTESTDATA("../../xslt/report-templates/Column_View.xsl"));
}

void HtmlExporterTest::cleanupTestCase() {
  Tellico::ImageFactory::clean(true);
}

void HtmlExporterTest::testHtml() {
  Tellico::Config::setImageLocation(Tellico::Config::ImagesInLocalDir);
  // the default collection will use a temporary directory as a local image dir
  QVERIFY(!Tellico::ImageFactory::localDir().isEmpty());

  QString tempDirName;
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  tempDir.setAutoRemove(true);
  tempDirName = tempDir.path();
  QString fileName = tempDirName + "/with-image.tc";
  QString imageDirName = tempDirName + "/with-image_files/";

  // copy a collection file that includes an image into the temporary directory
  QVERIFY(QFile::copy(QFINDTESTDATA("data/with-image.tc"), fileName));

  Tellico::Data::Document* doc = Tellico::Data::Document::self();
  QVERIFY(doc->openDocument(QUrl::fromLocalFile(fileName)));
  QCOMPARE(Tellico::ImageFactory::localDir(), imageDirName);
  // save the document, so the images get copied out of the .tc file into the local image directory
  QVERIFY(doc->saveDocument(QUrl::fromLocalFile(fileName)));

  Tellico::Data::CollPtr coll = doc->collection();
  QVERIFY(coll);

  Tellico::Export::HTMLExporter exp(coll);
  exp.setEntries(coll->entries());
  exp.setExportEntryFiles(true);
  exp.setEntryXSLTFile(QLatin1String("Fancy"));
  exp.setColumns(QStringList() << QLatin1String("Title") << QLatin1String("Gift")
                               << QLatin1String("Rating") << QLatin1String("Front Cover"));
  exp.setURL(QUrl::fromLocalFile(tempDirName + "/testHtml.html"));

  QString output = exp.text();
  QVERIFY(!output.isEmpty());

  // verify the relative location of the tellico2html.js file
  QVERIFY(output.contains(QLatin1String("src=\"testHtml_files/tellico2html.js")));
  // verify relative location of image pics
  QVERIFY(output.contains(QLatin1String("src=\"testHtml_files/pics/checkmark.png")));
  // verify relative location of entry link
  QVERIFY(output.contains(QLatin1String("href=\"testHtml_files/Catching_Fire__The_Second_Book_of_the_Hunger_Games_-1.html")));
  // verify relative location of image file
  QVERIFY(output.contains(QLatin1String("src=\"testHtml_files/17b54b2a742c6d342a75f122d615a793.jpeg")));

  QVERIFY(exp.exec());
  QFile f(tempDirName + "/testHtml.html");
  QVERIFY(f.exists());
  QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream in(&f);
  QString fileText = in.readAll();
  QVERIFY(fileText.contains(QLatin1String("src=\"testHtml_files/tellico2html.js")));
  QVERIFY(fileText.contains(QLatin1String("src=\"testHtml_files/pics/checkmark.png")));
  QVERIFY(fileText.contains(QLatin1String("href=\"testHtml_files/Catching_Fire__The_Second_Book_of_the_Hunger_Games_-1.html")));
  QVERIFY(fileText.contains(QLatin1String("src=\"testHtml_files/17b54b2a742c6d342a75f122d615a793.jpeg")));

  QVERIFY(QFile::exists(tempDirName + "/testHtml_files/tellico2html.js"));
  QVERIFY(QFile::exists(tempDirName + "/testHtml_files/pics/checkmark.png"));
  QVERIFY(QFile::exists(tempDirName + "/testHtml_files/17b54b2a742c6d342a75f122d615a793.jpeg"));

  // check entry html output
  QFile f2(tempDirName + "/testHtml_files/Catching_Fire__The_Second_Book_of_the_Hunger_Games_-1.html");
  QVERIFY(f2.exists());
  QVERIFY(f2.open(QIODevice::ReadOnly | QIODevice::Text));

  QTextStream in2(&f2);
  QString entryText = in2.readAll();
  // verify relative location of image file
  QVERIFY(entryText.contains(QLatin1String("src=\"./17b54b2a742c6d342a75f122d615a793.jpeg")));
  // verify relative location of image pics
  QVERIFY(entryText.contains(QLatin1String("src=\"pics/checkmark.png")));
  // verify link to parent html file
  QVERIFY(entryText.contains(QLatin1String("href=\"../testHtml.html")));

  // sanity check, the directory should not exists after QTemporaryDir destruction
  tempDir.remove();
  QVERIFY(!QDir(tempDirName).exists());
}

void HtmlExporterTest::testHtmlTitle() {
  Tellico::Data::CollPtr coll(new Tellico::Data::BookCollection(true));
  coll->setTitle(QLatin1String("Robby's Books"));

  Tellico::Data::EntryPtr e(new Tellico::Data::Entry(coll));
  coll->addEntries(e);

  Tellico::Export::HTMLExporter exporter(coll);
  exporter.setEntries(coll->entries());

  QString output = exporter.text();
//  qDebug() << output;
  QVERIFY(!output.isEmpty());

  // check https://bugs.kde.org/show_bug.cgi?id=348381
  QRegExp rx("<title>.*</title>");
  rx.setMinimal(true);
  QVERIFY(output.contains(rx));
  QCOMPARE(rx.cap(), QLatin1String("<title>Robby's Books</title>"));
}

void HtmlExporterTest::testReportHtml() {
  Tellico::Data::CollPtr coll(new Tellico::Data::BookCollection(true));
  coll->setTitle(QLatin1String("Robby's Books"));

  Tellico::Data::EntryPtr e(new Tellico::Data::Entry(coll));
  e->setField(QLatin1String("title"), QLatin1String("My Title"));
  e->setField(QLatin1String("rating"), QLatin1String("3"));
  coll->addEntries(e);

  Tellico::Export::HTMLExporter exporter(coll);
  exporter.setXSLTFile(QFINDTESTDATA("../../xslt/report-templates/Column_View.xsl"));
  exporter.setEntries(coll->entries());

  QString output = exporter.text();
  QVERIFY(!output.isEmpty());

  // check that cdate is passed correctly
  QRegExp rx("<p id=\"header-right\">(.*)</p>");
  rx.setMinimal(true);
  QVERIFY(output.contains(rx));
  QCOMPARE(rx.cap(1), QLocale().toString(QDate::currentDate()));

  // test image location in tmp directory
  Tellico::Export::HTMLExporter exporter2(coll);
  exporter2.setXSLTFile(QFINDTESTDATA("../../xslt/report-templates/Image_List.xsl"));
  exporter2.setEntries(coll->entries());
  exporter2.setColumns(QStringList() << QLatin1String("Title") << QLatin1String("Rating"));

  QString output2 = exporter2.text();
  QVERIFY(!output2.isEmpty());
  // the rating pic image needs to be an absolute local path, starting with "/"
  QVERIFY(output2.contains(QRegExp(QLatin1String("src=\"/[^\"]+stars3.png"))));
}

void HtmlExporterTest::testDirectoryNames() {
  Tellico::Data::CollPtr coll(new Tellico::Data::BookCollection(true));
  Tellico::Export::HTMLExporter exp(coll);

  exp.setURL(QUrl::fromLocalFile(QDir::homePath() + QLatin1String("/test.html")));
  QCOMPARE(exp.fileDir(), QUrl::fromLocalFile(QDir::homePath() + QLatin1String("/test_files/")));
  QCOMPARE(exp.fileDirName(), QLatin1String("test_files/"));

  // setCollectionUrl used when exporting entry files only
  exp.setCollectionURL(QUrl::fromLocalFile(QDir::homePath()));
  QCOMPARE(exp.fileDirName(), QLatin1String("/"));
}
