# Glitch Aegis Unreal SDK

The No-Code solution for integrating Glitch Gaming payouts, DRM, and Cloud Saves.

## Installation
1. Download the `GlitchAegis` folder.
2. In your Unreal Project directory, create a folder named `Plugins` (if it doesn't exist).
3. Drop the `GlitchAegis` folder inside.
4. Restart Unreal Engine.

## Configuration
1. Go to **Edit > Project Settings**.
2. Scroll down to the **Plugins** section and select **Glitch Aegis**.
3. Paste your **Title ID** and **Title Token** from the Glitch Developer Dashboard.
4. Ensure **Enable Automatic Heartbeat** is checked to receive playtime payouts ($0.10/hr).

## Usage
### Payouts
Automatic. No code required. The plugin detects the session ID from the Glitch Launcher and starts the heartbeat loop.

### Security (DRM)
In your Level Blueprint or GameMode, use the **Validate License** node on `BeginPlay`. If it returns false, the user does not have a valid license (or their rental expired).

### Cloud Saves
Use the **Save To Cloud** node. 
- **Slot Index:** 0-99.
- **Payload:** Convert your SaveGame object to a Base64 string.
- The system automatically handles versioning and conflicts.

### Telemetry
Use the **Record Game Event** node to track player behavior (e.g., "Level_1", "Boss_Defeated").