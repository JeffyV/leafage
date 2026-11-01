/*
 * Copyright 2004-2020 Sandboxie Holdings, LLC 
 * Copyright 2020 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
// Sandboxie Application Programming Interface
//---------------------------------------------------------------------------


#ifndef _MY_SBIEAPI_H
#define _MY_SBIEAPI_H


#ifdef __cplusplus
extern "C" {
#endif


#ifndef SBIEAPI_EXPORT
#define SBIEAPI_EXPORT  __declspec(dllexport)
#endif


//---------------------------------------------------------------------------
// Sandboxie API Defines
//---------------------------------------------------------------------------


#include "core/drv/api_flags.h"


//---------------------------------------------------------------------------
// Sandboxie API Calls
//---------------------------------------------------------------------------

SBIEAPI_EXPORT
long CrashApi_Ioctl(ULONG64* parms);

SBIEAPI_EXPORT
LONG CrashApi_Call(ULONG api_code, LONG arg_num, ...);

SBIEAPI_EXPORT LONG CrashApi_GetVersion(
    WCHAR *version_string);         // WCHAR [16]

SBIEAPI_EXPORT LONG CrashApi_GetVersionEx(
    WCHAR* version_string,          // WCHAR [16]
    ULONG* abi_version);

/*SBIEAPI_EXPORT
LONG CrashApi_GetWork(
    ULONG SessionId,
    void *Buffer,
    ULONG *Length);*/

SBIEAPI_EXPORT 
ULONG CrashApi_GetMessage(
	ULONG* MessageNum,
	ULONG SessionId,
	ULONG *MessageId,
	ULONG *Pid,
	wchar_t *Buffer,
	ULONG Length);

SBIEAPI_EXPORT LONG CrashApi_Log(
    ULONG msgid, const WCHAR *format, ...);

SBIEAPI_EXPORT LONG CrashApi_LogEx(
    ULONG session_id, ULONG msgid, const WCHAR *format, ...);

SBIEAPI_EXPORT LONG CrashApi_vLogEx(
    ULONG session_id, ULONG msgid, const WCHAR *format, va_list va_args);

SBIEAPI_EXPORT LONG CrashApi_LogMsgEx(
	ULONG session_id, ULONG msgid, const WCHAR* msg_data, USHORT msg_len);

SBIEAPI_EXPORT LONG CrashApi_LogMsgExt(
	ULONG session_id, ULONG msgid, const WCHAR** strings);

SBIEAPI_EXPORT
LONG CrashApi_GetHomePath(
    WCHAR *NtPath, ULONG NtPathMaxLen,
    WCHAR *DosPath, ULONG DosPathMaxLen);



//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_QueryProcess(
    HANDLE ProcessId,
    WCHAR *out_box_name_wchar34,        // WCHAR [34]
    WCHAR *out_image_name_wchar96,      // WCHAR [96]
    WCHAR *out_sid_wchar96,             // WCHAR [96]
    ULONG *out_session_id);             // ULONG

SBIEAPI_EXPORT
LONG CrashApi_QueryProcessEx(
    HANDLE ProcessId,
    ULONG image_name_len_in_wchars,
    WCHAR *out_box_name_wchar34,        // WCHAR [34]
    WCHAR *out_image_name_wcharXXX,     // WCHAR [?]
    WCHAR *out_sid_wchar96,             // WCHAR [96]
    ULONG *out_session_id);             // ULONG

SBIEAPI_EXPORT
LONG CrashApi_QueryProcessEx2(
    HANDLE ProcessId,
    ULONG image_name_len_in_wchars,
    WCHAR *out_box_name_wchar34,        // WCHAR [34]
    WCHAR *out_image_name_wcharXXX,     // WCHAR [?]
    WCHAR *out_sid_wchar96,             // WCHAR [96]
    ULONG *out_session_id,              // ULONG
    ULONG64 *out_create_time);          // ULONG64

SBIEAPI_EXPORT
ULONG64 CrashApi_QueryProcessInfo(
    HANDLE ProcessId,
    ULONG info_type);

SBIEAPI_EXPORT
ULONG64 CrashApi_QueryProcessInfoEx(
    HANDLE ProcessId,
    ULONG info_type,
    ULONG64 ext_data);

