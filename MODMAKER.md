# TrenchKit Integration Guide for Mod Makers

Hey mod makers! This guide explains how TrenchKit detects and handles mod updates, along with best practices to ensure smooth integration for your users.

## Table of Contents
- [Quick Reference](#quick-reference)
- [Mod Manifest](#mod-manifest)
- [Platform-Specific Guides](#platform-specific-guides)
  - [Nexus Mods](#nexus-mods)
  - [Itch.io](#itchio)
- [Version Numbering](#version-numbering)
- [File Naming Best Practices](#file-naming-best-practices)
- [Edge Cases Explained](#edge-cases-explained)
- [Testing Your Integration](#testing-your-integration)
- [FAQ](#faq)

## Quick Reference

### ✅ Do This
- **Use semantic versioning**: `1.0.0`, `1.2.3`, `2.0.0`
- **Include version in filenames**: `MyMod_v1.2.pak`
- **Keep filenames consistent** across versions (helps auto-matching)
- **Nexus**: Use the file replacement feature to mark updates explicitly

### ❌ Avoid This
- **Don't** use single-number versions (`v1.pak`) - version won't be detected, update will work less reliably
- **Don't** use non-standard prefixes (`r5`, `build42`) - use `v` or nothing for version detection
- **Don't** backdate uploads on itch.io - dates are used for update detection
- **Nexus**: Don't skip the version field - it's required for updates

## Mod Manifest

Adding a small XML manifest to your `.pak` unlocks several features in TrenchKit:

- **Display name** — your mod shows its real name instead of a cleaned-up filename
- **Author attribution** — shown in the mod row, supports multiple authors
- **Notices** — show a warning, info, or error icon with a tooltip directly in the mod list (e.g. "Requires the latest game update")
- **Dependency management** — TrenchKit warns users about missing dependencies, version mismatches, disabled dependencies, and incorrect load order
- **Tags** — categorize your mod for future filtering/search support
- **Links** — link to your homepage, Nexus, or itch.io page

All it takes is a single XML file inside your pak.

### Quick start

Place this file at `Mod/Manifest.xml` inside your pak:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ModManifest version="1">
  <Id>myName.mycoolmod</Id>
  <Name>My Cool Mod</Name>
  <Version>1.2.0</Version>
  <Author>YourName</Author>
  <Description>Replaces textures with HD versions.</Description>
</ModManifest>
```

That's it. TrenchKit picks it up automatically when the mod is installed.

### Full schema

```xml
<ModManifest version="1">
  <Id>myName.mycoolmod</Id>                     <!-- required: stable unique id -->
  <Name>My Cool Mod</Name>                      <!-- required: display name -->
  <Version>1.2.0</Version>
  <Author>First Author</Author>                 <!-- repeatable -->
  <Author>Second Author</Author>
  <Description>Short summary...</Description>
  <Homepage>https://example.com</Homepage>
  <Nexus>https://www.nexusmods.com/.../</Nexus>
  <Itch>https://example.itch.io/...</Itch>

  <Notice icon="warning">Requires the latest game update.</Notice>

  <Dependencies>
    <Dependency id="otherAuthor.othermod" minVersion="1.0.0" maxVersion="2.0.0" required="true" />
  </Dependencies>

  <Tags>
    <Tag>Textures</Tag>
  </Tags>
</ModManifest>
```

### Field reference

| Field | Required | Notes |
|---|---|---|
| `Id` | Yes | Stable unique identifier (e.g. `yourName.modName`). Used for dependency resolution. |
| `Name` | Yes | Display name shown in the mod list. |
| `Version` | No | Semantic version (e.g. `1.2.0`). |
| `Author` | No | Can appear multiple times for multiple authors. |
| `Description` | No | Short summary of the mod. |
| `Homepage` | No | Link to your mod's website. |
| `Nexus` | No | Link to the Nexus Mods page. |
| `Itch` | No | Link to the itch.io page. |
| `Notice` | No | Message shown as a tooltip icon in the mod row. Supports `icon` attribute: `info`, `warning`, `error`, `question`, `lightbulb`. |
| `Dependencies` | No | List of `Dependency` entries. Each has `id` (required), `minVersion`, `maxVersion`, and `required` (default: `true`). |
| `Tags` | No | List of `Tag` entries for categorization. |

### How dependencies work in TrenchKit

When your manifest declares dependencies, TrenchKit checks them in real-time and warns users about:

- **Missing dependencies** — the required mod isn't installed
- **Version mismatches** — the installed version is outside the min/max range
- **Disabled dependencies** — the dependency is installed but not enabled
- **Load order issues** — the dependency must load before your mod but is ordered after it

These warnings appear as an icon with a tooltip on the mod row, so users can fix issues before launching the game.

### Packaging the manifest

Place your manifest at `War/Mod/Manifest.xml` in your content directory before packing.

**With u4pak:**
```sh
u4pak.py pack War-WindowsNoEditor_MyMod.pak War
```

**Important:** The manifest must be stored **uncompressed** inside the pak. TrenchKit cannot read compressed entries. If your packer compresses everything by default, make sure `Mod/Manifest.xml` is excluded from compression.

## Platform-Specific Guides

### Nexus Mods

#### How Update Detection Works

TrenchKit uses a **3-tier system** to find updates:

**1. Explicit File Updates (Preferred)**
- Uses Nexus's built-in file replacement chain
- You mark "File B replaces File A" in your mod's file management
- This is the most reliable method
- **How to set it up**: When uploading a new version, use Nexus's "File updates" field

**2. Category-based Fallback**
- If no explicit chain exists, checks MAIN and UPDATE categories
- Picks the newest file from these categories
- Ignores ARCHIVED files automatically

**3. Any Non-archived File**
- Last resort: picks the newest non-archived file
- Can sometimes flag optional files as updates

#### Version Comparison

TrenchKit uses **semantic version comparison**:
- `1.0.0` < `1.1.0` < `2.0.0` ✅
- `1.2.0` vs `1.10.0` → correctly picks `1.10.0` ✅
- Compares major → minor → patch in order

#### Best Practices

1. **Always use the file replacement feature** when uploading new versions
2. **Use semantic versioning** (major.minor.patch)
3. **Set proper file categories**:
   - MAIN: Primary mod file
   - UPDATE: Patches and updates
   - ARCHIVED: Old versions (automatically excluded from updates)
4. **Keep old files as ARCHIVED** instead of deleting them

### Itch.io

#### How Update Detection Works

TrenchKit uses **date-based detection**:
- Compares upload dates/timestamps
- Any upload newer than the installed mod's date is a candidate
- Upload time is more precise than just the date

#### Single vs Multiple Uploads

**Single visible upload**:
- TrenchKit auto-downloads it without prompting
- Perfect for keeping things simple

**Multiple visible uploads**:
Users see a selection dialog with all candidates, sorted by date (newest first)

**Automatic matching**:
If TrenchKit finds an upload with the **exact same filename** as the installed mod, it auto-selects it without showing the dialog.

**Example**:
```
Installed: MyTextMod_v1.0.pak (uploaded Jan 1)
Available uploads:
  - MyTextMod_v1.1.pak (Jan 15) ← Exact match, auto-selected!
  - MyTextMod_v1.1_alt.pak (Jan 15) ← Won't auto-match
```

#### Version Extraction

TrenchKit tries to extract version from the filename:
- Pattern: `v?(\d+\.\d+(?:\.\d+)?)`
- ✅ Matches: `MyMod_v1.2.pak`, `MyMod_2.5.3.pak`, `MyMod-v10.0.pak`
- ❌ Doesn't match: `MyMod_v1.pak`, `MyMod_r5.pak`, `MyMod_beta.pak`

If no version is found, falls back to showing the upload date: `"Updated: 2024-01-15"`

#### Ignoring Updates

Users can click "Ignore These Updates" in the file selection dialog. This adds all current candidate upload IDs to an ignore list. If you upload a **new** file later, it will show up as an update (it's not on the ignore list).

#### Best Practices

1. **Include version in filename**: `MyMod_v1.2.3.pak`
2. **Keep filenames consistent** across versions for auto-matching
3. **Hide old uploads** to keep the page clean (one visible upload = auto-download)
4. **Don't backdate uploads** - dates are authoritative
5. Upload times matter - users will see all files uploaded after their installed version

## Version Numbering

### Recommended Format

Use **semantic versioning**: `major.minor.patch`

```
1.0.0 - Initial release
1.1.0 - New features, backward compatible
1.1.1 - Bug fixes only
2.0.0 - Breaking changes
```

### What TrenchKit Recognizes

**Regex pattern**: `v?(\d+\.\d+(?:\.\d+)?)`

✅ **Works**:
- `v1.0` or `1.0`
- `v2.5.3` or `2.5.3`
- `MyMod_v1.2.pak` → extracts "1.2"
- `MyMod-2.0.1.pak` → extracts "2.0.1"

❌ **Doesn't Work**:
- `v1` (single number)
- `r5` or `build42` (wrong prefix)
- `beta` or `alpha` (no numbers)
- `1.2b` (letter suffix not supported)

### Comparison Logic

**Nexus**: Full semantic version comparison using Qt's `QVersionNumber`
- Handles any number of segments: `1.2.3.4.5`
- Correctly orders: `1.2` < `1.10` < `1.20`

**Itch**: String comparison after extraction
- Use consistent segment count for best results
- `1.0.0` vs `1.10.0` works correctly
- Mixing `1.0` and `1.0.0` formats is fine

## File Naming Best Practices

### Consistency is Key

If you name your first version `MyMod_v1.0.pak`, stick with that pattern:
- ✅ `MyMod_v1.0.pak` → `MyMod_v1.1.pak` → `MyMod_v2.0.pak`
- ❌ `MyMod_v1.0.pak` → `MyMod_1.1.pak` → `MyMod-v2.0.pak`

Consistent names enable auto-matching on itch.io!

### Recommended Patterns

```
ModName_v1.2.pak              ← Simple, clean
ModName_v1.2.3.pak            ← With patch number
ModName-v1.2-FoxholeV1.0.pak  ← With game version tag
```

### Platform-Specific Tips

**Nexus**:
- Filename matters less (uses file IDs)
- Still helpful for users browsing files
- Keep it descriptive

**Itch**:
- Filename matching is important
- Keep the base name consistent
- Version number can change

## Edge Cases Explained

### Multiple Itch Uploads on Same Date

**Scenario**: You upload v1.1 and v1.1_hotfix on the same day.

**What happens**:
- Both show as update candidates
- User sees a file selection dialog
- They pick which one to install

**Recommendation**: Use timestamps strategically - upload the main version first, hotfix a bit later.

### Filename Changes Between Versions

**Nexus**:
- No problem! Uses file IDs for tracking
- Example: `MyMod.pak` → `MyMod_Remastered.pak` ✅

**Itch**:
- Won't auto-match, user sees file selection dialog
- Not a big issue, just requires one extra click
- Example: `MyMod_v1.0.pak` → `MyMod_v2.0_Redux.pak`

### No Version Information (Itch Only)

**What happens**:
- Update detection still works (date-based)
- Users see: `"Updated: 2024-01-15"` instead of version number
- Functionally fine, just less clear

**Recommendation**: Include version in filename for better UX.

### Archived Files (Nexus Only)

**What happens**:
- Automatically excluded from update detection
- Cannot be accidentally selected as updates
- Perfect for keeping old versions accessible without confusing users

**Use case**: Archive old versions instead of deleting them.

### Hidden Uploads (Itch Only)

**What happens**:
- TrenchKit only sees **visible** uploads
- Hidden uploads are ignored completely
- Single visible upload = auto-download

**Your workflow** ✅:
- Upload v1.1, hide v1.0
- Users see only v1.1
- TrenchKit auto-downloads it without prompting

### Rate Limiting

**Nexus**:
- API rate limits can trigger during update checks
- TrenchKit stops checking remaining mods if hit
- Users can retry later
- Premium vs free doesn't affect update checks (only downloads)

**Itch**:
- 500ms delay between API calls (built-in throttling)
- Rarely an issue
- No differentiation between account types

## Testing Your Integration

### For Nexus Mods

1. **Upload your mod** through TrenchKit's Add Mod → Nexus
2. **Check that metadata is saved**:
   - Right-click mod → "Edit Metadata"
   - Verify Nexus Mod ID, File ID, and Version are populated
3. **Upload a new version** on Nexus with a higher version number
4. **Mark it as replacement** using Nexus's file update field
5. **Click "Check for Updates"** in TrenchKit
6. **Verify** the update is detected

### For Itch.io

1. **Upload your mod** through TrenchKit's Add Mod → Itch.io
2. **Upload a new version** on itch with a later timestamp
3. **Click "Check for Updates"** in TrenchKit
4. **Verify** the update is detected
5. **Test scenarios**:
   - Single visible upload (should auto-download)
   - Multiple visible uploads (should show selection)
   - Same filename (should auto-match)

### Manual Metadata Testing

If you distributed a mod outside TrenchKit, users can add metadata:
1. Right-click mod → "Edit Metadata"
2. Add Nexus Mod ID + File ID + Version
   OR
   Add Itch Game ID
3. Updates will now work for that mod

## FAQ

### Q: Do users need TrenchKit to use my mod?

**A:** No! Mods work fine without TrenchKit. It just makes installation and updates easier.

---

### Q: Will update checks work if users manually install my mod?

**A:** Only if they add metadata via "Edit Metadata". Otherwise, TrenchKit has no way to know where the mod came from.

---

### Q: Can I have both Nexus and Itch versions?

**A:** Yes! They're treated as separate mods. A user who installs from Nexus will only see Nexus updates, and vice versa.

---

### Q: What if I change my mod's itch page URL?

**A:** TrenchKit uses the game ID, not the URL. As long as the itch.io game ID stays the same, updates will work.

---

### Q: Does "Check for Updates" download automatically?

**A:** No, it only **detects** updates. Users still need to click the update button and confirm the download.

---

### Q: What if my Nexus file has no version field?

**A:** Update detection won't work for that mod. Version is mandatory on Nexus.

---

### Q: Can users downgrade to an older version?

**A:** Not through the update system. They'd need to manually download and install the older file.

---

### Q: Do you support pre-release versions (beta, alpha)?

**A:** Not explicitly. Stick to numeric versions. You can use Nexus categories or itch.io tags to indicate beta status.

---

### Q: What happens if I upload a mod with the same version number?

**Nexus**: Won't be detected as an update (version comparison fails)
**Itch**: Will be detected if upload date is newer

---

## Need Help?

- **Issues**: [GitHub Issues](https://github.com/Tapawingo/TrenchKit/issues)
- **Discussions**: [GitHub Discussions](https://github.com/Tapawingo/TrenchKit/discussions)
- **Main README**: [README.md](README.md)

Thanks for making mods for Foxhole!
