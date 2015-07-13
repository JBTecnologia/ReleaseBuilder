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

#include "xmlparser.h"
#include <QDomDocument>
#include <QFile>
#include <QDebug>
#include <QProcess>

QHash<xmlParser::softTypeEnum, QString> xmlParser::softTypeToStringHash = xmlParser().softHashInit();
QHash<xmlParser::osTypeEnum, QString> xmlParser::osTypeToStringHash = xmlParser().osHashInit();
QHash<int, QString> xmlParser::hwTypeToStringHash = xmlParser().hwTypeInit();


xmlParser::xmlParser(QObject *parent):QObject(parent)
{


}

xmlParser::~xmlParser()
{

}

QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> xmlParser::parseXML(QString xml, bool &success)
{
    qDebug() << "Parsing:" << xml;
    releaseTypeEnum releaseType;
    osTypeEnum osType;
    softTypeEnum softType;
    QMultiHash<releaseTypeEnum, xmlParser::softData> ret;
    QDomDocument *doc = new QDomDocument();
    QString error;
    int line = 0;
    int column = 0;
    if(!doc->setContent(xml, &error, &line, &column)) {
        emit outputMessage(QString("Error(%0) parsing xml on line %1 column %2").arg(error).arg(line).arg(column));
        QStringList lines = xml.split(QRegExp("[\r\n]"),QString::SkipEmptyParts);
        if(line > 1)
            emit outputMessage(lines.at(line - 1));
        emit outputMessage(lines.at(line));
        emit outputMessage(lines.at(line + 1));
        success = false;
        return QMultiHash<releaseTypeEnum, xmlParser::softData>();
    }
    QDomNode root = doc->firstChild();
    foreach (QDomElement release, nodesToElementList(root.childNodes())) {
        qDebug() << "Release type:" << release.tagName();
        if(release.tagName() == "current_release")
            releaseType = RELEASE_CURRENT;
        else if(release.tagName() == "old_release")
            releaseType = RELEASE_OLD;
        else if(release.tagName() == "test_release")
            releaseType = RELEASE_TEST;
        else {
            success = false;
            return QMultiHash<releaseTypeEnum, xmlParser::softData>();
        }
        foreach(QDomElement os, nodesToElementList(release.childNodes())) {
            qDebug() << "OS:" << os.tagName();
            if(os.tagName() == "win32")
                osType = OS_WIN32;
            else if(os.tagName() == "win64")
                osType = OS_WIN64;
            else if(os.tagName() == "osx32")
                osType = OS_OSX32;
            else if(os.tagName() == "osx64")
                osType = OS_OSX64;
            else if(os.tagName() == "linux32")
                osType = OS_LINUX32;
            else if(os.tagName() == "linux64")
                osType = OS_LINUX64;
            else if(os.tagName() == "embeded")
                osType = OS_EMBEDED;
            else {
                success = false;
                return QMultiHash<releaseTypeEnum, xmlParser::softData>();
            }
            foreach (QDomElement soft, nodesToElementList(os.childNodes())) {
                qDebug() << "Software type:" << soft.tagName();
                if  (soft.tagName() == "gcs")
                    softType = SOFT_GCS;
                else if (soft.tagName() == "slim_gcs")
                    softType = SOFT_SLIM_GCS;
                else if (soft.tagName() == "updater")
                    softType = SOFT_UPDATER;
                else if (soft.tagName().startsWith("t")) {
                    foreach (QDomElement embeded, nodesToElementList(soft.childNodes())) {
                        qDebug() << "T:" << embeded.tagName();
                        foreach (QDomElement s, nodesToElementList(embeded.childNodes())) {
                            qDebug() << "T Softype:" << embeded.tagName();
                            if(embeded.tagName() == "firmware")
                                softType = SOFT_FIRMWARE;
                            else if(embeded.tagName() == "settings")
                                softType = SOFT_SETTINGS;
                            else if(embeded.tagName() == "bootloader")
                                softType = SOFT_BOOTLOADER;
                            else {
                                success = false;
                                return QMultiHash<releaseTypeEnum, xmlParser::softData>();
                            }
                            ret.insert(releaseType, assembleSoftData(osType, softType, soft.tagName().remove(0, 1).toInt(), s));
                        }
                    }
                }
                else {
                    success = false;
                    return QMultiHash<releaseTypeEnum, xmlParser::softData>();
                }
                if ((softType != SOFT_FIRMWARE) && (softType != SOFT_SETTINGS) && (softType != SOFT_BOOTLOADER)) {
                    foreach (QDomElement child, nodesToElementList(soft.childNodes())) {
                        ret.insert(releaseType, assembleSoftData(osType, softType, 0, child));
                    }
                }
            }
        }
    }
    delete doc;
    success = true;
    return ret;
}

