// =============================================================================
//  DELEX by Piradex — browser de presets (pesquisa, categorias, favoritos, utilizador)
// =============================================================================
#pragma once

#include "Widgets.h"

namespace delex::ui
{
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void()> onClose;
    std::function<void()> onLoaded;

    explicit PresetBrowser (DelexProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Nome, fonte ou resultado…"), col::dim);
        search.setFont (font (14.0f));
        search.setIndents (10, 7);
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);

        const juce::StringArray cats = [] { juce::StringArray c { juce::String::fromUTF8 ("★ Favoritos"), "Todos" }; c.addArray (presetCategories()); c.add ("Utilizador"); return c; }();
        for (int i = 0; i < cats.size(); ++i)
        {
            auto* b = catButtons.add (new juce::TextButton (cats[i]));
            b->setClickingTogglesState (false);
            b->getProperties().set ("flat", true);
            b->getProperties().set ("noEdge", true);
            b->getProperties().set ("fontH", 13.0f);
            b->getProperties().set ("left", true);
            b->setColour (juce::TextButton::buttonOnColourId, i == 0 ? col::gold : col::cyan);
            b->setColour (juce::TextButton::textColourOffId, i == 0 ? col::gold : col::text2);
            b->onClick = [this, i] { category = i; rebuild(); };
            addAndMakeVisible (b);
        }

        list.setModel (this);
        list.setRowHeight (60);
        list.setColour (juce::ListBox::backgroundColourId, col::bg);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);

        load.setButtonText ("CARREGAR");
        load.getProperties().set ("solid", true);
        load.getProperties().set ("fontH", 14.0f);
        load.setToggleState (true, juce::dontSendNotification);
        load.onClick = [this] { loadSelected(); };
        addAndMakeVisible (load);

        saveAs.setButtonText (juce::String::fromUTF8 ("GUARDAR COMO…"));
        saveAs.onClick = [this] { askSave(); };
        addAndMakeVisible (saveAs);

        openFolder.setButtonText ("PASTA");
        openFolder.setTooltip ("Abrir a pasta dos presets do utilizador");
        openFolder.onClick = [this] { auto d = proc.userPresetFolder(); d.createDirectory(); d.startAsProcess(); };
        addAndMakeVisible (openFolder);

        close.outline = true;
        close.setTooltip ("Fechar");
        close.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (close);

        // começa na categoria do preset actual
        const auto& fp = factoryPresets();
        const int cur = proc.getCurrentProgram();
        if (juce::isPositiveAndBelow (cur, (int) fp.size()))
            category = 2 + presetCategories().indexOf (juce::String::fromUTF8 (fp[(size_t) cur].category));
        rebuild();
    }

    void refreshList() { rebuild(); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.fillAll (col::deep);
        auto r = getLocalBounds().toFloat();
        auto head = r.removeFromTop (52.0f);
        g.setColour (col::panel); g.fillRect (head);
        g.setColour (col::line); g.drawHorizontalLine (52, 0, r.getWidth());
        icons::logo (g, { 18.0f, 18.0f, 18.0f, 16.0f });
        label (g, "DELEX", { 44.0f, 13.0f, 90.0f, 26.0f }, 23.0f, W::Bold, col::text, Justification::centredLeft, 0.22f);
        g.setColour (col::muted); g.setFont (font (13.0f));
        g.drawText (String::fromUTF8 ("Presets · ") + String ((int) factoryPresets().size()) + String::fromUTF8 (" de fábrica · ") + String (userFiles.size()) + " do utilizador",
                    Rectangle<float> (128.0f, 18.0f, 400.0f, 18.0f), Justification::centredLeft);

        g.setColour (col::bg); g.fillRect (Rectangle<float> (0, 53, 220, r.getHeight()));
        g.setColour (col::line); g.drawVerticalLine (220, 53, (float) getHeight());
        label (g, "PROCURAR", { 12.0f, 64.0f, 190.0f, 14.0f }, 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);

        g.setColour (col::bg); g.fillRect (Rectangle<float> (221, 53, (float) getWidth() - 471, r.getHeight()));
        label (g, heading.toUpperCase(), { 237.0f, 64.0f, 400.0f, 14.0f }, 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);

        // detalhe
        auto d = Rectangle<float> ((float) getWidth() - 250.0f, 53.0f, 250.0f, (float) getHeight() - 53.0f);
        g.setColour (col::panel); g.fillRect (d);
        g.setColour (col::line); g.drawVerticalLine ((int) d.getX(), d.getY(), d.getBottom());
        auto dd = d.reduced (16.0f);
        if (auto* e = selectedEntry())
        {
            label (g, e->name, dd.removeFromTop (28.0f), 20.0f, W::Bold, col::text, Justification::centredLeft, 0.0f);
            dd.removeFromTop (6.0f);
            g.setColour (col::text2); g.setFont (font (14.0f));
            auto descR = dd.removeFromTop (60.0f);
            g.drawFittedText (e->desc, descR.toNearestInt(), Justification::topLeft, 4, 1.0f);
            if (e->use.isNotEmpty())
            {
                dd.removeFromTop (6.0f);
                label (g, "USO SUGERIDO", dd.removeFromTop (16.0f), 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);
                g.setColour (col::text2); g.setFont (font (14.0f));
                g.drawFittedText (e->use, dd.removeFromTop (40.0f).toNearestInt(), Justification::topLeft, 3, 1.0f);
            }
            dd.removeFromTop (8.0f);
            float x = dd.getX(), y = dd.getY();
            g.setFont (font (12.0f));
            for (auto& t : StringArray::fromTokens (e->tags, ",", ""))
            {
                const auto tt = t.trim(); if (tt.isEmpty()) continue;
                const float w = GlyphArrangement::getStringWidth (font (12.0f), tt) + 16.0f;
                if (x + w > dd.getRight()) { x = dd.getX(); y += 24.0f; }
                g.setColour (col::line2); g.drawRoundedRectangle (Rectangle<float> (x, y, w, 20.0f), 10.0f, 1.0f);
                g.setColour (col::soft); g.drawText (tt, Rectangle<float> (x, y, w, 20.0f), Justification::centred);
                x += w + 6.0f;
            }
            if (e->summary.isNotEmpty())
            {
                g.setColour (col::muted); g.setFont (mono (11.0f));
                g.drawFittedText (e->summary, Rectangle<float> (dd.getX(), y + 34.0f, dd.getWidth(), 60.0f).toNearestInt(), Justification::topLeft, 4);
            }
        }
        else
        {
            g.setColour (col::muted); g.setFont (font (14.0f));
            g.drawFittedText (String::fromUTF8 ("Escolhe um preset na lista. Duplo clique carrega logo."), dd.removeFromTop (60.0f).toNearestInt(), Justification::topLeft, 3);
        }
    }

    void resized() override
    {
        close.setBounds (getWidth() - 48, 11, 30, 30);
        saveAs.setBounds (close.getX() - 136, 11, 128, 30);
        openFolder.setBounds (saveAs.getX() - 70, 11, 62, 30);
        search.setBounds (12, 82, 196, 32);
        int y = 124;
        for (auto* b : catButtons) { b->setBounds (8, y, 204, 29); y += 31; }
        list.setBounds (229, 84, getWidth() - 229 - 250 - 12, getHeight() - 84 - 10);
        load.setBounds (getWidth() - 234, getHeight() - 54, 218, 38);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey && onClose) { onClose(); return true; }
        if (k == juce::KeyPress::returnKey) { loadSelected(); return true; }
        return false;
    }

