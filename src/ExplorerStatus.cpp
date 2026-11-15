#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "gdi32.lib")

static HMODULE g_hModule = nullptr;
static volatile LONG g_cDllRef = 0;

static const wchar_t* CLSID_ContextMenu = L"{9D0D2A91-6A51-4A67-9A75-7C7D5F9B1A01}";
static const wchar_t* CLSID_Working     = L"{C8C8F8F0-2A7B-4B3D-A2C7-8E7F7F4E2B02}";
static const wchar_t* CLSID_Done        = L"{C8C8F8F0-2A7B-4B3D-A2C7-8E7F7F4E2B03}";
static const wchar_t* CLSID_Waiting     = L"{C8C8F8F0-2A7B-4B3D-A2C7-8E7F7F4E2B04}";
static const wchar_t* CLSID_Problem     = L"{C8C8F8F0-2A7B-4B3D-A2C7-8E7F7F4E2B05}";

enum class Status { None, Working, Done, Waiting, Problem };

static std::wstring DllPath() {
    wchar_t p[MAX_PATH]{};
    GetModuleFileNameW(g_hModule, p, MAX_PATH);
    return p;
}


/* ============================================================
   Kontextmenü-Icons
   Diese Funktion verwendet weiterhin ausschließlich:
       menu_working.ico
       menu_done.ico
       menu_waiting.ico
       menu_problem.ico
   ============================================================ */

static HBITMAP LoadMenuBitmap(const wchar_t* iconName) {
    std::wstring p = DllPath();
    const size_t slash = p.find_last_of(L"\\/");

    if (slash == std::wstring::npos)
        return nullptr;

    p.resize(slash + 1);
    p += L"icons\\";
    p += iconName;

    HICON hIcon = static_cast<HICON>(
        LoadImageW(
            nullptr,
            p.c_str(),
            IMAGE_ICON,
            16,
            16,
            LR_LOADFROMFILE
        )
    );

    if (!hIcon)
        return nullptr;

    ICONINFO ii{};
    HBITMAP hBitmap = nullptr;

    if (GetIconInfo(hIcon, &ii)) {
        hBitmap = ii.hbmColor;

        if (ii.hbmMask)
            DeleteObject(ii.hbmMask);
    }

    DestroyIcon(hIcon);

    return hBitmap;
}


static std::wstring DataDir() {
    std::wstring p = DllPath();
    const auto pos = p.find_last_of(L"\\/");

    if (pos != std::wstring::npos)
        p.resize(pos);

    return p + L"\\data";
}


static std::wstring StatusFile() {
    return DataDir() + L"\\status.json";
}


static void EnsureDataDir() {
    CreateDirectoryW(DataDir().c_str(), nullptr);
}


static std::wstring NormalizePath(const std::wstring& p) {
    wchar_t full[MAX_PATH]{};

    DWORD n = GetFullPathNameW(
        p.c_str(),
        MAX_PATH,
        full,
        nullptr
    );

    std::wstring s =
        (n && n < MAX_PATH)
        ? std::wstring(full)
        : p;

    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        towlower
    );

    return s;
}


static std::wstring JsonEscape(const std::wstring& s) {
    std::wstring r;

    for (wchar_t c : s) {
        if (c == L'\\')
            r += L"\\\\";
        else if (c == L'"')
            r += L"\\\"";
        else if (c == L'\r')
            r += L"\\r";
        else if (c == L'\n')
            r += L"\\n";
        else if (c == L'\t')
            r += L"\\t";
        else
            r += c;
    }

    return r;
}


static std::wstring StatusName(Status s) {
    switch (s) {
    case Status::Working:
        return L"working";

    case Status::Done:
        return L"done";

    case Status::Waiting:
        return L"waiting";

    case Status::Problem:
        return L"problem";

    default:
        return L"";
    }
}


static Status ParseStatus(const std::wstring& s) {
    if (s == L"working")
        return Status::Working;

    if (s == L"done")
        return Status::Done;

    if (s == L"waiting")
        return Status::Waiting;

    if (s == L"problem")
        return Status::Problem;

    return Status::None;
}