xmlParser::softData xmlParser::assembleSoftData(osTypeEnum os, softTypeEnum soft, quint16 hwID, QDomElement element)
{
    xmlParser::softData data;
    data.osType = os;
    data.type = soft;
    data.hwType = hwID;
    data.name = element.attribute("name");
    data.uavHash = element.attribute("uavohash");
    data.md5 = element.attribute("md5");
    data.packageLink = QUrl(element.attribute("packageLink"));
    data.releaseLink = QUrl(element.attribute("releaseLink"));
    data.scriptLink = QUrl(element.attribute("scriptLink"));
    data.date = QDate::fromString(element.tagName().remove(0, 1), "ddMMyyyy");
    return data;
}

void xmlParser::processOSTypeList(QDomDocument *doc, QDomElement *element, QList<xmlParser::softData> data)
{
    QMultiHash<osTypeEnum, xmlParser::softData> groupByOS;

    foreach (xmlParser::softData item, data) {
            groupByOS.insert(item.osType, item);
    }
    if (groupByOS.values(OS_WIN32).length()) {
        QDomElement OSWin32 = doc->createElement("win32");
        element->appendChild(OSWin32);
        processSoftType(doc, OSWin32, groupByOS.values(OS_WIN32));
    }
    if (groupByOS.values(OS_WIN64).length()) {
        QDomElement OSWin64 = doc->createElement("win64");
        element->appendChild(OSWin64);
        processSoftType(doc, OSWin64, groupByOS.values(OS_WIN64));
    }
    if (groupByOS.values(OS_OSX32).length()) {
        QDomElement OSOsx32 = doc->createElement("osx32");
        element->appendChild(OSOsx32);
        processSoftType(doc, OSOsx32, groupByOS.values(OS_OSX32));
    }
    if (groupByOS.values(OS_OSX64).length()) {
        QDomElement OSOsx64 = doc->createElement("osx64");
        element->appendChild(OSOsx64);
        processSoftType(doc, OSOsx64, groupByOS.values(OS_OSX64));
    }
    if (groupByOS.values(OS_LINUX32).length()) {
        QDomElement OSLinux32 = doc->createElement("linux32");
        element->appendChild(OSLinux32);
        processSoftType(doc, OSLinux32, groupByOS.values(OS_LINUX32));
    }
    if (groupByOS.values(OS_LINUX64).length()) {
        QDomElement OSLinux64 = doc->createElement("linux64");
        element->appendChild(OSLinux64);
        processSoftType(doc, OSLinux64, groupByOS.values(OS_LINUX64));
    }
    if (groupByOS.values(OS_EMBEDED).length()) {
        QDomElement OSEmbeded = doc->createElement("embeded");
        element->appendChild(OSEmbeded);
        processHwType(doc, OSEmbeded, groupByOS.values(OS_EMBEDED));
    }
}
void xmlParser::processSoftType(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data)
{
    QMultiHash<softTypeEnum, xmlParser::softData> groupBySoftType;

    foreach (xmlParser::softData item, data) {
            groupBySoftType.insert(item.type, item);
    }

    if(groupBySoftType.values(SOFT_GCS).length()) {
        QDomElement Gcs = doc->createElement("gcs");
        element.appendChild(Gcs);
        xmlAddFields(doc, Gcs, groupBySoftType.values(SOFT_GCS));
    }
    if(groupBySoftType.values(SOFT_SLIM_GCS).length()) {
        QDomElement Tbs = doc->createElement("slim_gcs");
        element.appendChild(Tbs);
        xmlAddFields(doc, Tbs, groupBySoftType.values(SOFT_SLIM_GCS));
    }
    if(groupBySoftType.values(SOFT_UPDATER).length()) {
        QDomElement Updater = doc->createElement("updater");
        element.appendChild(Updater);
        xmlAddFields(doc, Updater, groupBySoftType.values(SOFT_UPDATER));
    }
}
void xmlParser::processHwType(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data)
{
    QMultiHash<quint16, xmlParser::softData> groupByHwType;
    QMultiHash<softTypeEnum, xmlParser::softData> groupBySoftType;
    foreach (xmlParser::softData item, data) {
            groupByHwType.insert(item.hwType, item);
    }
    foreach (quint16 value, groupByHwType.keys().toSet().toList()) {
        QDomElement e = doc->createElement("t" + QString::number(value));
        element.appendChild(e);
        groupBySoftType.clear();
        foreach (xmlParser::softData item, groupByHwType.values(value)) {
            groupBySoftType.insert(item.type, item);
        }
        if (groupBySoftType.values(SOFT_FIRMWARE).length()) {
            QDomElement fw = doc->createElement("firmware");
            e.appendChild(fw);
            xmlAddFields(doc, fw, groupBySoftType.values(SOFT_FIRMWARE));
        }
        if (groupBySoftType.values(SOFT_BOOTLOADER).length()) {
            QDomElement bl = doc->createElement("bootloader");
            e.appendChild(bl);
            xmlAddFields(doc, bl, groupBySoftType.values(SOFT_BOOTLOADER));
        }
        if (groupBySoftType.values(SOFT_SETTINGS).length()) {
            QDomElement settings = doc->createElement("settings");
            e.appendChild(settings);
            xmlAddFields(doc, settings, groupBySoftType.values(SOFT_SETTINGS));
        }
    }
}

