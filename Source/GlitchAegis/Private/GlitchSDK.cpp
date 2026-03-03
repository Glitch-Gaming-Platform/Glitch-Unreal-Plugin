#include "GlitchSDK.h"
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <iostream>

// Platform-specific includes for fingerprinting
#ifdef _WIN32
    #include <windows.h>
    #include <intrin.h>
    #include <VersionHelpers.h>
#elif __APPLE__
    #include <sys/utsname.h>
    #include <sys/sysctl.h>
#elif __linux__
    #include <sys/utsname.h>
    #include <fstream>
#endif

namespace GlitchSDK 
{
    // Internal helper implementations
    namespace Internal 
    {
        size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) 
        {
            ((std::string*)userdata)->append((char*)ptr, size * nmemb);
            return size * nmemb;
        }

        std::string EscapeJSON(const std::string& input) 
        {
            std::string escaped;
            for (char c : input) {
                switch (c) {
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped += c; break;
                }
            }
            return escaped;
        }

        std::string GetSystemInfo(const std::string& key) 
        {
            #ifdef _WIN32
                if (key == "os_name") return "Windows";
                if (key == "device_type") return "desktop";
                
                if (key == "os_version") {
                    OSVERSIONINFOEX osInfo = {};
                    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
                    if (GetVersionEx((OSVERSIONINFO*)&osInfo)) {
                        std::stringstream ss;
                        ss << osInfo.dwMajorVersion << "." << osInfo.dwMinorVersion << "." << osInfo.dwBuildNumber;
                        return ss.str();
                    }
                    return "10.0"; // Fallback
                }
                
                if (key == "architecture") {
                    SYSTEM_INFO sysInfo;
                    GetSystemInfo(&sysInfo);
                    return (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "x86" : "unknown";
                }
                
            #elif __APPLE__
                if (key == "os_name") return "MacOS";
                if (key == "device_type") return "desktop";
                
                if (key == "os_version") {
                    struct utsname buffer;
                    if (uname(&buffer) == 0) {
                        return std::string(buffer.release);
                    }
                    return "unknown";
                }
                
            #elif __linux__
                if (key == "os_name") return "Linux";
                if (key == "device_type") return "desktop";
                
                if (key == "os_version") {
                    struct utsname buffer;
                    if (uname(&buffer) == 0) {
                        return std::string(buffer.release);
                    }
                    return "unknown";
                }
            #endif
            
            return "unknown";
        }
    }