static std::wstring ReadUtf8(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);

    if (!f)
        return L"";

    std::string b(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>()
    );

    if (b.empty())
        return L"";

    int n = MultiByteToWideChar(
        CP_UTF8,
        0,
        b.data(),
        (int)b.size(),
        nullptr,
        0
    );

    if (!n)
        return L"";

    std::wstring w(n, L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        b.data(),
        (int)b.size(),
        w.data(),
        n
    );

    return w;
}


static bool WriteUtf8(
    const std::wstring& path,
    const std::wstring& text
) {
    int n = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        (int)text.size(),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    std::string b(n, '\0');

    if (n) {
        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            (int)text.size(),
            b.data(),
            n,
            nullptr,
            nullptr
        );
    }

    std::ofstream f(
        path,
        std::ios::binary | std::ios::trunc
    );

    if (!f)
        return false;

    f.write(
        b.data(),
        (std::streamsize)b.size()
    );

    return f.good();
}


static std::vector<std::pair<std::wstring, Status>> LoadStatuses() {
    std::vector<std::pair<std::wstring, Status>> out;

    const std::wstring j = ReadUtf8(StatusFile());

    size_t p = 0;

    while (true) {
        p = j.find(L'"', p);

        if (p == std::wstring::npos)
            break;

        ++p;

        bool esc = false;
        std::wstring key;

        while (p < j.size()) {
            wchar_t c = j[p++];

            if (esc) {
                key += c;
                esc = false;
                continue;
            }

            if (c == L'\\') {
                esc = true;
                continue;
            }

            if (c == L'"')
                break;

            key += c;
        }

        size_t colon = j.find(L':', p);

        if (colon == std::wstring::npos)
            break;

        size_t q1 = j.find(L'"', colon);

        if (q1 == std::wstring::npos)
            break;

        size_t q2 = q1 + 1;

        while (
            q2 < j.size() &&
            !(j[q2] == L'"' && j[q2 - 1] != L'\\')
        ) {
            ++q2;
        }

        if (q2 >= j.size())
            break;

        out.push_back({
            key,
            ParseStatus(
                j.substr(
                    q1 + 1,
                    q2 - q1 - 1
                )
            )
        });

        p = q2 + 1;
    }

    return out;
}


static Status GetStatus(const std::wstring& path) {
    const std::wstring n = NormalizePath(path);

    for (const auto& x : LoadStatuses()) {
        if (NormalizePath(x.first) == n)
            return x.second;
    }

    return Status::None;
}


static bool SetStatus(
    const std::wstring& path,
    Status status
) {
    EnsureDataDir();

    const std::wstring n = NormalizePath(path);

    auto items = LoadStatuses();

    std::vector<std::pair<std::wstring, Status>> out;

    bool found = false;

    for (const auto& x : items) {
        if (NormalizePath(x.first) == n) {
            found = true;

            if (status != Status::None)
                out.push_back({ n, status });
        }
        else {
            out.push_back(x);
        }
    }

    if (!found && status != Status::None)
        out.push_back({ n, status });

    std::wstringstream ss;

    ss << L"{\n";

    for (size_t i = 0; i < out.size(); ++i) {
        ss
            << L"  \""
            << JsonEscape(out[i].first)
            << L"\": \""
            << StatusName(out[i].second)
            << L"\"";

        if (i + 1 < out.size())
            ss << L",";

        ss << L"\n";
    }

    ss << L"}\n";

    const std::wstring tmp =
        StatusFile() + L".tmp";

    if (!WriteUtf8(tmp, ss.str()))
        return false;

    if (!MoveFileExW(
        tmp.c_str(),
        StatusFile().c_str(),
        MOVEFILE_REPLACE_EXISTING
    )) {
        DeleteFileW(tmp.c_str());
        return false;
    }

    return true;
}


static void NotifyExplorer(
    const std::vector<std::wstring>& paths
) {
    for (const auto& p : paths) {
        SHChangeNotify(
            SHCNE_UPDATEITEM,
            SHCNF_PATHW,
            p.c_str(),
            nullptr
        );
    }
}