private:
    struct Entry
    {
        juce::String name, category, desc, use, tags, timeLabel, summary;
        int factory = -1;
        juce::File file;
    };

    static juce::String timeOf (const char* data)
    {
        const auto kv = parsePresetData (data);
        if (kv["sync"] == "0") { const float t = kv["time"].getFloatValue(); return juce::String (t, 0) + " ms"; }
        return kv.containsKey ("div") ? kv["div"] : juce::String ("1/4");
    }
    static juce::String summaryOf (const char* data)
    {
        const auto kv = parsePresetData (data);
        auto get = [&] (const char* k, const char* def) { return kv.containsKey (k) ? kv[k] : juce::String (def); };
        return "TIME " + timeOf (data) + "   FB " + get ("feedback", "40") + " %\nSPACE " + get ("space", "30") + " %   TONE " + get ("tone", "0")
             + "\nCHAR " + get ("character", "0") + "   MIX " + get ("mix", "25") + " %";
    }

    void rebuild()
    {
        userFiles = proc.userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.dlxpreset");
        userFiles.sort();
        entries.clear();
        const auto q = search.getText().trim().toLowerCase();
        const auto& fp = factoryPresets();
        const int ncat = presetCategories().size();

        auto matches = [&] (const Entry& e)
        {
            if (q.isEmpty()) return true;
            return (e.name + " " + e.category + " " + e.desc + " " + e.use + " " + e.tags).toLowerCase().contains (q);
        };

        for (int i = 0; i < (int) fp.size(); ++i)
        {
            Entry e { juce::String::fromUTF8 (fp[(size_t) i].name), juce::String::fromUTF8 (fp[(size_t) i].category),
                      juce::String::fromUTF8 (fp[(size_t) i].description), juce::String::fromUTF8 (fp[(size_t) i].use),
                      juce::String::fromUTF8 (fp[(size_t) i].tags), timeOf (fp[(size_t) i].data), summaryOf (fp[(size_t) i].data), i, {} };
            const bool inCat = category == 1 || (category == 0 && proc.isFavourite (e.name))
                            || (category >= 2 && category < 2 + ncat && e.category == presetCategories()[category - 2]);
            if ((q.isNotEmpty() || inCat) && matches (e)) entries.push_back (e);
        }
        for (auto& f : userFiles)
        {
            Entry e { f.getFileNameWithoutExtension(), "Utilizador", juce::String::fromUTF8 ("Preset guardado por ti."), {}, "utilizador", {}, {}, -1, f };
            const bool inCat = category == 1 || category == 2 + ncat || (category == 0 && proc.isFavourite (e.name));
            if ((q.isNotEmpty() || inCat) && matches (e)) entries.push_back (e);
        }

        heading = q.isNotEmpty() ? juce::String::fromUTF8 ("Resultados para “") + search.getText().trim() + juce::String::fromUTF8 ("”")
                                 : catButtons[category]->getButtonText();
        for (int i = 0; i < catButtons.size(); ++i)
        {
            catButtons[i]->setToggleState (q.isEmpty() && i == category, juce::dontSendNotification);
            catButtons[i]->getProperties().set ("flat", ! catButtons[i]->getToggleState());
        }
        list.updateContent();
        selected = -1;
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].name == proc.currentPresetName()) selected = i;
        list.selectRow (selected, true, true);
        repaint();
    }

    Entry* selectedEntry() { return juce::isPositiveAndBelow (selected, (int) entries.size()) ? &entries[(size_t) selected] : nullptr; }

    void loadSelected()
    {
        if (auto* e = selectedEntry())
        {
            if (e->factory >= 0) proc.loadFactoryPreset (e->factory);
            else proc.loadUserPreset (e->file);
            if (onLoaded) onLoaded();
            repaint();
        }
    }

    void askSave()
    {
        auto* w = new juce::AlertWindow ("Guardar preset", "Nome do preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.currentPresetName());
        w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
        {
            if (r == 1 && safe != nullptr)
            {
                const auto name = w->getTextEditorContents ("name");
                if (safe->proc.saveUserPreset (name)) { safe->category = 2 + presetCategories().size(); safe->search.clear(); safe->rebuild(); }
                if (safe->onLoaded) safe->onLoaded();
            }
        }), true);
    }

    // ListBoxModel
    int getNumRows() override { return (int) entries.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        using namespace juce;
        if (! isPositiveAndBelow (row, (int) entries.size())) return;
        const auto& e = entries[(size_t) row];
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (2.0f, 3.0f);
        g.setColour (sel ? col::coralDeep : col::panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (sel ? col::coral : col::line); g.drawRoundedRectangle (r, 8.0f, 1.0f);
        const bool fav = proc.isFavourite (e.name);
        icons::star (g, Rectangle<float> (r.getX() + 6, r.getY(), 24, r.getHeight()), fav ? col::gold : Colour (0xff4a4e57), fav);
        auto t = r.withTrimmedLeft (36.0f).withTrimmedRight (10.0f);
        g.setColour (sel ? col::coral : col::muted); g.setFont (mono (11.0f));
        g.drawText (e.timeLabel, t, Justification::centredRight);
        t.removeFromRight (56.0f);
        label (g, e.name, t.removeFromTop (r.getHeight() * 0.55f).withTrimmedTop (8.0f), 16.0f, W::Semi, col::text, Justification::bottomLeft, 0.0f);
        g.setColour (sel ? col::soft : col::muted); g.setFont (font (13.0f));
        g.drawText ((category == 1 || search.getText().isNotEmpty() ? e.category + String::fromUTF8 (" · ") : String()) + e.desc,
                    t.withTrimmedBottom (6.0f), Justification::topLeft, true);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, (int) entries.size())) return;
        if (e.x < 40) { proc.toggleFavourite (entries[(size_t) row].name); if (category == 0) { rebuild(); return; } list.repaintRow (row); }
        selected = row; repaint();
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { selected = row; loadSelected(); }
    void selectedRowsChanged (int row) override { if (row >= 0) { selected = row; repaint(); } }
    void returnKeyPressed (int) override { loadSelected(); }

    DelexProcessor& proc;
    juce::TextEditor search;
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::ListBox list;
    juce::TextButton load, saveAs, openFolder;
    IconButton close { "fechar", [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::cross (g, r, c); } };
    std::vector<Entry> entries;
    juce::Array<juce::File> userFiles;
    juce::String heading;
    int category = 1, selected = -1;
};

} // namespace delex::ui
