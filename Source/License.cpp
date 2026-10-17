#include "License.h"
#include "i18n/PiradexLanguage.h"
#include <iterator>
#include <cstring>

namespace lic
{
// ---------------------------------------------------------------------------
//  Chaves aceites (só salt + hash; as senhas nunca aparecem no código)
// ---------------------------------------------------------------------------
static const KeyEntry kKeys[] =
{
    { "master", Kind::Master,
      "6cc1354f8527ef1a5f0c7a4ca471227f",
      "0eeeeffc91108b9b84ba1540c446644b2d141604e0cf31babfc13024d1835389", 0, 0, 0 },

    { "beta-01", Kind::Trial,
      "07cc03321270e2d228cc04145b3eeed8",
      "b62dbf90dbf17886dcb8da0b44977c93570f6e8eaf33dff0fc0fcc21fae5de00", 2026, 12, 31 },

    { "beta-02", Kind::Trial,
      "16ae1e3a6e206c580d2000fa51558d97",
      "36b742ac9c2f570baef88e1ebc6173d9d1608f31c135c7adc746160d82e1907a", 2026, 12, 31 },

    { "beta-03", Kind::Trial,
      "f6bdadaf1178aef3a13bad3c6499b77f",
      "26698f3906cb43168c1d083fcce2927fa27365bc6fc8bdfbc9f82f8b18ff1c73", 2026, 12, 31 },

    { "beta-04", Kind::Trial,
      "fc266cf4e858812689d647b1a8f3875f",
      "2c53e8a5181086d1534fc0332c030384207e7f5f2f52ebf46f09810d691870db", 2026, 12, 31 },

    { "beta-05", Kind::Trial,
      "382c7491ef20565fa5e683fe9a69e0c1",
      "abc5155250ffc036aa667f63ed8bec76454ebb3d472c1b22d4e2c4e7402938ff", 2026, 12, 31 },

    { "beta-06", Kind::Trial,
      "fd2fc728423c154a4dc0e0e4a038eba6",
      "ef46f876f6533972b0bfa2093917b1dcd44ceb7c3a9ce26076a7a763b573b1ae", 2026, 12, 31 },

    { "beta-07", Kind::Trial,
      "7783bde98d41afda4ae8db492246e337",
      "1d1c2edbbd6b3991bddb76cf8434122a8ee755370e5b7ac331cfbd719397e700", 2026, 12, 31 },

    { "beta-08", Kind::Trial,
      "f55293e5c1a02c438df5db157e413417",
      "c038e54799c0422ac32133703ebcc37a7e33fabf588f0bd8ef75ebbec69a392c", 2026, 12, 31 },

    { "beta-09", Kind::Trial,
      "2ab101a87649e56df2bc7a7581fecf31",
      "a0cc2972d4caa3035212fdb793245028b1440f7f422dad93bad038041248803b", 2026, 12, 31 },

    { "beta-10", Kind::Trial,
      "e10e686a3f1ed7fdf0d7eddf9fd5786a",
      "d393c0e3613377ae91ff224fdfb19711e4fb16203ccac4cc6b88c62e27b956a7", 2026, 12, 31 },
};

// ---------------------------------------------------------------------------
static juce::MemoryBlock sha256 (const void* data, size_t size)
{
    return juce::SHA256 (data, size).getRawData();
}

static juce::MemoryBlock hmacSha256 (const juce::MemoryBlock& key, const juce::MemoryBlock& msg)
{
    constexpr size_t B = 64;
    juce::MemoryBlock k = key.getSize() > B ? sha256 (key.getData(), key.getSize()) : key;
    k.ensureSize (B, true);
    uint8_t ipad[B], opad[B];
    for (size_t i = 0; i < B; ++i)
    {
        ipad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x36);
        opad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x5c);
    }
    juce::MemoryBlock inner (ipad, B);
    inner.append (msg.getData(), msg.getSize());
    const auto ih = sha256 (inner.getData(), inner.getSize());
    juce::MemoryBlock outer (opad, B);
    outer.append (ih.getData(), ih.getSize());
    return sha256 (outer.getData(), outer.getSize());
}

