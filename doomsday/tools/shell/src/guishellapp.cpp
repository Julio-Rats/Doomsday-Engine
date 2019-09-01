/** @file shellapp.cpp  Shell GUI application.
 *
 * @authors Copyright © 2013-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "guishellapp.h"

#include "aboutdialog.h"
#include "linkwindow.h"
#include "localserverdialog.h"
#include "opendialog.h"
#include "preferences.h"
#include "shellwindowsystem.h"
//#include "utils.h"
#include <de/EscapeParser>
#include <de/Id>
#include <de/comms/LocalServer>
#include <de/comms/ServerFinder>
//#include <QMenuBar>
//#include <QMessageBox>
//#include <QUrl>
//#include <QSettings>
//#include <QTimer>
//#include <QDesktopServices>

//Q_DECLARE_METATYPE(de::Address)

using namespace de;
using namespace de::shell;

DE_PIMPL_NOREF(GuiShellApp)
{
    std::unique_ptr<ShellWindowSystem> winSys;
    ServerFinder finder;

//    QMenuBar *menuBar;
//    QMenu *localMenu;
    PopupMenuWidget *localMenu;
#ifdef MACOSX
//    QAction *stopAction;
//    QAction *disconnectAction;
#endif
//    QList<LinkWindow *> windows;
//    QHash<int, LocalServer *> localServers; // port as key
//    QTimer localCheckTimer;

    Preferences *prefs;

    Impl() : prefs(0)
    {
//        localCheckTimer.setInterval(1000);
//        localCheckTimer.setSingleShot(false);
    }

    ~Impl()
    {
//        foreach (LinkWindow *win, windows)
//        {
//            delete win;
//        }
    }
};

GuiShellApp::GuiShellApp(const StringList &args)
    : BaseGuiApp(args)
    , d(new Impl)
{
//    setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Applicatio metadata.
    {
        auto &md = metadata();
        md.set(ORG_DOMAIN, "dengine.net");
        md.set(ORG_NAME, "Deng Team");
        md.set(APP_NAME, "Shell");
        md.set(APP_VERSION, SHELL_VERSION);
    }

//    d->localMenu = new QMenu(tr("Running Servers"));
//    connect(d->localMenu, SIGNAL(aboutToShow()), this, SLOT(updateLocalServerMenu()));

#ifdef MACOSX
//    setQuitOnLastWindowClosed(false);

//    // On macOS, the menu is not window-specific.
//    d->menuBar = new QMenuBar(0);

//    QMenu *menu = d->menuBar->addMenu(tr("Connection"));
//    menu->addAction(tr("Connect..."), this, SLOT(connectToServer()),
//                    QKeySequence(tr("Ctrl+O", "Connection|Connect")));
//    d->disconnectAction = menu->addAction(tr("Disconnect"), this, SLOT(disconnectFromServer()),
//                                          QKeySequence(tr("Ctrl+D", "Connection|Disconnect")));
//    d->disconnectAction->setDisabled(true);
//    menu->addSeparator();
//    menu->addAction(tr("Close Window"), this, SLOT(closeActiveWindow()),
//                    QKeySequence(tr("Ctrl+W", "Connection|Close Window")));

//    QMenu *svMenu = d->menuBar->addMenu(tr("Server"));
//    svMenu->addAction(tr("New Local Server..."), this, SLOT(startLocalServer()),
//                      QKeySequence(tr("Ctrl+N", "Server|New Local Server")));
//    d->stopAction = svMenu->addAction(tr("Stop"), this, SLOT(stopServer()));
//    svMenu->addSeparator();
//    svMenu->addMenu(d->localMenu);

//    connect(menu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));
//    connect(svMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));

//    // These will appear in the application menu:
//    menu->addAction(tr("Preferences..."), this, SLOT(showPreferences()), QKeySequence(tr("Ctrl+,")));
//    menu->addAction(tr("About"), this, SLOT(aboutShell()));

//    d->menuBar->addMenu(makeHelpMenu());
#endif

//    connect(&d->localCheckTimer, SIGNAL(timeout()), this, SLOT(checkLocalServers()));
//    d->localCheckTimer.start();

//    newOrReusedConnectionWindow();
}

void GuiShellApp::initSubsystems(App::SubsystemInitFlags flags)
{
    BaseGuiApp::initSubsystems(flags);
    d->winSys.reset(new ShellWindowSystem);
}

LinkWindow *GuiShellApp::newOrReusedConnectionWindow()
{
    LinkWindow *found = nullptr;
//    QWidget *other = activeWindow(); // for positioning a new window

//    // Look for a window with a closed connection.
//    foreach (LinkWindow *win, d->windows)
//    {
//        if (!win->isConnected())
//        {
//            found = win;
//            found->raise();
//            found->activateWindow();
//            d->windows.removeOne(win);
//            break;
//        }
//        if (!other) other = win;
//    }

    if (!found)
    {
        found = d->winSys->newWindow<LinkWindow>(Stringf("link%04u", Id().asUInt32()));
//        connect(found, SIGNAL(linkOpened(LinkWindow*)),this, SLOT(updateMenu()));
//        connect(found, SIGNAL(linkClosed(LinkWindow*)), this, SLOT(updateMenu()));
//        connect(found, SIGNAL(closed(LinkWindow *)), this, SLOT(windowClosed(LinkWindow *)));

        // Initial position and size.
//        if (other)
//        {
//            found->move(other->pos() + QPoint(30, 30));
//        }
    }

//    d->windows.prepend(found);
//    found->show();
    return found;
}

GuiShellApp &GuiShellApp::app()
{
    return *static_cast<GuiShellApp *>(DE_BASE_GUI_APP);
}

PopupMenuWidget &GuiShellApp::localServersMenu()
{
    return *d->localMenu;
}

//QMenu *GuiShellApp::makeHelpMenu()
//{
//    QMenu *helpMenu = new QMenu(tr("&Help"));
//    helpMenu->addAction(tr("Shell Help"), this, SLOT(showHelp()));
//    return helpMenu;
//}

ServerFinder &GuiShellApp::serverFinder()
{
    return d->finder;
}

void GuiShellApp::connectToServer()
{
    LinkWindow *win = newOrReusedConnectionWindow();

//    QScopedPointer<OpenDialog> dlg(new OpenDialog(win));
//    dlg->setWindowModality(Qt::WindowModal);

//    if (dlg->exec() == OpenDialog::Accepted)
//    {
//        win->openConnection(dlg->address());
//    }
}

void GuiShellApp::connectToLocalServer()
{
//    QAction *act = dynamic_cast<QAction *>(sender());
//    Address host = act->data().value<Address>();

//    LinkWindow *win = newOrReusedConnectionWindow();
//    win->openConnection(convert(host.asText()));
}

void GuiShellApp::disconnectFromServer()
{
//    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
//    if (win)
//    {
//        win->closeConnection();
//    }
}

void GuiShellApp::closeActiveWindow()
{
//    QWidget *win = activeWindow();
//    if (win) win->close();
}

void GuiShellApp::startLocalServer()
{
    try
    {
#ifdef MACOSX
//        // App folder randomization means we can't find Doomsday.app on our own.
//        if (!QSettings().contains("Preferences/appFolder"))
//        {
//            showPreferences();
//            return;
//        }
#endif
//        LocalServerDialog dlg;
//        if (dlg.exec() == QDialog::Accepted)
//        {
//            QStringList opts = dlg.additionalOptions();
//            if (!Preferences::iwadFolder().isEmpty())
//            {
//                opts << "-iwad" << convert(Preferences::iwadFolder());
//            }

//            auto *sv = new LocalServer;
//            sv->setApplicationPath(convert(QSettings().value("Preferences/appFolder").toString()));
//            if (!dlg.name().isEmpty())
//            {
//                sv->setName(convert(dlg.name()));
//            }
//            sv->start(dlg.port(),
//                      convert(dlg.gameMode()),
//                      map<StringList>(opts, convertToString), //[](const QString &s) { return convert(s); }),
//                      dlg.runtimeFolder());
//            d->localServers[dlg.port()] = sv;

//            newOrReusedConnectionWindow()->waitForLocalConnection
//                    (dlg.port(), sv->errorLogPath(), dlg.name());
//        }
    }
    catch (const Error &er)
    {
        EscapeParser esc;
        esc.parse(er.asText());

//        QMessageBox::critical(0, tr("Failed to Start Server"), esc.plainText().c_str());

        showPreferences();
    }
}

void GuiShellApp::stopServer()
{
//    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
//    if (win && win->isConnected())
//    {
//        if (QMessageBox::question(win, tr("Stop Server?"),
//                                 tr("Are you sure you want to stop this server?"),
//                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
//        {
//            win->sendCommandToServer("quit");
//        }
//    }
}

void GuiShellApp::updateLocalServerMenu()
{
//    d->localMenu->setDisabled(d->finder.foundServers().isEmpty());
//    d->localMenu->clear();

//    foreach (Address const &host, d->finder.foundServers())
//    {
//        QString label = QString("%1 - %2 (%3/%4)")
//                .arg(host.asText().c_str())
//                .arg(d->finder.name(host).c_str())
//                .arg(d->finder.playerCount(host))
//                .arg(d->finder.maxPlayers(host));

//        QAction *act = d->localMenu->addAction(label, this, SLOT(connectToLocalServer()));
//        act->setData(QVariant::fromValue(host));
//    }
}

void GuiShellApp::aboutShell()
{
//    AboutDialog().exec();
}

void GuiShellApp::showHelp()
{
//    QDesktopServices::openUrl(QUrl(tr("http://wiki.dengine.net/w/Shell_Help")));
}

void GuiShellApp::openWebAddress(const String &url)
{
//    QDesktopServices::openUrl(QUrl(url));
}

void GuiShellApp::showPreferences()
{
//    if (!d->prefs)
//    {
//        d->prefs = new Preferences;
//        connect(d->prefs, SIGNAL(finished(int)), this, SLOT(preferencesDone()));
//        foreach (LinkWindow *win, d->windows)
//        {
//            connect(d->prefs, SIGNAL(consoleFontChanged()), win, SLOT(updateConsoleFontFromPreferences()));
//        }
//        d->prefs->show();
//    }
//    else
//    {
//        d->prefs->activateWindow();
//    }
}

void GuiShellApp::preferencesDone()
{
//    d->prefs->deleteLater();
//    d->prefs = 0;
}

void GuiShellApp::updateMenu()
{
#ifdef MACOSX
//    LinkWindow *win = dynamic_cast<LinkWindow *>(activeWindow());
//    d->stopAction->setEnabled(win && win->isConnected());
//    d->disconnectAction->setEnabled(win && win->isConnected());
#endif
    updateLocalServerMenu();
}

void GuiShellApp::windowClosed(LinkWindow *window)
{
//    d->windows.removeAll(window);
//    window->deleteLater();
}

void GuiShellApp::checkLocalServers()
{
//    QMutableHashIterator<int, LocalServer *> iter(d->localServers);
//    while (iter.hasNext())
//    {
//        iter.next();
//        if (!iter.value()->isRunning())
//        {
//            emit localServerStopped(iter.key());

//            delete iter.value();
//            iter.remove();
//        }
//    }
}
