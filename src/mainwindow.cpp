/**
 ******************************************************************************
 * @file       mainwindow.cpp
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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "webfileutils.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <qftp.h>
#include "ftpcredentials.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), releaseTable(NULL), oldReleasesTable(NULL)
{
    process = new QProcess(this);
    eventLoop = new QEventLoop(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(onReadyReadFromProcess()));
    connect(process, SIGNAL(finished(int)), eventLoop, SLOT(quit()));
    ui->setupUi(this);
    ftp = new QFtp(this);
    connect(ftp, SIGNAL(stateChanged(int)), this, SLOT(onFtpStateChanged(int)));
    connect(ftp, SIGNAL(commandFinished(int,bool)), SLOT(onFtpOperationEnded(int,bool)));
    connect(ftp, SIGNAL(dataTransferProgress(qint64,qint64)), this, SLOT(onFtpTransferProgress(qint64, qint64)));
    fileUtils = new webFileUtils(this);
    settings = new Settings(this);
    connect(fileUtils, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64, qint64)));
    connect(fileUtils, SIGNAL(downloaded(bool,QByteArray,QString,QNetworkReply::NetworkError)), this, SLOT(onWebFileDownloaded(bool, QByteArray,QString,QNetworkReply::NetworkError)));
    parser = new xmlParser(this);
    connect(parser, SIGNAL(outputMessage(QString)), this, SLOT(onXMLParserMessage(QString)));
    QMultiHash<bool, xmlParser::softData>  t = parser->temp();
    releaseTable = new TableWidgetData(this, ui->featuredReleasesTable, t.values(true));
    oldReleasesTable = new TableWidgetData(this, ui->oldReleasesTable, t.values(false));
    this->fillTable(releaseTable);
    this->fillTable(oldReleasesTable);

    connect(ui->fetchTB, SIGNAL(clicked()), this, SLOT(onFetchButtonPressed()));
    connect(ui->createItemTB, SIGNAL(clicked()), this, SLOT(onCreateItemButtonPressed()));
    connect(ui->processNewItemTB, SIGNAL(clicked()), this, SLOT(onProcessNewItemButtonPressed()));
    connect(ui->cancelNewItemTB, SIGNAL(clicked()), this, SLOT(onCancelNewItemButtonPressed()));
    connect(ui->settingsTB, SIGNAL(clicked()), this, SLOT(onSettingsButtonPressed()));
    connect(ui->deleteTB, SIGNAL(clicked()), this, SLOT(onDeleteButtonPressed()));
    connect(ui->pushTB, SIGNAL(clicked()), this, SLOT(onPushButtonPressed()));
    processStatusChange(STATUS_IDLE);
    foreach (QString typeStr, xmlParser::osTypeToStringHash.values()) {
        ui->osCB->addItem(typeStr, xmlParser::osTypeToStringHash.key(typeStr));
    }
    foreach (QString typeStr, xmlParser::softTypeToStringHash.values()) {
        ui->typeCB->addItem(typeStr, xmlParser::softTypeToStringHash.key(typeStr));
    }
    foreach (QString typeStr, xmlParser::hwTypeToStringHash.values()) {
        ui->hwCB->addItem(typeStr, xmlParser::hwTypeToStringHash.key(typeStr));
    }
    connect(ui->osCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));
    connect(ui->typeCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));
    connect(ui->hwCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));

    QDir(QDir::temp().absolutePath() + QDir::separator() + "release_builder").removeRecursively();
    workingRoot = QDir::temp().absolutePath() + QDir::separator() + "release_builder" + QDir::separator();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::fillTable(TableWidgetData *tableData)
{
    QTableWidget *table = tableData->table;
    table->clearContents();
    table->setRowCount(0);
    QList<xmlParser::softData> data = tableData->data;
    QTableWidgetItem *item;
    table->setRowCount(data.length());
    for(int x = 0; x < data.length(); ++x) {
        item = new QTableWidgetItem(xmlParser::softTypeToString(data.at(x).type), 0);
        item->setData(Qt::UserRole, x);
        table->setItem(x, 0, item);
        table->setItem(x, 2, new QTableWidgetItem(QString::number(data.at(x).hwType), 0));
        table->setItem(x, 9, new QTableWidgetItem((data.at(x).md5), 0));
        table->setItem(x, 4, new QTableWidgetItem((data.at(x).name), 0));
        table->setItem(x, 1, new QTableWidgetItem(xmlParser::osTypeToString(data.at(x).osType), 0));
        table->setItem(x, 5, new QTableWidgetItem((data.at(x).packageLink.toString()), 0));
        table->setItem(x, 6, new QTableWidgetItem((data.at(x).releaseLink.toString()), 0));
        table->setItem(x, 7, new QTableWidgetItem((data.at(x).scriptLink.toString()), 0));
        table->setItem(x, 3, new QTableWidgetItem((data.at(x).date.toString()), 0));
        table->setItem(x, 8, new QTableWidgetItem((data.at(x).uavHash), 0));
        QColor color;
        if(tableData->actionPerItem.value(x, TableWidgetData::ACTION_NONE) == TableWidgetData::ACTION_CHANGED_METADATA)
            color = Qt::yellow;
        else if(tableData->actionPerItem.value(x, TableWidgetData::ACTION_NONE) == TableWidgetData::ACTION_COPY_TO_SERVER)
            color = Qt::green;
        else if(tableData->actionPerItem.value(x, TableWidgetData::ACTION_NONE) == TableWidgetData::ACTION_DELETE_FROM_SERVER)
            color = Qt::red;
        if(tableData->actionPerItem.value(x, TableWidgetData::ACTION_NONE) != TableWidgetData::ACTION_NONE) {
            for(int y = 0; y < table->columnCount(); ++y) {
                table->item(x, y)->setBackground(color);
            }
        }
    }
}

void MainWindow::processStatusChange(MainWindow::status newStatus)
{
    switch (newStatus) {
    case STATUS_IDLE:
        ui->fetchTB->setEnabled(true);
        ui->createItemTB->setEnabled(false);
        ui->fetchTB->setEnabled(true);
        ui->deleteTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(true);
        ui->testReleaseCB->setEnabled(true);
        // ui->testReleaseCB->setChecked(false);
        ui->createItemFrame->setVisible(false);
        break;
    case STATUS_NEW_SYSTEM:
    case STATUS_EDITING_TEST_RELEASE_FROM_TEST:
    case STATUS_EDITING_TEST_RELEASE_FROM_RELEASE:
        ui->fetchTB->setEnabled(true);
        ui->deleteTB->setEnabled(true);
        ui->pushTB->setEnabled(true);
        ui->createItemTB->setEnabled(true);
        ui->makeReleaseTB->setEnabled(false);
        ui->testReleaseCB->setEnabled(false);
        ui->testReleaseCB->setChecked(true);
        ui->createItemFrame->setVisible(false);
        break;
    case STATUS_CREATING_ITEM:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->testReleaseCB->setEnabled(false);
        ui->createItemFrame->setVisible(true);
        ui->dateEdit->setDateTime(QDateTime::currentDateTime());
        oldStatus = currentStatus;
        break;
    case STATUS_FETCHING_INFO_FILE:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->testReleaseCB->setEnabled(false);
        ui->createItemFrame->setVisible(false);
        oldStatus = currentStatus;
        break;
    case STATUS_PARSING_INFO_FILE:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->testReleaseCB->setEnabled(false);
        ui->createItemFrame->setVisible(false);
        break;
    default:
        break;
    }
    currentStatus = newStatus;
}

int MainWindow::addNewItem(xmlParser::softData data)
{
    int index = -1;
    switch (data.type) {
    case xmlParser::SOFT_BOOTLOADER:
    case xmlParser::SOFT_FIRMWARE:
        for (int i = 0; i < releaseTable->data.length(); ++i) {
            if(data.hwType == releaseTable->data.at(i).hwType && releaseTable->data.at(i).type == data.type) {
                index = i;
                break;
            }
        }
        break;
    case xmlParser::SOFT_UPDATER:
    case xmlParser::SOFT_GCS:
    case xmlParser::SOFT_TBS_AGENT:
        for (int i = 0; i < releaseTable->data.length(); ++i) {
            if(data.osType == releaseTable->data.at(i).osType && releaseTable->data.at(i).type == data.type) {
                index = i;
                break;
            }
        }
        break;
    default:
        break;
    }
    if(index > -1) {
        oldReleasesTable->data.append(releaseTable->data.at(index));
        oldReleasesTable->actionPerItem.insert(oldReleasesTable->data.length() -1, TableWidgetData::ACTION_CHANGED_METADATA);
        releaseTable->data.removeAt(index);
        this->fillTable(oldReleasesTable);
    }
    releaseTable->data.append(data);
    releaseTable->actionPerItem.insert(releaseTable->data.length() - 1, TableWidgetData::ACTION_COPY_TO_SERVER);
    this->fillTable(releaseTable);
    processStatusChange(oldStatus);
    return releaseTable->data.length() - 1;
}

void MainWindow::t(xmlParser::softData d)
{
    qDebug()<<d.name;
}

void MainWindow::onFetchButtonPressed()
{
    QString text;
    if(ui->testReleaseCB->isChecked())
        text = settings->settings.infoTestFilename;
    else
        text = settings->settings.infoReleaseFilename;
    ui->console->append(QString("Starting INFO file %0 download").arg(text));
    if(settings->settings.infoUseFtp) {
        if(!ftpLogin())
            return;
        QString file = settings->settings.infoPath + text;
        processStatusChange(STATUS_FETCHING_INFO_FILE);
        int ftpOpId = ftp->get(file);
        ftpOperations.insert(ftpOpId, QString("Fetching file %0").arg(file));
        ftpDownloads.append(ftpOpId);
    }
    else {
        processStatusChange(STATUS_FETCHING_INFO_FILE);
        QString file;
        if(!settings->settings.infoPath.endsWith("/"))
            file = settings->settings.infoPath + "/" + text;
        else
            file = settings->settings.infoPath + text;
        QString user;
        if(settings->settings.infoUseFtp)
            user = settings->settings.ftpUserName + "@";
        ui->console->append(QString("Downloading from %0%1").arg(user).arg(file));
        fileUtils->startFileDownload(QUrl(file));
    }
}

bool MainWindow::ftpLogin() {
    ftpCredentials::credentials cred;
    if(!settings->settings.ftpPassword.isEmpty() && !settings->settings.ftpUserName.isEmpty()) {
        ui->console->append(QString("Using stored FTP credentials, username=%0").arg(settings->settings.ftpUserName));
        cred.password = settings->settings.ftpPassword;
        cred.username = settings->settings.ftpUserName;
    }
    else {
        ftpCredentials *credentials = new ftpCredentials(this);
        cred = credentials->getCredentials(settings->settings.ftpUserName, settings->settings.ftpPassword);
        if(cred.remember) {
            settings->settings.ftpUserName = cred.username;
            settings->settings.ftpPassword = cred.password;
            settings->saveSettings();
        }
    }
    if(ftp->state() != QFtp::Connected) {
        QEventLoop loop;
        bool statusOK = false;
        connect(ftp, SIGNAL(stateChanged(int)), &loop, SLOT(quit()));
        connect(ftp, SIGNAL(commandFinished(int,bool)), &loop, SLOT(quit()));
        int ftpID = ftp->connectToHost(settings->settings.ftpServerUrl);
        ftpOperations.insert(ftpID, "Connect to host");
        while(!statusOK) {
            loop.exec();
            if(ftp->error() != QFtp::NoError) {
                return false;
            }
            if(ftp->state() == QFtp::Connected)
                statusOK = true;
        }
        statusOK = false;
        ftpOperations.insert(ftp->login(cred.username, cred.password), "Logging in to host");
        while(!statusOK) {
            loop.exec();
            if(ftp->error() != QFtp::NoError) {
                return false;
            }
            if(ftp->state() == QFtp::LoggedIn)
                statusOK = true;
        }
    }
    return true;
}

void MainWindow::onPushButtonPressed()
{
    QStringList filesToPush;
    QHash<QString, QString> localFiles;
    QStringList filesToDelete;
    QList<TableWidgetData* > workingTablesList;
    workingTablesList << releaseTable << oldReleasesTable;
    QString filename;
    foreach(TableWidgetData * workingTable, workingTablesList)
    {
        foreach (int i, workingTable->actionPerItem.keys()) {
            switch (workingTable->actionPerItem.value(i)) {
            case TableWidgetData::ACTION_DELETE_FROM_SERVER:
                filesToDelete.append(workingTable->data.at(i).releaseLink.toString());
                if(!workingTable->data.at(i).scriptLink.isEmpty()) {
                    filesToDelete.append(workingTable->data.at(i).scriptLink.toString());
                }
                break;
            case TableWidgetData::ACTION_COPY_TO_SERVER:
                filesToPush.append(workingTable->data.at(i).releaseLink.toString());
                filename = QFileInfo(workingTable->data.at(i).releaseLink.toString()).fileName();
                localFiles.insert(workingTable->data.at(i).releaseLink.toString(), QDir::temp().absolutePath() + QDir::separator() + "release_builder" + QDir::separator() + "release" + QString::number(i) + QDir::separator() + filename);
                if(!workingTable->data.at(i).scriptLink.isEmpty()) {
                    filesToPush.append(workingTable->data.at(i).scriptLink.toString());
                    filename = QFileInfo(workingTable->data.at(i).scriptLink.toString()).fileName();
                    localFiles.insert(workingTable->data.at(i).scriptLink.toString(), QDir::temp().absolutePath() + QDir::separator() + "release_builder" + QDir::separator() + "release" + QString::number(i) + QDir::separator() + filename);
                }
                break;
            default:
                break;
            }
        }
    }
    QMultiHash<bool, xmlParser::softData> dataList;
    foreach (xmlParser::softData data, releaseTable->data) {
        dataList.insert(true, data);
    }
    foreach (xmlParser::softData data, oldReleasesTable->data) {
        dataList.insert(false, data);
    }
    QString xml = parser->convertSoftDataToXMLString(dataList);
    if(!filesToDelete.isEmpty()) {
        ui->console->append("GOING TO DELETE FILES:");
        foreach (QString file, filesToDelete) {
            ui->console->append(file);
        }
    }
    if(!filesToPush.isEmpty()) {
        ui->console->append("GOING TO PUSH FILES:");
        foreach (QString file, filesToPush) {
            ui->console->append(file);
        }
    }
    if(QMessageBox::question(this, "Please Confirm actions", "Do you really want to perform this actions?") != QMessageBox::Yes)
        return;
    ftpLogin();
    foreach (QString file, filesToDelete) {
        ftpOperations.insert(ftp->remove(file), QString("Removing file %0 from server").arg(file));
    }
    foreach (QString file, filesToPush) {
        QFile m_file(localFiles.value(file));
        if(m_file.open(QIODevice::ReadOnly)) {
            ftpOperations.insert(ftp->put(m_file.readAll(), file), QString("Pushing file %0 to %1 on server").arg(localFiles.value(file).arg(file)));
        }
        else {
            ui->console->append(QString("ERROR could not open local file %0. Skipping").arg(localFiles.value(file)));
        }
    }
}


void MainWindow::onMakeReleaseButtonPressed()
{

}

void MainWindow::onDeleteButtonPressed()
{
    TableWidgetData *currentWidget;
    if(ui->tabWidget->currentIndex() == 0)
        currentWidget = releaseTable;
    else
        currentWidget = oldReleasesTable;
    currentWidget->actionPerItem.insert(currentWidget->table->item(currentWidget->table->selectedItems().at(0)->row(), 0)->data(Qt::UserRole).toInt(), TableWidgetData::ACTION_DELETE_FROM_SERVER);
    fillTable(currentWidget);
}

void MainWindow::onCreateItemButtonPressed()
{
    processStatusChange(STATUS_CREATING_ITEM);
}

void MainWindow::onCancelNewItemButtonPressed()
{
    processStatusChange(oldStatus);
}

void MainWindow::onProcessNewItemButtonPressed()
{
    QString filename = QFileInfo(ui->packageLinkLE->text()).fileName();
    QString completePath = workingRoot + filename;
    completePath = completePath.remove(".exe").remove(".tar.xz").remove(".zip").remove(".tar.gz");
    if(QDir(completePath).exists()) {
        currentFilename = filename;
        createNewItem(true);
        ui->console->append(QString("File %0 already present on temporary folder, skipping download").arg(filename));
    }
    ui->console->append(QString("Starting %0 download").arg(ui->packageLinkLE->text()));
    QString text = ui->packageLinkLE->text();
    currentFilename = text.right(text.size() - text.lastIndexOf("/"));
    if(!settings->settings.infoUseFtp) {
        processStatusChange(STATUS_PROCESSING_NEW_ITEM);
        fileUtils->startFileDownload(QUrl(ui->packageLinkLE->text()));
    }
    else {
        if(ftpLogin()) {
            processStatusChange(STATUS_PROCESSING_NEW_ITEM);
            int id = ftp->get(ui->packageLinkLE->text());
            ftpOperations.insert(id, "downloading " + ui->packageLinkLE->text());
            ftpDownloads.append(id);
        }
    }
}

void MainWindow::onWebFileDownloaded(bool result, QByteArray data, QString errorStr, QNetworkReply::NetworkError error)
{
    if(!result)
        ui->console->append("File download failed with error:" + errorStr);
    switch (currentStatus) {
    case STATUS_FETCHING_INFO_FILE:
        if(!result) {
            if(error == QNetworkReply::ContentNotFoundError) {
                if(!ui->testReleaseCB->isChecked()) {
                    if(QMessageBox::question(this, "Information file apears not to exist on the server", "Do you want to create one?") == QMessageBox::Yes) {
                        processStatusChange(STATUS_NEW_SYSTEM);

                    }
                }
                else
                {
                    ui->console->append("ERROR there is no test build available on the server");
                    processStatusChange(oldStatus);
                    return;
                }
            }
        }
        else
        {
            ui->console->append("File download succeded");
            if(processInformationFile(data)) {
            }
            else
                processStatusChange(oldStatus);
        }
        break;
    case STATUS_PROCESSING_NEW_ITEM:
        if(result)
            createNewItem(false, data);
        else
            createNewItem(false, data, true, errorStr);
        break;
    default:
        Q_ASSERT(false);
        break;
    }
}

bool MainWindow::createNewItem(bool alreadyDownloaded, QByteArray data, bool error, QString errorStr){
    QString extractedPath;
    QString gitHash;
    QString completePath = workingRoot + currentFilename;
    QString tempDir = workingRoot;
    extractedPath = completePath;
    extractedPath = extractedPath.remove(".exe").remove(".tar.xz").remove(".zip").remove(".tar.gz");
    extractedPath += QDir::separator();
    if(!QDir(tempDir).exists())
        QDir().mkpath(tempDir);
    bool result;
    if(error) {
        ui->console->append("File download failed with error:" + errorStr);
        processStatusChange(STATUS_CREATING_ITEM);
        return false;
    }
    if(!alreadyDownloaded) {
        ui->console->append("File download succeded");
        ui->console->append("Starting package processing");
        ui->console->append(QString("Saving downloaded file to %0").arg(workingRoot));
        if(!saveDownloadedFile(data, workingRoot, currentFilename)) {
            ui->console->append("Failed to save downloaded file");
            processStatusChange(STATUS_CREATING_ITEM);
            return false;
        }
        if(((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_SETTINGS) && ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_UPDATER)) {
            QStringList arguments;
            arguments << "-xvf";
            arguments << completePath;
            arguments << "-C";
            arguments << tempDir;
            ui->console->append("Decompressing downloaded file");
            process->start("tar", arguments);
            eventLoop->exec();
            if(process->exitStatus() == QProcess::NormalExit) {
                if(process->exitCode() == 0) {
                    ui->console->append("File decompressed to " + extractedPath);
                }
                else {
                    ui->console->append(QString("Decompression FAILED with exit code %0").arg(process->exitCode()));
                    processStatusChange(STATUS_CREATING_ITEM);
                    return false;
                }
            }
            else {
                ui->console->append("Decompression FAILED");
                processStatusChange(STATUS_CREATING_ITEM);
                return false;
            }
        }
    }
    ui->console->append("Checking if downloaded file has the necessary files");
    bool condition1 = false;
    bool condition2 = false;
    switch ((xmlParser::osTypeEnum)ui->osCB->currentData().toInt()) {
    case xmlParser::OS_LINUX64:
    case xmlParser::OS_LINUX32:
        switch ((xmlParser::osTypeEnum)ui->typeCB->currentData().toInt()) {
        case xmlParser::SOFT_TBS_AGENT:
            condition1 = QDir(extractedPath + "slimgcs").exists();
            condition2 = QFileInfo(extractedPath + "tbsagent").isSymLink();
            break;
        case xmlParser::SOFT_GCS:
            condition1 = QDir(extractedPath + "gcs").exists();
            condition2 = QFileInfo(extractedPath + "taulabsgcs").isSymLink();
        case xmlParser::SOFT_UPDATER:
            condition2 = QFile(completePath).exists();
            condition1 = true;
        default:
            break;
        }
        break;
    case xmlParser::OS_OSX64:
    case xmlParser::OS_OSX32:
        //TODO
        break;
    case xmlParser::OS_WIN64:
    case xmlParser::OS_WIN32:
        //TODO
        break;
    case xmlParser::OS_EMBEDED:
        switch ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()) {
        case xmlParser::SOFT_FIRMWARE:
            condition2 = QFile(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + QString("fw_%0.tlfw").arg(ui->hwCB->currentText().toLower())).exists();
            condition1 = true;
            break;
        case xmlParser::SOFT_BOOTLOADER:
            condition2 = QFile(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + QString("bu_%0.tlfw").arg(ui->hwCB->currentText())).exists();
            condition1 = true;
            break;
        case xmlParser::SOFT_SETTINGS:
            condition1 = QFile(completePath).exists();
            condition2 = true;
        default:
            condition1 = true;
            condition2 = true;
            break;
        }
    default:
        break;
    }
    if(!condition1 || !condition2) {
        ui->console->append("FAILED to find required files, ABORTING!");
        processStatusChange(STATUS_CREATING_ITEM);
        return false;
    }
    if(((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_UPDATER) && ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_SETTINGS) ) {
        ui->console->append("Processing INFO file");
        QFile infoFile(extractedPath + "BUILD_INFO");
        if(!infoFile.open(QIODevice::ReadOnly)) {
            ui->console->append("FAILED to process INFO file");
            processStatusChange(STATUS_CREATING_ITEM);
            return false;
        }
        QTextStream in(&infoFile);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            QStringList l = line.split("=");
            if(l.length() == 2)
            {
                if(l.at(0) == "BRANCH") {
                }
                else if(l.at(0) == "GIT_HASH") {
                    gitHash = l.at(1);
                }
                else if(l.at(0) == "DATE") {
                    ui->dateEdit->setDate(QDate::fromString(l.at(1), "yyyyMMdd"));
                }
                else if(l.at(0) == "UAVO_HASH") {
                    QString temp;
                    temp = l.at(1);
                    ui->uavoHashLE->setText(temp.remove(",").remove("0x"));
                }
            }
        }
        QString script = settings->settings.updaterScriptPath.value((xmlParser::osTypeEnum)ui->osCB->currentData().toInt());
        QString updater = settings->settings.updaterBinaryPath.value((xmlParser::osTypeEnum)ui->osCB->currentData().toInt());
        QString sourceDir;
        QStringList arguments;
        arguments.append("-u");
        sourceDir = extractedPath;
        process->setWorkingDirectory(tempDir);
        ui->console->append(QString("Running ruby script with command:ruby %0 -p %1 -v 0 -u %2 %3 %4 ./currentbuild").arg(settings->settings.rubyScriptPath, ui->osCB->currentText().remove(" "), updater, sourceDir, script));
        process->start(QString("ruby %0 -p %1 -v 0 -u %2 %3 %4 ./currentbuild").arg(settings->settings.rubyScriptPath, ui->osCB->currentText().remove(" "), updater, sourceDir, script));
        eventLoop->exec();
    }
    QString releasePartialPath;
    releasePartialPath = ui->osCB->currentText() + QDir::separator();
    if((xmlParser::osTypeEnum)ui->osCB->currentData().toInt() != xmlParser::OS_EMBEDED && (xmlParser::softTypeEnum)ui->typeCB->currentData().toInt() != xmlParser::SOFT_UPDATER) {
        releasePartialPath += ui->typeCB->currentText();
    }
    else if ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt() != xmlParser::SOFT_UPDATER){
        releasePartialPath += ui->hwCB->currentText().toLower();
    }
    releasePartialPath = releasePartialPath.remove(" ");
    QString releaseStoragePath = workingRoot + "releases" + QDir::separator();
    QDir().mkpath(releaseStoragePath);
    QString serverStoragePath = settings->settings.ftpPath + releasePartialPath + QDir::separator();
    QString file;
    switch ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()) {
    case xmlParser::SOFT_BOOTLOADER:
        file = QString("bu_%0_%1.tlfw").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        result = QFile::copy(QString(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + "bu_%0.tlfw").arg(ui->hwCB->currentText().toLower()), releaseStoragePath + file);
        ui->md5LE->setText(QString(QCryptographicHash::hash((QFile(releaseStoragePath + file).readAll()),QCryptographicHash::Md5).toHex()));
        break;
    case xmlParser::SOFT_FIRMWARE:
        file = QString("fw_%0_%1.tlfw").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        result = QFile::copy(QString(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + "fw_%0.tlfw").arg(ui->hwCB->currentText().toLower()), releaseStoragePath + file);
        ui->md5LE->setText(QString(QCryptographicHash::hash((QFile(releaseStoragePath + file).readAll()),QCryptographicHash::Md5).toHex()));
        break;
    case xmlParser::SOFT_SETTINGS:
        file = QString( "settings_%0_%1.xml").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        result = QFile::copy(completePath, releaseStoragePath + file);
        ui->md5LE->setText(QString(QCryptographicHash::hash((QFile(releaseStoragePath + file).readAll()),QCryptographicHash::Md5).toHex()));
        break;
    case xmlParser::SOFT_GCS:
    case xmlParser::SOFT_TBS_AGENT:
        file = QString("%0_%1.zip").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        result = QFile::copy(QString(workingRoot + "currentbuild" + QDir::separator() + "app.zip"), releaseStoragePath + file);
        ui->md5LE->setText(QString(QCryptographicHash::hash((QFile(releaseStoragePath + file).readAll()),QCryptographicHash::Md5).toHex()));
        file = QString("%0_%1.xml").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->scritLinkLE->setText(serverStoragePath + file);
        result &= QFile::copy(QString(workingRoot + "currentbuild" + QDir::separator() + "file_list.xml"), releaseStoragePath + file);
        break;
    case xmlParser::SOFT_UPDATER:
        switch ((xmlParser::osTypeEnum)ui->osCB->currentData().toInt()) {
        case xmlParser::OS_LINUX32:
        case xmlParser::OS_LINUX64:
            file = QString("updater");
            ui->releaseLinkE->setText(serverStoragePath + file);
            result = QFile::copy(completePath, releaseStoragePath + file);
            ui->md5LE->setText(QString(QCryptographicHash::hash((QFile(releaseStoragePath + file).readAll()),QCryptographicHash::Md5).toHex()));
            break;
            //TODO
        default:
            Q_ASSERT(false);
            break;
        }
        break;
    default:
        Q_ASSERT(false);
        break;
    }
    if(result)
    {
        ui->console->append("Release packages copied to local temp directory");
        xmlParser::softData newItem;
        newItem.date = ui->dateEdit->date();
        newItem.hwType = ui->hwCB->currentData().toInt();
        newItem.md5 = ui->md5LE->text();
        newItem.name = ui->nameLE->text();
        newItem.osType = (xmlParser::osTypeEnum) ui->osCB->currentData().toInt();
        newItem.packageLink = ui->packageLinkLE->text();
        newItem.releaseLink = ui->releaseLinkE->text();
        newItem.scriptLink = ui->scritLinkLE->text();
        newItem.type = (xmlParser::softTypeEnum) ui->typeCB->currentData().toInt();
        newItem.uavHash = ui->uavoHashLE->text();
        QString original = workingRoot + "releases";
        QDir dir;
        int newItemIndex = addNewItem(newItem);
        QString dest = workingRoot + "release" + QString::number(newItemIndex);
        ui->console->append(QString("Moving temporary directory %0 to %1").arg(original, dest));
        if( !dir.rename( original, dest ) ){
            ui->console->append("Moving failed, Aborting");
            processStatusChange(oldStatus);
            return false;
        }
        else {
            ui->console->append("Adding new item DONE");
            return true;
        }
    }
    else {
        ui->console->append("Release packages copy FAILED");
        return false;
    }
}


void MainWindow::onDownloadProgress(qint64 current, qint64 total)
{
    if(total == 0)
        return;
    if(ui->console->toPlainText().right(1) == "%")
    {
        int x = ui->console->toPlainText().length() - ui->console->toPlainText().lastIndexOf(":");
        for(int i = 0; i < x - 1; ++i)
            ui->console->textCursor().deletePreviousChar();
        ui->console->insertPlainText(QString::number((current * 100) / total) + "%");
    }
    else
        ui->console->append(QString("Download file progress:%0%").arg((current * 100) / total));
}

void MainWindow::onReadyReadFromProcess()
{
    QByteArray array = process->readAll();
    if(!array.isEmpty())
        ui->console->append(array);
}

void MainWindow::onSettingsButtonPressed()
{
    settings->show();
}

void MainWindow::onFtpStateChanged(int state)
{
    QString text;
    switch (state) {
    case QFtp::Unconnected:
        text = "Unconnected";
        break;
    case QFtp::HostLookup:
        text = "Host Lookup";
        break;
    case QFtp::Connecting:
        text = "Connecting";
        break;
    case QFtp::Connected:
        text = "Connected";
        break;
    case QFtp::LoggedIn:
        text = "Logged In";
        break;
    case QFtp::Closing:
        text = "Unconnected";
        break;
    default:
        break;
    }
    ui->console->append(QString("FTP state changed to:%0").arg(text));
}

void MainWindow::onFtpOperationEnded(int opID, bool error)
{
    if(ftpOperations.keys().contains(opID)) {
        if(!error)
            ui->console->append(QString("FTP %0 operation was successfull").arg(ftpOperations.value(opID)));
        else
            ui->console->append(QString("FTP %0 operation was unsuccessfull. Error:%1").arg(ftpOperations.value(opID)).arg(ftp->errorString()));
    }
    else {
        if(error)
            ui->console->append(QString("FTP operation was unsuccessfull. Error:%0").arg(ftp->errorString()));
        else
            ui->console->append("FTP operation was successfull");
    }
    if(ftpDownloads.contains(opID)) {
        switch (currentStatus) {
        case STATUS_FETCHING_INFO_FILE:
            if(!error) {
                processStatusChange(STATUS_PARSING_INFO_FILE);
                if(processInformationFile(ftp->readAll())){
                }
                else {
                    processStatusChange(oldStatus);
                }
            }
            else {
                if(!ui->testReleaseCB->isChecked()) {
                    if(QMessageBox::question(this, "Information file apears not to exist on the server", "Do you want to create one?") == QMessageBox::Yes) {
                        processStatusChange(STATUS_NEW_SYSTEM);
                    }
                }
                else
                {
                    ui->console->append("ERROR there is no test build available on the server");
                    processStatusChange(oldStatus);
                    return;
                }
            }
            break;
        case STATUS_PROCESSING_NEW_ITEM:
            if(error) {
                createNewItem(false, QByteArray(), true, ftp->errorString());
            }
            else {
                createNewItem(false, ftp->readAll());
            }
            break;
        default:
            Q_ASSERT(false);
            break;
        }
    }

}

void MainWindow::onFtpTransferProgress(qint64 current, qint64 total)
{
    onDownloadProgress(current, total);
}

bool MainWindow::processInformationFile(QByteArray array)
{
    bool success = false;
    QMultiHash<bool, xmlParser::softData> dataset = parser->parseXML(QString(array), success);
    if(!success) {
        ui->console->append("XML information file parsing FAILED");
        return false;
    }
    if(releaseTable)
        delete releaseTable;
    if(oldReleasesTable)
        delete oldReleasesTable;
    if(releaseTable)
        delete releaseTable;
    if(oldReleasesTable)
        delete oldReleasesTable;
    releaseTable = new TableWidgetData(this, ui->featuredReleasesTable, dataset.values(true));
    oldReleasesTable = new TableWidgetData(this, ui->oldReleasesTable, dataset.values(false));
    this->fillTable(releaseTable);
    this->fillTable(oldReleasesTable);
    if(ui->testReleaseCB->isChecked())
        processStatusChange(STATUS_EDITING_TEST_RELEASE_FROM_TEST);
    else
        processStatusChange(STATUS_EDITING_TEST_RELEASE_FROM_RELEASE);
    return true;
}

void MainWindow::onComboboxesCurrentChanged(int index)
{
    Q_UNUSED(index);
    QComboBox *box = dynamic_cast <QComboBox *>(sender());
    if(!box)
        return;
    if(box == ui->typeCB) {
        if((ui->typeCB->currentData().toInt() == xmlParser::SOFT_FIRMWARE) || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_BOOTLOADER)
                || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_SETTINGS)) {
            ui->osCB->setCurrentIndex(ui->osCB->findData(xmlParser::OS_EMBEDED));
            if(ui->hwCB->currentText() == "none")
                ui->hwCB->setCurrentIndex(ui->hwCB->findText("CopterControl"));
        }
        else {
            ui->hwCB->setCurrentIndex(ui->hwCB->findText("none"));
            if(ui->osCB->currentIndex() == ui->osCB->findData(xmlParser::OS_EMBEDED))
                ui->osCB->setCurrentIndex(ui->osCB->findData(xmlParser::OS_LINUX64));
        }
    }
    else if(box == ui->osCB) {
        if((ui->osCB->currentData().toInt() == xmlParser::OS_EMBEDED)) {
            if(ui->hwCB->currentText() == "none") {
                ui->hwCB->setCurrentIndex(ui->hwCB->findText("CopterControl"));
            }
            if(!((ui->typeCB->currentData().toInt() == xmlParser::SOFT_FIRMWARE) || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_BOOTLOADER)
                 || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_SETTINGS))) {
                ui->typeCB->setCurrentIndex(ui->typeCB->findData(xmlParser::SOFT_FIRMWARE));
            }
        }
        else {
            if(ui->hwCB->currentText() != "none") {
                ui->hwCB->setCurrentIndex(ui->hwCB->findText("none"));
            }
            if(((ui->typeCB->currentData().toInt() == xmlParser::SOFT_FIRMWARE) || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_BOOTLOADER)
                || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_SETTINGS))) {
                ui->typeCB->setCurrentIndex(ui->typeCB->findData(xmlParser::SOFT_GCS));
            }
        }
    }
    else if(box == ui->hwCB) {
        if(ui->hwCB->currentText() == "none") {
            if(ui->osCB->currentData().toInt() == xmlParser::OS_EMBEDED) {
                ui->osCB->setCurrentIndex(ui->osCB->findData(xmlParser::OS_LINUX64));
            }
            if(((ui->typeCB->currentData().toInt() == xmlParser::SOFT_FIRMWARE) || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_BOOTLOADER)
                || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_SETTINGS))) {
                ui->typeCB->setCurrentIndex(ui->typeCB->findData(xmlParser::SOFT_GCS));
            }
        }
        else {
            ui->osCB->setCurrentIndex(ui->osCB->findData(xmlParser::OS_EMBEDED));
            if(!((ui->typeCB->currentData().toInt() == xmlParser::SOFT_FIRMWARE) || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_BOOTLOADER)
                 || (ui->typeCB->currentData().toInt() == xmlParser::SOFT_SETTINGS))) {
                ui->typeCB->setCurrentIndex(ui->typeCB->findData(xmlParser::SOFT_FIRMWARE));
            }
        }
    }
}

void MainWindow::onXMLParserMessage(QString text)
{
    ui->console->append(QString("XMLParser:%0").arg(text));
}

bool MainWindow::saveDownloadedFile(QByteArray data, QString path, QString fileName)
{
    if(!QDir(path).exists())
        QDir().mkpath(path);
    if(!path.endsWith(QDir::separator()))
        path.append(QDir::separator());
    QFile localFile(path + fileName);
    if (!localFile.open(QIODevice::WriteOnly))
        return false;
    localFile.write(data);
    localFile.close();
    return true;
}

TableWidgetData::TableWidgetData(QObject *parent, QTableWidget *table, QList<xmlParser::softData> data):QObject(parent), table(table), data(data)
{
    connect(table, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(onCurrentCellChanged(int,int,int,int)));
}

void TableWidgetData::onCurrentCellChanged(int x, int y, int xx, int yy)
{
    Q_UNUSED(y);
    Q_UNUSED(xx);
    Q_UNUSED(yy);
    emit currentRowChanged(x);
    emit currentItemChanged(data.at(table->item(x, 0)->data(Qt::UserRole).toInt()));
}

