#pragma once

#include "PresetBrowser.h"
#include "Login.h"

namespace delex::ui { class DelexUI; }

class DelexEditor : public juce::AudioProcessorEditor
{
public:
    explicit DelexEditor (DelexProcessor&);
    ~DelexEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setScale (float s);

    static constexpr int kW = 1000, kH = 640;

private:
    DelexProcessor& proc;
    delex::ui::LnF lnf;
    std::unique_ptr<delex::ui::DelexUI> ui;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelexEditor)
};
