/**
 ******************************************************************************
 * @file       mainwindow.h
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @addtogroup [Group]
 * @{
 * @addtogroup MainWindow
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qftp.h"
#include <QMainWindow>
#include <xmlparser.h>
#include <QTableWidget>
#include <xmlparser.h>
#include <QObject>
#include <webfileutils.h>
#include <QProcess>
#include <QEventLoop>
#include <settings.h>
#include <QBuffer>

namespace Ui {
class MainWindow;
}
class TableWidgetData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    enum status {STATUS_IDLE                                        = 0x00000000,
                 STATUS_NEW_SYSTEM                                  = 0x00000001,
                 STATUS_FETCHING_INFO_FILE                          = 0x00000010,
                 STATUS_EDITING_TEST_RELEASE_FROM_TEST              = 0x00000100,
                 STATUS_EDITING_TEST_RELEASE_FROM_RELEASE           = 0x00001000,
                 STATUS_CREATING_ITEM, STATUS_PROCESSING_NEW_ITEM   = 0x00010000,
                 STATUS_PARSING_INFO_FILE                           = 0x00100000};
    void fillTable(TableWidgetData *);
    Ui::MainWindow *ui;
    webFileUtils *fileUtils;
    xmlParser *parser;
    TableWidgetData *releaseTable;
    TableWidgetData *oldReleasesTable;
    status currentStatus;
    status oldStatus;
    void processStatusChange(status newStatus);
    int addNewItem(xmlParser::softData data);
    void deleteItem(xmlParser::softData data);
    QProcess *process;
    QEventLoop *eventLoop;
    QFtp *ftp;
    int tt;
    Settings *settings;
    QHash<int, QString> ftpOperations;
    QList<int> ftpDownloads;
    QString workingRoot;
    bool createNewItem(bool alreadyDownloaded, QByteArray data = QByteArray(), bool error = false, QString errorString = "");
    bool saveDownloadedFile(QByteArray data, QString path, QString fileName);
    QString currentFilename;
    bool ftpLogin();
private slots:
    void t(xmlParser::softData d);
    void onFetchButtonPressed();
    void onPushButtonPressed();
    void onMakeReleaseButtonPressed();
    void onDeleteButtonPressed();
    void onCreateItemButtonPressed();
    void onCancelNewItemButtonPressed();
    void onProcessNewItemButtonPressed();
    void onWebFileDownloaded(bool, QByteArray, QString, QNetworkReply::NetworkError);
    void onDownloadProgress(qint64, qint64);
    void onReadyReadFromProcess();
    void onSettingsButtonPressed();
    void onFtpStateChanged(int);
    void onFtpOperationEnded(int, bool);
    void onFtpTransferProgress(qint64, qint64);
    bool processInformationFile(QByteArray array);
    void onComboboxesCurrentChanged(int index);
    void onXMLParserMessage(QString text);
};

class TableWidgetData : public QObject
{
    Q_OBJECT
public:
    enum action {ACTION_NONE, ACTION_DELETE_FROM_SERVER, ACTION_COPY_TO_SERVER, ACTION_CHANGED_METADATA };
    TableWidgetData(QObject *parent, QTableWidget* table, QList<xmlParser::softData> data);
    QTableWidget *table;
    QList<xmlParser::softData> data;
    QHash<int, TableWidgetData::action> actionPerItem;
signals:
    void currentRowChanged(int);
    void currentItemChanged(xmlParser::softData);
private slots:
    void onCurrentCellChanged(int,int,int,int);
};
#endif // MAINWINDOW_H
