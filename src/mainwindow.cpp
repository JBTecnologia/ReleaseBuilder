/**
 ******************************************************************************
 * @file       mainwindow.cpp
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2015
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

//#define USE_TEST_DATA

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
    ui(new Ui::MainWindow), releaseTable(NULL), oldReleaseTable(NULL)
{
    ui->setupUi(this);
    //process used to run ruby script and tar
    process = new QProcess(this);
    eventLoop = new QEventLoop(this);
    ftp = new QFtp(this);
    fileUtils = new webFileUtils(this);
    settings = new Settings(this);
    parser = new xmlParser(this);

    //text output channels
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(onReadyReadFromProcess()));
    connect(process, SIGNAL(finished(int)), eventLoop, SLOT(quit()));

    connect(ftp, SIGNAL(stateChanged(int)), this, SLOT(onFtpStateChanged(int)));
    connect(ftp, SIGNAL(commandFinished(int,bool)), SLOT(onFtpOperationEnded(int,bool)));
    connect(ftp, SIGNAL(dataTransferProgress(qint64,qint64)), this, SLOT(onFtpTransferProgress(qint64, qint64)));
    connect(ftp, SIGNAL(listInfo(QUrlInfo)), this, SLOT(onftpListInfo(QUrlInfo)));

    connect(fileUtils, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64, qint64)));
    connect(fileUtils, SIGNAL(downloaded(bool,QByteArray,QString,QNetworkReply::NetworkError)), this, SLOT(onWebFileDownloaded(bool, QByteArray,QString,QNetworkReply::NetworkError)));

    connect(parser, SIGNAL(outputMessage(QString)), this, SLOT(onXMLParserMessage(QString)));

#ifdef USE_TEST_DATA
    QMultiHash<bool, xmlParser::softData>  t = parser->temp();
#else
    QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData>  t;
#endif
    releaseTable = new TableWidgetData(this, ui->featuredReleasesTable, t.values(xmlParser::RELEASE_CURRENT));
    oldReleaseTable = new TableWidgetData(this, ui->oldReleasesTable, t.values(xmlParser::RELEASE_OLD));
    testReleaseTable = new TableWidgetData(this, ui->testReleasesTable, t.values(xmlParser::RELEASE_TEST));
    this->fillTable(releaseTable);
    this->fillTable(oldReleaseTable);
    this->fillTable(testReleaseTable);

    //widgets signals connection
    connect(ui->fetchTB, SIGNAL(clicked()), this, SLOT(onFetchButtonPressed()));
    connect(ui->createItemTB, SIGNAL(clicked()), this, SLOT(onCreateItemButtonPressed()));
    connect(ui->processNewItemTB, SIGNAL(clicked()), this, SLOT(onProcessNewItemButtonPressed()));
    connect(ui->cancelNewItemTB, SIGNAL(clicked()), this, SLOT(onCancelNewItemButtonPressed()));
    connect(ui->settingsTB, SIGNAL(clicked()), this, SLOT(onSettingsButtonPressed()));
    connect(ui->deleteTB, SIGNAL(clicked()), this, SLOT(onDeleteButtonPressed()));
    connect(ui->pushTB, SIGNAL(clicked()), this, SLOT(onPushButtonPressed()));
    connect(ui->osCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));
    connect(ui->typeCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));
    connect(ui->hwCB, SIGNAL(currentIndexChanged(int)), this, SLOT(onComboboxesCurrentChanged(int)));
    connect(ui->makeReleaseTB, SIGNAL(clicked()), this, SLOT(onMakeReleaseButtonPressed()));
    processStatusChange(STATUS_IDLE);
    fillComboBoxes();

    //delete /temp/realease_builder
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
    QTableWidgetItem *item;
    table->setRowCount(tableData->dataActionPerItem.values().length());
    int index = 0;
    foreach (int x, tableData->dataActionPerItem.keys()) {
        item = new QTableWidgetItem(xmlParser::softTypeToString(tableData->dataActionPerItem.value(x).data.type), 0);
        item->setData(Qt::UserRole, x);
        table->setItem(index, 0, item);
        table->setItem(index, 2, new QTableWidgetItem(QString::number(tableData->dataActionPerItem.value(x).data.hwType), 0));
        table->setItem(index, 9, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.md5), 0));
        table->setItem(index, 4, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.name), 0));
        table->setItem(index, 1, new QTableWidgetItem((xmlParser::osTypeToString(tableData->dataActionPerItem.value(x).data.osType)), 0));
        table->setItem(index, 5, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.packageLink.toString()), 0));
        table->setItem(index, 6, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.releaseLink.toString()), 0));
        table->setItem(index, 7, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.scriptLink.toString()), 0));
        table->setItem(index, 3, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.date.toString()), 0));
        table->setItem(index, 8, new QTableWidgetItem((tableData->dataActionPerItem.value(x).data.uavHash), 0));
        QColor color;
        if(tableData->dataActionPerItem.value(x).action == TableWidgetData::ACTION_CHANGED_METADATA)
            color = Qt::yellow;
        else if(tableData->dataActionPerItem.value(x).action == TableWidgetData::ACTION_COPY_TO_SERVER)
            color = Qt::green;
        else if(tableData->dataActionPerItem.value(x).action == TableWidgetData::ACTION_DELETE_FROM_SERVER)
            color = Qt::red;
        if(tableData->dataActionPerItem.value(x).action != TableWidgetData::ACTION_NONE) {
            for(int y = 0; y < table->columnCount(); ++y) {
                table->item(index, y)->setBackground(color);
            }
        }
        ++index;
    }
}

int MainWindow::getFirstFreeIndex(TableWidgetData *data)
{
    int x = 0;
    bool found = false;
    do
    {
        if(!data->dataActionPerItem.keys().contains(x))
            found = true;
        else
            x = x + 1;
    } while(!found);
    return x;
}

int MainWindow::getFirstFreeIndex(QHash<int, TableWidgetData::dataActionStruct> &data)
{
    int x = 0;
    bool found = false;
    do
    {
        if(!data.keys().contains(x))
            found = true;
        else
            x = x + 1;
    } while(!found);
    return x;
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
        ui->createItemFrame->setVisible(false);
        break;
    case STATUS_NEW_SYSTEM:
    case STATUS_EDITING_RELEASE:
        ui->fetchTB->setEnabled(true);
        ui->deleteTB->setEnabled(true);
        ui->pushTB->setEnabled(true);
        ui->createItemTB->setEnabled(true);
        ui->makeReleaseTB->setEnabled(true);
        ui->createItemFrame->setVisible(false);
        break;
    case STATUS_CREATING_ITEM:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->createItemFrame->setVisible(true);
        ui->dateEdit->setDateTime(QDateTime::currentDateTime());
        if(oldStatus != STATUS_CREATING_ITEM)
            oldStatus = currentStatus;
        break;
    case STATUS_FETCHING_INFO_FILE:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->createItemFrame->setVisible(false);
        oldStatus = currentStatus;
        break;
    case STATUS_PARSING_INFO_FILE:
        ui->fetchTB->setEnabled(false);
        ui->deleteTB->setEnabled(false);
        ui->pushTB->setEnabled(false);
        ui->createItemTB->setEnabled(false);
        ui->makeReleaseTB->setEnabled(false);
        ui->createItemFrame->setVisible(false);
        break;
    default:
        break;
    }
    currentStatus = newStatus;
}
/*
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
    case xmlParser::SOFT_SLIM_GCS:
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
        oldReleaseTable->data.append(releaseTable->data.at(index));
        oldReleaseTable->actionPerItem.insert(oldReleaseTable->data.length() -1, TableWidgetData::ACTION_CHANGED_METADATA);
        releaseTable->data.removeAt(index);
        this->fillTable(oldReleaseTable);
    }
    releaseTable->data.append(data);
    releaseTable->actionPerItem.insert(releaseTable->data.length() - 1, TableWidgetData::ACTION_COPY_TO_SERVER);
    this->fillTable(releaseTable);
    processStatusChange(oldStatus);
    return releaseTable->data.length() - 1;
}
*/
int MainWindow::addNewItem(xmlParser::softData data)
{
    ui->console->append(QString("Adding new item %0").arg(data.name));
    QList<int> repeated;
    switch (data.type) {
    case xmlParser::SOFT_BOOTLOADER:
    case xmlParser::SOFT_FIRMWARE:
        foreach (int i, testReleaseTable->dataActionPerItem.keys()) {
            if(data.hwType == testReleaseTable->dataActionPerItem.value(i).data.hwType && testReleaseTable->dataActionPerItem.value(i).data.type == data.type) {
                ui->console->append(QString("Found repeated (hw and type are the same) item %0").arg(testReleaseTable->dataActionPerItem.value(i).data.name));
                repeated.append(i);
            }
        }
        break;
    case xmlParser::SOFT_UPDATER:
    case xmlParser::SOFT_GCS:
    case xmlParser::SOFT_SLIM_GCS:
        foreach (int i, testReleaseTable->dataActionPerItem.keys()) {
            if(data.osType == testReleaseTable->dataActionPerItem.value(i).data.osType && testReleaseTable->dataActionPerItem.value(i).data.type == data.type) {
                ui->console->append(QString("Found repeated (OS and type are the same) item %0").arg(testReleaseTable->dataActionPerItem.value(i).data.name));
                repeated.append(i);
            }
        }
        break;
    default:
        break;
    }
    if(repeated.length() > 0) {
        foreach (int i, repeated) {
            if(testReleaseTable->dataActionPerItem.value(i).action == TableWidgetData::ACTION_NONE) {
                TableWidgetData::dataActionStruct temp;
                temp.data = testReleaseTable->dataActionPerItem.value(i).data;
                temp.action = TableWidgetData::ACTION_DELETE_FROM_SERVER;
                testReleaseTable->dataActionPerItem.insert(i, temp);
                ui->console->append(QString("Item %0 marked for deletion from server").arg(testReleaseTable->dataActionPerItem.value(i).data.name));
            }
            else if(testReleaseTable->dataActionPerItem.value(i).action == TableWidgetData::ACTION_COPY_TO_SERVER) {
                ui->console->append(QString("Item %0 removed from list").arg(testReleaseTable->dataActionPerItem.value(i).data.name));
                testReleaseTable->dataActionPerItem.remove(i);
            }
        }
    }
    TableWidgetData::dataActionStruct temp;
    temp.data = data;
    temp.action = TableWidgetData::ACTION_COPY_TO_SERVER;
    int newIndex = getFirstFreeIndex(testReleaseTable);
    testReleaseTable->dataActionPerItem.insert(newIndex, temp);
    this->fillTable(testReleaseTable);
    processStatusChange(oldStatus);
    return newIndex;
}

