#-------------------------------------------------
#
# Project created by QtCreator 2015-04-06T22:19:20
#
#-------------------------------------------------

QT       += core gui
QT       += webkitwidgets
QT       += xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ReleaseBuilder
TEMPLATE = app

INCLUDEPATH+=../qftp .
DEPENDPATH+=../qftp .
SOURCES += main.cpp\
        mainwindow.cpp \
    webfileutils.cpp \
    xmlparser.cpp \
    settings.cpp \
    ftpcredentials.cpp

HEADERS  += mainwindow.h \
    webfileutils.h \
    xmlparser.h \
    settings.h \
    ftpcredentials.h

FORMS    += mainwindow.ui \
    settings.ui \
    ftpcredentials.ui

RESOURCES += \
    resources.qrc

LIBS*= -L../qftp -lQtFtp
