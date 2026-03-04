# Glitch Gaming — Unreal Engine Plugin

> **Connect your Unreal game to the [Glitch Gaming Platform](https://glitch.fun).**
> Earn developer payouts, protect your game with DRM, track player analytics, and attribute installs to your marketing campaigns — all with zero server infrastructure on your end.

---

## Table of Contents

1. [What Does This Plugin Do?](#what-does-this-plugin-do)
2. [Requirements](#requirements)
3. [Installation](#installation)
4. [Quick Start (Zero-Code Setup)](#quick-start-zero-code-setup)
5. [Feature Guide](#feature-guide)
   - [Payout Heartbeat](#1-payout-heartbeat)
   - [DRM License Validation](#2-drm-license-validation)
   - [Install & Retention Tracking](#3-install--retention-tracking)
   - [Fingerprinting & Cross-Device Attribution](#4-fingerprinting--cross-device-attribution)
   - [Behavioral Events & Funnels](#5-behavioral-events--funnels)
   - [Purchase & Revenue Tracking](#6-purchase--revenue-tracking)
   - [Cloud Saves](#7-cloud-saves)
   - [Voiding Installs](#8-voiding-installs)
6. [Project Settings Reference](#project-settings-reference)
7. [Blueprint Reference](#blueprint-reference)
8. [C++ Reference](#c-reference)
9. [File Structure](#file-structure)
10. [FAQ](#faq)

---

## What Does This Plugin Do?

When a player launches your game through the Glitch platform, a lot happens automatically:

| Feature | What it means for you |
|---|---|
| **Payout Heartbeat** | You earn **$0.10/hour** per active player. The plugin sends a signal every 30 seconds to prove a human is playing. |
| **DRM Validation** | Confirms the player has a valid Glitch license before your game loads. Stops unauthorized access. |
| **Analytics** | Tracks installs, session length, and player retention across your whole player base. |
| **Attribution** | Links every install back to the ad or influencer that drove it, even across devices. |
| **Behavioral Events** | Records what players do inside your game so you can see where they quit. |
| **Revenue Tracking** | Logs in-app purchases for LTV and ROI reporting. |
| **Cloud Saves** | Stores player save data server-side so it persists across devices. |

---

## Requirements

- **Unreal Engine 5.0 or later** (tested on 5.1–5.4)
- A free [Glitch Gaming Platform](https://glitch.fun) developer account
- Your game's **Title ID** and a **Title Token** (generated in your game's dashboard)

---

## Installation

### Step 1 — Create the plugin folder

Inside your Unreal project folder, create this path if it doesn't exist:

```
YourProject/
└── Plugins/
    └── GlitchAegis/
```

### Step 2 — Drop in the files

Your final folder structure must look exactly like this:

```
Plugins/
└── GlitchAegis/
    ├── GlitchAegis.uplugin
    └── Source/
        └── GlitchAegis/
            ├── GlitchAegis.Build.cs
            ├── Public/
            │   ├── GlitchSDK.h
            │   ├── GlitchAegisSettings.h
            │   ├── GlitchAegisSubsystem.h
            │   └── GlitchAegisLibrary.h
            └── Private/
                ├── GlitchSDK.cpp
                ├── GlitchAegisSubsystem.cpp
                └── GlitchAegisLibrary.cpp
```

### Step 3 — Regenerate project files

**Close Unreal Engine**, then right-click your `.uproject` file in Windows Explorer and choose **"Generate Visual Studio project files"**. This is required whenever you add a new plugin.

### Step 4 — Enable the plugin

Open your project in Unreal, go to **Edit → Plugins**, search for **"Glitch Aegis"**, and tick the **Enabled** checkbox. Restart when prompted.

### Step 5 — Configure your credentials

Go to **Edit → Project Settings → Glitch Aegis** and fill in:

- **Title ID** — your game's ID from the Glitch dashboard
- **Title Token** — the secret token you generated on the Technical Integration page

> ⚠️ **Keep your Title Token secret.** Never commit it to a public GitHub repo. Treat it like a password.

---

## Quick Start (Zero-Code Setup)

If you just want payouts working as fast as possible:

1. **Fill in Title ID + Title Token** in Project Settings (see Step 5 above).
2. Make sure **"Enable Automatic Heartbeat"** is checked (it is by default).
3. Build and deploy your game through the Glitch platform.

That's it. The plugin's `GlitchAegisSubsystem` starts automatically when the game launches. It reads the player's session ID from the command line (injected by the Glitch launcher), registers the install, validates the license, and starts the payout heartbeat — all without any code from you.

---

## Feature Guide

### 1. Payout Heartbeat

**What it is:** Every 30 seconds while your game is running, the plugin pings the Glitch server. Each ping proves a real human is playing and credits one minute toward your **$0.10/hour developer payout**.

**How it works automatically:**
The `GlitchAegisSubsystem` handles this entirely if `bEnableAutomaticHeartbeat = true` in settings. Nothing to code.

**Manual control (optional):**
If you want to start the heartbeat at a specific moment (e.g., after your own loading screen), disable auto-heartbeat in settings and call it from Blueprint:

```
[Get GlitchAegisSubsystem] → [Start Heartbeat Loop]
```

**Configuring the interval:**
In Project Settings, `Heartbeat Interval Seconds` defaults to `30`. You can increase it to `60` if you prefer — just note that shorter intervals give more accurate session data for analytics.

---

### 2. DRM License Validation

**What it is:** On startup, the plugin checks with Glitch's server to confirm the player has a valid, active license for your game. If they don't, you can block access.

**Automatic check:**
When `bEnableAutomaticHeartbeat = true`, the subsystem automatically runs the validation and broadcasts the result via the `OnDrmValidated` event. Bind to it in your GameMode:

**In Blueprint:**

1. Get the `GlitchAegisSubsystem` from your Game Instance.
2. Bind an event to **On Drm Validated**.
3. In your event: if `bIsValid` is **true** → proceed to main menu. If **false** → show "Access Denied" and quit.

```
[Event BeginPlay]
  → [Get Game Instance Subsystem: GlitchAegisSubsystem]
  → [Bind Event to On Drm Validated]
       → [Branch: Is Valid?]
            True  → [Open Level: MainMenu]
            False → [Quit Game]
```

**On-demand check (Blueprint function):**

Use **Validate License (Async)** from the Blueprint library anywhere in your game:

```
[Validate License Async]
  → On Complete
       → [Branch: Is Valid?]
            True  → continue
            False → show "Please purchase on Glitch"
```

> ℹ️ There is also a legacy `Validate License` node (non-async). It is deprecated — it fires the request but silently ignores the 403 "denied" response, so it won't actually stop unauthorized players. Use **Validate License Async** instead.

---

### 3. Install & Retention Tracking

**What it is:** Records when a player first installs/runs your game, and tracks how long they keep playing. This powers the Retention dashboard on Glitch.

**How it works:**
The subsystem sends the install record automatically on first launch. Every heartbeat after that is detected by the backend as a retention ping — no separate setup needed.

**What gets tracked automatically:**
- Platform (`pc`)
- Device type (`desktop`)
- Game version (from your `GameVersion` setting)
- Referral source (from your `ReferralSource` setting)
- Hardware fingerprint (CPU, GPU, OS, screen, etc.)

**Recommended settings to fill in:**

| Setting | Example | Why |
|---|---|---|
| `GameVersion` | `1.2.3` | Track retention across updates |
| `ReferralSource` | `advertising` | See which channels drive installs |

---

### 4. Fingerprinting & Cross-Device Attribution

**What it is:** When a player clicks your ad on a browser, then installs your game on their PC, we need to connect those two events to prove the ad worked. Fingerprinting does this by matching hardware signals (CPU model, GPU, screen resolution, OS version, etc.) between the web session and the game client.

**How it works automatically:**
When the subsystem registers the install, it calls `CollectSystemFingerprint()` automatically, which gathers all available hardware signals and includes them in the install record. No setup required.

**Keyboard layout matching:**
The docs describe keyboard layout as one of the most powerful fingerprinting signals for web-to-game matching. The plugin collects this automatically using platform APIs on Windows and Mac.

**Manual fingerprinted install (Blueprint):**
If you disabled auto-heartbeat and want to send a fingerprinted install at a specific moment:

```
[Send Fingerprinted Install]
  → On Complete
       → [Branch: Success?]
            True  → continue
            False → log error
```

---

### 5. Behavioral Events & Funnels

**What it is:** Track specific actions players take inside your game. Combine them into Funnels to see exactly where players are dropping off — for example, how many players who start the tutorial actually finish it.

**The two labels every event needs:**
- **Step Key** — the screen or stage the player is on (e.g. `"onboarding"`, `"level_1"`, `"shop"`)
- **Action Key** — what they did (e.g. `"start"`, `"complete"`, `"player_death"`, `"open"`)

**Sending a single event (Blueprint):**

Use **Record Game Event** from the Blueprint library:

```
[Record Game Event]
  Step:   "tutorial"
  Action: "completed"
  Metadata: "{\"time_seconds\": 120}"
```

The `Metadata` field is optional. It accepts a JSON string if you want to attach extra data (scores, difficulty, item names, etc.).

**Sending multiple events at once (Bulk — recommended for mobile):**

Batching events saves battery and bandwidth. Use **Record Game Events (Bulk)**:

```
[Make Array of FGlitchEventData]
  → Element 0: StepKey="menu",     ActionKey="open"
  → Element 1: StepKey="menu",     ActionKey="click_play"
  → Element 2: StepKey="level_1",  ActionKey="start"

[Record Game Events Bulk] ← pass the array in
```

Call the bulk version every 1–2 minutes, or when the player transitions between major screens.

**Building a funnel on the dashboard:**
Once events are flowing, go to the **Behavioral Funnels** tab in your Glitch dashboard. Select your steps in order (e.g. `tutorial_start → tutorial_complete → level_1_start`) and the dashboard calculates the drop-off rate between each step automatically.

---

### 6. Purchase & Revenue Tracking

**What it is:** Records when a player buys something — an in-app purchase, DLC, cosmetic, etc. This data feeds your Revenue and LTV dashboards and can be linked back to the ad that drove the purchase.

**Blueprint — Record Purchase:**

```
[Make FGlitchPurchaseData]
  GameInstallId:   (your active install UUID)
  PurchaseType:    "in_app"
  PurchaseAmount:  4.99
  Currency:        "USD"
  TransactionId:   "receipt-abc-123"   ← prevents duplicate records
  ItemSku:         "starter_pack"
  ItemName:        "Beginner Pack"
  Quantity:        1

[Record Purchase] ← pass the struct in
```

> 💡 Always include `TransactionId` if your payment provider gives you one. The server uses it to ignore duplicate webhook calls and keep your revenue numbers accurate.

**Where to get `GameInstallId`:**
This is the UUID returned by the `/installs` endpoint when the session was registered. From Blueprint you can get it via:

```
[Get Game Instance Subsystem: GlitchAegisSubsystem] → [Get Install Id]
```

---

### 7. Cloud Saves

**What it is:** Store your player's save data on Glitch's servers so it persists across reinstalls and devices.

**Blueprint — Save To Cloud:**

```
[Make FGlitchSaveData]
  SlotIndex:     0               ← slot 0–99
  PayloadBase64: (your save data, base64 encoded)
  Checksum:      (SHA-256 hex of the raw bytes — see note below)
  SaveType:      "manual"        ← or "auto" for checkpoints

[Save To Cloud] ← pass the struct in
```

> ⚠️ **Checksum is required.** You must compute a SHA-256 hash of your raw save bytes (before base64 encoding) and put the hex string in `Checksum`. The server uses it to detect data corruption and resolve sync conflicts. If the field is empty, the save will be rejected. In C++ you can use `FMD5` or a platform SHA-256 function; in Blueprint you may need a small utility function or plugin.

---

### 8. Voiding Installs

**What it is:** Sometimes an install record gets created but shouldn't count — a duplicate, a fraudulent entry, or a user who uninstalled immediately. You can mark it as void so it's excluded from your analytics.

**Blueprint — Void Install Record:**

```
[Void Install Record]
  InstallUuid: "uuid-of-the-install-record"   ← the UUID from the /installs response
  bVoid:       true                            ← false to restore it later
  On Complete → [Branch: Success?]
```

> ℹ️ `InstallUuid` is the database UUID returned by the install endpoint — it is **not** the same as `user_install_id` (the string you pass in). You can get it from the server response body, or from `Get Install Id` on the subsystem after the first heartbeat.

---

## Project Settings Reference

Go to **Edit → Project Settings → Glitch Aegis** to find all settings.

| Setting | Default | Description |
|---|---|---|
| **Title Id** | *(empty)* | Your game's ID from the Glitch dashboard. Required. |
| **Title Token** | *(empty)* | Secret auth token from the Technical Integration page. Required. Keep private. |
| **Enable Automatic Heartbeat** | `true` | Auto-starts DRM validation, install registration, and heartbeat on launch. |
| **Heartbeat Interval Seconds** | `30` | How often (in seconds) to ping the retention endpoint. Range: 10–120. |
| **Game Version** | *(empty)* | Your current build version, e.g. `1.2.3`. Recommended for retention tracking. |
| **Referral Source** | *(empty)* | How players find your game: `advertising`, `social_media`, `organic`, etc. |

---

## Blueprint Reference

All functions are in the **Glitch** category in the Blueprint function library.

| Node | Category | Description |
|---|---|---|
| **Validate License Async** | Glitch\|Security | Async DRM check. Fires delegate with `bIsValid` and `UserName`. Use this. |
| ~~Validate License~~ | Glitch\|Security | *Deprecated.* Use Validate License Async instead. |
| **Record Game Event** | Glitch\|Telemetry | Send a single Step/Action telemetry event. |
| **Record Game Events (Bulk)** | Glitch\|Telemetry | Send an array of events in one request. Recommended for mobile. |
| **Record Purchase** | Glitch\|Revenue | Log a purchase or revenue event. |
| **Save To Cloud** | Glitch\|CloudSave | Write a save slot to Glitch's servers. |
| **Void Install Record** | Glitch\|Install | Mark an install as invalid (or restore it). |
| **Send Fingerprinted Install** | Glitch\|Fingerprinting | Manually send install + hardware fingerprint. |

**On the Subsystem** (`GlitchAegisSubsystem` via Get Game Instance Subsystem):

| Node | Description |
|---|---|
| **Start Heartbeat Loop** | Manually starts the payout heartbeat (use if auto-start is disabled). |
| **Get Install Id** | Returns the current session's install UUID. |
| **On Drm Validated** *(event)* | Broadcasts after the automatic startup DRM check completes. |

---

## C++ Reference

If you're comfortable with C++, you can call the SDK layer directly for more control.

```cpp
#include "GlitchSDK.h"

// Register an install with full attribution data
GlitchSDK::FInstallData Data;
Data.UserInstallId = TEXT("my-unique-player-id");
Data.Platform      = TEXT("steam");
Data.GameVersion   = TEXT("1.2.3");
Data.UtmSource     = TEXT("facebook");
Data.UtmMedium     = TEXT("cpc");

GlitchSDK::FFingerprintComponents FP = GlitchSDK::CollectSystemFingerprint();

GlitchSDK::CreateInstall(
    TitleToken, TitleId, Data,
    GlitchSDK::FOnGlitchResponse::CreateLambda([](bool bSuccess, const FString& Body)
    {
        // handle response
    }),
    &FP  // pass nullptr to skip fingerprint
);

// Record a purchase
GlitchSDK::FPurchaseData Purchase;
Purchase.GameInstallID  = TEXT("install-uuid-here");
Purchase.PurchaseType   = TEXT("in_app");
Purchase.PurchaseAmount = 4.99f;
Purchase.Currency       = TEXT("USD");
Purchase.TransactionID  = TEXT("receipt-abc-123");

GlitchSDK::RecordPurchase(TitleToken, TitleId, Purchase, Callback);

// Void a fraudulent install
GlitchSDK::VoidInstall(TitleToken, TitleId, TEXT("install-uuid"), true, Callback);

// Send bulk behavioral events
TArray<GlitchSDK::FGameEventData> Events;
// ... populate events ...
GlitchSDK::RecordEventsBulk(TitleToken, TitleId, Events, Callback);
```

All functions are async and non-blocking. Callbacks fire on the game thread.

---

## File Structure

```
Plugins/
└── GlitchAegis/
    ├── GlitchAegis.uplugin           ← Plugin descriptor (engine version, module list)
    └── Source/
        └── GlitchAegis/
            ├── GlitchAegis.Build.cs  ← Module dependencies (Http, etc.)
            ├── Public/               ← Headers (accessible to your game code)
            │   ├── GlitchSDK.h           Core API functions and data structs
            │   ├── GlitchAegisSettings.h Project Settings panel definition
            │   ├── GlitchAegisSubsystem.h Auto-start subsystem + OnDrmValidated event
            │   └── GlitchAegisLibrary.h  Blueprint function library
            └── Private/              ← Implementation (internal)
                ├── GlitchSDK.cpp         HTTP layer, JSON serialization, fingerprinting
                ├── GlitchAegisSubsystem.cpp  Startup logic, heartbeat timer
                └── GlitchAegisLibrary.cpp    Blueprint node implementations
```

---

## FAQ

**My game isn't on the Glitch platform yet — can I still test the plugin?**
Yes. Open your game normally (outside the Glitch launcher) and the plugin will silently do nothing. It only activates when launched with a `-install_id=UUID` command-line argument, which the Glitch launcher injects automatically.

**What happens if the player goes offline?**
Glitch allows a 24-hour offline grace period. If the heartbeat can't reach the server, let the player keep playing. After 24 hours, the system requires an online check. You can detect this in the `OnDrmValidated` callback firing `false` after a timeout.

**Why does `ValidateLicense` (the old node) exist if it's deprecated?**
It's kept so existing Blueprint graphs don't break. It fires the HTTP request but the response is discarded, so a 403 "denied" response won't stop anyone from playing. Migrate to **Validate License Async** to actually gate access.

**How is "idle time" handled?**
The Glitch web wrapper monitors mouse and keyboard activity in the browser. If no input is detected for 5 minutes, your heartbeats are flagged as `is_idle: true` server-side and no payout is credited for those minutes. Your plugin doesn't need to handle this — the web layer does it automatically.

**What is the difference between `user_install_id` and the install UUID?**
`user_install_id` is a string *you* choose to identify a player (e.g. a Steam ID or your own account ID). The install UUID is a database record ID *Glitch generates* and returns in the `/installs` response. The `VoidInstall` function needs the UUID; the heartbeat needs the `user_install_id`.

**Can I use this for Steam, Epic, or downloadable builds (not just web)?**
Yes. The same heartbeat endpoint works for any platform. Set `Platform` to `"steam"` or `"windows"` in your `FInstallData` and bundle your Title Token with the executable.

---

## Support

- 💬 [Discord](https://discord.gg/RPYU9KgEmU) — fastest response
- 📖 [Glitch Developer Docs](https://glitch.fun)
- 🐛 [GitHub Issues](https://github.com/Glitch-Gaming-Platform/Glitch-Unreal-Plugin/issues)