SBIEAPI_EXPORT
LONG CrashApi_QueryProcessInfoStr(
    HANDLE ProcessId,
    ULONG info_type,
    WCHAR *out_str,
    ULONG *inout_str_len);

SBIEAPI_EXPORT
LONG CrashApi_QueryBoxPath(
    const WCHAR *box_name,              // WCHAR [34]
    WCHAR *out_file_path,
    WCHAR *out_key_path,
    WCHAR *out_ipc_path,
    ULONG *inout_file_path_len,
    ULONG *inout_key_path_len,
    ULONG *inout_ipc_path_len);

SBIEAPI_EXPORT
LONG CrashApi_QueryProcessPath(
    HANDLE ProcessId,
    WCHAR *out_file_path,
    WCHAR *out_key_path,
    WCHAR *out_ipc_path,
    ULONG *inout_file_path_len,
    ULONG *inout_key_path_len,
    ULONG *inout_ipc_path_len);

SBIEAPI_EXPORT
LONG CrashApi_QueryPathList(
    ULONG path_code,
    ULONG *path_len,
    WCHAR *path_str,
    HANDLE process_id,
    BOOLEAN prepend_level);

SBIEAPI_EXPORT
LONG CrashApi_EnumProcessEx(
    const WCHAR* box_name,          // WCHAR [34]
    BOOLEAN all_sessions,
    ULONG which_session,            // -1 for current session
    ULONG* boxed_pids,             // ULONG [512]
    ULONG* boxed_count);

#define CrashApi_EnumProcess(box_name,boxed_pids) \
    CrashApi_EnumProcessEx(box_name,FALSE,-1,boxed_pids, NULL)


//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_SessionLeader(
    ULONG session_id,
    HANDLE *ProcessId);

SBIEAPI_EXPORT
LONG CrashApi_DisableForceProcess(
    ULONG *NewState,
    ULONG *OldState);


//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_MonitorControl(
    ULONG *NewState,
    ULONG *OldState);

SBIEAPI_EXPORT
    LONG CrashApi_MonitorPut(
    ULONG Type,
    const WCHAR *Name);

SBIEAPI_EXPORT
LONG CrashApi_MonitorPut2(
    ULONG Type,
    const WCHAR *Name,
    BOOLEAN bCheckObjectExists);

SBIEAPI_EXPORT
    LONG CrashApi_MonitorPutMsg(
    ULONG Type,
    const WCHAR *Message);

SBIEAPI_EXPORT
LONG CrashApi_MonitorPut2Ex(
    ULONG Type,
    ULONG NameLen,
    const WCHAR *Name,
    BOOLEAN bCheckObjectExists,
    BOOLEAN bIsMessage);

SBIEAPI_EXPORT
LONG CrashApi_MonitorPutEx(
    ULONG Type,
    ULONG Pid,
    ULONG Tid,
    const WCHAR *Message);          // WCHAR [256]

//SBIEAPI_EXPORT
//LONG CrashApi_MonitorGet(
//    ULONG *Type,
//    WCHAR *Name);                   // WCHAR [256]

SBIEAPI_EXPORT
LONG CrashApi_MonitorGetEx(
	ULONG *Type,
    ULONG *Pid,
    ULONG *Tid,
	WCHAR *Name);                   // WCHAR [256]


//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_HookTramp(void *Source, void *Trampoline);


//---------------------------------------------------------------------------


//SBIEAPI_EXPORT
LONG CrashApi_RenameFile(
    HANDLE FileHandle,
    const WCHAR *TargetDir, const WCHAR *TargetName,
    BOOLEAN ReplaceIfExists);

SBIEAPI_EXPORT
LONG CrashApi_GetFileName(
    HANDLE FileHandle,
    WCHAR *NameBuf,
    ULONG *NameLen,
    ULONG *ObjType);

//SBIEAPI_EXPORT
LONG CrashApi_OpenFile(
    HANDLE *FileHandle,
    const WCHAR *Path);

SBIEAPI_EXPORT
LONG CrashApi_CheckInternetAccess(
    HANDLE ProcessId,
    const WCHAR *DeviceName32,
    BOOLEAN IssueMessage);

SBIEAPI_EXPORT
LONG CrashApi_GetBlockedDll(
    WCHAR *DllNameBuf,
    ULONG DllNameLen);


//---------------------------------------------------------------------------


