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

#ifndef WEBFILEUTILS_H
#define WEBFILEUTILS_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QNetworkReply>

class webFileUtils : public QObject
{
    Q_OBJECT
public:
    struct webFile {
        QUrl url;
        QString name;
    };

    webFileUtils(QObject *parent);
    ~webFileUtils();
    QList<webFile> getWebFiles(QUrl url);
    void startFileDownload(QUrl url);
    QByteArray getDownloadedByteArray() const;
    bool saveDownloadedFile(QString path, QString &fileName, QString &completePath);
signals:
    void downloaded(bool success, QByteArray data, QString, QNetworkReply::NetworkError);
    void downloadProgress(qint64, qint64);
private slots:
    void fileDownloaded(QNetworkReply* pReply);
private:
    QNetworkRequest request;
    QNetworkAccessManager m_WebCtrl;
    QByteArray m_DownloadedData;
    QString currentFilename;  
    QString m_localFilename;
    QString m_remoteFilename;
};
#endif // WEBFILEUTILS_H
