/***************************************************************************
    Copyright (C) 2003-2009 Robby Stephenson <robby@periapsis.org>
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

#ifndef TELLICO_BIBTEXHANDLER_H
#define TELLICO_BIBTEXHANDLER_H

#include "../datavectors.h"

#include <QStringList>
#include <QHash>
#include <QRegExp>

namespace Tellico {

/**
 * @author Robby Stephenson
 */
class BibtexHandler {
public:
  enum QuoteStyle { BRACES=0, QUOTES=1 };
  static QStringList bibtexKeys(const Data::EntryList& entries);
  static QString bibtexKey(Data::EntryPtr entry);
  static QString importText(char* text);
  static QString exportText(const QString& text, const QStringList& macros);
  /**
   * Strips the text of all vestiges of LaTeX.
   *
   * @param text A reference to the text
   * @return A reference to the text
   */
  static QString& cleanText(QString& text);

  static QuoteStyle s_quoteStyle;

private:
  typedef QHash<QString, QStringList> StringListHash;

  static QString bibtexKey(const QString& author, const QString& title, const QString& year);
  static void loadTranslationMaps();
  static QString addBraces(const QString& string);

  static StringListHash s_utf8LatexMap;
  static const QRegExp s_badKeyChars;
};

} // end namespace
#endif