    std::string CreateInstallRecord(const std::string& authToken, const std::string& titleId, 
                                  const std::string& userInstallId, const std::string& platform)
    {
        CURL* curl = curl_easy_init();
        if (!curl) return "Failed to init curl";

        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/installs";

        // Simple JSON payload for basic install
        std::string jsonBody = R"({"user_install_id":")" + userInstallId + 
                             R"(","platform":")" + platform + R"("})";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + authToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());

        std::string responseString;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseString = "CURL error: " + std::string(curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return responseString;
    }

    std::string CreateInstallRecordWithFingerprint(const std::string& authToken, const std::string& titleId,
                                                 const std::string& userInstallId, const std::string& platform,
                                                 const FingerprintComponents& fingerprint,
                                                 const std::string& gameVersion,
                                                 const std::string& referralSource)
    {
        CURL* curl = curl_easy_init();
        if (!curl) return "Failed to init curl";

        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/installs";

        // Build JSON payload with fingerprint data
        std::stringstream jsonPayload;
        jsonPayload << R"({)";
        jsonPayload << R"("user_install_id":")" << Internal::EscapeJSON(userInstallId) << R"(",)";
        jsonPayload << R"("platform":")" << Internal::EscapeJSON(platform) << R"(",)";
        
        if (!gameVersion.empty()) {
            jsonPayload << R"("game_version":")" << Internal::EscapeJSON(gameVersion) << R"(",)";
        }
        
        if (!referralSource.empty()) {
            jsonPayload << R"("referral_source":")" << Internal::EscapeJSON(referralSource) << R"(",)";
        }

        // Add fingerprint components
        jsonPayload << R"("fingerprint_components":)" << FingerprintToJSON(fingerprint);
        jsonPayload << R"(})";

        std::string jsonBody = jsonPayload.str();

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + authToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());

        std::string responseString;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseString = "CURL error: " + std::string(curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return responseString;
    }

    std::string RecordPurchase(const std::string& authToken, const std::string& titleId, 
                              const PurchaseData& purchaseData)
    {
        CURL* curl = curl_easy_init();
        if (!curl) return "Failed to init curl";

        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/purchases";
        std::string jsonBody = PurchaseToJSON(purchaseData);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + authToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());

        std::string responseString;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            responseString = "CURL error: " + std::string(curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return responseString;
    }

    FingerprintComponents CollectSystemFingerprint() 
    {
        FingerprintComponents fingerprint;

        // Basic system info
        fingerprint.OSName = Internal::GetSystemInfo("os_name");
        fingerprint.OSVersion = Internal::GetSystemInfo("os_version");
        fingerprint.DeviceType = Internal::GetSystemInfo("device_type");
        fingerprint.Architecture = Internal::GetSystemInfo("architecture");

        #ifdef _WIN32
            // Windows-specific fingerprinting
            fingerprint.FormFactors = {"Desktop"};
            fingerprint.Bitness = "64"; // Assume 64-bit for modern systems
            fingerprint.PlatformVersion = Internal::GetSystemInfo("os_version");

            // Get CPU info
            int cpuInfo[4];
            __cpuid(cpuInfo, 0x80000000);
            if (cpuInfo[0] >= 0x80000004) {
                char cpuString[49] = {0};
                __cpuid((int*)(cpuString), 0x80000002);
                __cpuid((int*)(cpuString + 16), 0x80000003);
                __cpuid((int*)(cpuString + 32), 0x80000004);
                fingerprint.CPUModel = std::string(cpuString);
            }

            // Get memory info
            MEMORYSTATUSEX statex;
            statex.dwLength = sizeof(statex);
            if (GlobalMemoryStatusEx(&statex)) {
                fingerprint.MemoryMB = static_cast<int>(statex.ullTotalPhys / (1024 * 1024));
            }

            // Get display resolution
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);
            if (screenWidth > 0 && screenHeight > 0) {
                std::stringstream ss;
                ss << screenWidth << "x" << screenHeight;
                fingerprint.DisplayResolution = ss.str();
            }

        #elif __APPLE__
            // macOS-specific fingerprinting
            fingerprint.FormFactors = {"Desktop"};
            
            size_t size;
            // Get CPU model
            size = 0;
            sysctlbyname("machdep.cpu.brand_string", NULL, &size, NULL, 0);
            if (size > 0) {
                char* cpuModel = new char[size];
                sysctlbyname("machdep.cpu.brand_string", cpuModel, &size, NULL, 0);
                fingerprint.CPUModel = std::string(cpuModel);
                delete[] cpuModel;
            }

            // Get core count
            int cores;
            size = sizeof(cores);
            if (sysctlbyname("hw.physicalcpu", &cores, &size, NULL, 0) == 0) {
                fingerprint.CPUCores = cores;
            }

            // Get memory
            int64_t memSize;
            size = sizeof(memSize);
            if (sysctlbyname("hw.memsize", &memSize, &size, NULL, 0) == 0) {
                fingerprint.MemoryMB = static_cast<int>(memSize / (1024 * 1024));
            }

        #elif __linux__
            // Linux-specific fingerprinting
            fingerprint.FormFactors = {"Desktop"};
            
            // Read CPU info from /proc/cpuinfo
            std::ifstream cpuinfo("/proc/cpuinfo");
            if (cpuinfo.is_open()) {
                std::string line;
                while (std::getline(cpuinfo, line)) {
                    if (line.find("model name") != std::string::npos) {
                        size_t pos = line.find(": ");
                        if (pos != std::string::npos) {
                            fingerprint.CPUModel = line.substr(pos + 2);
                            break;
                        }
                    }
                }
                cpuinfo.close();
            }

            // Read memory info from /proc/meminfo
            std::ifstream meminfo("/proc/meminfo");
            if (meminfo.is_open()) {
                std::string line;
                while (std::getline(meminfo, line)) {
                    if (line.find("MemTotal:") == 0) {
                        std::stringstream ss(line);
                        std::string label;
                        int memKB;
                        ss >> label >> memKB;
                        fingerprint.MemoryMB = memKB / 1024;
                        break;
                    }
                }
                meminfo.close();
            }
        #endif

        // Set some defaults
        if (fingerprint.DeviceType.empty()) fingerprint.DeviceType = "desktop";
        if (fingerprint.Language.empty()) fingerprint.Language = "en-US";
        
        return fingerprint;
    }

    std::map<std::string, std::string> GenerateKeyboardLayout() 
    {
        std::map<std::string, std::string> layout;
        
        // Canonical key list (must match the documentation)
        std::vector<std::string> canonicalKeys = {
            "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO", "KeyP",
            "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
            "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM",
            "Backquote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", 
            "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
            "BracketLeft", "BracketRight", "Backslash", "Semicolon", "Quote", 
            "Comma", "Period", "Slash"
        };

        #ifdef _WIN32
            // Windows keyboard layout detection
            for (const std::string& keyCode : canonicalKeys) {
                UINT vkCode = 0;
                
                // Map key codes to VK codes (simplified mapping)
                if (keyCode == "KeyQ") vkCode = 'Q';
                else if (keyCode == "KeyW") vkCode = 'W';
                else if (keyCode == "KeyE") vkCode = 'E';
                // ... (continue for all keys, or use a lookup table)
                else if (keyCode == "Digit1") vkCode = '1';
                else if (keyCode == "Semicolon") vkCode = VK_OEM_1;
                
                if (vkCode != 0) {
                    UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
                    char keyChar[2] = {0};
                    if (GetKeyNameTextA(scanCode << 16, keyChar, sizeof(keyChar))) {
                        layout[keyCode] = std::string(1, std::tolower(keyChar[0]));
                    } else {
                        layout[keyCode] = "?";
                    }
                }
            }
        #else
            // For non-Windows platforms, provide a basic QWERTY layout as fallback
            // In a real implementation, you'd use platform-specific APIs
            std::map<std::string, std::string> qwertyLayout = {
                {"KeyQ", "q"}, {"KeyW", "w"}, {"KeyE", "e"}, {"KeyR", "r"}, {"KeyT", "t"},
                {"KeyY", "y"}, {"KeyU", "u"}, {"KeyI", "i"}, {"KeyO", "o"}, {"KeyP", "p"},
                {"KeyA", "a"}, {"KeyS", "s"}, {"KeyD", "d"}, {"KeyF", "f"}, {"KeyG", "g"},
                {"KeyH", "h"}, {"KeyJ", "j"}, {"KeyK", "k"}, {"KeyL", "l"},
                {"KeyZ", "z"}, {"KeyX", "x"}, {"KeyC", "c"}, {"KeyV", "v"}, {"KeyB", "b"},
                {"KeyN", "n"}, {"KeyM", "m"}, {"Digit1", "1"}, {"Digit2", "2"}, 
                {"Digit3", "3"}, {"Digit4", "4"}, {"Digit5", "5"}, {"Digit6", "6"},
                {"Digit7", "7"}, {"Digit8", "8"}, {"Digit9", "9"}, {"Digit0", "0"},
                {"Minus", "-"}, {"Equal", "="}, {"BracketLeft", "["}, {"BracketRight", "]"},
                {"Backslash", "\\"}, {"Semicolon", ";"}, {"Quote", "'"}, {"Comma", ","},
                {"Period", "."}, {"Slash", "/"}, {"Backquote", "`"}
            };
            layout = qwertyLayout;
        #endif
        
        return layout;
    }

    std::string FingerprintToJSON(const FingerprintComponents& fingerprint) 
    {
        std::stringstream json;
        json << "{";
        
        // Device section
        json << R"("device":{)";
        if (!fingerprint.DeviceModel.empty()) {
            json << R"("model":")" << Internal::EscapeJSON(fingerprint.DeviceModel) << R"(",)";
        }
        if (!fingerprint.DeviceType.empty()) {
            json << R"("type":")" << Internal::EscapeJSON(fingerprint.DeviceType) << R"(",)";
        }
        if (!fingerprint.DeviceManufacturer.empty()) {
            json << R"("manufacturer":")" << Internal::EscapeJSON(fingerprint.DeviceManufacturer) << R"(",)";
        }
        json.seekp(-1, std::ios_base::cur); // Remove trailing comma
        json << "},";
        
        // OS section
        json << R"("os":{)";
        if (!fingerprint.OSName.empty()) {
            json << R"("name":")" << Internal::EscapeJSON(fingerprint.OSName) << R"(",)";
        }
        if (!fingerprint.OSVersion.empty()) {
            json << R"("version":")" << Internal::EscapeJSON(fingerprint.OSVersion) << R"(",)";
        }
        json.seekp(-1, std::ios_base::cur); // Remove trailing comma
        json << "},";
        
        // Display section
        if (!fingerprint.DisplayResolution.empty() || fingerprint.DisplayDensity > 0) {
            json << R"("display":{)";
            if (!fingerprint.DisplayResolution.empty()) {
                json << R"("resolution":")" << Internal::EscapeJSON(fingerprint.DisplayResolution) << R"(",)";
            }
            if (fingerprint.DisplayDensity > 0) {
                json << R"("density":)" << fingerprint.DisplayDensity << ",";
            }
            json.seekp(-1, std::ios_base::cur); // Remove trailing comma
            json << "},";
        }
        
        // Hardware section
        json << R"("hardware":{)";
        if (!fingerprint.CPUModel.empty()) {
            json << R"("cpu":")" << Internal::EscapeJSON(fingerprint.CPUModel) << R"(",)";
        }
        if (fingerprint.CPUCores > 0) {
            json << R"("cores":)" << fingerprint.CPUCores << ",";
        }
        if (!fingerprint.GPUModel.empty()) {
            json << R"("gpu":")" << Internal::EscapeJSON(fingerprint.GPUModel) << R"(",)";
        }
        if (fingerprint.MemoryMB > 0) {
            json << R"("memory":)" << fingerprint.MemoryMB << ",";
        }
        json.seekp(-1, std::ios_base::cur); // Remove trailing comma
        json << "},";
        
        // Environment section
        json << R"("environment":{)";
        if (!fingerprint.Language.empty()) {
            json << R"("language":")" << Internal::EscapeJSON(fingerprint.Language) << R"(",)";
        }
        if (!fingerprint.Timezone.empty()) {
            json << R"("timezone":")" << Internal::EscapeJSON(fingerprint.Timezone) << R"(",)";
        }
        if (!fingerprint.Region.empty()) {
            json << R"("region":")" << Internal::EscapeJSON(fingerprint.Region) << R"(",)";
        }
        json.seekp(-1, std::ios_base::cur); // Remove trailing comma
        json << "},";
        
        // Desktop data section (for PC platforms)
        if (!fingerprint.FormFactors.empty() || !fingerprint.Architecture.empty()) {
            json << R"("desktop_data":{)";
            if (!fingerprint.FormFactors.empty()) {
                json << R"("formFactors":[)";
                for (size_t i = 0; i < fingerprint.FormFactors.size(); ++i) {
                    json << R"(")" << Internal::EscapeJSON(fingerprint.FormFactors[i]) << R"(")";
                    if (i < fingerprint.FormFactors.size() - 1) json << ",";
                }
                json << "],";
            }
            if (!fingerprint.Architecture.empty()) {
                json << R"("architecture":")" << Internal::EscapeJSON(fingerprint.Architecture) << R"(",)";
            }
            if (!fingerprint.Bitness.empty()) {
                json << R"("bitness":")" << Internal::EscapeJSON(fingerprint.Bitness) << R"(",)";
            }
            if (!fingerprint.PlatformVersion.empty()) {
                json << R"("platformVersion":")" << Internal::EscapeJSON(fingerprint.PlatformVersion) << R"(",)";
            }
            json << R"("wow64":)" << (fingerprint.IsWow64 ? "true" : "false");
            json << "},";
        }
        
        // Keyboard layout section
        if (!fingerprint.KeyboardLayout.empty()) {
            json << R"("keyboard_layout":{)";
            bool first = true;
            for (const auto& pair : fingerprint.KeyboardLayout) {
                if (!first) json << ",";
                json << R"(")" << Internal::EscapeJSON(pair.first) << R"(":")" 
                     << Internal::EscapeJSON(pair.second) << R"(")";
                first = false;
            }
            json << "},";
        }
        
        // Identifiers section
        if (!fingerprint.AdvertisingID.empty()) {
            json << R"("identifiers":{)";
            json << R"("advertising_id":")" << Internal::EscapeJSON(fingerprint.AdvertisingID) << R"(")";
            json << "},";
        }
        
        // Remove trailing comma and close
        json.seekp(-1, std::ios_base::cur);
        json << "}";
        
        return json.str();
    }

    std::string PurchaseToJSON(const PurchaseData& purchase) 
    {
        std::stringstream json;
        json << "{";
        
        // Required field
        json << R"("game_install_id":")" << Internal::EscapeJSON(purchase.GameInstallID) << R"(")";
        
        // Optional fields
        if (!purchase.PurchaseType.empty()) {
            json << R"(,"purchase_type":")" << Internal::EscapeJSON(purchase.PurchaseType) << R"(")";
        }
        
        if (purchase.PurchaseAmount > 0.0f) {
            json << R"(,"purchase_amount":)" << purchase.PurchaseAmount;
        }
        
        if (!purchase.Currency.empty()) {
            json << R"(,"currency":")" << Internal::EscapeJSON(purchase.Currency) << R"(")";
        }
        
        if (!purchase.TransactionID.empty()) {
            json << R"(,"transaction_id":")" << Internal::EscapeJSON(purchase.TransactionID) << R"(")";
        }
        
        if (!purchase.ItemSKU.empty()) {
            json << R"(,"item_sku":")" << Internal::EscapeJSON(purchase.ItemSKU) << R"(")";
        }
        
        if (!purchase.ItemName.empty()) {
            json << R"(,"item_name":")" << Internal::EscapeJSON(purchase.ItemName) << R"(")";
        }
        
        if (purchase.Quantity > 0) {
            json << R"(,"quantity":)" << purchase.Quantity;
        }
        
        if (!purchase.MetadataJSON.empty()) {
            // Assume MetadataJSON is already valid JSON
            json << R"(,"metadata":)" << purchase.MetadataJSON;
        }
        
        json << "}";
        return json.str();
    }

    std::string SendHeartbeat(const std::string& titleToken, const std::string& titleId, const std::string& installId, const std::string& analyticsSessionId)
    {
        std::stringstream json;
        json << "{"
             << R"("user_install_id":")" << Internal::EscapeJSON(installId) << R"(",)"
             << R"("analytics_session_id":")" << Internal::EscapeJSON(analyticsSessionId) << R"(",)"
             << R"("platform":"pc")"
             << "}";
        
        // Re-uses the logic from CreateInstallRecord but with the updated payload
        return CreateInstallRecordWithFingerprint(titleToken, titleId, installId, "pc", CollectSystemFingerprint());
    }

    std::string ValidateInstall(const std::string& titleToken, const std::string& titleId, const std::string& installId)
    {
        CURL* curl = curl_easy_init();
        std::string responseString;
        if (!curl) return "Failed to init curl";

        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/installs/" + installId + "/validate";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "Authorization: Bearer " + titleToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}"); // Empty body for POST
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return responseString;
    }

    // --- 2. Aegis Cloud Save ---

    std::string ListSaves(const std::string& titleToken, const std::string& titleId, const std::string& installId)
    {
        CURL* curl = curl_easy_init();
        std::string responseString;
        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/installs/" + installId + "/saves";

        struct curl_slist *headers = NULL;
        std::string authHeader = "Authorization: Bearer " + titleToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return responseString;
    }

    std::string StoreSave(const std::string& titleToken, const std::string& titleId, const std::string& installId, const GameSaveData& saveData)
    {
        CURL* curl = curl_easy_init();
        std::string responseString;
        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/installs/" + installId + "/saves";

        std::stringstream json;
        json << "{"
             << R"("slot_index":)" << saveData.SlotIndex << ","
             << R"("payload":")" << saveData.PayloadBase64 << R"(",)"
             << R"("checksum":")" << saveData.Checksum << R"(",)"
             << R"("base_version":)" << saveData.BaseVersion << ","
             << R"("save_type":")" << saveData.SaveType << R"(",)"
             << R"("client_timestamp":")" << saveData.ClientTimestamp << R"(")";
        if(!saveData.MetadataJSON.empty()) json << R"(,"metadata":)" << saveData.MetadataJSON;
        json << "}";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + titleToken).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return responseString;
    }

    // --- 3. Behavioral Telemetry ---

    std::string RecordEvent(const std::string& titleToken, const std::string& titleId, const GameEventData& event)
    {
        CURL* curl = curl_easy_init();
        std::string responseString;
        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/events";

        std::stringstream json;
        json << "{"
             << R"("game_install_id":")" << event.GameInstallID << R"(",)"
             << R"("step_key":")" << Internal::EscapeJSON(event.StepKey) << R"(",)"
             << R"("action_key":")" << Internal::EscapeJSON(event.ActionKey) << R"(")";
        if(!event.MetadataJSON.empty()) json << R"(,"metadata":)" << event.MetadataJSON;
        json << "}";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + titleToken).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return responseString;
    }

    // --- 4. Wishlist Intelligence ---

    std::string ToggleWishlist(const std::string& userJwt, const std::string& titleId, const std::string& fingerprintId)
    {
        CURL* curl = curl_easy_init();
        std::string responseString;
        std::string url = "https://api.glitch.fun/api/titles/" + titleId + "/wishlist";

        std::stringstream json;
        json << "{";
        if(!fingerprintId.empty()) json << R"("fingerprint_id":")" << fingerprintId << R"(")";
        json << "}";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + userJwt).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return responseString;
    }

} // namespace GlitchSDK