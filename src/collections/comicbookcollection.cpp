/***************************************************************************
    Copyright (C) 2003-2016 Robby Stephenson <robby@periapsis.org>
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

#include "comicbookcollection.h"
#include "../entrycomparison.h"

#include <KLocalizedString>

namespace {
  static const char* comic_general = I18N_NOOP("General");
  static const char* comic_publishing = I18N_NOOP("Publishing");
  static const char* comic_classification = I18N_NOOP("Classification");
  static const char* comic_personal = I18N_NOOP("Personal");
}

using Tellico::Data::ComicBookCollection;

ComicBookCollection::ComicBookCollection(bool addDefaultFields_, const QString& title_)
   : Collection(title_.isEmpty() ? i18n("My Comic Books") : title_) {
  setDefaultGroupField(QLatin1String("series"));
  if(addDefaultFields_) {
    addFields(defaultFields());
  }
}

Tellico::Data::FieldList ComicBookCollection::defaultFields() {
  FieldList list;
  FieldPtr field;

  list.append(Field::createDefaultField(Field::TitleField));

  field = new Field(QLatin1String("subtitle"), i18n("Subtitle"));
  field->setCategory(i18n(comic_general));
  field->setFormatType(FieldFormat::FormatTitle);
  list.append(field);

  field = new Field(QLatin1String("writer"), i18n("Writer"));
  field->setCategory(i18n(comic_general));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatType(FieldFormat::FormatName);
  list.append(field);

  field = new Field(QLatin1String("artist"), i18nc("Comic Book Illustrator", "Artist"));
  field->setCategory(i18n(comic_general));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatType(FieldFormat::FormatName);
  list.append(field);

  field = new Field(QLatin1String("series"), i18n("Series"));
  field->setCategory(i18n(comic_general));
  field->setFlags(Field::AllowCompletion | Field::AllowGrouped);
  field->setFormatType(FieldFormat::FormatTitle);
  list.append(field);

  field = new Field(QLatin1String("issue"), i18n("Issue"), Field::Number);
  field->setCategory(i18n(comic_general));
  field->setFlags(Field::AllowMultiple);
  list.append(field);

  field = new Field(QLatin1String("publisher"), i18n("Publisher"));
  field->setCategory(i18n(comic_publishing));
  field->setFlags(Field::AllowCompletion | Field::AllowGrouped);
  field->setFormatType(FieldFormat::FormatPlain);
  list.append(field);

  field = new Field(QLatin1String("edition"), i18n("Edition"));
  field->setCategory(i18n(comic_publishing));
  field->setFlags(Field::AllowCompletion);
  field->setFormatType(FieldFormat::FormatPlain);
  list.append(field);

  field = new Field(QLatin1String("pub_year"), i18n("Publication Year"),  Field::Number);
  field->setCategory(i18n(comic_publishing));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QLatin1String("pages"), i18n("Pages"), Field::Number);
  field->setCategory(i18n(comic_publishing));
  list.append(field);

  field = new Field(QLatin1String("country"), i18n("Country"));
  field->setCategory(i18n(comic_publishing));
  field->setFlags(Field::AllowCompletion | Field::AllowGrouped | Field::AllowMultiple);
  field->setFormatType(FieldFormat::FormatPlain);
  list.append(field);

  field = new Field(QLatin1String("language"), i18n("Language"));
  field->setCategory(i18n(comic_publishing));
  field->setFlags(Field::AllowCompletion | Field::AllowGrouped | Field::AllowMultiple);
  field->setFormatType(FieldFormat::FormatPlain);
  list.append(field);

  field = new Field(QLatin1String("genre"), i18n("Genre"));
  field->setCategory(i18n(comic_classification));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatType(FieldFormat::FormatPlain);
  list.append(field);

  field = new Field(QLatin1String("keyword"), i18n("Keywords"));
  field->setCategory(i18n(comic_classification));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  list.append(field);

  QStringList cond = i18nc("Comic book grade levels - "
                           "Mint,Near Mint,Very Fine,Fine,Very Good,Good,Fair,Poor",
                           "Mint,Near Mint,Very Fine,Fine,Very Good,Good,Fair,Poor")
                     .split(QRegExp(QLatin1String("\\s*,\\s*")), QString::SkipEmptyParts);
  field = new Field(QLatin1String("condition"), i18n("Condition"), cond);
  field->setCategory(i18n(comic_classification));
  list.append(field);

  field = new Field(QLatin1String("pur_date"), i18n("Purchase Date"));
  field->setCategory(i18n(comic_personal));
  field->setFormatType(FieldFormat::FormatDate);
  list.append(field);

  field = new Field(QLatin1String("pur_price"), i18n("Purchase Price"));
  field->setCategory(i18n(comic_personal));
  list.append(field);

  field = new Field(QLatin1String("signed"), i18n("Signed"), Field::Bool);
  field->setCategory(i18n(comic_personal));
  list.append(field);

  field = new Field(QLatin1String("gift"), i18n("Gift"), Field::Bool);
  field->setCategory(i18n(comic_personal));
  list.append(field);

  field = new Field(QLatin1String("loaned"), i18n("Loaned"), Field::Bool);
  field->setCategory(i18n(comic_personal));
  list.append(field);

  field = new Field(QLatin1String("rating"), i18n("Rating"), Field::Rating);
  field->setCategory(i18n(comic_personal));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QLatin1String("cover"), i18n("Front Cover"), Field::Image);
  list.append(field);

  field = new Field(QLatin1String("plot"), i18n("Plot Summary"), Field::Para);
  list.append(field);

  field = new Field(QLatin1String("comments"), i18n("Comments"), Field::Para);
  list.append(field);

  list.append(Field::createDefaultField(Field::IDField));
  list.append(Field::createDefaultField(Field::CreatedDateField));
  list.append(Field::createDefaultField(Field::ModifiedDateField));

  return list;
}

int ComicBookCollection::sameEntry(Tellico::Data::EntryPtr entry1_, Tellico::Data::EntryPtr entry2_) const {
  if(!entry1_ || !entry2_) {
    return 0;
  }
  // equal isbn's or lccn's are easy, give it a weight of 100
  // special for Bedetheque links for match
  if(EntryComparison::score(entry1_, entry2_, QLatin1String("isbn"), this) > 0 ||
     EntryComparison::score(entry1_, entry2_, QLatin1String("lccn"), this) > 0) {
    return 100; // great match
  }
  // special for Bedetheque links for match
  if(EntryComparison::score(entry1_, entry2_, QLatin1String("lien-bel"), this) > 0) {
    return 100; // great match
  }
  int res = 3*EntryComparison::score(entry1_, entry2_, QLatin1String("title"), this);
  res += 2*EntryComparison::score(entry1_, entry2_, QLatin1String("series"), this);
  res += 2*EntryComparison::score(entry1_, entry2_, QLatin1String("writer"), this);
  res += EntryComparison::score(entry1_, entry2_, QLatin1String("artist"), this);
  res += EntryComparison::score(entry1_, entry2_, QLatin1String("issue"), this);
  res += EntryComparison::score(entry1_, entry2_, QLatin1String("publisher"), this);
  res += EntryComparison::score(entry1_, entry2_, QLatin1String("pub_year"), this);
  return res;
}