// Extract selected files/folders from Explorer's IDataObject.
static std::vector<std::wstring> ExtractPaths(
    IDataObject* data
) {
    std::vector<std::wstring> paths;

    if (!data)
        return paths;

    FORMATETC fmt{
        CF_HDROP,
        nullptr,
        DVASPECT_CONTENT,
        -1,
        TYMED_HGLOBAL
    };

    STGMEDIUM stg{};

    if (FAILED(data->GetData(&fmt, &stg)))
        return paths;

    HDROP drop =
        static_cast<HDROP>(
            GlobalLock(stg.hGlobal)
        );

    if (drop) {
        UINT n = DragQueryFileW(
            drop,
            0xFFFFFFFF,
            nullptr,
            0
        );

        for (UINT i = 0; i < n; ++i) {
            wchar_t p[MAX_PATH]{};

            if (DragQueryFileW(
                drop,
                i,
                p,
                MAX_PATH
            )) {
                paths.emplace_back(p);
            }
        }

        GlobalUnlock(stg.hGlobal);
    }

    ReleaseStgMedium(&stg);

    return paths;
}


/* ============================================================
   Kontextmenü
   ============================================================ */

class ContextMenu final
    : public IContextMenu,
      public IShellExtInit {

    ULONG ref_ = 1;

    std::vector<std::wstring> paths_;

    std::vector<HBITMAP> menuBitmaps_;

public:

    ContextMenu() {
        InterlockedIncrement(&g_cDllRef);
    }


    ~ContextMenu() {
        for (HBITMAP bmp : menuBitmaps_) {
            if (bmp)
                DeleteObject(bmp);
        }

        InterlockedDecrement(&g_cDllRef);
    }


    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void** ppv
    ) override {
        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (
            riid == IID_IUnknown ||
            riid == IID_IContextMenu
        ) {
            *ppv =
                static_cast<IContextMenu*>(this);
        }
        else if (riid == IID_IShellExtInit) {
            *ppv =
                static_cast<IShellExtInit*>(this);
        }

        if (!*ppv)
            return E_NOINTERFACE;

        AddRef();

        return S_OK;
    }


    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++ref_;
    }


    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --ref_;

        if (!r)
            delete this;

        return r;
    }


    HRESULT STDMETHODCALLTYPE Initialize(
        PCIDLIST_ABSOLUTE,
        IDataObject* data,
        HKEY
    ) override {
        paths_ = ExtractPaths(data);

        return paths_.empty()
            ? E_FAIL
            : S_OK;
    }


    HRESULT STDMETHODCALLTYPE QueryContextMenu(
        HMENU menu,
        UINT index,
        UINT first,
        UINT,
        UINT flags
    ) override {

        if (flags & CMF_DEFAULTONLY)
            return MAKE_HRESULT(
                SEVERITY_SUCCESS,
                0,
                0
            );

        HMENU sub = CreatePopupMenu();

        if (!sub)
            return E_OUTOFMEMORY;

        struct MenuEntry {
            UINT id;
            const wchar_t* text;
            const wchar_t* icon;
        };


        /*
           WICHTIG:
           Das Kontextmenü verwendet weiterhin
           ausschließlich die normalen Icons.
        */

        const MenuEntry entries[] = {
            {
                first + 0,
                L"In Bearbeitung",
                L"menu_working.ico"
            },
            {
                first + 1,
                L"Erledigt",
                L"menu_done.ico"
            },
            {
                first + 2,
                L"Wartet",
                L"menu_waiting.ico"
            },
            {
                first + 3,
                L"Problem",
                L"menu_problem.ico"
            },
            {
                first + 4,
                L"Status entfernen",
                nullptr
            }
        };


        for (const auto& entry : entries) {

            MENUITEMINFO item{sizeof(item)};

            item.fMask =
                MIIM_ID |
                MIIM_STRING;

            item.wID = entry.id;

            item.dwTypeData =
                const_cast<LPWSTR>(
                    entry.text
                );


            if (entry.icon) {

                HBITMAP bmp =
                    LoadMenuBitmap(entry.icon);

                if (bmp) {

                    item.fMask |= MIIM_BITMAP;

                    item.hbmpItem = bmp;

                    menuBitmaps_.push_back(bmp);
                }
            }


            if (!InsertMenuItemW(
                sub,
                GetMenuItemCount(sub),
                TRUE,
                &item
            )) {
                return E_FAIL;
            }
        }


        MENUITEMINFO mi{sizeof(mi)};

        mi.fMask =
            MIIM_SUBMENU |
            MIIM_STRING;

        mi.hSubMenu = sub;

        mi.dwTypeData =
            const_cast<LPWSTR>(
                L"ExplorerStatus"
            );


        if (!InsertMenuItemW(
            menu,
            index,
            TRUE,
            &mi
        )) {
            DestroyMenu(sub);
            return E_FAIL;
        }


        return MAKE_HRESULT(
            SEVERITY_SUCCESS,
            0,
            5
        );
    }


    HRESULT STDMETHODCALLTYPE InvokeCommand(
        LPCMINVOKECOMMANDINFO ici
    ) override {

        if (
            !ici ||
            HIWORD(ici->lpVerb) != 0
        )
            return E_INVALIDARG;

        Status st;

        switch (LOWORD(ici->lpVerb)) {

        case 0:
            st = Status::Working;
            break;

        case 1:
            st = Status::Done;
            break;

        case 2:
            st = Status::Waiting;
            break;

        case 3:
            st = Status::Problem;
            break;

        case 4:
            st = Status::None;
            break;

        default:
            return E_INVALIDARG;
        }


        for (const auto& p : paths_)
            SetStatus(p, st);

        NotifyExplorer(paths_);

        return S_OK;
    }


    HRESULT STDMETHODCALLTYPE GetCommandString(
        UINT_PTR,
        UINT type,
        UINT*,
        LPSTR buffer,
        UINT cchMax
    ) override {

        if (!buffer || !cchMax)
            return E_INVALIDARG;

        if (type == GCS_HELPTEXTA) {
            lstrcpynA(
                buffer,
                "ExplorerStatus",
                cchMax
            );

            return S_OK;
        }

        return E_NOTIMPL;
    }
};