void xmlParser::xmlAddFields(QDomDocument *doc, QDomElement &element, QList<xmlParser::softData> data)
{
    if(data.length() == 0)
        return;
    xmlParser::softData mostRecent = data.at(0);
    QList<xmlParser::softData> orderedData;
    while(!data.isEmpty())
    {
        int t = 0;
        for (int x = 0; x < data.length(); ++x) {
            if(data.at(x).date < mostRecent.date)
            {
                t = x;
                mostRecent = data.at(x);
            }
        }
        data.removeAt(t);
        orderedData.append(mostRecent);
    }

    foreach (xmlParser::softData item, orderedData) {
        QDomElement e = doc->createElement("d" + item.date.toString("ddMMyyyy"));
        e.setAttribute("name", item.name);
        e.setAttribute("uvohash", item.uavHash);
        e.setAttribute("md5", item.md5);
        e.setAttribute("packageLink", item.packageLink.toString());
        e.setAttribute("releaseLink", item.releaseLink.toString());
        e.setAttribute("scriptLink", item.scriptLink.toString());
        element.appendChild(e);
    }
}

QString xmlParser::convertSoftDataToXMLString(QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> list)
{
    QList<xmlParser::softData> releases = list.values(RELEASE_CURRENT);
    QList<xmlParser::softData> oldReleases = list.values(RELEASE_OLD);
    QList<xmlParser::softData> testReleases = list.values(RELEASE_TEST);
    QDomDocument *doc = new QDomDocument("tauLabssoftware");
    QDomElement root = doc->createElement("root");
    doc->appendChild(root);
    if(releases.length()) {
        QDomElement releaseRoot = doc->createElement("current_release");
        root.appendChild(releaseRoot);
        processOSTypeList(doc, &releaseRoot, releases);
    }
    if(oldReleases.length()) {
        QDomElement oldReleasesRoot = doc->createElement("old_release");
        root.appendChild(oldReleasesRoot);
        processOSTypeList(doc, &oldReleasesRoot, oldReleases);
    }
    if(testReleases.length()) {
        QDomElement testReleasesRoot = doc->createElement("test_release");
        root.appendChild(testReleasesRoot);
        processOSTypeList(doc, &testReleasesRoot, testReleases);
    }
    QString ret = doc->toString(4);
    delete doc;
    return ret;
}

