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

#ifndef XMLPARSER_H
#define XMLPARSER_H

#include <QObject>
#include <QHash>
#include <QUrl>
#include <QDate>
#include <QDomElement>

class xmlParser : public QObject
{
    Q_OBJECT
public:
    enum softTypeEnum {SOFT_GCS, SOFT_SLIM_GCS, SOFT_UPDATER, SOFT_FIRMWARE, SOFT_BOOTLOADER, SOFT_SETTINGS};
    enum osTypeEnum {OS_WIN32, OS_WIN64, OS_OSX32, OS_OSX64, OS_LINUX32, OS_LINUX64, OS_EMBEDED};
    enum releaseTypeEnum {RELEASE_CURRENT, RELEASE_TEST, RELEASE_OLD};
    struct softData {
        softTypeEnum type;
        osTypeEnum osType;
        quint16 hwType;
        QString name;
        QUrl packageLink;
        QUrl releaseLink;
        QUrl scriptLink;
        QDate date;
        QString uavHash;
        QString md5;
    };
    xmlParser(QObject *parent = NULL);
    ~xmlParser();
    QMultiHash<releaseTypeEnum, softData> parseXML(QString xml, bool &success);
    void processOSTypeList(QDomDocument *doc, QDomElement *element, QList<xmlParser::softData> data);
    void processSoftType(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data);
    QString convertSoftDataToXMLString(QMultiHash<releaseTypeEnum, softData> list);
    static QList<xmlParser::osTypeEnum> osTypesList();
    static QString softTypeToString(xmlParser::softTypeEnum type);
    static QString osTypeToString(xmlParser::osTypeEnum type);
    static QHash<xmlParser::softTypeEnum, QString> softTypeToStringHash;
    static QHash<xmlParser::osTypeEnum, QString> osTypeToStringHash;
    static QHash<int, QString> hwTypeToStringHash;
    QMultiHash<releaseTypeEnum, softData> temp();


    QHash<xmlParser::osTypeEnum, QString> osHashInit();
    QHash<xmlParser::softTypeEnum, QString> softHashInit();
    QHash<int, QString> hwTypeInit();
signals:
    void outputMessage(QString);
private:
    void xmlAddFields(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data);
    void processHwType(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data);
    xmlParser::softData assembleSoftData(osTypeEnum os, softTypeEnum soft, quint16 hwID, QDomElement element);
    QList<QDomElement> nodesToElementList(QDomNodeList list);
    QDomDocument tempDoc;
    QByteArray array;
    QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> tempv;

};
#endif // XMLPARSER_H
