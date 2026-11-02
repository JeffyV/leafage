#include "killdse.h"
#include "MinHook.h"
#include <ImageHlp.h>
#include <strsafe.h>

extern GLOBALS g;
extern "C" void InitProxy();
extern "C" void UninitProxy();

HMODULE g_hModule;

LPBYTE ResolveRVA(LPBYTE pFile, DWORD dwRVA) {
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pFile + ((IMAGE_DOS_HEADER*)pFile)->e_lfanew);
    WORD wSectionCount = pNt->FileHeader.NumberOfSections;
    IMAGE_SECTION_HEADER* pSections = (IMAGE_SECTION_HEADER*)(((LPBYTE)(&pNt->OptionalHeader)) + sizeof(IMAGE_OPTIONAL_HEADER));
    for (int i = 0; i < wSectionCount; i++) {
        DWORD dwSectionRVA = pSections[i].VirtualAddress;
        DWORD cbSectionRVA = pSections[i].Misc.VirtualSize;
        if (dwRVA < dwSectionRVA || dwRVA > (dwSectionRVA + cbSectionRVA)) {
            continue;
        }
        DWORD dwSectionOffset = pSections[i].PointerToRawData;
        return pFile + (dwRVA - (dwSectionRVA - dwSectionOffset));
    }
    return NULL;
}

int PatchDriver(LPCWSTR DriverPath) {
    // PatchDriver wipes the PE security directory and rewrites the
    // BCryptVerifySignature import thunk in place, which is not reversible.
    // Keep one pristine copy before the first modification so it can be restored.
    {
        wchar_t backupPath[MAX_PATH + 16];
        if (SUCCEEDED(StringCchPrintfW(backupPath, MAX_PATH + 16, L"%s.sbiekey.bak", DriverPath)) &&
            GetFileAttributesW(backupPath) == INVALID_FILE_ATTRIBUTES)
        {
            CopyFileW(DriverPath, backupPath, FALSE);
        }
    }

    HANDLE hFile = CreateFile(DriverPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    DWORD dwSizeHigh;
    DWORD dwSize = GetFileSize(hFile, &dwSizeHigh);
    if (dwSize == 0 || dwSizeHigh != 0) {
        CloseHandle(hFile);
        return 2;
    }

    HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hFileMapping) {
        CloseHandle(hFile);
        return 3;
    }

    LPBYTE pFile = (LPBYTE)MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!pFile) {
        CloseHandle(hFileMapping);
        CloseHandle(hFile);
        return 4;
    }

    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pFile + ((IMAGE_DOS_HEADER*)pFile)->e_lfanew);
    if (pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size != 0 ||
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress != 0)
    {
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
        pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
        PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ResolveRVA(pFile, pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        int iFunc = -1;
        for (int i = 0; pImportDesc[i].OriginalFirstThunk != 0; i++) {
            LPSTR szImportLibName = (LPSTR)ResolveRVA(pFile, pImportDesc[i].Name);
            if (!_stricmp(szImportLibName, "ksecdd.sys")) {
                PDWORD_PTR pImportLookup = (PDWORD_PTR)ResolveRVA(pFile, pImportDesc[i].OriginalFirstThunk);
                for (int j = 0; pImportLookup[j] != 0; j++) {
                    if ((pImportLookup[j] & 0x8000000000000000ull) == 0) {
                        LPSTR szImportFuncName = (LPSTR)ResolveRVA(pFile, pImportLookup[j] & 0x7FFFFFFF) + 2;
                        if (!strcmp(szImportFuncName, "BCryptVerifySignature")) {
                            iFunc = j;
                        }
                    }
                }
            }
        }
        if (iFunc != -1) {
            for (int i = 0; pImportDesc[i].FirstThunk != 0; i++) {
                LPSTR szImportLibName = (LPSTR)ResolveRVA(pFile, pImportDesc[i].Name);
                if (!_stricmp(szImportLibName, "ksecdd.sys")) {
                    DWORD dwRVAImportAddressEntry = (DWORD)(((PDWORD_PTR)pImportDesc[i].FirstThunk) + iFunc);
                    WORD wSectionCount = pNt->FileHeader.NumberOfSections;
                    IMAGE_SECTION_HEADER* pSections = (IMAGE_SECTION_HEADER*)(((LPBYTE)(&pNt->OptionalHeader)) + sizeof(IMAGE_OPTIONAL_HEADER));
                    for (int i = 0; i < wSectionCount; i++) {
                        if (!memcmp(pSections[i].Name, ".text\0\0", 8)) {
                            DWORD dwSectionRVA = pSections[i].VirtualAddress;
                            DWORD dwSectionOffset = pSections[i].PointerToRawData;
                            for (LPBYTE pData = pFile + dwSectionOffset; pData < pFile + dwSectionOffset + pSections[i].SizeOfRawData; pData++) {
                                DWORD dwOffset = dwRVAImportAddressEntry - (((pData - (pFile + dwSectionOffset)) + dwSectionRVA) + 6);
                                if (pData[0] == 0xFF && pData[1] == 0x25 && *((PDWORD)(pData+2)) == dwOffset) {
                                    pData[0] = 0x31; // xor eax, eax
                                    pData[1] = 0xC0;
                                    pData[2] = 0xC3; // ret
                                }
                            }
                        }
                    }
                }
            }
        }
        DWORD oldSum;
        CheckSumMappedFile(pFile, dwSize, &oldSum, &pNt->OptionalHeader.CheckSum);
    }

    UnmapViewOfFile(pFile);
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
    return 0;
}

