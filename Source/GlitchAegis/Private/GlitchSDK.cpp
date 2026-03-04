#include "GlitchSDK.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// Platform-specific includes for fingerprinting (used only in CollectSystemFingerprint)
#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <windows.h>
	#include <intrin.h>
	#include "Windows/HideWindowsPlatformTypes.h"
#elif PLATFORM_MAC
	#include <sys/utsname.h>
	#include <sys/sysctl.h>
#elif PLATFORM_LINUX
	#include <sys/utsname.h>
#endif

namespace GlitchSDK
{
	static const FString BaseUrl = TEXT("https://api.glitch.fun/api");

	// =========================================================================
	// Internal helpers
	// =========================================================================

	namespace Internal
	{
		FString EscapeJSON(const FString& Input)
		{
			// Unreal's built-in JSON serializer handles escaping properly.
			// This wrapper is for raw string-building paths that don't use TJsonWriter.
			FString Out;
			Out.Reserve(Input.Len());
			for (TCHAR Ch : Input)
			{
				switch (Ch)
				{
				case TEXT('"'):  Out += TEXT("\\\""); break;
				case TEXT('\\'): Out += TEXT("\\\\"); break;
				case TEXT('\n'): Out += TEXT("\\n");  break;
				case TEXT('\r'): Out += TEXT("\\r");  break;
				case TEXT('\t'): Out += TEXT("\\t");  break;
				default:         Out += Ch;           break;
				}
			}
			return Out;
		}

		/**
		 * Fire-and-forget async POST with JSON body.
		 * The delegate is called on the game thread when the response arrives.
		 */
		void PostJSON(const FString& Url, const FString& Token, const FString& Body, FOnGlitchResponse OnComplete)
		{
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(Url);
			Request->SetVerb(TEXT("POST"));
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));
			Request->SetContentAsString(Body);

			Request->OnProcessRequestComplete().BindLambda(
				[OnComplete](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
				{
					if (bConnected && Response.IsValid())
					{
						const bool bOk = EHttpResponseCodes::IsOk(Response->GetResponseCode());
						OnComplete.ExecuteIfBound(bOk, Response->GetContentAsString());
					}
					else
					{
						OnComplete.ExecuteIfBound(false, TEXT("HTTP request failed: no response"));
					}
				});

			Request->ProcessRequest();
		}

