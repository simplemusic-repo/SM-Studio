// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginScanDialog.h"

#include "Theme.h"

namespace sms
{

namespace
{
    class ScanProgress final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit ScanProgress (PluginHost& h) : host (h)
        {
            title.setText ("Plug-ins werden gesucht", juce::dontSendNotification);
            title.setFont (theme::font (16.0f, true));
            addAndMakeVisible (title);

            info.setText (u8"Installierte VST3-Plug-ins (am Mac auch Audio Units) werden geprüft. "
                          u8"Das kann einige Minuten dauern.", juce::dontSendNotification);
            info.setFont (theme::font (12.5f));
            info.setColour (juce::Label::textColourId, theme::colours::textDim);
            info.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (info);

            bar.setColour (juce::ProgressBar::foregroundColourId, theme::colours::accent);
            bar.setColour (juce::ProgressBar::backgroundColourId, theme::colours::panelDark);
            addAndMakeVisible (bar);

            current.setFont (theme::font (12.0f));
            current.setColour (juce::Label::textColourId, theme::colours::textDim);
            current.setMinimumHorizontalScale (0.6f);
            addAndMakeVisible (current);

            cancel.setTooltip (u8"Suche beenden – bisher gefundene Plug-ins bleiben erhalten");
            cancel.onClick = [this]
            {
                host.cancelScan();
                cancel.setEnabled (false);
                cancel.setButtonText (u8"Wird beendet …");
            };
            addAndMakeVisible (cancel);

            setSize (520, 190);
            startTimerHz (15);
        }

        ~ScanProgress() override
        {
            // window closed with the title bar button: that also cancels the search
            if (host.isScanning())
                host.cancelScan();
        }

        void paint (juce::Graphics& g) override { g.fillAll (theme::colours::panel); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 16);
            title.setBounds (r.removeFromTop (24));
            r.removeFromTop (4);
            info.setBounds (r.removeFromTop (34));
            r.removeFromTop (8);
            bar.setBounds (r.removeFromTop (22));
            r.removeFromTop (6);
            current.setBounds (r.removeFromTop (20));
            cancel.setBounds (r.removeFromBottom (28).removeFromRight (150));
        }

    private:
        void timerCallback() override
        {
            progress = (double) host.getScanProgress();
            const auto name = host.getScanningName();
            current.setText (name.isNotEmpty() ? juce::String (u8"Prüfe: ") + name : juce::String (u8"…"), juce::dontSendNotification);

            if (! host.isScanning())
            {
                stopTimer();
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState (0);
            }
        }

        PluginHost& host;
        double progress = 0.0;
        juce::Label title, info, current;
        juce::ProgressBar bar { progress };
        juce::TextButton cancel { "Abbrechen" };
    };
}

void startPluginScanWithProgress (AppContext& ctx, bool full)
{
    if (! ctx.plugins.isScanning())
        ctx.plugins.startScan (full);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ScanProgress (ctx.plugins));
    o.dialogTitle = "SM Studio";
    o.dialogBackgroundColour = theme::colours::panel;
    o.escapeKeyTriggersCloseButton = false;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();

    // modal: blocks mouse and keyboard input to the rest of the application
    o.launchAsync();
}

} // namespace sms