void MainWindow::onFetchButtonPressed()
{
    ui->console->append(QString("Starting INFO file %0 download").arg(settings->settings.infoReleaseFilename));
    if(settings->settings.infoUseFtp) {
        if(!ftpLogin())
            return;
        QString file = settings->settings.infoPath + settings->settings.infoReleaseFilename;
        processStatusChange(STATUS_FETCHING_INFO_FILE);
        int ftpOpId = ftp->get(file);
        ftpOperations.insert(ftpOpId, QString("Fetching file %0").arg(file));
        ftpDownloads.append(ftpOpId);
    }
    else {
        processStatusChange(STATUS_FETCHING_INFO_FILE);
        QString file;
        if(!settings->settings.infoPath.endsWith("/"))
            file = settings->settings.infoPath + "/" + settings->settings.infoReleaseFilename;
        else
            file = settings->settings.infoPath + settings->settings.infoReleaseFilename;
        QString user;
        if(settings->settings.infoUseFtp)
            user = settings->settings.ftpUserName + "@";
        ui->console->append(QString("Downloading from %0%1").arg(user).arg(file));
        fileUtils->startFileDownload(QUrl(file));
    }
}



void MainWindow::fillComboBoxes()
{
    QStringList temp = xmlParser::osTypeToStringHash.values();
    temp.sort();
    foreach (QString typeStr, temp) {
        ui->osCB->addItem(typeStr, xmlParser::osTypeToStringHash.key(typeStr));
    }
    temp = xmlParser::softTypeToStringHash.values();
    temp.sort();
    foreach (QString typeStr, temp) {
        ui->typeCB->addItem(typeStr, xmlParser::softTypeToStringHash.key(typeStr));
    }
    temp = xmlParser::hwTypeToStringHash.values();
    temp.sort();
    foreach (QString typeStr, temp) {
        ui->hwCB->addItem(typeStr, xmlParser::hwTypeToStringHash.key(typeStr));
    }
}