		/** Async GET helper */
		void GetRequest(const FString& Url, const FString& Token, FOnGlitchResponse OnComplete)
		{
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(Url);
			Request->SetVerb(TEXT("GET"));
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Token));

			Request->OnProcessRequestComplete().BindLambda(
				[OnComplete](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
				{
					if (bConnected && Response.IsValid())
					{
						const bool bOk = EHttpResponseCodes::IsOk(Response->GetResponseCode());
						OnComplete.ExecuteIfBound(bOk, Response->GetContentAsString());
					}
					else
					{
						OnComplete.ExecuteIfBound(false, TEXT("HTTP request failed: no response"));
					}
				});

			Request->ProcessRequest();
		}
	} // namespace Internal

	// =========================================================================
	// Core API
	// =========================================================================

	void ValidateInstall(const FString& TitleToken, const FString& TitleId, const FString& InstallId, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/validate"), *BaseUrl, *TitleId, *InstallId);
		Internal::PostJSON(Url, TitleToken, TEXT("{}"), OnComplete);
	}

	void SendHeartbeat(const FString& TitleToken, const FString& TitleId, const FString& InstallId, const FString& AnalyticsSessionId, FOnGlitchResponse OnComplete)
	{
		// Heartbeat has its own dedicated endpoint and payload — separate from installs.
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/heartbeat"), *BaseUrl, *TitleId, *InstallId);

		const FString Body = FString::Printf(
			TEXT("{\"user_install_id\":\"%s\",\"analytics_session_id\":\"%s\",\"platform\":\"pc\"}"),
			*Internal::EscapeJSON(InstallId),
			*Internal::EscapeJSON(AnalyticsSessionId)
		);

		Internal::PostJSON(Url, TitleToken, Body, OnComplete);
	}

	void CreateInstallRecord(const FString& AuthToken, const FString& TitleId, const FString& UserInstallId, const FString& Platform, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs"), *BaseUrl, *TitleId);

		const FString Body = FString::Printf(
			TEXT("{\"user_install_id\":\"%s\",\"platform\":\"%s\"}"),
			*Internal::EscapeJSON(UserInstallId),
			*Internal::EscapeJSON(Platform)
		);

		Internal::PostJSON(Url, AuthToken, Body, OnComplete);
	}

	void CreateInstallRecordWithFingerprint(
		const FString& AuthToken,
		const FString& TitleId,
		const FString& UserInstallId,
		const FString& Platform,
		const FFingerprintComponents& Fingerprint,
		const FString& GameVersion,
		const FString& ReferralSource,
		FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs"), *BaseUrl, *TitleId);

		FString Body = FString::Printf(
			TEXT("{\"user_install_id\":\"%s\",\"platform\":\"%s\""),
			*Internal::EscapeJSON(UserInstallId),
			*Internal::EscapeJSON(Platform)
		);

		if (!GameVersion.IsEmpty())
		{
			Body += FString::Printf(TEXT(",\"game_version\":\"%s\""), *Internal::EscapeJSON(GameVersion));
		}
		if (!ReferralSource.IsEmpty())
		{
			Body += FString::Printf(TEXT(",\"referral_source\":\"%s\""), *Internal::EscapeJSON(ReferralSource));
		}

		Body += FString::Printf(TEXT(",\"fingerprint_components\":%s}"), *FingerprintToJSON(Fingerprint));

		Internal::PostJSON(Url, AuthToken, Body, OnComplete);
	}

	void RecordPurchase(const FString& AuthToken, const FString& TitleId, const FPurchaseData& Purchase, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/purchases"), *BaseUrl, *TitleId);
		Internal::PostJSON(Url, AuthToken, PurchaseToJSON(Purchase), OnComplete);
	}

	// =========================================================================
	// Cloud Save
	// =========================================================================

	void ListSaves(const FString& TitleToken, const FString& TitleId, const FString& InstallId, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/saves"), *BaseUrl, *TitleId, *InstallId);
		Internal::GetRequest(Url, TitleToken, OnComplete);
	}

	void StoreSave(const FString& TitleToken, const FString& TitleId, const FString& InstallId, const FGameSaveData& SaveData, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/saves"), *BaseUrl, *TitleId, *InstallId);

		// Build body into a local FString (fixes the dangling pointer bug from original)
		FString Body = FString::Printf(
			TEXT("{\"slot_index\":%d,\"payload\":\"%s\",\"checksum\":\"%s\",\"base_version\":%d,\"save_type\":\"%s\",\"client_timestamp\":\"%s\""),
			SaveData.SlotIndex,
			*Internal::EscapeJSON(SaveData.PayloadBase64),
			*Internal::EscapeJSON(SaveData.Checksum),
			SaveData.BaseVersion,
			*Internal::EscapeJSON(SaveData.SaveType),
			*Internal::EscapeJSON(SaveData.ClientTimestamp)
		);

		if (!SaveData.MetadataJSON.IsEmpty())
		{
			Body += FString::Printf(TEXT(",\"metadata\":%s"), *SaveData.MetadataJSON);
		}
		Body += TEXT("}");

		Internal::PostJSON(Url, TitleToken, Body, OnComplete);
	}

	void ResolveSaveConflict(
		const FString& TitleToken,
		const FString& TitleId,
		const FString& InstallId,
		const FString& SaveId,
		const FString& ConflictId,
		const FString& Choice,
		FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/saves/%s/resolve"), *BaseUrl, *TitleId, *InstallId, *SaveId);

		const FString Body = FString::Printf(
			TEXT("{\"conflict_id\":\"%s\",\"choice\":\"%s\"}"),
			*Internal::EscapeJSON(ConflictId),
			*Internal::EscapeJSON(Choice)
		);

		Internal::PostJSON(Url, TitleToken, Body, OnComplete);
	}

	// =========================================================================
	// Behavioral Telemetry
	// =========================================================================

	void RecordEvent(const FString& TitleToken, const FString& TitleId, const FGameEventData& Event, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/events"), *BaseUrl, *TitleId);

		FString Body = FString::Printf(
			TEXT("{\"game_install_id\":\"%s\",\"step_key\":\"%s\",\"action_key\":\"%s\""),
			*Internal::EscapeJSON(Event.GameInstallID),
			*Internal::EscapeJSON(Event.StepKey),
			*Internal::EscapeJSON(Event.ActionKey)
		);

		if (!Event.MetadataJSON.IsEmpty())
		{
			Body += FString::Printf(TEXT(",\"metadata\":%s"), *Event.MetadataJSON);
		}
		Body += TEXT("}");

		Internal::PostJSON(Url, TitleToken, Body, OnComplete);
	}

	void RecordEventsBulk(const FString& TitleToken, const FString& TitleId, const TArray<FGameEventData>& Events, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/events/bulk"), *BaseUrl, *TitleId);

		FString EventsArray;
		for (int32 i = 0; i < Events.Num(); ++i)
		{
			const FGameEventData& Ev = Events[i];
			FString EventJson = FString::Printf(
				TEXT("{\"game_install_id\":\"%s\",\"step_key\":\"%s\",\"action_key\":\"%s\""),
				*Internal::EscapeJSON(Ev.GameInstallID),
				*Internal::EscapeJSON(Ev.StepKey),
				*Internal::EscapeJSON(Ev.ActionKey)
			);
			if (!Ev.MetadataJSON.IsEmpty())
			{
				EventJson += FString::Printf(TEXT(",\"metadata\":%s"), *Ev.MetadataJSON);
			}
			EventJson += TEXT("}");

			EventsArray += EventJson;
			if (i < Events.Num() - 1)
			{
				EventsArray += TEXT(",");
			}
		}

		const FString Body = FString::Printf(TEXT("{\"events\":[%s]}"), *EventsArray);
		Internal::PostJSON(Url, TitleToken, Body, OnComplete);
	}

	// =========================================================================
	// Wishlist Intelligence
	// =========================================================================

	void ToggleWishlist(const FString& UserJwt, const FString& TitleId, const FString& FingerprintId, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/wishlist"), *BaseUrl, *TitleId);

		FString Body = TEXT("{");
		if (!FingerprintId.IsEmpty())
		{
			Body += FString::Printf(TEXT("\"fingerprint_id\":\"%s\""), *Internal::EscapeJSON(FingerprintId));
		}
		Body += TEXT("}");

		Internal::PostJSON(Url, UserJwt, Body, OnComplete);
	}

	void UpdateWishlistScore(const FString& UserJwt, const FString& TitleId, int32 Score, FOnGlitchResponse OnComplete)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/wishlist/score"), *BaseUrl, *TitleId);
		const FString Body = FString::Printf(TEXT("{\"score\":%d}"), Score);
		Internal::PostJSON(Url, UserJwt, Body, OnComplete);
	}

	// =========================================================================
	// Fingerprinting
	// =========================================================================

	FFingerprintComponents CollectSystemFingerprint()
	{
		FFingerprintComponents FP;

#if PLATFORM_WINDOWS
		FP.OSName = TEXT("Windows");
		FP.DeviceType = TEXT("desktop");
		FP.FormFactors.Add(TEXT("Desktop"));
		FP.Bitness = TEXT("64");
		FP.Architecture = TEXT("x86_64");

		// OS version — use RtlGetVersion to avoid deprecated GetVersionEx
		{
			typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
			HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
			if (hNtdll)
			{
				auto pfn = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
				if (pfn)
				{
					RTL_OSVERSIONINFOW osInfo = {};
					osInfo.dwOSVersionInfoSize = sizeof(osInfo);
					if (pfn(&osInfo) == 0)
					{
						FP.OSVersion = FString::Printf(TEXT("%lu.%lu.%lu"),
							osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber);
						FP.PlatformVersion = FP.OSVersion;
					}
				}
			}
		}

		// CPU model via CPUID
		{
			int cpuInfo[4] = {};
			__cpuid(cpuInfo, 0x80000000);
			if ((unsigned)cpuInfo[0] >= 0x80000004u)
			{
				char cpuBrand[49] = {};
				__cpuid((int*)(cpuBrand),      0x80000002);
				__cpuid((int*)(cpuBrand + 16), 0x80000003);
				__cpuid((int*)(cpuBrand + 32), 0x80000004);
				FP.CPUModel = UTF8_TO_TCHAR(cpuBrand);
				FP.CPUModel.TrimStartAndEndInline();
			}
		}

		// RAM
		{
			MEMORYSTATUSEX statex = {};
			statex.dwLength = sizeof(statex);
			if (GlobalMemoryStatusEx(&statex))
			{
				FP.MemoryMB = static_cast<int32>(statex.ullTotalPhys / (1024 * 1024));
			}
		}

		// Display resolution
		{
			int w = GetSystemMetrics(SM_CXSCREEN);
			int h = GetSystemMetrics(SM_CYSCREEN);
			if (w > 0 && h > 0)
			{
				FP.DisplayResolution = FString::Printf(TEXT("%dx%d"), w, h);
			}
		}

#elif PLATFORM_MAC
		FP.OSName = TEXT("MacOS");
		FP.DeviceType = TEXT("desktop");
		FP.FormFactors.Add(TEXT("Desktop"));

		struct utsname buf;
		if (uname(&buf) == 0)
		{
			FP.OSVersion = UTF8_TO_TCHAR(buf.release);
		}

		{
			size_t size = 0;
			sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0);
			if (size > 0)
			{
				TArray<char> cpuBuf;
				cpuBuf.SetNum(size);
				sysctlbyname("machdep.cpu.brand_string", cpuBuf.GetData(), &size, nullptr, 0);
				FP.CPUModel = UTF8_TO_TCHAR(cpuBuf.GetData());
			}
		}

		{
			int cores = 0;
			size_t sz = sizeof(cores);
			if (sysctlbyname("hw.physicalcpu", &cores, &sz, nullptr, 0) == 0)
			{
				FP.CPUCores = cores;
			}
		}

		{
			int64_t memSize = 0;
			size_t sz = sizeof(memSize);
			if (sysctlbyname("hw.memsize", &memSize, &sz, nullptr, 0) == 0)
			{
				FP.MemoryMB = static_cast<int32>(memSize / (1024 * 1024));
			}
		}