/* ============================================================
   ClassFactory
   ============================================================ */

class ClassFactory final
    : public IClassFactory {

    ULONG ref_ = 1;

    bool overlay_;

public:

    explicit ClassFactory(bool overlay)
        : overlay_(overlay) {

        InterlockedIncrement(&g_cDllRef);
    }


    ~ClassFactory() {
        InterlockedDecrement(&g_cDllRef);
    }


    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void** ppv
    ) override {

        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (
            riid == IID_IUnknown ||
            riid == IID_IClassFactory
        ) {
            *ppv =
                static_cast<IClassFactory*>(this);
        }

        if (!*ppv)
            return E_NOINTERFACE;

        AddRef();

        return S_OK;
    }


    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++ref_;
    }


    ULONG STDMETHODCALLTYPE Release() override {

        ULONG r = --ref_;

        if (!r)
            delete this;

        return r;
    }


    HRESULT STDMETHODCALLTYPE LockServer(
        BOOL lock
    ) override {

        if (lock)
            InterlockedIncrement(&g_cDllRef);
        else
            InterlockedDecrement(&g_cDllRef);

        return S_OK;
    }


    HRESULT STDMETHODCALLTYPE CreateInstance(
        IUnknown* outer,
        REFIID riid,
        void** ppv
    ) override {

        if (outer)
            return CLASS_E_NOAGGREGATION;

        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (overlay_)
            return E_NOINTERFACE;

        auto* obj =
            new ContextMenu();

        HRESULT hr =
            obj->QueryInterface(
                riid,
                ppv
            );

        obj->Release();

        return hr;
    }
};


/* ============================================================
   Overlay
   ============================================================ */

