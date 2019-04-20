#pragma once

#include "Utils.h"
#include <NTLib.h>
#include <map>

namespace System
{
// =================

    class Handle : private std::shared_ptr<void>
    {
    public:
        typedef void(*DestroyObjectRoutine)(HANDLE object);

    private:
        static void ObjectDeleter(HANDLE object);

    public:
        Handle();
        Handle(HANDLE object, DestroyObjectRoutine destroyer = &ObjectDeleter);

        bool IsValid();

        HANDLE GetNativeHandle();

    protected:

        void SetHandle(HANDLE object, DestroyObjectRoutine destroyer = &ObjectDeleter);
    };

// =================

    class Process : public Handle
    {
    private:
        DWORD  _processId;

        template<typename T>
        void ReadMemoryToContainer(void* address, T& buffer, size_t size);

        static void WithoutRelease(HANDLE object);

    public:
        Process(DWORD processId, DWORD access = PROCESS_ALL_ACCESS);
        Process(HANDLE process);

        DWORD GetProcessID();

        void ReadMemory(void* address, std::string& buffer, size_t size);
        void ReadMemory(void* address, std::wstring& buffer, size_t size);
        void WriteMemory(void* address, std::string& buffer, bool unprotect = true);
        void WriteMemory(void* address, std::wstring& buffer, bool unprotect = true);
    };

    typedef std::shared_ptr<Process> ProcessPtr;

// =================

    class ProcessesSnapshot : protected Handle
    {
    private:
        bool _fromStart;

    public:
        ProcessesSnapshot();

        bool GetNextProcess(DWORD& processId);
        bool GetNextProcess(DWORD& processId, std::wstring& name);
        void ResetWalking();
    };

// =================

    class ModulesSnapshot : protected Handle
    {
    private:
        bool _fromStart;

    public:
        ModulesSnapshot(DWORD processId);

        bool GetNextModule(HMODULE& module);
        bool GetNextModule(HMODULE& module, std::wstring& name);
        void ResetWalking();
    };

// =================

    class ProcessEnvironment;
    typedef std::shared_ptr<ProcessEnvironment> ProcessEnvironmentPtr;

    class ProcessEnvironmentBlock;
    typedef std::shared_ptr<ProcessEnvironmentBlock> ProcessEnvironmentBlockPtr;

// =================

    class ProcessInformation
    {
    private:
        
        ProcessPtr _process;

        PPEB                       _pebAddress;
        ProcessEnvironmentBlockPtr _peb;

    public:
        ProcessInformation(DWORD processId);

        ProcessPtr GetProcess();

        PPEB GetPEBAddress();
        ProcessEnvironmentBlockPtr GetProcessEnvironmentBlock();

        void GetImagePath(std::wstring& path);
        void GetImageDirectory(std::wstring& directory);

        void GetModulePath(HMODULE module, std::wstring& path);
    };

// =================

    class ProcessEnvironmentBlock
    {
    private:
        ProcessPtr  _process;
        
        std::string _pebBuffer;
        PPEB        _peb;

        std::string                  _paramsBuffer;
        PRTL_USER_PROCESS_PARAMETERS _params;
        std::wstring                 _paramsEnv;

        std::wstring                 _currentDirectory;

        void LoadProcessParameters();

    public:
        ProcessEnvironmentBlock(ProcessInformation& processInfo);

        ProcessEnvironmentPtr GetProcessEnvironment();

        void GetCurrentDir(std::wstring& directory);
    };

// =================

    class ProcessEnvironment
    {
    private:
        std::map<std::wstring, std::wstring> _variables;

    public:
        ProcessEnvironment(std::wstring& environment);

        bool GetValue(const wchar_t* key, std::wstring& output);
    };

// =================

    enum class IntegrityLevel
    {
        Untrusted,
        //BelowLow, ???
        Low,
        //MediumLow, ???
        Medium,
        MediumPlus,
        High,
        System,
        Protected,
        Secure
    };

    class TokenBase : public Handle
    {
    protected:
        TokenBase() {}

    public:
        void SetPrivilege(wchar_t* privelege, bool enable);

        IntegrityLevel GetIntegrityLevel();
        void SetIntegrityLevel(IntegrityLevel level);

        HANDLE GetLinkedToken();
        void SetLinkedToken(HANDLE token);

        void GetUserNameString(std::wstring& userName);
        void GetUserSIDString(std::wstring& sid);

        bool IsElevated();

    private:
        PSID AllocateSidByIntegrityLevel(IntegrityLevel level);
    };

    class PrimaryToken : public TokenBase
    {
    public:
        PrimaryToken(Process& source, DWORD access = TOKEN_ALL_ACCESS, bool duplicate = false);
        PrimaryToken(HANDLE token, bool duplicate = false);
    };

    typedef std::shared_ptr<PrimaryToken> PrimaryTokenPtr;

    class ImpersonateToken : public TokenBase
    {
    public:
        ImpersonateToken(Process& source, DWORD access = TOKEN_ALL_ACCESS);
        ImpersonateToken(HANDLE token, bool duplicate = false);
    };

    typedef std::shared_ptr<ImpersonateToken> ImpersonateTokenPtr;

    class SecurityDescriptor
    {
    private:
        PSECURITY_DESCRIPTOR _descriptor;
        PACL _dacl;
        PSID _owner;
        PSID _group;

    public:
        SecurityDescriptor(Handle& file);
        ~SecurityDescriptor();

        PSECURITY_DESCRIPTOR GetNativeSecurityDescriptor();
    };

    class TokenAccessChecker
    {
    private:
        ImpersonateToken _token;

    public:
        TokenAccessChecker(Process& process);
        TokenAccessChecker(ImpersonateToken& token);
        
        bool IsFileObjectAccessible(SecurityDescriptor& descriptor, DWORD desiredAccess);
    };

// =================

    class File : public Handle
    {
    public:
        File(const wchar_t* path, DWORD access = READ_CONTROL, DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);
    };

    class ImageMapping : public Handle
    {
    private:
        void*  _mapping;
        size_t _mappingSize;

    public:
        ImageMapping(const wchar_t* path);
        ~ImageMapping();

        void* GetAddress();
        size_t GetSize();
    };

// =================

    class Directory : public Handle
    {
    public:
        Directory(const wchar_t* path, DWORD access = READ_CONTROL, DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);

        static bool IsDirectory(const wchar_t* path);
    };

// =================

    class SystemInformation
    {
    public:
        static std::wstring GetSystem32Dir();
        static std::wstring GetSystemDir();
        static std::wstring GetWindowsDir();
    };

// =================

    enum class BaseKeys
    {
        Root,
        CurrentConfig,
        CurrentUser,
        LocalMachine
    };

    class RegistryKey
    {
    private:
        HKEY _hkey;

    public:
        RegistryKey(BaseKeys base, const wchar_t* key, DWORD access = KEY_QUERY_VALUE);
        ~RegistryKey();

        HKEY GetNativeHKEY();

    private:
        HKEY ConvertBaseToHKEY(BaseKeys base);
    };

    class RegistryValue
    {
    private:
        DWORD        _type;
        std::wstring _value;

    public:
        RegistryValue(RegistryKey& key, const wchar_t* value);

        DWORD GetType() const;
        const std::wstring& GetValue() const;
    };

    typedef std::map<std::wstring, RegistryValue> RegistryValues;

    class EnumRegistryValues
    {
    private:
        RegistryValues _values;

    public:
        EnumRegistryValues(BaseKeys base, const wchar_t* key);

        const RegistryValues GetValues();
    };

};
