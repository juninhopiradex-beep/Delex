// =============================================================================
//  DELEX by Piradex — ecrã de senha (tapa o plugin até a senha ser aceite)
//  A verificação (PBKDF2) corre numa thread de fundo para não bloquear o DAW.
// =============================================================================
#pragma once

#include "Widgets.h"
#include "License.h"

namespace delex::ui
{
class LoginOverlay : public juce::Component
{
public:
    std::function<void()> onUnlocked;

    LoginOverlay()
    {
        password.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        password.setFont (mono (18.0f));
        password.setJustification (juce::Justification::centred);
        password.setTextToShowWhenEmpty ("Senha", col::dim);
        password.setColour (juce::TextEditor::backgroundColourId, col::deep);
        password.setColour (juce::TextEditor::outlineColourId, col::line2);
        password.setColour (juce::TextEditor::focusedOutlineColourId, col::coral);
        password.onReturnKey = [this] { submit(); };
        addAndMakeVisible (password);

        remember.setButtonText ("Lembrar neste computador");
        remember.setToggleState (true, juce::dontSendNotification);
        remember.setColour (juce::ToggleButton::textColourId, col::text2);
        remember.setColour (juce::ToggleButton::tickColourId, col::coral);
        remember.setColour (juce::ToggleButton::tickDisabledColourId, col::line2);
        addAndMakeVisible (remember);

        enter.getProperties().set ("solid", true);
        enter.getProperties().set ("fontH", 14.0f);
        enter.setColour (juce::TextButton::buttonOnColourId, col::coral);
        enter.setToggleState (true, juce::dontSendNotification);
        enter.onClick = [this] { submit(); };
        addAndMakeVisible (enter);

        message.setJustificationType (juce::Justification::centred);
        message.setFont (font (14.0f));
        addAndMakeVisible (message);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg.withAlpha (0.97f));
        const auto c = card().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (c, 12.0f);
        g.setColour (col::line2); g.drawRoundedRectangle (c, 12.0f, 1.2f);

        auto t = c.reduced (24.0f, 26.0f);
        auto head = t.removeFromTop (44.0f);
        const float w = 30.0f + juce::GlyphArrangement::getStringWidth (font (38.0f, W::Bold, 0.22f), "DELEX");
        auto logoArea = head.withSizeKeepingCentre (w, 44.0f);
        icons::logo (g, logoArea.removeFromLeft (22.0f).withSizeKeepingCentre (18.0f, 16.0f));
        label (g, "DELEX", logoArea, 38.0f, W::Bold, col::text, juce::Justification::centredRight, 0.22f);
        g.setColour (col::muted); g.setFont (font (15.0f, W::Medium, 0.05f));
        g.drawText ("by Piradex", t.removeFromTop (22.0f), juce::Justification::centred);
        t.removeFromTop (6.0f);
        g.setColour (col::muted); g.setFont (font (14.0f));
        g.drawText (juce::String::fromUTF8 ("Introduza a sua senha para começar"), t.removeFromTop (24.0f), juce::Justification::centred);
    }

    void resized() override
    {
        auto c = card().reduced (48, 0);
        c.removeFromTop (138);
        password.setBounds (c.removeFromTop (42));
        c.removeFromTop (12);
        remember.setBounds (c.removeFromTop (26).withSizeKeepingCentre (220, 26));
        c.removeFromTop (12);
        enter.setBounds (c.removeFromTop (40).withSizeKeepingCentre (170, 40));
        c.removeFromTop (8);
        message.setBounds (c.removeFromTop (36));
    }

    void visibilityChanged() override
    {
        if (isVisible() && isShowing()) password.grabKeyboardFocus();
    }

private:
    juce::Rectangle<int> card() const { return getLocalBounds().withSizeKeepingCentre (440, 340); }

    void submit()
    {
        if (password.isEmpty() || busy) return;
        busy = true;
        enter.setEnabled (false);
        message.setColour (juce::Label::textColourId, col::muted);
        message.setText (juce::String::fromUTF8 ("A verificar…"), juce::dontSendNotification);

        const auto pw = password.getText();
        const bool keep = remember.getToggleState();
        juce::Component::SafePointer<LoginOverlay> safe (this);
        juce::Thread::launch ([safe, pw, keep]
        {
            bool expired = false;
            const int idx = lic::License::matchPassword (pw, expired);   // lento de propósito
            juce::MessageManager::callAsync ([safe, idx, expired, keep]
            {
                const auto r = lic::License::get().applyMatch (idx, expired, keep);
                if (safe != nullptr) safe->finished (r);
            });
        });
    }

    void finished (lic::License::Result r)
    {
        busy = false;
        enter.setEnabled (true);
        if (r == lic::License::Result::Ok)
        {
            password.clear();
            message.setText ({}, juce::dontSendNotification);
            if (onUnlocked) onUnlocked();
            return;
        }
        password.selectAll();
        message.setColour (juce::Label::textColourId, col::coral);
        message.setText (r == lic::License::Result::Expired ? juce::String::fromUTF8 ("Esta senha beta já expirou.")
                                                            : juce::String ("Senha incorreta."),
                         juce::dontSendNotification);
    }

    juce::TextEditor password;
    juce::ToggleButton remember;
    juce::TextButton enter { "ENTRAR" };
    juce::Label message;
    bool busy = false;
};
} // namespace delex::ui