QList<xmlParser::osTypeEnum> xmlParser::osTypesList()
{
    QList<xmlParser::osTypeEnum> list;
    list << xmlParser::OS_LINUX32 << xmlParser::OS_LINUX64 << xmlParser::OS_OSX32 << xmlParser::OS_OSX64 << xmlParser::OS_WIN32 << xmlParser::OS_WIN64;
    return list;
}
QString xmlParser::softTypeToString(xmlParser::softTypeEnum type)
{
    return xmlParser::softTypeToStringHash.value(type, "Unknown");
}

QString xmlParser::osTypeToString(xmlParser::osTypeEnum type)
{
    return xmlParser::osTypeToStringHash.value(type, "Unknown");
}

QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> xmlParser::temp()
{
    QMultiHash<releaseTypeEnum, xmlParser::softData> list;
    xmlParser::softData d0;
    xmlParser::softData d1;
    xmlParser::softData d2;
    xmlParser::softData d3;
    xmlParser::softData d4;
    xmlParser::softData d5;
    xmlParser::softData d6;
    xmlParser::softData d7;
    xmlParser::softData d8;
    xmlParser::softData d9;
    d0.date = QDate(2015, 2, 1);
    d0.hwType = 0xAAAA;
    d0.md5 = "kopkpofaksfpoka";
    d0.name = "d0name";
    d0.osType = OS_EMBEDED;
    d0.packageLink = QUrl("d0packageLink");
    d0.releaseLink = QUrl("d0releaselink");
    d0.scriptLink = QUrl("d0scriptlink");
    d0.type = SOFT_FIRMWARE;/////////////////////////////////////
    d0.uavHash = "d0uavohash";
    list.insert(RELEASE_OLD, d0);

    d1.date = QDate(2015, 2, 2);
    d1.hwType = 0xAAAB;
    d1.md5 = "kopkpofaksfpokad1";
    d1.name = "d1namegcs";
    d1.osType = OS_WIN64;
    d1.packageLink = QUrl("d1packageLink");
    d1.releaseLink = QUrl("d1releaselink");
    d1.scriptLink = QUrl("d1scriptlink");
    d1.type = SOFT_GCS;
    d1.uavHash = "d1uavohash";
    list.insert(RELEASE_OLD, d1);

    d2.date = QDate(2015, 2, 1);
    d2.hwType = 0xBBBB;//48059
    d2.md5 = "kopkpofaksfpokafirmwared2";
    d2.name = "d2namefirmware";
    d2.osType = OS_EMBEDED;
    d2.packageLink = QUrl("d2packageLink");
    d2.releaseLink = QUrl("d2releaselink");
    d2.scriptLink = QUrl("d2scriptlink");
    d2.type = SOFT_FIRMWARE;///////////////////////////////
    d2.uavHash = "d2uavohash";
    list.insert(RELEASE_CURRENT, d2);

    d4.date = QDate(2015, 11, 11);
    d4.hwType = 0xBBBB;//48059
    d4.md5 = "kopkpofaksfpokafirmwared4";
    d4.name = "d4namesettings";
    d4.osType = OS_EMBEDED;
    d4.packageLink = QUrl("d4packageLink");
    d4.releaseLink = QUrl("d4releaselink");
    d4.scriptLink = QUrl("d4scriptlink");
    d4.type = SOFT_SETTINGS;///////////////////////////////
    d4.uavHash = "d4uavohash";
    list.insert(RELEASE_CURRENT, d4);

    d3.date = QDate(2015, 2, 11);
    d3.hwType = 0xAAAB;
    d3.md5 = "agentkopkpofaksfpokad1";
    d3.name = "d3nameagent";
    d3.osType = OS_LINUX64;
    d3.packageLink = QUrl("linuxd1packageLink");
    d3.releaseLink = QUrl("linuxd1releaselink");
    d3.scriptLink = QUrl("linuxd1scriptlink");
    d3.type = SOFT_SLIM_GCS;
    d3.uavHash = "linuxd1uavohash";
    list.insert(RELEASE_CURRENT, d3);
    tempv = list;
    QString out = convertSoftDataToXMLString(list);
    bool success;
    QMultiHash<releaseTypeEnum, xmlParser::softData> parsed = parseXML(out, success);
    return parsed;
}