QString MainWindow::calculateMD5(QString filename)
{
    ui->console->append(QString("Calculating MD5 of %0").arg(filename));
    QFile file(filename);
    if(file.open(QIODevice::ReadOnly)) {
        QString ret = QString(QCryptographicHash::hash((file.readAll()),QCryptographicHash::Md5).toHex());
        ui->console->append(QString("MD5=%0").arg(ret));
        file.close();
        return ret;
    }
    else {
        ui->console->append("Could not open file to calculate MD5");
    }
    return QString();
}



void MainWindow::onDeleteButtonPressed()
{
    TableWidgetData *currentWidget;
    if(ui->tabWidget->currentIndex() == 0)
        currentWidget = releaseTable;
    if(ui->tabWidget->currentIndex() == 1)
        currentWidget = testReleaseTable;
    else
        currentWidget = oldReleaseTable;
    if(currentWidget->table->selectedItems().count() < 1)
        return;
    int index = currentWidget->table->item(currentWidget->table->selectedItems().at(0)->row(), 0)->data(Qt::UserRole).toInt();
    if(currentWidget->dataActionPerItem.value(index).action != TableWidgetData::ACTION_COPY_TO_SERVER) {//TODO CHECK
        TableWidgetData::dataActionStruct temp;
        temp.data = currentWidget->dataActionPerItem.value(index).data;
        temp.action = TableWidgetData::ACTION_DELETE_FROM_SERVER;
        currentWidget->dataActionPerItem.insert(index, temp);
    }
    else {
        currentWidget->dataActionPerItem.remove(currentWidget->table->item(currentWidget->table->selectedItems().at(0)->row(), 0)->data(Qt::UserRole).toInt());
    }
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
        return;
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
        if(!result || (data.length() == 0)) {
            if(error == QNetworkReply::ContentNotFoundError || data.length() == 0) {
                    if(QMessageBox::question(this, "Information file apears not to exist on the server", "Do you want to create one?") == QMessageBox::Yes) {
                        processStatusChange(STATUS_NEW_SYSTEM);

                    }
                    else
                        processStatusChange(oldStatus);
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
    QString condition1_text;
    QString condition2_text;
    switch ((xmlParser::osTypeEnum)ui->osCB->currentData().toInt()) {
    case xmlParser::OS_LINUX64:
    case xmlParser::OS_LINUX32:
        switch ((xmlParser::osTypeEnum)ui->typeCB->currentData().toInt()) {
        case xmlParser::SOFT_SLIM_GCS:
            condition1 = QDir(extractedPath + "slimgcs").exists();
            condition1_text = "Directory slimgcs exists";
            condition2 = QFileInfo(extractedPath + "tbsagent").isSymLink();//TODO
            condition2_text = "Symlink to slimgcs binary exists";
            break;
        case xmlParser::SOFT_GCS:
            condition1 = QDir(extractedPath + "gcs").exists();
            condition1_text = "Directory gcs exists";
            condition2 = QFileInfo(extractedPath + "taulabsgcs").isSymLink();
            condition2_text = "Symlink to taulabsgcs binary exists";
        case xmlParser::SOFT_UPDATER:
            condition2 = QFile(completePath).exists();
            condition2_text = completePath + "exists";
            condition1 = true;
            condition1_text = "";
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
            condition2_text = extractedPath + "flight" + QString(QDir::separator()) + ui->hwCB->currentText().toLower() + QString(QDir::separator()) + QString("fw_%0.tlfw").arg(ui->hwCB->currentText().toLower()) + " exists on the extracted directory";
            condition1 = true;
            condition1_text = "";
            qDebug() << "CHECKING " << condition2_text;
            break;
        case xmlParser::SOFT_BOOTLOADER:
            condition2 = QFile(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + QString("bu_%0.tlfw").arg(ui->hwCB->currentText().toLower())).exists();
            condition2_text = extractedPath + "flight" + QString(QDir::separator()) + ui->hwCB->currentText().toLower() + QString(QDir::separator()) + QString("bu_%0.tlfw").arg(ui->hwCB->currentText().toLower()) + " exists on the extracted directory";
            condition1 = true;
            condition1_text = "";
            break;
        case xmlParser::SOFT_SETTINGS:
            condition1 = QFile(completePath).exists();
            condition1_text = completePath + " exists";
            condition2 = true;
        default:
            condition1 = true;
            condition2 = true;
            break;
        }
    default:
        break;
    }
    if(!condition1_text.isEmpty()) {
        if(condition1)
            ui->console->append(QString("%0? = SUCCESS").arg(condition1_text));
        else
            ui->console->append(QString("%0? = FAILED").arg(condition1_text));
    }
    if(!condition2_text.isEmpty()) {
        if(condition2)
            ui->console->append(QString("%0? = SUCCESS").arg(condition2_text));
        else
            ui->console->append(QString("%0? = FAILED").arg(condition2_text));
    }
    if(!condition1 || !condition2) {
        ui->console->append("FAILED to find required files, ABORTING!");
        processStatusChange(STATUS_CREATING_ITEM);
        return false;
    }
    //only if not updater binary or settings file
    if(((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_UPDATER) && ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()!=xmlParser::SOFT_SETTINGS) ) {
        ui->console->append("Processing INFO file");
        QFile infoFile(extractedPath + "BUILD_INFO");
        if(!infoFile.open(QIODevice::ReadOnly)) {
            ui->console->append("FAILED to open INFO file");
            processStatusChange(STATUS_CREATING_ITEM);
            return false;
        }
        QTextStream in(&infoFile);
        QString tagStr, valueStr;
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
                    tagStr = "gitHash";
                    valueStr = gitHash;
                }
                else if(l.at(0) == "DATE") {
                    ui->dateEdit->setDate(QDate::fromString(l.at(1), "yyyyMMdd"));
                    tagStr = "Date";
                    valueStr = l.at(1);
                }
                else if(l.at(0) == "UAVO_HASH") {
                    QString temp;
                    temp = l.at(1);
                    temp = temp.remove(",").remove("0x");
                    ui->uavoHashLE->setText(temp);
                    tagStr = "UAVO Hash";
                    valueStr = temp;
                }
                ui->console->append(QString("Info file says %0=%1").arg(tagStr).arg(valueStr));
            }
        }
        ui->console->append("Done processing INFO file");
        if((xmlParser::osTypeEnum)ui->osCB->currentData().toInt() != xmlParser::OS_EMBEDED) {
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
    serverStoragePath = serverStoragePath.replace(QString("%0%1").arg(QDir::separator()).arg(QDir::separator()), QDir::separator());
    ui->console->append(QString("Setting %0 to %1").arg("releasePartialPath").arg(releasePartialPath));
    ui->console->append(QString("Setting %0 to %1").arg("releaseStoragePath").arg(releaseStoragePath));
    ui->console->append(QString("Setting %0 to %1").arg("serverStoragePath").arg(serverStoragePath));

    QString file;
    QString source, destination;
    bool partialResult = false;
    switch ((xmlParser::softTypeEnum)ui->typeCB->currentData().toInt()) {
    case xmlParser::SOFT_BOOTLOADER:
        file = QString("bu_%0_%1.tlfw").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        source = QString(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + "bu_%0.tlfw").arg(ui->hwCB->currentText().toLower());
        destination = releaseStoragePath + file;
        result = QFile::copy(source, destination);
        ui->md5LE->setText(calculateMD5(destination));
        ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(result));
        break;
    case xmlParser::SOFT_FIRMWARE:
        file = QString("fw_%0_%1.tlfw").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        source = QString(extractedPath + "flight" + QDir::separator() + ui->hwCB->currentText().toLower() + QDir::separator() + "fw_%0.tlfw").arg(ui->hwCB->currentText().toLower());
        destination = releaseStoragePath + file;
        result = QFile::copy(source, destination);
        ui->md5LE->setText(calculateMD5(destination));
        ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(result));
        break;
    case xmlParser::SOFT_SETTINGS:
        file = QString( "settings_%0_%1.xml").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        source = completePath;
        destination = releaseStoragePath + file;
        result = QFile::copy(source, destination);
        ui->md5LE->setText(calculateMD5(destination));
        ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(result));
        break;
    case xmlParser::SOFT_GCS:
    case xmlParser::SOFT_SLIM_GCS:
        file = QString("%0_%1.zip").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->releaseLinkE->setText(serverStoragePath + file);
        source = QString(workingRoot + "currentbuild" + QDir::separator() + "app.zip");
        destination = releaseStoragePath + file;
        result = QFile::copy(source, destination);
        ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(result));
        ui->md5LE->setText(calculateMD5(destination));
        file = QString("%0_%1.xml").arg(ui->dateEdit->date().toString("yyyyMMdd")).arg(gitHash);
        ui->scritLinkLE->setText(serverStoragePath + file);
        source = QString(workingRoot + "currentbuild" + QDir::separator() + "file_list.xml");
        destination = releaseStoragePath + file;
        partialResult = QFile::copy(source, destination);
        ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(partialResult));
        result &= partialResult;
        break;
    case xmlParser::SOFT_UPDATER:
        switch ((xmlParser::osTypeEnum)ui->osCB->currentData().toInt()) {
        case xmlParser::OS_LINUX32:
        case xmlParser::OS_LINUX64:
            file = QString("updater");
            ui->releaseLinkE->setText(serverStoragePath + file);
            source = completePath;
            destination = releaseStoragePath + file;
            result = QFile::copy(source, destination);
            ui->md5LE->setText(calculateMD5(destination));
            ui->console->append(QString("Copying %0 to %1 RESULT=%3").arg(source).arg(destination).arg(partialResult));
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
        if(QDir(dest).exists())
            QDir(dest).removeRecursively();
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
    if(ftpDirsCheckOperations.contains(opID)) {
        ftpDirsCheckOperations.removeAll(opID);
        lastFtpOperationSuccess = !error;
        ftpDirCheckEventLoop.quit();
    }
    if(ftpMkDirOperations.contains(opID)) {
        ftpMkDirOperations.removeAll(opID);
        lastFtpOperationSuccess = !error;
        ftpDirCheckEventLoop.quit();
    }
    if(ftpDownloads.contains(opID)) {
        ftpDownloads.removeAll(opID);
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
                    if(QMessageBox::question(this, "Information file apears not to exist on the server", "Do you want to create one?") == QMessageBox::Yes) {
                        processStatusChange(STATUS_NEW_SYSTEM);
                    }
                    else
                        processStatusChange(oldStatus);
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
    QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> dataset = parser->parseXML(QString(array), success);
    if(!success) {
        ui->console->append("XML information file parsing FAILED");
        return false;
    }
    if(releaseTable)
        delete releaseTable;
    if(oldReleaseTable)
        delete oldReleaseTable;
    if(testReleaseTable)
        delete testReleaseTable;
    releaseTable = new TableWidgetData(this, ui->featuredReleasesTable, dataset.values(xmlParser::RELEASE_CURRENT));
    oldReleaseTable = new TableWidgetData(this, ui->oldReleasesTable, dataset.values(xmlParser::RELEASE_OLD));
    testReleaseTable = new TableWidgetData(this, ui->testReleasesTable, dataset.values(xmlParser::RELEASE_TEST));
    this->fillTable(releaseTable);
    this->fillTable(oldReleaseTable);
    this->fillTable(testReleaseTable);
    processStatusChange(STATUS_EDITING_RELEASE);
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

void MainWindow::onftpListInfo(QUrlInfo info)
{
    ftpLastListing.append(info);
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

TableWidgetData::TableWidgetData(QObject *parent, QTableWidget *table, QList<xmlParser::softData> data):QObject(parent), table(table)
{
    for(int x = 0; x < data.length(); ++x) {
        TableWidgetData::dataActionStruct temp;
        temp.data = data.at(x);
        temp.action = TableWidgetData::ACTION_NONE;
        dataActionPerItem.insert(x, temp);
    }
    connect(table, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(onCurrentCellChanged(int,int,int,int)));
}

void TableWidgetData::onCurrentCellChanged(int x, int y, int xx, int yy)
{
    Q_UNUSED(y);
    Q_UNUSED(xx);
    Q_UNUSED(yy);
    emit currentRowChanged(x);
}
void MainWindow::onPushButtonPressed()
{
    QStringList filesToPush;
    QHash<QString, QString> localFiles;
    QStringList filesToDelete;
    QList<TableWidgetData* > workingTablesList;
    workingTablesList << testReleaseTable << oldReleaseTable << releaseTable;
    QString filename;
    foreach(TableWidgetData * workingTable, workingTablesList)
    {
        foreach (int i, workingTable->dataActionPerItem.keys()) {
            switch (workingTable->dataActionPerItem.value(i).action) {
            case TableWidgetData::ACTION_DELETE_FROM_SERVER:
                filesToDelete.append(workingTable->dataActionPerItem.value(i).data.releaseLink.toString());
                if(!workingTable->dataActionPerItem.value(i).data.scriptLink.isEmpty()) {
                    filesToDelete.append(workingTable->dataActionPerItem.value(i).data.scriptLink.toString());
                }
                break;
            case TableWidgetData::ACTION_COPY_TO_SERVER:
                filesToPush.append(workingTable->dataActionPerItem.value(i).data.releaseLink.toString());
                filename = QFileInfo(workingTable->dataActionPerItem.value(i).data.releaseLink.toString()).fileName();
                localFiles.insert(workingTable->dataActionPerItem.value(i).data.releaseLink.toString(), QDir::temp().absolutePath() + QDir::separator() + "release_builder" + QDir::separator() + "release" + QString::number(i) + QDir::separator() + filename);
                if(!workingTable->dataActionPerItem.value(i).data.scriptLink.isEmpty()) {
                    filesToPush.append(workingTable->dataActionPerItem.value(i).data.scriptLink.toString());
                    filename = QFileInfo(workingTable->dataActionPerItem.value(i).data.scriptLink.toString()).fileName();
                    localFiles.insert(workingTable->dataActionPerItem.value(i).data.scriptLink.toString(), QDir::temp().absolutePath() + QDir::separator() + "release_builder" + QDir::separator() + "release" + QString::number(i) + QDir::separator() + filename);
                }
                break;
            default:
                break;
            }
        }
    }
    QMultiHash<xmlParser::releaseTypeEnum, xmlParser::softData> dataList;
    foreach (TableWidgetData::dataActionStruct data, releaseTable->dataActionPerItem.values()) {
        dataList.insert(xmlParser::RELEASE_CURRENT, data.data);
    }
    foreach (TableWidgetData::dataActionStruct data, oldReleaseTable->dataActionPerItem.values()) {
        dataList.insert(xmlParser::RELEASE_OLD, data.data);
    }
    foreach (TableWidgetData::dataActionStruct data, testReleaseTable->dataActionPerItem.values()) {
        dataList.insert(xmlParser::RELEASE_TEST, data.data);
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
    if(ftpLogin()) {
        foreach (QString file, filesToDelete) {
            ftpOperations.insert(ftp->remove(file), QString("Removing file %0 from server").arg(file));
        }
        foreach (QString file, filesToPush) {
            QFile m_file(localFiles.value(file));
            if(m_file.open(QIODevice::ReadOnly)) {
                ui->console->append(QFileInfo(file).path());
                if(ftpCreateDirectory((QFileInfo(file).path()))) {
                    qDebug()<<"FILE="<<file;
                    ftp->cd("~");
                    ftpOperations.insert(ftp->put(m_file.readAll(), file), QString("Pushing file %0 to %1 on server").arg(localFiles.value(file)).arg(file));
                }
                else {
                    ui->console->append("Failed to create directory");
                }
            }
            else {
                ui->console->append(QString("ERROR could not open local file %0. Skipping").arg(localFiles.value(file)));
            }
        }
        ui->console->append("Pushing xml information file");
        ftpOperations.insert(ftp->put(xml.toUtf8(), settings->settings.infoReleaseFilename), QString("Pushing file:%0").arg(settings->settings.infoReleaseFilename));
        QList<TableWidgetData*> dataTables;
        dataTables << testReleaseTable << releaseTable << oldReleaseTable;
        foreach (TableWidgetData *table, dataTables) {
            foreach (int key, table->dataActionPerItem.keys()) {
                table->dataActionPerItem[key].action = TableWidgetData::ACTION_NONE;
            }
            fillTable(table);
        }
    }
}


void MainWindow::onMakeReleaseButtonPressed()
{
    QList<TableWidgetData*> tableList;
    tableList << testReleaseTable << releaseTable << oldReleaseTable;
    foreach (TableWidgetData *table, tableList) {
        foreach (TableWidgetData::dataActionStruct data, table->dataActionPerItem.values()) {
            if(data.action != TableWidgetData::ACTION_NONE) {
                QMessageBox::warning(this, "Can't make release", "Your working tables still have changes not submited to server, please push them and try again");
                return;
            }
        }
    }
    if(QMessageBox::question(this, "Please Confirm actions", "Do you really want to move the current test releases to the ALFA release?") != QMessageBox::Yes)
        return;
    QHash<int, TableWidgetData::dataActionStruct> releaseData = releaseTable->dataActionPerItem;
    QHash<int, TableWidgetData::dataActionStruct> oldReleaseData = oldReleaseTable->dataActionPerItem;

    foreach (int tkey, testReleaseTable->dataActionPerItem.keys()) {
        foreach (int key, releaseTable->dataActionPerItem.keys()) {
            if(testReleaseTable->dataActionPerItem.value(tkey).data.type == releaseTable->dataActionPerItem.value(key).data.type &&
                    testReleaseTable->dataActionPerItem.value(tkey).data.osType == releaseTable->dataActionPerItem.value(key).data.osType &&
                    testReleaseTable->dataActionPerItem.value(tkey).data.hwType == releaseTable->dataActionPerItem.value(key).data.hwType) {
                if(testReleaseTable->dataActionPerItem.value(tkey).data.osType == xmlParser::OS_EMBEDED)
                    ui->console->append(QString("Same %0 file already exists for %1 on current releases moving it to old releases").arg(xmlParser::softTypeToString(testReleaseTable->dataActionPerItem.value(tkey).data.type)).arg(xmlParser::hwTypeToStringHash.value(testReleaseTable->dataActionPerItem.value(tkey).data.hwType)));
                else
                    ui->console->append(QString("Same %0 file already exists for %1 on current releases moving it to old releases").arg(xmlParser::softTypeToString(testReleaseTable->dataActionPerItem.value(tkey).data.type)).arg(xmlParser::osTypeToString(testReleaseTable->dataActionPerItem.value(tkey).data.osType)));
                TableWidgetData::dataActionStruct temp;
                temp.data = releaseTable->dataActionPerItem.value(key).data;
                temp.action = TableWidgetData::ACTION_CHANGED_METADATA;
                oldReleaseData.insert(getFirstFreeIndex(oldReleaseData), temp);
                releaseData.remove(key);
                break;
            }
        }
        TableWidgetData::dataActionStruct temp;
        temp.data = testReleaseTable->dataActionPerItem.value(tkey).data;
        temp.action = TableWidgetData::ACTION_CHANGED_METADATA;
        releaseData.insert(getFirstFreeIndex(releaseData), temp);
    }
    QString uavHash;
    bool allEqual = true;
    ui->console->append("Checking if all release assets share the same UAVO");
    foreach (TableWidgetData::dataActionStruct data, releaseData.values()) {
        if(data.data.type != xmlParser::SOFT_UPDATER) {
            if(uavHash.isEmpty())
                uavHash = data.data.uavHash;
            else if(data.data.uavHash != uavHash) {
                allEqual = false;
                break;
            }
        }
    }
    if(!allEqual) {
        QMessageBox::warning(this, "Can't make release", "There are inconsistencies regarding the UAVO hashes of some release assets");
        ui->console->append("UAVO Hash inconsistencies found");
        return;
    }
    ui->console->append("No UAVO Hash inconsistencies found");
    testReleaseTable->dataActionPerItem.clear();
    releaseTable->dataActionPerItem = releaseData;
    oldReleaseTable->dataActionPerItem = oldReleaseData;
    fillTable(testReleaseTable);
    fillTable(releaseTable);
    fillTable(oldReleaseTable);
    QMessageBox::information(this, "Make release succeeded", "Check new tables and push release if everything looks OK");
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
    if(ftp->state() != QFtp::LoggedIn) {
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

bool MainWindow::ftpCreateDirectory(QString dir)
{
    int ftpCurrentPathIndexCheck = 0;
    ftpLastListing.clear();
    bool keepGoing = true;
    QStringList dirs = dir.split("/");
    dirs.insert(0,"~");
    qDebug() << dirs;
    dirs.removeAll("");
    while(keepGoing) {
        if(ftpCurrentPathIndexCheck < (dirs.length() -1)) {
            ftpOperations.insert(ftp->cd(dirs.at(ftpCurrentPathIndexCheck)), "CD to " + dirs.at(ftpCurrentPathIndexCheck));
            ftpLastListing.clear();
            int opID = ftp->list();
            ftpDirsCheckOperations.append(opID);
            lastFtpOperationSuccess = false;
            ftpOperations.insert(opID, QString("Listing directory %0 contents").arg(dirs.at(ftpCurrentPathIndexCheck)));
            ftpDirCheckEventLoop.exec();
            if(!lastFtpOperationSuccess)
                return false;
            bool exists = false;
            foreach (QUrlInfo info, ftpLastListing) {
                if(info.isDir()) {
                    qDebug() << "INFO=" << info.name() << dirs.at(ftpCurrentPathIndexCheck + 1);
                    if(info.name() == dirs.at(ftpCurrentPathIndexCheck + 1)) {
                        exists = true;
                    }
                }
            }
            ui->console->append(QString("Checking if directory %0 exists on server:%1").arg(dirs.at(ftpCurrentPathIndexCheck + 1)).arg(exists));
            if(!exists) {
                opID = ftp->mkdir(dirs.at(ftpCurrentPathIndexCheck + 1));
                lastFtpOperationSuccess = false;
                ftpOperations.insert(opID, QString("Creating directory %0").arg(dirs.at(ftpCurrentPathIndexCheck +1)));
                ftpMkDirOperations.append(opID);
                ftpDirCheckEventLoop.exec();
                if(!lastFtpOperationSuccess)
                    return false;
            }
            ++ftpCurrentPathIndexCheck;
        }
        else {
            keepGoing = false;
        }

    }
    return true;
}
