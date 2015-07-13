/**
 ******************************************************************************
 * @file       settings.cpp
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @addtogroup [Group]
 * @{
 * @addtogroup Settings
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

#include "settings.h"
#include "ui_settings.h"
#include <QSettings>

Settings::Settings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Settings)
{
    m_sSettingsFile = QApplication::applicationDirPath() + "releasebuilder.ini";
    ui->setupUi(this);
    updaterBinaryPaths.insert(xmlParser::OS_LINUX32, ui->linux32UpdaterPath);
    updaterBinaryPaths.insert(xmlParser::OS_LINUX64, ui->linux64UpdaterPath);
    updaterBinaryPaths.insert(xmlParser::OS_OSX32, ui->osx32UpdaterPath);
    updaterBinaryPaths.insert(xmlParser::OS_OSX64, ui->osx64UpdaterPath);
    updaterBinaryPaths.insert(xmlParser::OS_WIN32, ui->windows32UpdaterPath);
    updaterBinaryPaths.insert(xmlParser::OS_WIN64, ui->windows64UpdaterPath);

    updaterScriptPaths.insert(xmlParser::OS_LINUX32, ui->linux32ScriptPath);
    updaterScriptPaths.insert(xmlParser::OS_LINUX64, ui->linux64ScriptPath);
    updaterScriptPaths.insert(xmlParser::OS_OSX32, ui->osx32ScriptPath);
    updaterScriptPaths.insert(xmlParser::OS_OSX64, ui->osx64ScriptPath);
    updaterScriptPaths.insert(xmlParser::OS_WIN32, ui->windows32ScriptPath);
    updaterScriptPaths.insert(xmlParser::OS_WIN64, ui->windows64ScriptPath);
    loadSettings();
    connect(ui->saveSettingsButton, SIGNAL(clicked()), this, SLOT(onSaveSettings()));
    connect(ui->cancelSettingsButton, SIGNAL(clicked()), this, SLOT(onCancelSettings()));
    foreach (xmlParser::osTypeEnum os, xmlParser::osTypesList()) {
        ui->updaterOS_ComboBox->addItem(xmlParser::osTypeToString(os), (int)os);
    }
    connect(ui->updaterOS_ComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeCurrentOS(int)));
    ui->updaterOS_ComboBox->setCurrentIndex(0);
    ui->updaterStackedWidget->setCurrentIndex(0);

}

Settings::~Settings()
{
    delete ui;
}

void Settings::saveSettings()
{
    onSaveSettings();
}

void Settings::loadSettings()
{
    QSettings m_settings(m_sSettingsFile, QSettings::NativeFormat);
    settings.ftpPassword = m_settings.value("ftppassword").toString();
    settings.ftpPath = m_settings.value("ftppath").toString();
    settings.ftpServerUrl = m_settings.value("ftpserverurl").toString();
    settings.ftpUserName = m_settings.value("ftpusername").toString();
    settings.infoPath = m_settings.value("infopath").toString();
    settings.infoReleaseFilename = m_settings.value("inforeleasefilename").toString();
    settings.infoUseFtp = m_settings.value("infouseftp").toBool();
    settings.rubyScriptPath = m_settings.value("rubyScriptPath").toString();
    foreach (xmlParser::osTypeEnum os, xmlParser::osTypesList()) {
        settings.updaterBinaryPath.insert(os, m_settings.value("updaterBinaryPath" + xmlParser::osTypeToString(os)).toString());
        updaterBinaryPaths.value(os)->setText(settings.updaterBinaryPath.value(os));
    }
    foreach (xmlParser::osTypeEnum os, xmlParser::osTypesList()) {
        settings.updaterScriptPath.insert(os, m_settings.value("updaterScriptPath" + xmlParser::osTypeToString(os)).toString());
        updaterScriptPaths.value(os)->setText(settings.updaterScriptPath.value(os));
    }
    ui->ftpAdress->setText(settings.ftpServerUrl);
    ui->ftpPassword->setText(settings.ftpPassword);
    ui->ftpPath->setText(settings.ftpPath);
    ui->ftpUsername->setText(settings.ftpUserName);
    if(settings.infoUseFtp)
        ui->infoMethod->setCurrentIndex(0);
    else
        ui->infoMethod->setCurrentIndex(1);
    ui->infoPath->setText(settings.infoPath);
    ui->infoReleaseName->setText(settings.infoReleaseFilename);
    ui->rubyScriptPath->setText(settings.rubyScriptPath);
}

void Settings::onSaveSettings()
{
    QSettings m_settings(m_sSettingsFile, QSettings::NativeFormat);
    settings.ftpPassword = ui->ftpPassword->text();
    m_settings.setValue("ftppassword", settings.ftpPassword);
    settings.ftpPath = ui->ftpPath->text();
    m_settings.setValue("ftppath", settings.ftpPath);
    settings.ftpServerUrl = ui->ftpAdress->text();
    m_settings.setValue("ftpserverurl", settings.ftpServerUrl);
    settings.ftpUserName = ui->ftpUsername->text();
    m_settings.setValue("ftpusername", settings.ftpUserName);
    if(!ui->infoPath->text().endsWith("/"))
        ui->infoPath->setText(ui->infoPath->text() + "/");
    settings.infoPath = ui->infoPath->text();
    m_settings.setValue("infopath", settings.infoPath);
    settings.infoReleaseFilename = ui->infoReleaseName->text();
    m_settings.setValue("inforeleasefilename", settings.infoReleaseFilename);
    settings.rubyScriptPath = ui->rubyScriptPath->text();
    m_settings.setValue("rubyScriptPath", settings.rubyScriptPath);
    if(ui->infoMethod->currentText()=="FTP")
        settings.infoUseFtp = true;
    else
        settings.infoUseFtp = false;
    m_settings.setValue("infouseftp", settings.infoUseFtp);
    foreach (xmlParser::osTypeEnum os, xmlParser::osTypesList()) {
        settings.updaterBinaryPath.insert(os, updaterBinaryPaths.value(os)->text());
        m_settings.setValue("updaterBinaryPath" + xmlParser::osTypeToString(os), settings.updaterBinaryPath.value(os));
    }
    foreach (xmlParser::osTypeEnum os, xmlParser::osTypesList()) {
        settings.updaterScriptPath.insert(os, updaterScriptPaths.value(os)->text());
        m_settings.setValue("updaterScriptPath" + xmlParser::osTypeToString(os), settings.updaterScriptPath.value(os));
    }
    this->close();
}

void Settings::onCancelSettings()
{
    this->close();
}

void Settings::changeCurrentOS(int i)
{
    ui->updaterStackedWidget->setCurrentIndex(i);
}
