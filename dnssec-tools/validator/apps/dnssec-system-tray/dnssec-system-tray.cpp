/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui>

#include <qdebug.h>

#include "dnssec-system-tray.h"
#include "DnssecSystemTrayPrefs.h"

Window::Window()
    : m_icon(":/images/justlock.png"), m_logFile(0), m_fileWatcher(0), m_logStream(0),
      m_maxRows(5), m_rowCount(0), m_warningIcon(":/images/trianglewarning.png")
{
    loadPreferences(true);
    createLogWidgets();
    createActions();
    createTrayIcon();
    setLayout(m_topLayout);

    createRegexps();

    // setup the tray icon
    trayIcon->setToolTip(tr("Shows the status of DNSSEC Requests on the system"));
    trayIcon->setIcon(m_icon);
    setWindowIcon(m_icon);

    connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));


    trayIcon->show();

    setWindowTitle(tr("DNSSEC Log Messages"));
}

void Window::loadPreferences(bool seekToEnd) {
    if (!m_fileWatcher) {
        delete m_fileWatcher;
        m_fileWatcher = 0;
    }
    QSettings settings("DNSSEC-Tools", "dnssec-system-tray");
    m_fileName = settings.value("logFile", "").toString();
    m_maxRows = settings.value("logNumber", 5).toInt();
    m_showStillRunningWarning = settings.value("stillRunningWarning", true).toBool();
    openLogFile(seekToEnd);
}

void Window::setVisible(bool visible)
{
    hideAction->setEnabled(visible);
    showAction->setEnabled(isMaximized() || !visible);
    QDialog::setVisible(visible);
}

void Window::closeEvent(QCloseEvent *event)
{
    if (trayIcon->isVisible()) {
        if (m_showStillRunningWarning)
            QMessageBox::information(this, tr("Systray"),
                                     tr("The program will keep running in the "
                                        "system tray. To terminate the program, "
                                        "choose <b>Quit</b> in the context menu "
                                        "of the system tray entry."));
        hide();
        event->ignore();
    }
}

void Window::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
    case QSystemTrayIcon::MiddleClick:
        showNormal();
        break;
    default:
        ;
    }
}

void Window::showMessage(const QString &message)
{
    trayIcon->showMessage("DNSSEC Validation", message, QSystemTrayIcon::Information, 5 * 1000);
}

void Window::messageClicked()
{
    showNormal();
}

void Window::createLogWidgets()
{
    m_topLayout = new QVBoxLayout();
    m_topLayout->addWidget(m_topTitle = new QLabel("DNSSEC Log Messages"));
    m_topLayout->addWidget(m_log = new QTableWidget(m_maxRows, 3));
    m_log->resizeRowsToContents();
    m_log->resizeColumnsToContents();
    m_log->setShowGrid(false);
}


void Window::createActions()
{
    hideAction = new QAction(tr("&Hide"), this);
    connect(hideAction, SIGNAL(triggered()), this, SLOT(hide()));

    showAction = new QAction(tr("&Show Log"), this);
    connect(showAction, SIGNAL(triggered()), this, SLOT(showNormal()));

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    prefsAction = new QAction(tr("&Preferences"), this);
    connect(prefsAction, SIGNAL(triggered()), this, SLOT(showPreferences()));
}

void Window::showPreferences() {
    DnssecSystemTrayPrefs prefs;
    connect(&prefs, SIGNAL(accepted()), this, SLOT(loadPreferences()));
    prefs.exec();
}

void Window::createTrayIcon()
{
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(hideAction);
    trayIconMenu->addAction(showAction);
    trayIconMenu->addAction(prefsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}

void Window::createRegexps() {
    m_bogusRegexp = QRegExp("Validation result for \\{([^,]+),.*BOGUS");
}

void Window::openLogFile(bool seekToEnd)
{
    if (m_logFile) {
        delete m_logFile;
        m_logFile = 0;
    }

    if (m_logStream) {
        delete m_logStream;
        m_logStream = 0;
    }

    // first watch this file if need be for changes (including creation)
    if (!m_fileWatcher) {
        m_fileWatcher = new QFileSystemWatcher();
        m_fileWatcher->addPath(m_fileName);
        connect(m_fileWatcher, SIGNAL(fileChanged(QString)), this, SLOT(parseTillEnd()));
    }

    // open the file if possible
    m_logFile = new QFile(m_fileName);
    if (!m_logFile->exists())
        return;

    if (!m_logFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
        delete m_logFile;
        m_logFile = 0;
        return;
    }

    // jump to the end; we only look for new things.
    if (seekToEnd)
        m_logFile->seek(m_logFile->size());

    // create the log stream
    m_logStream = new QTextStream(m_logFile);
    qDebug() << "succeeded in opening " << m_fileName;
}

void Window::parseTillEnd()
{
    if (!m_logStream) {
        openLogFile();
        if (!m_logStream)
            return;
    }
    while (!m_logStream->atEnd()) {
        QString line = m_logStream->readLine();
        parseLogMessage(line);
    }
}

void Window::parseLogMessage(const QString logMessage) {
    if (m_bogusRegexp.indexIn(logMessage) > -1) {
        showMessage(QString("DNSSEC Validation Failure on %1").arg(m_bogusRegexp.cap(1)));
        if (m_log->rowCount() != m_maxRows)
            m_log->setRowCount(m_maxRows);
        if (m_rowCount+1 >= m_maxRows) {
            // XXX
            for(int i = 0; i < m_maxRows; i++) {
                for(int j = 0; j < 3; j++) {
                    m_log->setItem(i, j, m_log->takeItem(i+1, j));
                }
            }
        }
        m_log->setItem(m_rowCount, 0, new QTableWidgetItem(m_warningIcon, ""));
        m_log->setItem(m_rowCount, 1, new QTableWidgetItem(m_bogusRegexp.cap(1)));
        m_log->setItem(m_rowCount, 2,
                       new QTableWidgetItem(QDateTime::currentDateTime().toString()));
        m_log->resizeColumnsToContents();
        m_log->resizeRowsToContents();
        if (m_rowCount+1 < m_maxRows)
            m_rowCount++;
    }
}

QSize Window::sizeHint() const {
    return QSize(800, 420); // should fit most modern devices.
}