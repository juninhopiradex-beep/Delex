// =============================================================================
//  DELEX by Piradex — packs de presets "Signature Series" (.dlxpack)
//
//  Um .dlxpack é um ZIP com:
//    pack.json              → { "format": "dlxpack", "version": 1, "name", "author",
//                               "description_pt", "description_en",
//                               "presets": [ { "file", "sha256", "desc_pt", "desc_en", "tags" } ] }
//    presets/*.dlxpreset    → estado completo do DELEX (inclui taps e throw map)
//    cover.png (opcional)
//  Na importação o DELEX confirma o formato e o SHA-256 de cada preset e copia tudo para
//  <pasta de presets>/Packs/<nome do pack>/. O browser mostra-os com o nome do pack e o autor.
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace delex::packs
{
struct PackInfo
{
    juce::String name, author, descPt, descEn;
};

inline juce::String sha256Of (const juce::MemoryBlock& m) { return juce::SHA256 (m.getData(), m.getSize()).toHexString(); }

inline juce::String safeName (const juce::String& s)
{
    auto n = juce::File::createLegalFileName (s.trim()).replaceCharacter ('/', '-').replaceCharacter ('\\', '-');
    return n.isEmpty() ? juce::String ("Pack") : n.substring (0, 80);
}

// Cria um .dlxpack a partir de ficheiros .dlxpreset
inline bool buildPack (const juce::Array<juce::File>& presets, const PackInfo& info, const juce::File& outFile, juce::String& error)
{
    if (presets.isEmpty()) { error = "empty"; return false; }
    if (info.name.trim().isEmpty()) { error = "name"; return false; }
    juce::ZipFile::Builder zb;
    juce::Array<juce::var> list;
    for (auto& f : presets)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb)) { error = "read:" + f.getFileName(); return false; }
        const auto entry = "presets/" + f.getFileName();
        zb.addEntry (new juce::MemoryInputStream (mb, true), 9, entry, juce::Time::getCurrentTime());
        auto* o = new juce::DynamicObject();
        o->setProperty ("file", entry);
        o->setProperty ("sha256", sha256Of (mb));
        list.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "dlxpack");
    root->setProperty ("version", 1);
    root->setProperty ("name", info.name.trim());
    root->setProperty ("author", info.author.trim());
    root->setProperty ("description_pt", info.descPt);
    root->setProperty ("description_en", info.descEn);
    root->setProperty ("presets", list);
    const auto json = juce::JSON::toString (juce::var (root));
    zb.addEntry (new juce::MemoryInputStream (json.toRawUTF8(), json.getNumBytesAsUTF8(), true), 9, "pack.json", juce::Time::getCurrentTime());

    outFile.deleteFile();
    juce::FileOutputStream out (outFile);
    if (! out.openedOk()) { error = "write"; return false; }
    if (! zb.writeToStream (out, nullptr)) { error = "write"; return false; }
    return true;
}

// Importa um .dlxpack para <userFolder>/Packs/<nome>/. Devolve o nome do pack.
inline bool importPack (const juce::File& packFile, const juce::File& userFolder, PackInfo& info, int& count, juce::String& error)
{
    count = 0;
    juce::ZipFile zip (packFile);
    const auto* manifestEntry = zip.getEntry ("pack.json");
    if (manifestEntry == nullptr) { error = "manifest"; return false; }
    std::unique_ptr<juce::InputStream> ms (zip.createStreamForEntry (*manifestEntry));
    if (ms == nullptr) { error = "manifest"; return false; }
    const auto json = juce::JSON::parse (ms->readEntireStreamAsString());
    if (json["format"].toString() != "dlxpack" || (int) json["version"] < 1) { error = "format"; return false; }
    if ((int) json["version"] > 1) { error = "newer"; return false; }
    info.name = json["name"].toString(); info.author = json["author"].toString();
    info.descPt = json["description_pt"].toString(); info.descEn = json["description_en"].toString();
    if (info.name.trim().isEmpty()) { error = "format"; return false; }

    // primeiro valida tudo, só depois escreve (nada fica a meio)
    struct Item { juce::String name; juce::MemoryBlock data; };
    std::vector<Item> items;
    if (auto* arr = json["presets"].getArray())
        for (auto& p : *arr)
        {
            const auto path = p["file"].toString();
            if (! path.startsWith ("presets/") || ! path.endsWithIgnoreCase (".dlxpreset") || path.contains ("..")) { error = "path"; return false; }
            const auto* e = zip.getEntry (path);
            if (e == nullptr) { error = "missing:" + path; return false; }
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (*e));
            juce::MemoryBlock mb;
            if (in == nullptr || in->readIntoMemoryBlock (mb) <= 0) { error = "missing:" + path; return false; }
            const auto want = p["sha256"].toString();
            if (want.isNotEmpty() && want != sha256Of (mb)) { error = "hash:" + path; return false; }
            if (juce::XmlDocument::parse (mb.toString()) == nullptr) { error = "xml:" + path; return false; }
            items.push_back ({ juce::File::createLegalFileName (path.fromLastOccurrenceOf ("/", false, false)), std::move (mb) });
        }
    if (items.empty()) { error = "empty"; return false; }

    auto dir = userFolder.getChildFile ("Packs").getChildFile (safeName (info.name));
    if (! dir.createDirectory()) { error = "write"; return false; }
    for (auto& it : items)
        if (dir.getChildFile (it.name).replaceWithData (it.data.getData(), it.data.getSize())) ++count;
    dir.getChildFile ("pack.json").replaceWithText (juce::JSON::toString (json));
    if (const auto* cover = zip.getEntry ("cover.png"))
        if (std::unique_ptr<juce::InputStream> cs (zip.createStreamForEntry (*cover)); cs != nullptr)
        {
            juce::FileOutputStream co (dir.getChildFile ("cover.png"));
            if (co.openedOk()) { co.setPosition (0); co.truncate(); co.writeFromInputStream (*cs, -1); }
        }
    return count > 0;
}

// Informação de um pack já importado (a partir da pasta)
inline PackInfo readInstalled (const juce::File& packDir)
{
    PackInfo i;
    const auto json = juce::JSON::parse (packDir.getChildFile ("pack.json"));
    i.name = json["name"].toString(); i.author = json["author"].toString();
    i.descPt = json["description_pt"].toString(); i.descEn = json["description_en"].toString();
    if (i.name.isEmpty()) i.name = packDir.getFileName();
    return i;
}
} // namespace delex::packs