QList<QDomElement> xmlParser::nodesToElementList(QDomNodeList list)
{
    QList<QDomElement> ret;
    int x = list.length();
    for (int i = 0; i < x; ++i) {
        if(list.at(i).isElement()) {
            ret.append(list.at(i).toElement());
        }
    }
    return ret;
}

QHash<xmlParser::osTypeEnum, QString> xmlParser::osHashInit()
{
    QHash<xmlParser::osTypeEnum, QString>  temp;
    temp.insert(OS_EMBEDED, "Embeded");
    temp.insert(OS_LINUX32, "Linux 32bit");
    temp.insert(OS_LINUX64, "Linux 64bit");
    temp.insert(OS_OSX32, "OSX 32bit");
    temp.insert(OS_OSX64, "OSX 64bit");
    temp.insert(OS_WIN32, "Windows 32bit");
    temp.insert(OS_WIN64, "Windows 64bit");
    temp.insert(OS_EMBEDED, "Embeded");
    temp.insert(OS_LINUX32, "Linux 32bit");
    temp.insert(OS_LINUX64, "Linux 64bit");
    temp.insert(OS_OSX32, "OSX 32bit");
    temp.insert(OS_OSX64, "OSX 64bit");
    temp.insert(OS_WIN32, "Windows 32bit");
    temp.insert(OS_WIN64, "Windows 64bit");
    return temp;
}

QHash<xmlParser::softTypeEnum, QString> xmlParser::softHashInit()
{
    QHash<xmlParser::softTypeEnum, QString>  temp;
    temp.insert(SOFT_FIRMWARE, "Firmware");
    temp.insert(SOFT_GCS, "GCS");
    temp.insert(SOFT_SETTINGS, "Settings");
    temp.insert(SOFT_SLIM_GCS, "TBS Agent");
    temp.insert(SOFT_UPDATER, "Updater");
    temp.insert(SOFT_BOOTLOADER, "Bootloader");
    return temp;
}

QHash<int, QString> xmlParser::hwTypeInit()
{
    QHash<int, QString> temp;
    temp.insert(136, "Sparky");
    temp.insert(137, "SparkyBGC");
    temp.insert(129, "Freedom");
    temp.insert(3, "PipXtreme");
    temp.insert(145, "Colibri");
    temp.insert(131, "Flyingf3");
    temp.insert(132, "Flyingf4");
    temp.insert(133, "Discoveryf4");
    temp.insert(134, "Quanton");
    temp.insert(4, "CopterControl");
    temp.insert(3, "PipXtreme");
    temp.insert(127, "Revolution");
    temp.insert(9, "RevoMini");
    temp.insert(0, "none");
    return temp;
}