//SBIEAPI_EXPORT
LONG CrashApi_OpenKey(
    HANDLE *KeyHandle,
    const WCHAR *Path);

//SBIEAPI_EXPORT
LONG CrashApi_SetLowLabelKey(
    const WCHAR *Path);


//---------------------------------------------------------------------------


//SBIEAPI_EXPORT
LONG CrashApi_CreateDirOrLink(
    const WCHAR *ObjectName, const WCHAR *TargetName);


//---------------------------------------------------------------------------


//SBIEAPI_EXPORT
LONG CrashApi_DuplicateObject(
    HANDLE *TargetHandle,
    HANDLE OtherProcessHandle,
    HANDLE SourceHandle,
    ACCESS_MASK DesiredAccess,
    ULONG Options);

SBIEAPI_EXPORT
LONG CrashApi_OpenProcess(
    HANDLE *ProcessHandle,
    HANDLE ProcessId);

//SBIEAPI_EXPORT
LONG CrashApi_OpenDeviceMap(
    HANDLE *DirectoryHandle);

//SBIEAPI_EXPORT
LONG CrashApi_QuerySymbolicLink(
    WCHAR *NameBuf,
    ULONG NameLen);

//---------------------------------------------------------------------------

SBIEAPI_EXPORT
LONG CrashApi_QueryDrvInfo(
    ULONG info_class,
    VOID* info_data,
    ULONG info_size);

//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_ReloadConf(ULONG session_id, ULONG flags);

SBIEAPI_EXPORT
ULONG CrashApi_UpdateConf(
    ULONG op, 
    const WCHAR *section_name,
    const WCHAR *setting_name, 
    const WCHAR *value_ptr);

SBIEAPI_EXPORT
LONG CrashApi_QueryConf(
    const WCHAR *section_name,      // WCHAR [66]
    const WCHAR *setting_name,      // WCHAR [66]
    ULONG setting_index,
    WCHAR *out_buffer,
    ULONG buffer_len);

#define CrashApi_QueryConfAsIs(bx, st, idx, buf, buflen) \
    CrashApi_QueryConf((bx), (st), ((idx) | CONF_GET_NO_EXPAND), buf, buflen)

SBIEAPI_EXPORT
BOOLEAN CrashApi_QueryConfBool(
    const WCHAR *section_name,      // WCHAR [66]
    const WCHAR *setting_name,      // WCHAR [66]
    BOOLEAN def);

SBIEAPI_EXPORT
ULONG CrashApi_QueryConfNumber(
    const WCHAR *section_name,      // WCHAR [66]
    const WCHAR *setting_name,      // WCHAR [66]
    ULONG def);

SBIEAPI_EXPORT
ULONG64 CrashApi_QueryConfNumber64(
    const WCHAR *section_name,      // WCHAR [66]
    const WCHAR *setting_name,      // WCHAR [66]
    ULONG64 def);

SBIEAPI_EXPORT
LONG CrashApi_EnumBoxes(
    LONG index,                     // initialize to -1
    WCHAR *box_name);               // WCHAR [34]

SBIEAPI_EXPORT
LONG CrashApi_EnumBoxesEx(
    LONG index,                     // initialize to -1
    WCHAR *box_name,                // WCHAR [34]
    BOOLEAN ignore_hidden);

SBIEAPI_EXPORT
LONG CrashApi_IsBoxEnabled(
    const WCHAR *box_name);         // WCHAR [34]

SBIEAPI_EXPORT
LONG CrashApi_SetUserName(
    const WCHAR *SidString,
    const WCHAR *UserName);


//---------------------------------------------------------------------------


SBIEAPI_EXPORT
LONG CrashApi_GetUnmountHive(
    WCHAR *path);                   // WCHAR [256]


//---------------------------------------------------------------------------

SBIEAPI_EXPORT
LONG CrashApi_ProcessExemptionControl(
	HANDLE process_id,
	ULONG action_id,
	ULONG *NewState,
	ULONG *OldState);

//---------------------------------------------------------------------------


SBIEAPI_EXPORT 
void* SbieDll_GetSysFunction(
    const WCHAR* name);

SBIEAPI_EXPORT 
BOOL SbieDll_RunStartExe(
    const WCHAR* cmd, 
    const wchar_t* boxname);

#ifdef __cplusplus
}
#endif

#endif /* _MY_SBIEAPI_H */