class Overlay final
    : public IShellIconOverlayIdentifier {

    ULONG ref_ = 1;

    Status status_;

    int iconIndex_;

public:

    Overlay(
        Status s,
        int idx
    )
        : status_(s),
          iconIndex_(idx) {

        InterlockedIncrement(&g_cDllRef);
    }


    ~Overlay() {
        InterlockedDecrement(&g_cDllRef);
    }


    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid,
        void** ppv
    ) override {

        if (!ppv)
            return E_POINTER;

        *ppv = nullptr;

        if (
            riid == IID_IUnknown ||
            riid == IID_IShellIconOverlayIdentifier
        ) {
            *ppv =
                static_cast<
                    IShellIconOverlayIdentifier*
                >(this);
        }

        if (!*ppv)
            return E_NOINTERFACE;

        AddRef();

        return S_OK;
    }


    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++ref_;
    }


    ULONG STDMETHODCALLTYPE Release() override {

        ULONG r = --ref_;

        if (!r)
            delete this;

        return r;
    }


    HRESULT STDMETHODCALLTYPE IsMemberOf(
        LPCWSTR path,
        DWORD
    ) override {

        if (!path)
            return E_INVALIDARG;

        return GetStatus(path) == status_
            ? S_OK
            : S_FALSE;
    }


    /* ========================================================
       NEU:
       Die Explorer-Overlays verwenden jetzt die vier
       separaten *_overlay.ico-Dateien.

       Kontextmenü-Icons werden hiervon NICHT beeinflusst.
       ======================================================== */

    HRESULT STDMETHODCALLTYPE GetOverlayInfo(
        LPWSTR file,
        int cch,
        int* index,
        DWORD* flags
    ) override {

        if (!file || !index || !flags)
            return E_POINTER;


        std::wstring name;


        switch (status_) {

        case Status::Working:
            name = L"working_overlay.ico";
            break;

        case Status::Done:
            name = L"done_overlay.ico";
            break;

        case Status::Waiting:
            name = L"waiting_overlay.ico";
            break;

        case Status::Problem:
            name = L"problem_overlay.ico";
            break;

        default:
            return E_FAIL;
        }


        std::wstring p = DllPath();

        const size_t slash =
            p.find_last_of(L"\\/");

        if (slash == std::wstring::npos)
            return E_FAIL;

        p.resize(slash + 1);

        p += L"icons\\";
        p += name;


        if (
            p.size() + 1 >
            static_cast<size_t>(cch)
        )
            return E_FAIL;


        lstrcpynW(
            file,
            p.c_str(),
            cch
        );


        *index = 0;

        *flags = ISIOI_ICONFILE;

        return S_OK;
    }


    HRESULT STDMETHODCALLTYPE GetPriority(
        int* priority
    ) override {

        if (!priority)
            return E_POINTER;

        *priority = 0;

        return S_OK;
    }
};


/* ============================================================
   Registrierung
   ============================================================ */

static bool ParseGuid(
    const wchar_t* s,
    CLSID& id
) {
    return SUCCEEDED(
        CLSIDFromString(s, &id)
    );
}


static std::wstring ClsidKey(
    const wchar_t* id
) {
    return
        std::wstring(
            L"Software\\Classes\\CLSID\\"
        ) + id;
}


static bool SetReg(
    HKEY root,
    const std::wstring& sub,
    const wchar_t* name,
    const std::wstring& value
) {

    HKEY h{};

    if (
        RegCreateKeyExW(
            root,
            sub.c_str(),
            0,
            nullptr,
            0,
            KEY_WRITE,
            nullptr,
            &h,
            nullptr
        ) != ERROR_SUCCESS
    )
        return false;


    LONG rc =
        RegSetValueExW(
            h,
            name,
            0,
            REG_SZ,
            reinterpret_cast<
                const BYTE*
            >(value.c_str()),
            static_cast<DWORD>(
                (value.size() + 1) *
                sizeof(wchar_t)
            )
        );


    RegCloseKey(h);

    return rc == ERROR_SUCCESS;
}


static bool DelReg(
    HKEY root,
    const std::wstring& sub
) {

    LONG rc =
        RegDeleteTreeW(
            root,
            sub.c_str()
        );

    return
        rc == ERROR_SUCCESS ||
        rc == ERROR_FILE_NOT_FOUND;
}


static bool RegisterClsid(
    const wchar_t* clsid,
    const wchar_t* desc,
    const std::wstring& dll
) {

    const std::wstring key =
        ClsidKey(clsid);

    return
        SetReg(
            HKEY_CURRENT_USER,
            key,
            nullptr,
            desc
        )
        &&
        SetReg(
            HKEY_CURRENT_USER,
            key +
                L"\\InprocServer32",
            nullptr,
            dll
        )
        &&
        SetReg(
            HKEY_CURRENT_USER,
            key +
                L"\\InprocServer32",
            L"ThreadingModel",
            L"Apartment"
        );
}