juce::MemoryBlock License::pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen)
{
    const auto pw = password.toRawUTF8();
    const juce::MemoryBlock key (pw, strlen (pw));
    juce::MemoryBlock out;
    for (uint32_t block = 1; (int) out.getSize() < dkLen; ++block)
    {
        juce::MemoryBlock s = salt;
        const uint8_t be[4] { (uint8_t) (block >> 24), (uint8_t) (block >> 16), (uint8_t) (block >> 8), (uint8_t) block };
        s.append (be, 4);
        auto u = hmacSha256 (key, s);
        juce::MemoryBlock t = u;
        for (int i = 1; i < iterations; ++i)
        {
            u = hmacSha256 (key, u);
            auto* tp = static_cast<uint8_t*> (t.getData());
            auto* up = static_cast<const uint8_t*> (u.getData());
            for (size_t j = 0; j < t.getSize(); ++j) tp[j] ^= up[j];
        }
        out.append (t.getData(), t.getSize());
    }
    out.setSize ((size_t) dkLen);
    return out;
}

juce::MemoryBlock License::fromHex (const char* hex)
{
    juce::MemoryBlock mb;
    mb.loadFromHexString (hex);
    return mb;
}

bool License::isExpired (const KeyEntry& k, juce::Time now)
{
    if (k.kind != Kind::Trial) return false;
    const juce::Time end (k.expYear, k.expMonth - 1, k.expDay, 23, 59, 59, 0, true);
    return now > end;
}

static bool constantTimeEquals (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    if (a.getSize() != b.getSize()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.getSize(); ++i)
        diff |= (uint8_t) (static_cast<const uint8_t*> (a.getData())[i] ^ static_cast<const uint8_t*> (b.getData())[i]);
    return diff == 0;
}

// ---------------------------------------------------------------------------
License& License::get()
{
    static License instance;
    return instance;
}

License::License()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "DELEX by Piradex";
    o.folderName          = "Piradex";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (o);
    tryAutoUnlock();
}

juce::String License::tokenFor (const KeyEntry& k)
{
    const juce::String s = juce::SystemStats::getUniqueDeviceID() + "|" + k.id + "|" + k.hashHex + "|DELEX2";
    return juce::SHA256 (s.toUTF8()).toHexString();
}

void License::setUnlocked (const KeyEntry& k)
{
    currentKey  = &k;
    currentKind = k.kind;
    unlocked.store (true);
}

void License::tryAutoUnlock()
{
    const auto id = props->getValue ("key");
    const auto token = props->getValue ("token");
    if (id.isEmpty() || token.isEmpty()) return;
    for (auto& k : kKeys)
        if (id == k.id && token == tokenFor (k) && ! isExpired (k))
        {
            setUnlocked (k);
            return;
        }
}

int License::matchPassword (const juce::String& password, bool& expired)
{
    const auto pw = password.trim();
    expired = false;
    for (int i = 0; i < (int) std::size (kKeys); ++i)
    {
        const auto& k = kKeys[i];
        const auto expected = fromHex (k.hashHex);
        const auto got = pbkdf2 (pw, fromHex (k.saltHex), kIterations, (int) expected.getSize());
        if (! constantTimeEquals (got, expected)) continue;
        if (isExpired (k)) { expired = true; continue; }
        return i;
    }
    return -1;
}

License::Result License::applyMatch (int index, bool expired, bool remember)
{
    if (index < 0 || index >= (int) std::size (kKeys))
        return expired ? Result::Expired : Result::Wrong;

    const auto& k = kKeys[index];
    setUnlocked (k);
    if (remember)
    {
        props->setValue ("key", juce::String (k.id));
        props->setValue ("token", tokenFor (k));
    }
    else
    {
        props->removeValue ("key");
        props->removeValue ("token");
    }
    props->saveIfNeeded();
    return Result::Ok;
}

License::Result License::tryPassword (const juce::String& password, bool remember)
{
    bool expired = false;
    const int idx = matchPassword (password, expired);
    return applyMatch (idx, expired, remember);
}

void License::lock()
{
    unlocked.store (false);
    currentKind = Kind::None;
    currentKey = nullptr;
    props->removeValue ("key");
    props->removeValue ("token");
    props->saveIfNeeded();
}

#if defined (DLX_ALLOW_TEST_UNLOCK)
void License::unlockForTests() { setUnlocked (kKeys[0]); }
#endif

void License::checkExpiry()
{
    if (unlocked.load() && currentKey != nullptr && isExpired (*currentKey))
        lock();
}

juce::String License::statusText() const
{
    if (! unlocked.load() || currentKey == nullptr) return {};
    if (currentKind == Kind::Master) return piradex::i18n::tr ("Licença completa", "Full license");
    return piradex::i18n::tr ("BETA · válida até ", "BETA · valid until ")
         + juce::String (currentKey->expDay).paddedLeft ('0', 2) + "/"
         + juce::String (currentKey->expMonth).paddedLeft ('0', 2) + "/" + juce::String (currentKey->expYear);
}
} // namespace lic
