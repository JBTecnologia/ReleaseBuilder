/**
 ******************************************************************************
 * @file
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @addtogroup [Group]
 * @{
 * @addtogroup
 * @{
 * @brief [Brief]
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "webfileutils.h"
#include <QWebPage>
#include <QWebFrame>
#include <QDebug>
#include <QWebElementCollection>
#include <QTimer>
#include <QEventLoop>
#include <QDir>

webFileUtils::webFileUtils(QObject *parent):QObject(parent)
{
    connect(&m_WebCtrl, SIGNAL(finished(QNetworkReply*)),
            SLOT(fileDownloaded(QNetworkReply*)));
}

webFileUtils::~webFileUtils()
{

}

QList<webFileUtils::webFile> webFileUtils::getWebFiles(QUrl url)
{
    QList<webFileUtils::webFile> list;
    QWebPage *webPage = new QWebPage(this);
    QEventLoop loop;
    connect(webPage->mainFrame(), SIGNAL(loadFinished(bool)), &loop, SLOT(quit()));
    webPage->mainFrame()->load(url);
    loop.exec();
    QUrl baseUrl = webPage->mainFrame()->baseUrl();

    QWebElementCollection collection = webPage->mainFrame()->findAllElements("a");
    foreach (QWebElement element, collection)
    {
        QString href = element.attribute("href");
        if (!href.isEmpty())
        {
            QUrl relativeUrl(href);
            QUrl absoluteUrl = baseUrl.resolved(relativeUrl);
            if(element.toPlainText().contains("exe") | element.toPlainText().contains("tar.xz") | element.toPlainText().contains("zip")) {
                webFile file;
                file.name = element.toPlainText();
                file.url = absoluteUrl;
                list.append(file);
            }
        }
    }
    return list;
}

void webFileUtils::fileDownloaded(QNetworkReply* pReply)
{
    bool success = true;
    if(pReply->error() != QNetworkReply::NoError)
        success = false;
    else
        m_DownloadedData = pReply->readAll();
    pReply->deleteLater();
    emit downloaded(success, m_DownloadedData, pReply->errorString(), pReply->error());
}

QByteArray webFileUtils::getDownloadedByteArray() const
{
    return m_DownloadedData;
}

bool webFileUtils::saveDownloadedFile(QString path, QString &fileName, QString &completePath)
{
    fileName = currentFilename;
    if(!path.endsWith(QDir::separator()))
        path.append(QDir::separator());
    if(!QDir().mkpath(path))
        return false;
    QString filename = path + currentFilename;
    completePath = filename;
    QFile localFile(filename);
    if (!localFile.open(QIODevice::WriteOnly))
            return false;
    localFile.write(m_DownloadedData);
    localFile.close();
    return true;
}

void webFileUtils::startFileDownload(QUrl url)
{
    currentFilename = url.fileName();
    request = QNetworkRequest(url);
    QNetworkReply *reply = m_WebCtrl.get(request);
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64, qint64)));
}