/* ============================================================
   DllRegisterServer
   ============================================================ */

extern "C"
HRESULT STDAPICALLTYPE
DllRegisterServer() {

    const std::wstring dll =
        DllPath();

    bool ok = true;


    ok &= RegisterClsid(
        CLSID_ContextMenu,
        L"ExplorerStatus Context Menu",
        dll
    );

    ok &= RegisterClsid(
        CLSID_Working,
        L"ExplorerStatus Working Overlay",
        dll
    );

    ok &= RegisterClsid(
        CLSID_Done,
        L"ExplorerStatus Done Overlay",
        dll
    );

    ok &= RegisterClsid(
        CLSID_Waiting,
        L"ExplorerStatus Waiting Overlay",
        dll
    );

    ok &= RegisterClsid(
        CLSID_Problem,
        L"ExplorerStatus Problem Overlay",
        dll
    );


    ok &= SetReg(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\ExplorerStatus",
        nullptr,
        CLSID_ContextMenu
    );


    ok &= SetReg(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\ExplorerStatus",
        nullptr,
        CLSID_ContextMenu
    );


    struct OverlayReg {
        const wchar_t* name;
        const wchar_t* clsid;
    };


    const OverlayReg overlays[] = {

        {
            L"ExplorerStatus 01 Working",
            CLSID_Working
        },

        {
            L"ExplorerStatus 02 Done",
            CLSID_Done
        },

        {
            L"ExplorerStatus 03 Waiting",
            CLSID_Waiting
        },

        {
            L"ExplorerStatus 04 Problem",
            CLSID_Problem
        }
    };


    for (const auto& ov : overlays) {

        std::wstring key =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\";

        key += ov.name;

        ok &= SetReg(
            HKEY_LOCAL_MACHINE,
            key,
            nullptr,
            ov.clsid
        );
    }


    EnsureDataDir();

    return ok
        ? S_OK
        : E_FAIL;
}


/* ============================================================
   DllUnregisterServer
   ============================================================ */

extern "C"
HRESULT STDAPICALLTYPE
DllUnregisterServer() {

    bool ok = true;


    const wchar_t* ids[] = {

        CLSID_ContextMenu,
        CLSID_Working,
        CLSID_Done,
        CLSID_Waiting,
        CLSID_Problem
    };


    for (const wchar_t* id : ids) {

        ok &= DelReg(
            HKEY_CURRENT_USER,
            ClsidKey(id)
        );
    }


    ok &= DelReg(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\ExplorerStatus"
    );


    ok &= DelReg(
        HKEY_CURRENT_USER,
        L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\ExplorerStatus"
    );


    const wchar_t* overlayNames[] = {

        L"ExplorerStatus 01 Working",
        L"ExplorerStatus 02 Done",
        L"ExplorerStatus 03 Waiting",
        L"ExplorerStatus 04 Problem"
    };


    for (const wchar_t* name : overlayNames) {

        std::wstring key =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\";

        key += name;

        ok &= DelReg(
            HKEY_LOCAL_MACHINE,
            key
        );
    }


    // Remove old per-user overlay keys
    // from previous ExplorerStatus builds.

    const wchar_t* oldIds[] = {

        CLSID_Working,
        CLSID_Done,
        CLSID_Waiting,
        CLSID_Problem
    };


    for (const wchar_t* id : oldIds) {

        std::wstring key =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\";

        key += id;

        ok &= DelReg(
            HKEY_CURRENT_USER,
            key
        );
    }


    return ok
        ? S_OK
        : E_FAIL;
}


/* ============================================================
   DllMain
   ============================================================ */

extern "C"
BOOL APIENTRY
DllMain(
    HMODULE h,
    DWORD reason,
    LPVOID
) {

    if (reason == DLL_PROCESS_ATTACH) {

        g_hModule = h;

        DisableThreadLibraryCalls(h);
    }

    return TRUE;
}


/* ============================================================
   DllCanUnloadNow
   ============================================================ */

extern "C"
HRESULT STDAPICALLTYPE
DllCanUnloadNow() {

    return
        g_cDllRef == 0
        ? S_OK
        : S_FALSE;
}


