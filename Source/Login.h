// =============================================================================
//  DELEX by Piradex — ecrã de senha (tapa o plugin até a senha ser aceite)
//  A verificação (PBKDF2) corre numa thread de fundo para não bloquear o DAW.
// =============================================================================
#pragma once

#include "Widgets.h"
#include "License.h"
#include "i18n/PiradexKitUI.h"

namespace delex::ui
{
class LoginOverlay : public juce::Component
{
public:
    std::function<void()> onUnlocked;

    // 1.0.2: ecra de entrada unico Piradex (kit partilhado); a verificacao continua aqui
    LoginOverlay()
    {
        screen.onSubmit = [this] (const juce::String& pw, bool keep) { submit (pw, keep); };
        addAndMakeVisible (screen);
    }
    void resized() override { screen.setBounds (getLocalBounds()); }
    void visibilityChanged() override { if (isVisible() && isShowing()) screen.focusField(); }

private:
    piradex::kit::LoginScreen screen { "DELEX", col::coral, JucePlugin_VersionString };
    bool busy = false;

    void submit (const juce::String& pw, bool keep)
    {
        if (busy) return;
        busy = true; screen.setBusy (true);
        juce::Component::SafePointer<LoginOverlay> safe (this);
        juce::Thread::launch ([safe, pw, keep]
        {
            bool expired = false;
            const int idx = lic::License::matchPassword (pw, expired);   // lento de proposito
            piradex::pcc::Check online;                                   // chave de licenca: pergunta ao PCC
            if (idx < 0 && ! expired) online = lic::License::checkOnline (pw);
            juce::MessageManager::callAsync ([safe, idx, expired, keep, online]
            {
                const auto r = online.outcome != piradex::pcc::Outcome::Disabled ? lic::License::get().applyOnline (online, keep)
                                                                                 : lic::License::get().applyMatch (idx, expired, keep);
                if (safe != nullptr) safe->finished (r);
            });
        });
    }

    void finished (lic::License::Result r)
    {
        busy = false; screen.setBusy (false);
        if (r == lic::License::Result::Ok)
        {
            screen.clearField(); screen.setStatus ({}, false);
            if (onUnlocked) onUnlocked();
            return;
        }
        screen.setStatus (r == lic::License::Result::Online  ? lic::License::get().onlineText()
                        : r == lic::License::Result::Expired ? tr ("Esta senha beta j\xc3\xa1 expirou.", "This beta password has expired.")
                                                             : tr ("Senha incorreta.", "Incorrect password."), true);
    }
};
} // namespace delex::ui