#elif PLATFORM_LINUX
		FP.OSName = TEXT("Linux");
		FP.DeviceType = TEXT("desktop");
		FP.FormFactors.Add(TEXT("Desktop"));

		{
			struct utsname buf;
			if (uname(&buf) == 0)
			{
				FP.OSVersion = UTF8_TO_TCHAR(buf.release);
			}
		}

		// CPU model from /proc/cpuinfo
		{
			TUniquePtr<FILE, TDefaultDelete<FILE>> f(fopen("/proc/cpuinfo", "r"));
			if (f)
			{
				char line[256];
				while (fgets(line, sizeof(line), f.Get()))
				{
					if (FCStringAnsi::Strncmp(line, "model name", 10) == 0)
					{
						const char* colon = FCStringAnsi::Strchr(line, ':');
						if (colon)
						{
							FP.CPUModel = UTF8_TO_TCHAR(colon + 2);
							FP.CPUModel.TrimStartAndEndInline();
						}
						break;
					}
				}
			}
		}

		// RAM from /proc/meminfo
		{
			TUniquePtr<FILE, TDefaultDelete<FILE>> f(fopen("/proc/meminfo", "r"));
			if (f)
			{
				char line[256];
				while (fgets(line, sizeof(line), f.Get()))
				{
					if (FCStringAnsi::Strncmp(line, "MemTotal:", 9) == 0)
					{
						int64 memKB = 0;
						sscanf(line, "MemTotal: %lld kB", &memKB);
						FP.MemoryMB = static_cast<int32>(memKB / 1024);
						break;
					}
				}
			}
		}