/* ============================================================
   DllGetClassObject
   ============================================================ */

extern "C"
HRESULT STDAPICALLTYPE
DllGetClassObject(
    REFCLSID rclsid,
    REFIID riid,
    LPVOID* ppv
) {

    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;


    CLSID id{};


    if (
        ParseGuid(
            CLSID_ContextMenu,
            id
        )
        &&
        IsEqualCLSID(
            rclsid,
            id
        )
    ) {

        auto* f =
            new ClassFactory(false);

        HRESULT hr =
            f->QueryInterface(
                riid,
                ppv
            );

        f->Release();

        return hr;
    }


    class OverlayFactory final
        : public IClassFactory {

        ULONG ref_ = 1;

        Status status_;

        int index_;


    public:

        OverlayFactory(
            Status s,
            int i
        )
            : status_(s),
              index_(i) {

            InterlockedIncrement(
                &g_cDllRef
            );
        }


        ~OverlayFactory() {

            InterlockedDecrement(
                &g_cDllRef
            );
        }


        HRESULT STDMETHODCALLTYPE
        QueryInterface(
            REFIID riid,
            void** ppv
        ) override {

            if (!ppv)
                return E_POINTER;

            *ppv = nullptr;


            if (
                riid == IID_IUnknown ||
                riid == IID_IClassFactory
            ) {

                *ppv =
                    static_cast<
                        IClassFactory*
                    >(this);
            }


            if (!*ppv)
                return E_NOINTERFACE;


            AddRef();

            return S_OK;
        }


        ULONG STDMETHODCALLTYPE
        AddRef() override {
            return ++ref_;
        }


        ULONG STDMETHODCALLTYPE
        Release() override {

            ULONG r = --ref_;

            if (!r)
                delete this;

            return r;
        }


        HRESULT STDMETHODCALLTYPE
        LockServer(
            BOOL lock
        ) override {

            if (lock)
                InterlockedIncrement(
                    &g_cDllRef
                );
            else
                InterlockedDecrement(
                    &g_cDllRef
                );

            return S_OK;
        }


        HRESULT STDMETHODCALLTYPE
        CreateInstance(
            IUnknown* outer,
            REFIID riid,
            void** ppv
        ) override {

            if (outer)
                return CLASS_E_NOAGGREGATION;

            if (!ppv)
                return E_POINTER;

            *ppv = nullptr;


            auto* obj =
                new Overlay(
                    status_,
                    index_
                );


            HRESULT hr =
                obj->QueryInterface(
                    riid,
                    ppv
                );


            obj->Release();

            return hr;
        }
    };


    if (
        ParseGuid(
            CLSID_Working,
            id
        )
        &&
        IsEqualCLSID(
            rclsid,
            id
        )
    ) {

        auto* f =
            new OverlayFactory(
                Status::Working,
                0
            );

        HRESULT hr =
            f->QueryInterface(
                riid,
                ppv
            );

        f->Release();

        return hr;
    }


    if (
        ParseGuid(
            CLSID_Done,
            id
        )
        &&
        IsEqualCLSID(
            rclsid,
            id
        )
    ) {

        auto* f =
            new OverlayFactory(
                Status::Done,
                0
            );

        HRESULT hr =
            f->QueryInterface(
                riid,
                ppv
            );

        f->Release();

        return hr;
    }


    if (
        ParseGuid(
            CLSID_Waiting,
            id
        )
        &&
        IsEqualCLSID(
            rclsid,
            id
        )
    ) {

        auto* f =
            new OverlayFactory(
                Status::Waiting,
                0
            );

        HRESULT hr =
            f->QueryInterface(
                riid,
                ppv
            );

        f->Release();

        return hr;
    }


    if (
        ParseGuid(
            CLSID_Problem,
            id
        )
        &&
        IsEqualCLSID(
            rclsid,
            id
        )
    ) {

        auto* f =
            new OverlayFactory(
                Status::Problem,
                0
            );

        HRESULT hr =
            f->QueryInterface(
                riid,
                ppv
            );

        f->Release();

        return hr;
    }


    return CLASS_E_CLASSNOTAVAILABLE;
}
