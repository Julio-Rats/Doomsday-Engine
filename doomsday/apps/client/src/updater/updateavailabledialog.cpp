/** @file updateavailabledialog.cpp Dialog for notifying the user about available updates.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/updatersettingsdialog.h"
#include "clientapp.h"
#include <de/GuiApp>
#include <de/Log>
#include <de/ProgressWidget>
#include <de/ToggleWidget>
//#include <de/SignalAction>
//#include <QUrl>
//#include <QDesktopServices>

using namespace de;

static TimeSpan const SHOW_ANIM_SPAN = 0.3;

DE_GUI_PIMPL(UpdateAvailableDialog)
, DE_OBSERVES(ToggleWidget, Toggle)
{
    ProgressWidget *checking;
    ToggleWidget *autoCheck;
    //ButtonWidget *settings;
    Version latestVersion;
    String changeLog;

    Impl(Public *d) : Base(*d)
    {
        initForChecking();
    }

    Impl(Public *d, Version const &latest) : Base(*d)
    {
        initForResult(latest);
    }

    void initForChecking(void)
    {
        init();
        showProgress(true, 0.0);
    }

    void initForResult(Version const &latest)
    {
        init();
        updateResult(latest, 0.0);
    }

    void showProgress(bool show, TimeSpan span)
    {
        checking->setOpacity(show? 1 : 0, span);
        self().area().setOpacity(show? 0 : 1, span);

        if (show)
        {
            // Set up a cancel button.
            self().buttons().clear()
                    << new DialogButtonItem(DialogWidget::Reject);
        }
    }

    void init()
    {
        checking = new ProgressWidget;
        checking->setText("Checking for Updates...");

        // The checking indicator is overlaid on the normal content.
        checking->rule().setRect(self().rule());
        self().add(checking);

        autoCheck = new ToggleWidget;
        self().area().add(autoCheck);
        autoCheck->setAlignment(ui::AlignLeft);
        autoCheck->setText("Check for updates automatically");
        autoCheck->audienceForToggle() += this;

        // Include the toggle in the layout.
        self().updateLayout();
    }

    static bool isMatchingChannel(String const &channel, String const &buildType)
    {
        if (channel == buildType) return true;
        if (channel == "RC/stable" && buildType != "unstable") return true;
        return false;
    }

    void updateResult(Version const &latest, TimeSpan showSpan)
    {
        showProgress(false, showSpan);

        latestVersion = latest;

        Version const currentVersion = Version::currentBuild();
        String const channel = (UpdaterSettings().channel() == UpdaterSettings::Stable? "stable" :
                                UpdaterSettings().channel() == UpdaterSettings::Unstable? "unstable" : "RC/stable");
        String const builtInType = CString(DOOMSDAY_RELEASE_TYPE).lower();
        bool askUpgrade    = false;
        bool askDowngrade  = false;

        if (latestVersion > currentVersion)
        {
            askUpgrade = true;

            self().title().setText("Update Available");
            self().title().setImage(style().images().image("updater"));
            self().message().setText(
                String::format("There is an update available. The latest %s release is %s, while "
                               "you are running %s.",
                               channel.c_str(),
                               (_E(b) + latestVersion.asHumanReadableText() + _E(.)).c_str(),
                               currentVersion.asHumanReadableText().c_str()));
        }
        else if (isMatchingChannel(channel, builtInType)) // same release type
        {
            self().title().setText("Up to Date");
            self().message().setText(
                String::format("The installed %1 is the latest available %2 build.",
                               currentVersion.asHumanReadableText().c_str(),
                               (_E(b) + channel + _E(.)).c_str()));
        }
        else if (latestVersion < currentVersion)
        {
            askDowngrade = true;

            self().title().setText(tr("Up to Date"));
            self().message().setText(tr("The installed %1 is newer than the latest available %2 build.")
                                   .arg(currentVersion.asHumanReadableText())
                                   .arg(_E(b) + channel + _E(.)));
        }

        autoCheck->setInactive(UpdaterSettings().onlyCheckManually());

        self().buttons().clear();

        if (askDowngrade)
        {
            self().buttons()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Downgrade to Older"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
        }
        else if (askUpgrade)
        {
            self().buttons()
                    << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Upgrade"))
                    << new DialogButtonItem(DialogWidget::Reject, tr("Not Now"));
        }
        else
        {
            self().buttons()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Reinstall"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
        }

        self().buttons()
                << new DialogButtonItem(DialogWidget::Action | DialogWidget::Id1,
                                        style().images().image("gear"),
                                        new SignalAction(thisPublic, SLOT(editSettings())));

        if (askUpgrade)
        {
            self().buttons()
                    << new DialogButtonItem(DialogWidget::Action, tr("What's New?"),
                                            new SignalAction(thisPublic, SLOT(showWhatsNew())));
        }
    }

    void toggleStateChanged(ToggleWidget &)
    {
        bool set = autoCheck->isInactive();
        UpdaterSettings().setOnlyCheckManually(set);

        LOG_DEBUG("Never check for updates: %b") << set;
    }
};

UpdateAvailableDialog::UpdateAvailableDialog()
    : MessageDialog("updateavailable"), d(new Impl(this))
{}

UpdateAvailableDialog::UpdateAvailableDialog(Version const &latestVersion, String changeLogUri)
    : MessageDialog("updateavailable"), d(new Impl(this, latestVersion))
{
    d->changeLog = changeLogUri;
}

void UpdateAvailableDialog::showResult(Version const &latestVersion, String changeLogUri)
{
    d->changeLog = changeLogUri;
    d->updateResult(latestVersion, SHOW_ANIM_SPAN);
}

void UpdateAvailableDialog::showWhatsNew()
{
    ClientApp::app().openInBrowser(d->changeLog);
}

void UpdateAvailableDialog::editSettings()
{
    UpdaterSettingsDialog *st = new UpdaterSettingsDialog;
    st->setAnchorAndOpeningDirection(buttonWidget(DialogWidget::Id1)->rule(), ui::Up);
    st->setDeleteAfterDismissed(true);
    if (st->exec(root()))
    {
        // The Gear button will soon be deleted, so we'll need to detach from it.
        st->detachAnchor();

        if (st->settingsHaveChanged())
        {
            d->autoCheck->setInactive(UpdaterSettings().onlyCheckManually());
            d->showProgress(true, SHOW_ANIM_SPAN);
            emit checkAgain();
        }
    }
}
