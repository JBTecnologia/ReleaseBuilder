/**
 ******************************************************************************
 * @file       settings.h
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include "xmlparser.h"
#include <QHash>
#include <QLineEdit>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
    Q_OBJECT

public:
    struct settingsStruct
    {
        QString ftpServerUrl;
        QString ftpUserName;
        QString ftpPassword;
        QString ftpPath;
        QString infoPath;
        QString infoReleaseFilename;
        QString rubyScriptPath;
        bool infoUseFtp;
        QHash<xmlParser::osTypeEnum, QString> updaterScriptPath;
        QHash<xmlParser::osTypeEnum, QString> updaterBinaryPath;
    }settings;

    explicit Settings(QWidget *parent = 0);
    ~Settings();
    void saveSettings();
private:
    QString m_sSettingsFile;
    Ui::Settings *ui;
    void loadSettings();
    QHash<xmlParser::osTypeEnum, QLineEdit *> updaterBinaryPaths;
    QHash<xmlParser::osTypeEnum, QLineEdit *> updaterScriptPaths;
private slots:
    void onSaveSettings();
    void onCancelSettings();
    void changeCurrentOS(int);
};

#endif // SETTINGS_H