NTSTATUS (NTAPI* NtUnloadDriver_p)(PCUNICODE_STRING DriverServiceName);
NTSTATUS (NTAPI* NtLoadDriver_p)(PCUNICODE_STRING DriverServiceName);
NTSTATUS (NTAPI* NtLoadDriver_o)(PCUNICODE_STRING DriverServiceName);
// Driver services whose load request must be pre-patched:
//   "SbieDrv"  - official Sandboxie
//   "LukDrv"   - Lucky fork (D:\LuckyPlus)
//   "LeafDrv" - leafage/LeafManager fork (test-signed driver, same WDKTestCert)
// All load paths end up in NtLoadDriver with a registry path of the form
// \Registry\Machine\System\CurrentControlSet\Services\<name>; the ImagePath of
// the matched key is what we strip the signature from.
static const wchar_t* kTargetDriverServices[] = {
    L"SbieDrv",
    L"LukDrv",
    L"LeafDrv",
};

NTSTATUS NTAPI NtLoadDriver_h(PCUNICODE_STRING DriverServiceName) {
    const wchar_t kServicesPrefix[] =
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\";
    const size_t prefixLen = sizeof(kServicesPrefix) / sizeof(wchar_t) - 1;
    const size_t nameLen = DriverServiceName->Length / sizeof(wchar_t);

    if (DriverServiceName->Buffer != NULL &&
        nameLen > prefixLen &&
        _wcsnicmp(DriverServiceName->Buffer, kServicesPrefix, prefixLen) == 0)
    {
        const wchar_t* svc = DriverServiceName->Buffer + prefixLen;
        size_t svcLen = nameLen - prefixLen;
        // only the exact service key counts - reject ...\Services\X\Enum etc.
        bool exactServiceKey = true;
        for (size_t i = 0; i < svcLen; i++) {
            if (svc[i] == L'\\') { exactServiceKey = false; break; }
        }
        if (exactServiceKey) {
            for (size_t t = 0; t < sizeof(kTargetDriverServices) / sizeof(kTargetDriverServices[0]); t++) {
                const wchar_t* target = kTargetDriverServices[t];
                size_t targetLen = wcslen(target);
                if (svcLen == targetLen && _wcsnicmp(svc, target, targetLen) == 0) {

                    // build "SYSTEM\CurrentControlSet\Services\<svc>" and read ImagePath
                    wchar_t subKey[160];
                    if (SUCCEEDED(StringCbPrintfW(subKey, sizeof(subKey),
                            L"SYSTEM\\CurrentControlSet\\Services\\%s", target)))
                    {
                        HKEY hKey;
                        if (!RegCreateKeyEx(HKEY_LOCAL_MACHINE, subKey, NULL, NULL, 0,
                                KEY_QUERY_VALUE, NULL, &hKey, NULL))
                        {
                            DWORD cbImagePath = 0;
                            if (!RegQueryValueEx(hKey, L"ImagePath", NULL, NULL, NULL, &cbImagePath)) {
                                LPWSTR szImagePath = (LPWSTR)calloc(cbImagePath + sizeof(wchar_t), 1);
                                if (szImagePath) {
                                    if (!RegQueryValueEx(hKey, L"ImagePath", NULL, NULL,
                                            (LPBYTE)szImagePath, &cbImagePath)) {
                                        PatchDriver(szImagePath);
                                    }
                                    free(szImagePath);
                                }
                            }
                            RegCloseKey(hKey);
                        }
                    }
                    break;
                }
            }
        }
    }
    DSEAction(ThreadTaskDisableDSE);
    NTSTATUS orig = NtLoadDriver_o(DriverServiceName);
    DSEAction(ThreadTaskRestoreDSE);
    return orig;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        g_hModule = hinstDLL;
        InitProxy();
        HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
        if (!hNtdll) {
            return FALSE;
        }
        NtLoadDriver_p = (decltype(NtLoadDriver_p))GetProcAddress(hNtdll, "NtLoadDriver");
        if (!NtLoadDriver_p) {
            return FALSE;
        }
        NtUnloadDriver_p = (decltype(NtLoadDriver_p))GetProcAddress(hNtdll, "NtUnloadDriver");
        if (!NtUnloadDriver_p) {
            return FALSE;
        }
        g.NtUnloadDriver_p = NtUnloadDriver_p;
        if (MH_Initialize() != MH_OK) {
            return FALSE;
        }
        if (MH_CreateHook(NtLoadDriver_p, &NtLoadDriver_h, reinterpret_cast<LPVOID*>(&NtLoadDriver_o)) != MH_OK) {
            return FALSE;
        }
        g.NtLoadDriver_p = NtLoadDriver_o;
        if (MH_EnableHook(NtLoadDriver_p) != MH_OK) {
            return FALSE;
        }
    }
    if (fdwReason == DLL_PROCESS_DETACH) {
        if (MH_DisableHook(NtLoadDriver_p) != MH_OK) {
            return FALSE;
        }
        if (MH_Uninitialize() != MH_OK) {
            return FALSE;
        }
        UninitProxy();
    }
    return TRUE;
}
