/***************************************************************************
    Copyright (C) 2005-2009 Robby Stephenson <robby@periapsis.org>
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

#include "urlfieldwidget.h"
#include "../field.h"
#include "../tellico_kernel.h"

#include <KLineEdit>
#include <KUrlRequester>
#include <KUrlLabel>

#include <QUrl>
#include <QDesktopServices>

using Tellico::GUI::URLFieldWidget;

// subclass of KUrlCompletion is needed so the KUrlLabel
// can open relative links. I don't want to have to have to update
// the base directory of the completion every time a new document is opened
QString URLFieldWidget::URLCompletion::makeCompletion(const QString& text_) {
  // KUrlCompletion::makeCompletion() uses an internal variable instead
  // of calling KUrlCompletion::dir() so need to set the base dir before completing
  setDir(Kernel::self()->URL().adjusted(QUrl::PreferLocalFile | QUrl::RemoveFilename));
  return KUrlCompletion::makeCompletion(text_);
}

URLFieldWidget::URLFieldWidget(Tellico::Data::FieldPtr field_, QWidget* parent_)
    : FieldWidget(field_, parent_) {

  m_requester = new KUrlRequester(this);
  m_requester->lineEdit()->setCompletionObject(new URLCompletion());
  m_requester->lineEdit()->setAutoDeleteCompletionObject(true);
  connect(m_requester, SIGNAL(textChanged(const QString&)), SLOT(checkModified()));
  connect(m_requester, SIGNAL(textChanged(const QString&)), label(), SLOT(setUrl(const QString&)));
  connect(label(), SIGNAL(leftClickedUrl(const QString&)), SLOT(slotOpenURL(const QString&)));
  registerWidget();

  // special case, remember if it's a relative url
  m_isRelative = field_->property(QLatin1String("relative")) == QLatin1String("true");
}

URLFieldWidget::~URLFieldWidget() {
}

QString URLFieldWidget::text() const {
  if(m_isRelative && Kernel::self()->URL().isLocalFile()) {
    //KURl::relativeUrl() has no QUrl analog
    QUrl base_url = Kernel::self()->URL();
    QUrl url = m_requester->url();
    //return Kernel::self()->URL().resolved(m_requester->url());
    return QDir(base_url.path()).relativeFilePath(url.path());
  }
  // for comparison purposes and to be consistent with the file listing importer
  // I want the full url here, including the protocol
  // the requester only returns the path, so create a QUrl
  // TODO: 2015-04-30 no longer necessary in KF5/Qt5?
  //return QUrl(m_requester->url()).url();
  return m_requester->url().url();
}

void URLFieldWidget::setTextImpl(const QString& text_) {
  m_requester->setUrl(QUrl::fromUserInput(text_));
  static_cast<KUrlLabel*>(label())->setUrl(text_);
}

void URLFieldWidget::clearImpl() {
  m_requester->clear();
  editMultiple(false);
}

void URLFieldWidget::updateFieldHook(Tellico::Data::FieldPtr, Tellico::Data::FieldPtr newField_) {
  m_isRelative = newField_->property(QLatin1String("relative")) == QLatin1String("true");
}

void URLFieldWidget::slotOpenURL(const QString& url_) {
  if(url_.isEmpty()) {
    return;
  }
  QDesktopServices::openUrl(m_isRelative ?
                            Kernel::self()->URL().resolved(QUrl::fromUserInput(url_)) :
                            QUrl::fromUserInput(url_));
}

QWidget* URLFieldWidget::widget() {
  return m_requester;
}
