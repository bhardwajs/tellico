/***************************************************************************
    Copyright (C) 2001-2009 Robby Stephenson <robby@periapsis.org>
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

#include <config.h>

#include "mainwindow.h"
#include "translators/translators.h" // needed for file type enum

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  KAboutData aboutData(QLatin1String("tellico"), QLatin1String("Tellico"),
                       QLatin1String(TELLICO_VERSION), i18n("Tellico - a collection manager for KDE"),
                       KAboutLicense::GPL_V2,
                       i18n("(c) 2001-2015, Robby Stephenson"),
                       QString(),
                       QLatin1String("http://tellico-project.org"),
                       QLatin1String("tellico-users@kde.org"));
  aboutData.addAuthor(QLatin1String("Robby Stephenson"), QString(), QLatin1String("robby@periapsis.org"));
  aboutData.addAuthor(QLatin1String("Mathias Monnerville"), i18n("Data source scripts"));
  aboutData.addAuthor(QLatin1String("Regis Boudin"), QString(), QLatin1String("regis@boudin.name"));
  aboutData.addAuthor(QLatin1String("Petri Damstén"), QString(), QLatin1String("damu@iki.fi"));
  aboutData.addAuthor(QLatin1String("Sebastian Held"), QString());

  aboutData.addCredit(QLatin1String("Virginie Quesnay"), i18n("Icons"));
  aboutData.addCredit(QLatin1String("Amarok"), i18n("Code examples and general inspiration"),
                      QString(), QLatin1String("http://amarok.kde.org"));
  aboutData.addCredit(QLatin1String("Greg Ward"), i18n("Author of btparse library"));
  aboutData.addCredit(QLatin1String("Robert Gamble"), i18n("Author of libcsv library"));
  aboutData.addCredit(QLatin1String("Valentin Lavrinenko"), i18n("Author of rtf2html library"));

  aboutData.addLicense(KAboutLicense::GPL_V3);

  QCommandLineParser parser;
  parser.addVersionOption();
  parser.addHelpOption();
  parser.addOption(QCommandLineOption(QStringList() << QLatin1String("nofile"), i18n("Do not reopen the last open file")));
  parser.addOption(QCommandLineOption(QStringList() << QLatin1String("bibtex"), i18n("Import <filename> as a bibtex file")));
  parser.addOption(QCommandLineOption(QStringList() << QLatin1String("mods"), i18n("Import <filename> as a MODS file")));
  parser.addOption(QCommandLineOption(QStringList() << QLatin1String("ris"), i18n("Import <filename> as a RIS file")));
  parser.addPositionalArgument(QLatin1String("[filename]"), i18n("File to open"));

  aboutData.setupCommandLine(&parser);

  parser.process(app);
  aboutData.processCommandLine(&parser);
  KAboutData::setApplicationData(aboutData);

  if(app.isSessionRestored()) {
    RESTORE(Tellico::MainWindow);
  } else {
    Tellico::MainWindow* tellico = new Tellico::MainWindow();
    tellico->show();
    tellico->slotShowTipOfDay(false);

    QStringList args = parser.positionalArguments();
    if(args.count() > 0) {
      if(parser.isSet(QLatin1String("bibtex"))) {
        tellico->importFile(Tellico::Import::Bibtex, args.at(0), Tellico::Import::Replace);
      } else if(parser.isSet(QLatin1String("mods"))) {
        tellico->importFile(Tellico::Import::MODS, args.at(0), Tellico::Import::Replace);
      } else if(parser.isSet(QLatin1String("ris"))) {
        tellico->importFile(Tellico::Import::RIS, args.at(0), Tellico::Import::Replace);
      } else {
        tellico->slotFileOpen(args.at(0));
      }
    } else {
      // bit of a hack, I just want the --nofile option
      // if --nofile is NOT passed, then the file option is set
      // is it's set, then go ahead and check for opening previous file
      tellico->initFileOpen(!parser.isSet(QLatin1String("nofile")));
    }
  }

  return app.exec();
}
