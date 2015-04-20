/**
 ******************************************************************************
 * @file       ftpcredentials.cpp
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @addtogroup [Group]
 * @{
 * @addtogroup ftpCredentials
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

#include "ftpcredentials.h"
#include "ui_ftpcredentials.h"
#include <QEventLoop>
#include <QDebug>

ftpCredentials::ftpCredentials(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ftpCredentials)
{
    this->setModal(true);
    loop = new QEventLoop(this);
    ui->setupUi(this);
    connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
    connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));
}

ftpCredentials::~ftpCredentials()
{
    delete ui;
}

ftpCredentials::credentials ftpCredentials::getCredentials(QString username, QString password)
{
    credentials cred;
    ui->username->setText(username);
    ui->password->setText(password);
    this->show();
    loop->exec();
    if(result() == QDialog::Accepted) {
        cred.password = ui->password->text();
        cred.username = ui->username->text();
        cred.remember = ui->remember->isChecked();
    }
    return cred;
}

void ftpCredentials::onAccepted()
{
    loop->quit();
}

void ftpCredentials::onRejected()
{
    loop->quit();
}
