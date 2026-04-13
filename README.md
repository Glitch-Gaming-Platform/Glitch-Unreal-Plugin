# Glitch Unreal Plugin v2.0.0

The official [Glitch](https://glitch.fun) plugin for **Unreal Engine 4.26 – 5.7+**. Zero-code heartbeat payouts and DRM via Project Settings, with simple Blueprint nodes and C++ calls for achievements, leaderboards, cloud saves, analytics, purchases, fingerprinting, and optional Steam-to-Glitch migration.

**Repository:** [github.com/Glitch-Gaming-Platform/Glitch-Unreal-Plugin](https://github.com/Glitch-Gaming-Platform/Glitch-Unreal-Plugin)

---

## Table of Contents

1. [Installation](#installation)
2. [Quick Start (Zero Code)](#quick-start-zero-code)
3. [Achievements](#achievements)
4. [Leaderboards](#leaderboards)
5. [Cloud Saves](#cloud-saves)
6. [Analytics Events](#analytics-events)
7. [Purchases / Revenue](#purchases--revenue)
8. [Fingerprinting & Attribution](#fingerprinting--attribution)
9. [Steam-to-Glitch Migration](#steam-to-glitch-migration)
10. [C++ API Reference](#c-api-reference)
11. [UE 4.26 Compatibility](#ue-426-compatibility)
12. [Troubleshooting](#troubleshooting)

---

## Installation

1. Copy the `GlitchAegis` folder into your project's `Plugins/` directory.
2. Open your project in Unreal Engine.
3. Go to **Edit → Plugins**, search for "Glitch Gaming", and confirm it's enabled.
4. Restart the editor if prompted.

The plugin compiles on **UE 4.26, 4.27, 5.0–5.7+** across Windows, Mac, and Linux. The Build.cs handles all engine version differences automatically (C++ standard, warning suppression APIs, etc.).

---

## Quick Start (Zero Code)

The heartbeat (payouts) and DRM validation work entirely through Project Settings — no Blueprints or C++ required.

### Step 1: Get Your Credentials

1. Go to [glitch.fun](https://glitch.fun) → **My Games** → select your game.
2. Open the **Technical Integration** page.
3. Copy your **Title ID** and **Title Token**.
4. Copy your **Developer Test Install ID** (for editor testing).

### Step 2: Configure the Plugin

Go to **Edit → Project Settings → Glitch Aegis** and fill in:

| Setting | What to Enter |
|---------|--------------|
| **Title Id** | Your UUID from the dashboard |
| **Title Token** | Your private API token |
| **Test Install ID** | Your dev test ID (for PIE testing) |

### Step 3: Toggle Features

| Setting | Default | What It Does |
|---------|---------|-------------|
| **bEnableAutomaticHeartbeat** | ✅ ON | Pings every 30s → earns $0.10/hr payouts |
| **HeartbeatIntervalSeconds** | 30 | Seconds between pings (10–120) |
| **bRequireValidation** | ❌ OFF | Validates license on startup |
| **bShowErrorScreenOnValidationFailure** | ✅ ON | Blocks game with error screen if DRM fails |
| **bEnableAchievements** | ✅ ON | Auto-loads achievement data on startup |
| **bAutoLoadAchievements** | ✅ ON | Fetches achievement cache at boot |
| **bEnableLeaderboards** | ✅ ON | Enables score submission |
| **bEnableCloudSaves** | ✅ ON | Enables cloud save/load |
| **bEnableSteamBridge** | ❌ OFF | Activates Steam-to-Glitch replacement functions |
| **bEnableFingerprinting** | ✅ ON | Sends hardware fingerprint for attribution |

### Step 4: Build & Deploy

Package your game for **Linux** (Pixel Streaming) or **Windows** and upload to the Glitch [Deploy Page](https://glitch.fun/games/admin).

**That's it.** The `GlitchAegisSubsystem` initializes automatically on game launch, reads `-install_id=UUID` from the command line (injected by the Glitch launcher), registers the session, and starts the heartbeat loop.

---

## Achievements

### Dashboard Setup

Define achievements on the Glitch dashboard with an **API Key** (e.g. `boss_killed`) and an **Unlock Threshold** (e.g. `1`).

### Blueprint Usage

Right-click in your Event Graph and search for "Glitch":

- **Report Achievement Progress** — sends progress toward an achievement
  - Api Key: `boss_killed`
  - Value: `1`

- **Is Achievement Unlocked** — returns true/false from local cache (instant, no network call)

- **Refresh Achievements** — force-reload from server

### C++ Usage

```cpp
// Via the subsystem (recommended — handles credentials automatically):
UGlitchAegisSubsystem* Glitch = GetGameInstance()->GetSubsystem<UGlitchAegisSubsystem>();

// Report progress:
Glitch->ReportAchievement(TEXT("boss_killed"), 1.0f);

// Cumulative progress (e.g. 50 out of 100 coins):
Glitch->ReportAchievement(TEXT("coin_collector"), 50.0f);

// Check if unlocked:
if (Glitch->IsAchievementUnlocked(TEXT("boss_killed")))
{
    // Show golden trophy
}
```

### How It Works

When you call `ReportAchievement`, the plugin sends a "Run Submission" to the Glitch server with your stat value. The server checks it against the threshold you defined on the dashboard. If the threshold is met, Glitch unlocks the achievement and the Aegis Bridge overlay shows a toast notification to the player automatically.

### Events

```cpp
// Listen for achievement load completion:
Glitch->OnAchievementsLoaded.AddDynamic(this, &AMyGameMode::HandleAchievementsLoaded);

void AMyGameMode::HandleAchievementsLoaded(bool bSuccess)
{
    if (bSuccess)
    {
        // Achievement cache is now populated — safe to call IsAchievementUnlocked
    }
}
```

---

## Leaderboards

### Dashboard Setup

Define leaderboards on the dashboard with an **API Key** (e.g. `high_score`) and **Sort Order** (High to Low or Low to High).

### Blueprint Usage

- **Submit Leaderboard Score** — sends a score
  - Board Api Key: `high_score`
  - Score: `5000`

- **Get Leaderboard (Async)** — downloads entries, result via callback

### C++ Usage

```cpp
UGlitchAegisSubsystem* Glitch = GetGameInstance()->GetSubsystem<UGlitchAegisSubsystem>();

// Submit a score:
Glitch->SubmitScore(TEXT("high_score"), 5000.0f);

// Or via the static Blueprint library:
UGlitchAegisLibrary::SubmitLeaderboardScore(TEXT("high_score"), 5000.0f);
```

---

## Cloud Saves

Cloud saves were already in the original plugin. Use the Blueprint node **Save To Cloud** or the C++ API:

```cpp
#include "GlitchAegisLibrary.h"

FGlitchSaveData SaveData;
SaveData.SlotIndex = 1;
SaveData.PayloadBase64 = FBase64::Encode(RawBytes);
SaveData.Checksum = ComputeSHA256Hex(RawBytes); // YOU must compute this
SaveData.SaveType = TEXT("manual");

UGlitchAegisLibrary::SaveToCloud(SaveData);
```

**Important:** The `Checksum` field must be a valid SHA-256 hex string of your raw (pre-Base64) save bytes. The server uses it for integrity verification and conflict resolution.

### Loading Saves

Use the low-level `GlitchSDK::ListSaves()` to download all slots, then parse the JSON response to find your slot by `slot_index`. The `payload` field contains your Base64-encoded data.

---

## Analytics Events

Track what players do in your game for funnel analysis:

### Blueprint

Use **Record Game Event**:
- Step: `boss_fight`
- Action: `player_death`
- MetadataJSON: `{"weapon":"sword","hp_remaining":0}`

### C++

```cpp
UGlitchAegisLibrary::RecordGameEvent(TEXT("boss_fight"), TEXT("player_death"), TEXT("{\"weapon\":\"sword\"}"));
```

### Bulk Events

For mobile or battery-sensitive scenarios, batch events with **Record Game Events (Bulk)**.

---

## Purchases / Revenue

Track in-app purchases and revenue events:

### Blueprint

Use **Record Purchase** with `FGlitchPurchaseData`:
- GameInstallId: the active session UUID
- PurchaseType: `"in_app"`
- PurchaseAmount: `4.99`
- Currency: `"USD"`
- TransactionId: your receipt ID (prevents duplicates)

### C++

```cpp
FGlitchPurchaseData Purchase;
Purchase.GameInstallId = Glitch->GetInstallId();
Purchase.PurchaseType = TEXT("in_app");
Purchase.PurchaseAmount = 4.99f;
Purchase.Currency = TEXT("USD");
Purchase.TransactionId = TEXT("txn_12345");
Purchase.ItemSku = TEXT("starter_pack");

UGlitchAegisLibrary::RecordPurchase(Purchase);
```

---

## Fingerprinting & Attribution

The plugin collects hardware fingerprint data (CPU model, RAM, display resolution, OS version, etc.) to improve cross-device attribution accuracy. This data is sent with the first install registration.

### Disabling Fingerprinting

If you experience crashes on **UE 4.26** or earlier, disable fingerprinting in Project Settings:

**Edit → Project Settings → Glitch Aegis → Fingerprinting → bEnableFingerprinting = false**

This skips all platform-specific system calls (CPUID, RtlGetVersion, sysctl) that may not be available on older engine versions or unusual OS configurations.

### Manual Fingerprinted Install

From Blueprints, use **Send Fingerprinted Install** to explicitly trigger a fingerprinted install registration with a callback.

---

## Steam-to-Glitch Migration

If your game already uses **Steam's Online Subsystem** or raw Steamworks API calls, the Steam Bridge lets you redirect achievements and leaderboards to Glitch without rewriting game logic.

### Prerequisites

1. On the Glitch dashboard, create achievements and leaderboards with **the same API key names** you used on Steam.
2. Set **bEnableSteamBridge = true** in Project Settings → Glitch Aegis → Steam Bridge.

### Blueprint Usage

Replace your Steam Blueprint nodes with the Glitch equivalents:

| Steam Node | Glitch Bridge Node |
|---|---|
| Write Achievement Progress | **Steam Bridge: Set Achievement** |
| Write Leaderboard Integer | **Steam Bridge: Upload Score** |
| (no equivalent — call after setting) | **Steam Bridge: Store Stats** |

The pattern is identical to Steam: buffer calls, then flush:

1. **Steam Bridge: Set Achievement** (`"ACH_WIN_GAME"`)
2. **Steam Bridge: Upload Score** (`"high_score"`, `5000`)
3. **Steam Bridge: Store Stats** ← flushes everything to Glitch

### C++ Usage

```cpp
// ─── BEFORE (Steam) ─────────────────────────
// IOnlineAchievements* Achievements = Online::GetAchievementsInterface();
// Achievements->WriteAchievements(...);

// ─── AFTER (Glitch Bridge) ──────────────────
UGlitchAegisLibrary::SteamBridgeSetAchievement(TEXT("ACH_WIN_GAME"));
UGlitchAegisLibrary::SteamBridgeUploadScore(TEXT("high_score"), 5000.0f);
UGlitchAegisLibrary::SteamBridgeStoreStats(); // Flushes all to Glitch
```

### Preprocessor Switch (Dual Build)

Maintain both Steam and Glitch builds from the same codebase:

```cpp
// In your Build.cs or a shared header:
// #define USE_GLITCH_BRIDGE 1

#if USE_GLITCH_BRIDGE
    UGlitchAegisLibrary::SteamBridgeSetAchievement(TEXT("ACH_WIN_GAME"));
    UGlitchAegisLibrary::SteamBridgeStoreStats();
#else
    // Your existing Steam code
    Achievements->WriteAchievements(...);
#endif
```

### What the Bridge Does NOT Handle

- **Steam Networking / Lobbies** — not applicable to Glitch
- **Steam Workshop / UGC** — not applicable
- **Steam Overlay** — Glitch has its own Aegis Bridge overlay for achievement toasts
- **SteamID / Friends** — Glitch has its own user identity system

---

## C++ API Reference

### GlitchAegisSubsystem (auto-created on game launch)

```cpp
UGlitchAegisSubsystem* Glitch = GetGameInstance()->GetSubsystem<UGlitchAegisSubsystem>();
```

| Method | Description |
|--------|------------|
| `GetInstallId()` | Returns the session UUID |
| `IsUsingTestInstallId()` | True if using the dev test ID |
| `StartHeartbeatLoop()` | Manually start heartbeat |
| `StopHeartbeatLoop()` | Pause heartbeat (e.g. pause menu) |
| `RunValidation()` | Manually trigger DRM check |
| `ReportAchievement(ApiKey, Value)` | Report achievement progress |
| `SubmitScore(BoardKey, Score)` | Submit a leaderboard score |
| `IsAchievementUnlocked(ApiKey)` | Check local cache (instant) |

| Delegate | Description |
|----------|------------|
| `OnDrmValidated(bIsValid, UserName)` | Fires after DRM validation |
| `OnAchievementsLoaded(bSuccess)` | Fires after achievement cache loads |

### GlitchAegisLibrary (static Blueprint functions)

| Function | Category | Description |
|----------|----------|------------|
| `ValidateLicenseAsync(OnComplete)` | Security | Async DRM check |
| `RecordGameEvent(Step, Action, Meta)` | Telemetry | Single analytics event |
| `RecordGameEventsBulk(Events)` | Telemetry | Batch analytics events |
| `SaveToCloud(SaveData)` | Cloud Save | Upload save slot |
| `RecordPurchase(PurchaseData)` | Revenue | Record purchase/revenue |
| `VoidInstall(UUID, bVoid, OnComplete)` | Install | Mark install as void |
| `SendFingerprintedInstall(OnComplete)` | Fingerprinting | Full fingerprinted install |
| `ReportAchievementProgress(Key, Value)` | Achievements | **NEW** Report progress |
| `IsAchievementUnlocked(Key)` | Achievements | **NEW** Check local cache |
| `RefreshAchievements()` | Achievements | **NEW** Force reload |
| `SubmitLeaderboardScore(Key, Score)` | Leaderboards | **NEW** Submit score |
| `GetLeaderboardAsync(Key, OnComplete)` | Leaderboards | **NEW** Download entries |
| `SteamBridgeSetAchievement(Name)` | Steam Bridge | **NEW** Buffer achievement |
| `SteamBridgeUploadScore(Key, Score)` | Steam Bridge | **NEW** Buffer score |
| `SteamBridgeStoreStats()` | Steam Bridge | **NEW** Flush to Glitch |

### GlitchSDK Namespace (low-level C++)

For advanced use cases, all HTTP calls are available in the `GlitchSDK` namespace. See `GlitchSDK.h` for the complete API including `CreateInstall`, `StoreSave`, `ResolveSaveConflict`, `RecordEventsBulk`, `ToggleWishlist`, and more.

---

## UE 4.26 Compatibility

The plugin is tested on UE 4.26 through 5.7+. Known considerations:

- **Fingerprinting crash on 4.26:** Set `bEnableFingerprinting = false` in Project Settings. The CPUID and RtlGetVersion calls are safe on most Windows configurations, but some UE 4.26 builds have stricter platform header configurations that can cause issues.

- **C++ standard:** The Build.cs automatically sets C++17 on UE 4.x/5.0–5.2 and defers to the engine default (C++20) on 5.3+. No manual configuration needed.

- **Warning suppression:** The Build.cs handles the three different APIs Epic used for `UndefinedIdentifierWarningLevel` across engine versions (`bEnableUndefinedIdentifierWarnings` for 4.x/5.0–5.4, `UndefinedIdentifierWarningLevel` for 5.5, `CppCompileWarningSettings.UndefinedIdentifierWarningLevel` for 5.6+).

- **DeveloperSettings module:** Available since UE 4.25. If you're on 4.24 or earlier (rare), you'll need to move settings to a different config approach.

---

## Troubleshooting

### "No -install_id= on command line" warning in PIE

This is normal when running in the editor. Paste your **Test Install ID** (from the dashboard Technical page) into **Project Settings → Glitch Aegis → Development → Test Install ID**.

### Heartbeat not starting

Check that:
1. `bEnableAutomaticHeartbeat` is true
2. `TitleId` and `TitleToken` are not empty
3. An `install_id` is available (Test Install ID or command-line arg)

### Achievement not unlocking

- Confirm the API Key matches exactly (case-sensitive) between your code and the Glitch dashboard
- Confirm the progress value meets or exceeds the Unlock Threshold
- Check the Output Log for "GlitchAegis:" messages

### DRM shows error screen during development

Set `bRequireValidation = false` while developing. Only enable it for production builds.

### Cloud save returns "Missing checksum"

You must compute a SHA-256 hex hash of your raw save bytes BEFORE Base64 encoding and pass it in `SaveData.Checksum`. The server rejects saves without a valid checksum.

### Build fails on Mac/Linux

Ensure the HTTP module is available. The plugin depends on `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `HTTP`, `Json`, and `JsonUtilities` — all are engine modules available on all platforms.

---

## Support

- **Dashboard**: [glitch.fun/games/admin](https://glitch.fun/games/admin)
- **Documentation**: [docs.glitch.fun](https://docs.glitch.fun)
- **Discord**: [discord.gg/RPYU9KgEmU](https://discord.gg/RPYU9KgEmU)
- **Issues**: [GitHub Issues](https://github.com/Glitch-Gaming-Platform/Glitch-Unreal-Plugin/issues)

---

## License

Free to use in any project. Credit appreciated but not required.
