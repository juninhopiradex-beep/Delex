// =============================================================================
//  DELEX by Piradex — browser de presets (pesquisa, categorias, favoritos, utilizador)
// =============================================================================
#pragma once

#include "Widgets.h"
#include "Search.h"
#include "Packs.h"
#include <map>

namespace delex::ui
{
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void()> onClose;
    std::function<void()> onLoaded;

    explicit PresetBrowser (DelexProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty (tr ("Descreve: \"voz escura e larga\", \"caixa curta\"…", "Describe: \"dark wide vocal\", \"short snare\"…"), col::dim);
        search.setFont (font (14.0f));
        search.setIndents (10, 7);
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);

        const juce::StringArray cats = [] { juce::StringArray c { tr ("★ Favoritos", "★ Favourites"), tr ("Todos", "All") }; c.addArray (presetCategories()); c.add (tr ("Utilizador", "User")); return c; }();
        for (int i = 0; i < cats.size(); ++i)
        {
            auto* b = catButtons.add (new juce::TextButton (cats[i]));
            b->setClickingTogglesState (false);
            b->getProperties().set ("flat", true);
            b->getProperties().set ("noEdge", true);
            b->getProperties().set ("fontH", 12.5f);
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

        load.setButtonText (tr ("CARREGAR", "LOAD"));
        load.getProperties().set ("solid", true);
        load.getProperties().set ("fontH", 14.0f);
        load.setToggleState (true, juce::dontSendNotification);
        load.onClick = [this] { loadSelected(); };
        addAndMakeVisible (load);

        saveAs.setButtonText (tr ("GUARDAR COMO…", "SAVE AS…"));
        saveAs.onClick = [this] { askSave(); };
        addAndMakeVisible (saveAs);

        openFolder.setButtonText (tr ("PASTA", "FOLDER"));
        openFolder.setTooltip (tr ("Abrir a pasta dos presets do utilizador", "Open the user presets folder"));
        openFolder.onClick = [this] { auto d = proc.userPresetFolder(); d.createDirectory(); d.startAsProcess(); };
        addAndMakeVisible (openFolder);

        // bloquear grupos ao trocar de preset (ex.: manter o tempo e o mix do projecto)
        const juce::StringArray lockNames { tr ("TEMPO", "TIME"), "MIX", tr ("DINÂM.", "DYN."), "EQ" };
        const juce::StringArray lockTips {
            tr ("Mantém tempo, sync, divisão e ratio ao trocar de preset", "Keeps time, sync, division and ratio when changing preset"),
            tr ("Mantém mix, wet trim e output ao trocar de preset", "Keeps mix, wet trim and output when changing preset"),
            tr ("Mantém ducker e gate ao trocar de preset", "Keeps ducker and gate when changing preset"),
            tr ("Mantém o EQ do loop ao trocar de preset", "Keeps the loop EQ when changing preset") };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = lockButtons.add (new juce::TextButton (lockNames[i]));
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, col::gold);
            b->getProperties().set ("fontH", 11.0f);
            b->setTooltip (lockTips[i]);
            b->setToggleState ((proc.getLockMask() & (1 << i)) != 0, juce::dontSendNotification);
            b->onClick = [this]
            {
                int m = 0;
                for (int k = 0; k < lockButtons.size(); ++k) if (lockButtons[k]->getToggleState()) m |= 1 << k;
                proc.setLockMask (m);
            };
            addAndMakeVisible (b);
        }

        importPack.setButtonText (tr ("IMPORTAR PACK…", "IMPORT PACK…"));
        importPack.setTooltip (tr ("Importa um pack Signature Series (.dlxpack)", "Imports a Signature Series pack (.dlxpack)"));
        importPack.onClick = [this] { askImport(); };
        addAndMakeVisible (importPack);
        exportPack.setButtonText (tr ("EXPORTAR PACK…", "EXPORT PACK…"));
        exportPack.setTooltip (tr ("Junta os teus presets num pack (.dlxpack) para partilhar ou vender", "Bundles your presets into a pack (.dlxpack) to share or sell"));
        exportPack.onClick = [this] { askExport(); };
        addAndMakeVisible (exportPack);

        close.outline = true;
        close.setTooltip (tr ("Fechar", "Close"));
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
    void setSearchText (const juce::String& t) { search.setText (t, juce::dontSendNotification); rebuild(); }
    void showCategory (const juce::String& cat)
    {
        const int i = presetCategories().indexOf (cat);
        if (i >= 0) { search.clear(); category = 2 + i; rebuild(); }
    }
    int numResults() const { return (int) entries.size(); }
    juce::String resultName (int i) const { return juce::isPositiveAndBelow (i, (int) entries.size()) ? entries[(size_t) i].name : juce::String(); }

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
        g.drawText (tr ("Presets · ", "Presets · ") + String ((int) factoryPresets().size()) + tr (" de fábrica · ", " factory · ") + String (userFiles.size()) + tr (" do utilizador", " user"),
                    Rectangle<float> (128.0f, 18.0f, 400.0f, 18.0f), Justification::centredLeft);

        g.setColour (col::bg); g.fillRect (Rectangle<float> (0, 53, 220, r.getHeight()));
        g.setColour (col::line); g.drawVerticalLine (220, 53, (float) getHeight());
        label (g, tr ("BLOQUEAR", "LOCK"), { (float) lockLabelX, 19.0f, 66.0f, 14.0f }, 11.0f, W::Semi, col::gold, Justification::centredRight, 0.18f);
        label (g, tr ("PROCURAR", "SEARCH"), { 12.0f, 64.0f, 190.0f, 14.0f }, 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);

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
            label (g, (e->category + (e->sub.isNotEmpty() ? String::fromUTF8 (" · ") + e->sub : String())).toUpperCase(),
                   dd.removeFromTop (16.0f), 11.0f, W::Semi, col::cyan, Justification::centredLeft, 0.14f);
            dd.removeFromTop (6.0f);
            g.setColour (col::text2); g.setFont (font (14.0f));
            auto descR = dd.removeFromTop (60.0f);
            g.drawFittedText (e->desc, descR.toNearestInt(), Justification::topLeft, 4, 1.0f);
            if (e->use.isNotEmpty())
            {
                dd.removeFromTop (6.0f);
                label (g, tr ("USO SUGERIDO", "SUGGESTED USE"), dd.removeFromTop (16.0f), 11.0f, W::Semi, col::muted, Justification::centredLeft, 0.18f);
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
            g.drawFittedText (tr ("Escolhe um preset na lista. Duplo clique carrega logo.", "Choose a preset from the list. Double-click loads it straight away."), dd.removeFromTop (60.0f).toNearestInt(), Justification::topLeft, 3);
        }
    }

    void resized() override
    {
        close.setBounds (getWidth() - 48, 11, 30, 30);
        saveAs.setBounds (close.getX() - 136, 11, 128, 30);
        openFolder.setBounds (saveAs.getX() - 70, 11, 62, 30);
        search.setBounds (12, 82, 196, 32);
        {
            int x = openFolder.getX() - 8;
            for (int i = lockButtons.size(); --i >= 0;)
            {
                auto* b = lockButtons[i];
                const int w = (int) juce::GlyphArrangement::getStringWidth (font (11.0f, W::Bold, 0.09f), b->getButtonText()) + 18;
                x -= w; b->setBounds (x, 15, w, 22); x -= 4;
            }
            lockLabelX = x - 70;
        }
        // 22 botões (favoritos, todos, 19 categorias, utilizador) cabem sempre na coluna
        const int step = juce::jlimit (18, 31, (getHeight() - 124 - 6) / juce::jmax (1, catButtons.size()));
        int y = 124;
        for (auto* b : catButtons) { b->setBounds (8, y, 204, step - 2); y += step; }
        list.setBounds (229, 84, getWidth() - 229 - 250 - 12, getHeight() - 84 - 10);
        load.setBounds (getWidth() - 234, getHeight() - 54, 218, 38);
        importPack.setBounds (getWidth() - 234, getHeight() - 96, 106, 30);
        exportPack.setBounds (getWidth() - 122, getHeight() - 96, 106, 30);
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
        juce::String sub;
        float score = 0;
    };

    static juce::String timeOf (const char* data)
    {
        const auto kv = parsePresetData (data);
        if (kv["sync"] == "0") { const float t = kv["time"].getFloatValue(); return num (t, 0) + " ms"; }
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
        userFiles = proc.userPresetFolder().findChildFiles (juce::File::findFiles, true, "*.dlxpreset");
        userFiles.sort();
        entries.clear();
        const auto q = search.getText().trim().toLowerCase();
        const auto& fp = factoryPresets();
        const int ncat = presetCategories().size();
        const auto tokens = search::queryTokens (q);

        // pesquisa por descrição (sinónimos PT/EN, nome > etiquetas > descrição)
        auto matches = [&] (Entry& e)
        {
            if (q.isEmpty()) return true;
            if (e.factory >= 0) e.score = search::score (search::fieldsOf (fp[(size_t) e.factory]), tokens);
            else { search::Fields f; f.name = search::fold (e.name); e.score = search::score (f, tokens); }
            return e.score > 0.0f;
        };

        for (int i = 0; i < (int) fp.size(); ++i)
        {
            Entry e { juce::String::fromUTF8 (fp[(size_t) i].name), juce::String::fromUTF8 (fp[(size_t) i].category),
                      presetDescription (fp[(size_t) i]), presetUse (fp[(size_t) i]),
                      presetTags (fp[(size_t) i]), timeOf (fp[(size_t) i].data), summaryOf (fp[(size_t) i].data), i, {},
                      juce::String::fromUTF8 (fp[(size_t) i].sub) };
            const bool inCat = category == 1 || (category == 0 && proc.isFavourite (e.name))
                            || (category >= 2 && category < 2 + ncat && e.category == presetCategories()[category - 2]);
            if ((q.isNotEmpty() || inCat) && matches (e)) entries.push_back (e);
        }
        std::map<juce::String, packs::PackInfo> packInfo;
        const auto packsDir = proc.userPresetFolder().getChildFile ("Packs");
        for (auto& f : userFiles)
        {
            Entry e { f.getFileNameWithoutExtension(), tr ("Utilizador", "User"), tr ("Preset guardado por ti.", "Preset saved by you."), {}, tr ("utilizador", "user"), {}, {}, -1, f, {} };
            if (f.isAChildOf (packsDir))
            {
                const auto dir = f.getParentDirectory();
                auto it = packInfo.find (dir.getFullPathName());
                if (it == packInfo.end()) it = packInfo.emplace (dir.getFullPathName(), packs::readInstalled (dir)).first;
                e.category = it->second.name;
                e.sub = it->second.author.isNotEmpty() ? tr ("por ", "by ") + it->second.author : juce::String();
                e.desc = tr (it->second.descPt.isNotEmpty() ? it->second.descPt.toRawUTF8() : "Preset de um pack Signature Series.",
                             it->second.descEn.isNotEmpty() ? it->second.descEn.toRawUTF8() : "Preset from a Signature Series pack.");
                e.tags = "pack, signature, " + it->second.author;
            }
            const bool inCat = category == 1 || category == 2 + ncat || (category == 0 && proc.isFavourite (e.name));
            if ((q.isNotEmpty() || inCat) && matches (e)) entries.push_back (e);
        }

        if (q.isNotEmpty())
            std::stable_sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.score > b.score; });
        heading = q.isNotEmpty() ? tr ("Resultados para “", "Results for “") + search.getText().trim() + juce::String::fromUTF8 ("”")
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

    void showMessage (const juce::String& title, const juce::String& text)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, title, text);
    }

    void askImport()
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("Escolhe o pack", "Choose the pack"), juce::File(), "*.dlxpack");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
            packs::PackInfo info; int n = 0; juce::String err;
            if (packs::importPack (fc.getResult(), safe->proc.userPresetFolder(), info, n, err))
            {
                safe->category = 2 + presetCategories().size(); safe->search.clear(); safe->rebuild();
                safe->showMessage (tr ("Pack importado", "Pack imported"), info.name + (info.author.isNotEmpty() ? tr (" por ", " by ") + info.author : juce::String())
                                   + "\n" + juce::String (n) + tr (" presets em Utilizador.", " presets in User."));
            }
            else safe->showMessage (tr ("Não foi possível importar", "Could not import"),
                                    err.startsWith ("hash") ? tr ("O pack está danificado ou foi alterado.", "The pack is damaged or was modified.")
                                  : err == "newer" ? tr ("Este pack precisa de uma versão mais recente do DELEX.", "This pack needs a newer DELEX version.")
                                                   : tr ("Não é um pack DELEX válido.", "Not a valid DELEX pack."));
        });
    }

    void askExport()
    {
        juce::Array<juce::File> mine;
        const auto packsDir = proc.userPresetFolder().getChildFile ("Packs");
        for (auto& f : proc.userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.dlxpreset")) if (! f.isAChildOf (packsDir)) mine.add (f);
        if (mine.isEmpty()) { showMessage (tr ("Sem presets", "No presets"), tr ("Guarda primeiro alguns presets teus (GUARDAR COMO…).", "Save some presets of your own first (SAVE AS…).")); return; }
        auto* w = new juce::AlertWindow (tr ("Exportar pack", "Export pack"), juce::String (mine.size()) + tr (" presets teus vão para o pack.", " of your presets go into the pack."), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", "Signature Series", tr ("Nome do pack", "Pack name"));
        w->addTextEditor ("author", "", tr ("Autor", "Author"));
        w->addTextEditor ("desc", "", tr ("Descrição", "Description"));
        w->addButton (tr ("Exportar", "Export"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (tr ("Cancelar", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, mine] (int r)
        {
            if (r != 1 || safe == nullptr) return;
            packs::PackInfo info { w->getTextEditorContents ("name"), w->getTextEditorContents ("author"), w->getTextEditorContents ("desc"), w->getTextEditorContents ("desc") };
            const auto dest = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (packs::safeName (info.name) + ".dlxpack");
            safe->chooser = std::make_unique<juce::FileChooser> (tr ("Guardar pack", "Save pack"), dest, "*.dlxpack");
            safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe, info, mine] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                juce::String err;
                const auto out = fc.getResult().withFileExtension ("dlxpack");
                if (packs::buildPack (mine, info, out, err)) safe->showMessage (tr ("Pack exportado", "Pack exported"), out.getFullPathName());
                else safe->showMessage (tr ("Erro", "Error"), tr ("Não foi possível escrever o pack.", "Could not write the pack."));
            });
        }), true);
    }

    void askSave()
    {
        auto* w = new juce::AlertWindow (tr ("Guardar preset", "Save preset"), tr ("Nome do preset:", "Preset name:"), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", proc.currentPresetName());
        w->addButton (tr ("Guardar", "Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton (tr ("Cancelar", "Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
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
        g.drawText ((category == 1 || search.getText().isNotEmpty() ? e.category + String::fromUTF8 (" · ") : String())
                    + (e.sub.isNotEmpty() ? e.sub + String::fromUTF8 (" · ") : String()) + e.desc,
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
    juce::OwnedArray<juce::TextButton> catButtons, lockButtons;
    int lockLabelX = 400;
    juce::ListBox list;
    juce::TextButton load, saveAs, openFolder, importPack, exportPack;
    std::unique_ptr<juce::FileChooser> chooser;
    IconButton close { "fechar", [] (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { icons::cross (g, r, c); } };
    std::vector<Entry> entries;
    juce::Array<juce::File> userFiles;
    juce::String heading;
    int category = 1, selected = -1;
};

} // namespace delex::ui