#endif

		if (FP.DeviceType.IsEmpty())  FP.DeviceType  = TEXT("desktop");
		if (FP.Language.IsEmpty())    FP.Language    = TEXT("en-US");

		return FP;
	}

	// =========================================================================
	// JSON serialization helpers
	// =========================================================================

	FString FingerprintToJSON(const FFingerprintComponents& FP)
	{
		// Using manual building — each section uses a bool-first pattern to
		// avoid the seekp(-1) undefined behaviour that was in the original code.
		auto AppendField = [](FString& Out, bool& bFirst, const FString& Key, const FString& Value)
		{
			if (!bFirst) Out += TEXT(",");
			Out += FString::Printf(TEXT("\"%s\":\"%s\""), *Key, *Value);
			bFirst = false;
		};
		auto AppendInt = [](FString& Out, bool& bFirst, const FString& Key, int32 Value)
		{
			if (!bFirst) Out += TEXT(",");
			Out += FString::Printf(TEXT("\"%s\":%d"), *Key, Value);
			bFirst = false;
		};

		FString Out = TEXT("{");

		// device
		{
			FString Section;
			bool bF = true;
			if (!FP.DeviceModel.IsEmpty())        AppendField(Section, bF, TEXT("model"),        Internal::EscapeJSON(FP.DeviceModel));
			if (!FP.DeviceType.IsEmpty())         AppendField(Section, bF, TEXT("type"),         Internal::EscapeJSON(FP.DeviceType));
			if (!FP.DeviceManufacturer.IsEmpty()) AppendField(Section, bF, TEXT("manufacturer"), Internal::EscapeJSON(FP.DeviceManufacturer));
			Out += FString::Printf(TEXT("\"device\":{%s}"), *Section);
		}

		// os
		{
			FString Section;
			bool bF = true;
			if (!FP.OSName.IsEmpty())    AppendField(Section, bF, TEXT("name"),    Internal::EscapeJSON(FP.OSName));
			if (!FP.OSVersion.IsEmpty()) AppendField(Section, bF, TEXT("version"), Internal::EscapeJSON(FP.OSVersion));
			Out += FString::Printf(TEXT(",\"os\":{%s}"), *Section);
		}

		// display
		if (!FP.DisplayResolution.IsEmpty() || FP.DisplayDensity > 0)
		{
			FString Section;
			bool bF = true;
			if (!FP.DisplayResolution.IsEmpty()) AppendField(Section, bF, TEXT("resolution"), Internal::EscapeJSON(FP.DisplayResolution));
			if (FP.DisplayDensity > 0)           AppendInt  (Section, bF, TEXT("density"),    FP.DisplayDensity);
			Out += FString::Printf(TEXT(",\"display\":{%s}"), *Section);
		}

		// hardware
		{
			FString Section;
			bool bF = true;
			if (!FP.CPUModel.IsEmpty()) AppendField(Section, bF, TEXT("cpu"),    Internal::EscapeJSON(FP.CPUModel));
			if (FP.CPUCores > 0)        AppendInt  (Section, bF, TEXT("cores"),  FP.CPUCores);
			if (!FP.GPUModel.IsEmpty()) AppendField(Section, bF, TEXT("gpu"),    Internal::EscapeJSON(FP.GPUModel));
			if (FP.MemoryMB > 0)        AppendInt  (Section, bF, TEXT("memory"), FP.MemoryMB);
			Out += FString::Printf(TEXT(",\"hardware\":{%s}"), *Section);
		}

		// environment
		{
			FString Section;
			bool bF = true;
			if (!FP.Language.IsEmpty()) AppendField(Section, bF, TEXT("language"), Internal::EscapeJSON(FP.Language));
			if (!FP.Timezone.IsEmpty()) AppendField(Section, bF, TEXT("timezone"), Internal::EscapeJSON(FP.Timezone));
			if (!FP.Region.IsEmpty())   AppendField(Section, bF, TEXT("region"),   Internal::EscapeJSON(FP.Region));
			Out += FString::Printf(TEXT(",\"environment\":{%s}"), *Section);
		}

		// desktop_data
		if (FP.FormFactors.Num() > 0 || !FP.Architecture.IsEmpty())
		{
			FString Section;
			bool bF = true;

			if (FP.FormFactors.Num() > 0)
			{
				FString ArrayStr = TEXT("[");
				for (int32 i = 0; i < FP.FormFactors.Num(); ++i)
				{
					if (i > 0) ArrayStr += TEXT(",");
					ArrayStr += FString::Printf(TEXT("\"%s\""), *Internal::EscapeJSON(FP.FormFactors[i]));
				}
				ArrayStr += TEXT("]");
				if (!bF) Section += TEXT(",");
				Section += FString::Printf(TEXT("\"formFactors\":%s"), *ArrayStr);
				bF = false;
			}

			if (!FP.Architecture.IsEmpty())    AppendField(Section, bF, TEXT("architecture"),   Internal::EscapeJSON(FP.Architecture));
			if (!FP.Bitness.IsEmpty())         AppendField(Section, bF, TEXT("bitness"),        Internal::EscapeJSON(FP.Bitness));
			if (!FP.PlatformVersion.IsEmpty()) AppendField(Section, bF, TEXT("platformVersion"),Internal::EscapeJSON(FP.PlatformVersion));
			if (!bF) Section += TEXT(",");
			Section += FString::Printf(TEXT("\"wow64\":%s"), FP.bIsWow64 ? TEXT("true") : TEXT("false"));

			Out += FString::Printf(TEXT(",\"desktop_data\":{%s}"), *Section);
		}

		// keyboard_layout
		if (FP.KeyboardLayout.Num() > 0)
		{
			FString Section;
			bool bF = true;
			for (const auto& Pair : FP.KeyboardLayout)
			{
				AppendField(Section, bF, Internal::EscapeJSON(Pair.Key), Internal::EscapeJSON(Pair.Value));
			}
			Out += FString::Printf(TEXT(",\"keyboard_layout\":{%s}"), *Section);
		}

		// identifiers
		if (!FP.AdvertisingID.IsEmpty())
		{
			Out += FString::Printf(TEXT(",\"identifiers\":{\"advertising_id\":\"%s\"}"),
				*Internal::EscapeJSON(FP.AdvertisingID));
		}

		Out += TEXT("}");
		return Out;
	}

	FString PurchaseToJSON(const FPurchaseData& P)
	{
		FString Out = FString::Printf(TEXT("{\"game_install_id\":\"%s\""), *Internal::EscapeJSON(P.GameInstallID));

		if (!P.PurchaseType.IsEmpty())  Out += FString::Printf(TEXT(",\"purchase_type\":\"%s\""),  *Internal::EscapeJSON(P.PurchaseType));
		if (P.PurchaseAmount > 0.f)     Out += FString::Printf(TEXT(",\"purchase_amount\":%f"),    P.PurchaseAmount);
		if (!P.Currency.IsEmpty())      Out += FString::Printf(TEXT(",\"currency\":\"%s\""),       *Internal::EscapeJSON(P.Currency));
		if (!P.TransactionID.IsEmpty()) Out += FString::Printf(TEXT(",\"transaction_id\":\"%s\""), *Internal::EscapeJSON(P.TransactionID));
		if (!P.ItemSKU.IsEmpty())       Out += FString::Printf(TEXT(",\"item_sku\":\"%s\""),       *Internal::EscapeJSON(P.ItemSKU));
		if (!P.ItemName.IsEmpty())      Out += FString::Printf(TEXT(",\"item_name\":\"%s\""),      *Internal::EscapeJSON(P.ItemName));
		if (P.Quantity > 0)             Out += FString::Printf(TEXT(",\"quantity\":%d"),           P.Quantity);
		if (!P.MetadataJSON.IsEmpty())  Out += FString::Printf(TEXT(",\"metadata\":%s"),           *P.MetadataJSON);

		Out += TEXT("}");
		return Out;
	}

	void CreateInstall(
		const FString& AuthToken,
		const FString& TitleId,
		const FInstallData& D,
		FOnGlitchResponse OnComplete,
		const FFingerprintComponents* Fingerprint)
	{
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs"), *BaseUrl, *TitleId);

		FString Body = FString::Printf(
			TEXT("{\"user_install_id\":\"%s\""),
			*Internal::EscapeJSON(D.UserInstallId)
		);

		if (!D.Platform.IsEmpty())
			Body += FString::Printf(TEXT(",\"platform\":\"%s\""), *Internal::EscapeJSON(D.Platform));
		if (!D.DeviceType.IsEmpty())
			Body += FString::Printf(TEXT(",\"device_type\":\"%s\""), *Internal::EscapeJSON(D.DeviceType));
		if (!D.GameVersion.IsEmpty())
			Body += FString::Printf(TEXT(",\"game_version\":\"%s\""), *Internal::EscapeJSON(D.GameVersion));

		if (!D.ReferralSource.IsEmpty())
			Body += FString::Printf(TEXT(",\"referral_source\":\"%s\""), *Internal::EscapeJSON(D.ReferralSource));

		// Optional UTM fields if your backend accepts them (safe to omit if empty)
		if (!D.UtmSource.IsEmpty())   Body += FString::Printf(TEXT(",\"utm_source\":\"%s\""), *Internal::EscapeJSON(D.UtmSource));
		if (!D.UtmMedium.IsEmpty())   Body += FString::Printf(TEXT(",\"utm_medium\":\"%s\""), *Internal::EscapeJSON(D.UtmMedium));
		if (!D.UtmCampaign.IsEmpty()) Body += FString::Printf(TEXT(",\"utm_campaign\":\"%s\""), *Internal::EscapeJSON(D.UtmCampaign));
		if (!D.UtmContent.IsEmpty())  Body += FString::Printf(TEXT(",\"utm_content\":\"%s\""), *Internal::EscapeJSON(D.UtmContent));
		if (!D.UtmTerm.IsEmpty())     Body += FString::Printf(TEXT(",\"utm_term\":\"%s\""), *Internal::EscapeJSON(D.UtmTerm));

		if (Fingerprint)
		{
			Body += FString::Printf(TEXT(",\"fingerprint_components\":%s"), *FingerprintToJSON(*Fingerprint));
		}

		Body += TEXT("}");

		Internal::PostJSON(Url, AuthToken, Body, OnComplete);
	}

	void VoidInstall(
		const FString& AuthToken,
		const FString& TitleId,
		const FString& InstallUuid,
		bool bVoid,
		FOnGlitchResponse OnComplete)
	{
		// If your backend uses a different route/body, update this to match.
		const FString Url = FString::Printf(TEXT("%s/titles/%s/installs/%s/void"), *BaseUrl, *TitleId, *InstallUuid);
		const FString Body = FString::Printf(TEXT("{\"void\":%s}"), bVoid ? TEXT("true") : TEXT("false"));
		Internal::PostJSON(Url, AuthToken, Body, OnComplete);
	}

} // namespace GlitchSDK