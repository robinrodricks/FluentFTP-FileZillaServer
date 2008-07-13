// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2004 - Tim Kosse <tim.kosse@gmx.de>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

// Permissions.cpp: Implementierung der Klasse CPermissions.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "misc\md5.h"
#include "Permissions.h"
#include "misc\MarkupSTL.h"
#include "options.h"
#include "iputils.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPermissionsHelperWindow

class CPermissionsHelperWindow
{
public:
	CPermissionsHelperWindow(CPermissions *pPermissions)
	{
		ASSERT(pPermissions);
		m_pPermissions = pPermissions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize = sizeof wndclass;
		wndclass.style = 0;
		wndclass.lpfnWndProc = WindowProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = GetModuleHandle(0);
		wndclass.hIcon = 0;
		wndclass.hCursor = 0;
		wndclass.hbrBackground = 0;
		wndclass.lpszMenuName = 0;
		wndclass.lpszClassName = _T("CPermissions Helper Window");
		wndclass.hIconSm = 0;

		RegisterClassEx(&wndclass);

		m_hWnd=CreateWindow(_T("CPermissions Helper Window"), _T("CPermissions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	virtual ~CPermissionsHelperWindow()
	{
		//Destroy window
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = 0;
		}
	}

	HWND GetHwnd()
	{
		return m_hWnd;
	}

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message==WM_USER)
		{
			/* If receiving WM_USER, update the permission data of the instance with the permission
			 * data from the global data
			 */

			// Get own instance
			CPermissionsHelperWindow *pWnd=(CPermissionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd)
				return 0;
			if (!pWnd->m_pPermissions)
				return 0;
	
			EnterCritSection(pWnd->m_pPermissions->m_sync);
	
			// Clear old group data and copy over the new data
			pWnd->m_pPermissions->m_GroupsList.clear();
			for (CPermissions::t_GroupsList::iterator groupiter=pWnd->m_pPermissions->m_sGroupsList.begin(); groupiter!=pWnd->m_pPermissions->m_sGroupsList.end(); groupiter++)
				pWnd->m_pPermissions->m_GroupsList.push_back(*groupiter);
	
			// Clear old user data and copy over the new data
			pWnd->m_pPermissions->m_UsersList.clear();
			for (CPermissions::t_UsersList::iterator iter=pWnd->m_pPermissions->m_sUsersList.begin(); iter!=pWnd->m_pPermissions->m_sUsersList.end(); iter++)
			{
				CUser user = *iter;
				user.pOwner = NULL;
				if (user.group != _T(""))
				{	// Set owner
					for (CPermissions::t_GroupsList::iterator groupiter=pWnd->m_pPermissions->m_GroupsList.begin(); groupiter!=pWnd->m_pPermissions->m_GroupsList.end(); groupiter++)
						if (groupiter->group == user.group)
						{
							user.pOwner = &(*groupiter);
							break;
						}
				}
				pWnd->m_pPermissions->m_UsersList.push_back(user);
			}
	
			LeaveCritSection(pWnd->m_pPermissions->m_sync);
		}
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}

protected:
	CPermissions *m_pPermissions;

private:
	HWND m_hWnd;
};

/////////////////////////////////////////////////////////////////////////////
// CPermissions

CCriticalSectionWrapper CPermissions::m_sync;
CPermissions::t_UsersList CPermissions::m_sUsersList;
CPermissions::t_GroupsList CPermissions::m_sGroupsList;
std::list<CPermissions *> CPermissions::m_sInstanceList;

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CPermissions::CPermissions()
{
	Init();

	m_pPermissionsHelperWindow = new CPermissionsHelperWindow(this);
}

CPermissions::~CPermissions()
{
	EnterCritSection(m_sync);
	std::list<CPermissions *>::iterator instanceIter;
	for (instanceIter=m_sInstanceList.begin(); instanceIter!=m_sInstanceList.end(); instanceIter++)
		if (*instanceIter==this)
			break;
	ASSERT(instanceIter != m_sInstanceList.end());
	if (instanceIter != m_sInstanceList.end())
		m_sInstanceList.erase(instanceIter);
	LeaveCritSection(m_sync);
	if (m_pPermissionsHelperWindow)
		delete m_pPermissionsHelperWindow;
}

void CPermissions::AddLongListingEntry(t_dirlisting *&pResult, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *)
{
	CFileStatus64 status;
	if (!pTime && GetStatus64(directory.dir, status))
	{
		size = status.m_size;
		pTime = &status.m_mtime;
	}

	unsigned int nameLen = strlen(name);

	// This wastes some memory but keeps the whole thing fast
	if ((8192 - pResult->len) < (60 + nameLen))
	{
		pResult->pNext = new t_dirlisting;
		pResult = pResult->pNext;
		pResult->len = 0;
		pResult->pNext = NULL;
	}

	if (isDir)
	{
		memcpy(pResult->buffer + pResult->len, "drwxr-xr-x", 10);
		pResult->len += 10;
	}
	else
	{
		pResult->buffer[pResult->len++] = '-';
		pResult->buffer[pResult->len++] = directory.bFileRead ? 'r' : '-';
		pResult->buffer[pResult->len++] = directory.bFileWrite ? 'w' : '-';

		BOOL isexe = FALSE;
		if (nameLen > 4)
		{
			CStdStringA ext = name + nameLen - 4;
			ext.MakeLower();
			if (ext.ReverseFind('.')!=-1)
			{
				if (ext == ".exe")
					isexe = TRUE;
				else if (ext == ".bat")
					isexe = TRUE;
				else if (ext == ".com")
					isexe = TRUE;
			}
		}
		pResult->buffer[pResult->len++] = isexe ? 'x' : '-';
		pResult->buffer[pResult->len++] = directory.bFileRead ? 'r' : '-';
		pResult->buffer[pResult->len++] = '-';
		pResult->buffer[pResult->len++] = isexe ? 'x' : '-';
		pResult->buffer[pResult->len++] = directory.bFileRead ? 'r' : '-';
		pResult->buffer[pResult->len++] = '-';
		pResult->buffer[pResult->len++] = isexe ? 'x' : '-';
	}

	memcpy(pResult->buffer + pResult->len, " 1 ftp ftp ", 11);
	pResult->len += 11;

	pResult->len += sprintf(pResult->buffer + pResult->len, "% 14I64d", size);

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime)
		mtime = *pTime;
	else
		mtime = fTime;

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	_int64 t1 = ((_int64)mtime.dwHighDateTime<<32) + mtime.dwLowDateTime;
	t1 -= offset;
	mtime.dwHighDateTime = (DWORD)(t1>>32);
	mtime.dwLowDateTime = (DWORD)(t1%0xFFFFFFFF);

	SYSTEMTIME sFileTime;
	FileTimeToSystemTime(&mtime, &sFileTime);

	_int64 t2 = ((_int64)fTime.dwHighDateTime<<32) + fTime.dwLowDateTime;
	const char months[][4]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	pResult->len += sprintf(pResult->buffer + pResult->len, " %s %02d ", months[sFileTime.wMonth-1], sFileTime.wDay);
	if (t1 > t2 || (t2-t1) > ((_int64)1000000*60*60*24*350))
		pResult->len += sprintf(pResult->buffer + pResult->len, " %d ", sFileTime.wYear);
	else
		pResult->len += sprintf(pResult->buffer + pResult->len, "%02d:%02d ", sFileTime.wHour, sFileTime.wMinute);

	memcpy(pResult->buffer + pResult->len, name, nameLen);
	pResult->len += nameLen;
	pResult->buffer[pResult->len++] = '\r';
	pResult->buffer[pResult->len++] = '\n';
}

void CPermissions::AddFactsListingEntry(t_dirlisting *&pResult, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *enabledFacts)
{
	CFileStatus64 status;
	if (!pTime && GetStatus64(directory.dir, status))
	{
		size = status.m_size;
		pTime = &status.m_mtime;
	}

	unsigned int nameLen = strlen(name);

	// This wastes some memory but keeps the whole thing fast
	if ((8192 - pResult->len) < (60 + nameLen))
	{
		pResult->pNext = new t_dirlisting;
		pResult = pResult->pNext;
		pResult->len = 0;
		pResult->pNext = NULL;
	}

	if (!enabledFacts || enabledFacts[0])
	{
		if (isDir)
		{
			memcpy(pResult->buffer + pResult->len, "type=dir;", 9);
			pResult->len += 9;
		}
		else
		{
			memcpy(pResult->buffer + pResult->len, "type=file;", 10);
			pResult->len += 10;
		}
	}

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime)
		mtime = *pTime;
	else
		mtime = fTime;

	if (!enabledFacts || enabledFacts[2])
	{
		if (mtime.dwHighDateTime || mtime.dwLowDateTime)
		{
			SYSTEMTIME time;
			FileTimeToSystemTime(&mtime, &time);
			CStdStringA str;
			str.Format("modify=%04d%02d%02d%02d%02d%02d;",
				time.wYear,
				time.wMonth,
				time.wDay,
				time.wHour,
				time.wMinute,
				time.wSecond);

			memcpy(pResult->buffer + pResult->len, str.c_str(), str.GetLength());
			pResult->len += str.GetLength();
		}
	}

	if (!enabledFacts || enabledFacts[1])
	{
		if (!isDir)
			pResult->len += sprintf(pResult->buffer + pResult->len, "size=%I64d;", size);
	}

	pResult->len += sprintf(pResult->buffer + pResult->len, " %s\r\n", name);
}

void CPermissions::AddShortListingEntry(t_dirlisting *&pResult, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *)
{
	unsigned int nameLen = strlen(name);
	unsigned int dirToDisplayLen = strlen(dirToDisplay);

	// This wastes some memory but keeps the whole thing fast
	if ((8192 - pResult->len) < (10 + nameLen + dirToDisplayLen))
	{
		pResult->pNext = new t_dirlisting;
		pResult = pResult->pNext;
		pResult->len = 0;
		pResult->pNext = NULL;
	}

	memcpy(pResult->buffer + pResult->len, dirToDisplay, dirToDisplayLen);
	pResult->len += dirToDisplayLen;
	memcpy(pResult->buffer + pResult->len, name, nameLen);
	pResult->len += nameLen;
	pResult->buffer[pResult->len++] = '\r';
	pResult->buffer[pResult->len++] = '\n';
}

int CPermissions::GetDirectoryListing(LPCTSTR username, CStdString currentDir, CStdString dirToDisplay,
									  t_dirlisting *&pResult, CStdString& physicalDir, 
									  CStdString& logicalDir, void (*addFunc)(t_dirlisting *&pResult, bool isDir, const char* name, const t_directory& directory, __int64 size, FILETIME* pTime, const char* dirToDisplay, bool *enabledFacts),
									  bool useUTF8, bool *enabledFacts /*=0*/)
{
	// Get user
	CUser user;
	if (!GetUser(username, user))
		return PERMISSION_DENIED;

	CStdString dir = CanonifyServerDir(currentDir, dirToDisplay);
	if (dir == _T(""))
		return PERMISSION_INVALIDNAME;
	logicalDir = dir;
	dir.MakeLower();

	// Get directory from directory name
	t_directory directory;
	BOOL bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	CStdString sFileSpec = _T("*"); // Which files to list in the directory
	if (res == PERMISSION_FILENOTDIR || res == PERMISSION_NOTFOUND) // Try listing using a direct wildcard filespec instead?
	{
		// Check dirToDisplay if we are allowed to go back a directory
		dirToDisplay.Replace('\\', '/');
		while (dirToDisplay.Replace(_T("//"), _T("/")));
		if (dirToDisplay.Right(1) == _T("/"))
			return res;
		int pos = dirToDisplay.ReverseFind('/');
		if (res != PERMISSION_FILENOTDIR && dirToDisplay.Mid(pos + 1).Find('*') == -1)
			return res;
		dirToDisplay = dirToDisplay.Left(pos + 1);

		if (dir == _T("/"))
			return res;
		
		pos = dir.ReverseFind('/');
		sFileSpec = dir.Mid(pos + 1);
		if (pos)
			dir = dir.Left(pos);
		else
			dir = _T("/");

		if (sFileSpec.Find(_T("*")) == -1 && res != PERMISSION_FILENOTDIR)
			return res;

		res = GetRealDirectory(dir, user, directory, bTruematch);
	}
	if (res)
		return res;

	// Check permissions
	if (!directory.bDirList)
		return PERMISSION_DENIED;

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	if (dirToDisplay != _T("") && dirToDisplay.Right(1) != _T("/"))
		dirToDisplay += _T("/");

	t_dirlisting *pDir = new t_dirlisting;
	pDir->len = 0;
	pDir->pNext = NULL;
	pResult = pDir;

	CStdString curDir = directory.dir;
	curDir.MakeLower();

	char* dirToDisplayUTF8 = ConvertFilename(dirToDisplay, useUTF8);
	if (!dirToDisplayUTF8)
		return PERMISSION_DENIED;
		
	// List aliases in current directory
	for (std::multimap<CStdString, CUser::t_alias>::const_iterator iter = user.aliasMap.find(curDir);
		 iter != user.aliasMap.end() && iter->first == curDir; iter++)
	{
		t_directory directory;
		BOOL truematch = false;
		if (GetRealDirectory(dir + _T("/") + iter->second.name, user, directory, truematch))
			continue;
		if (!directory.bDirList)
			continue;
		if (!truematch && !directory.bDirSubdirs)
			continue;

		if (sFileSpec != _T("*.*") && sFileSpec != _T("*"))
		{
			if (!WildcardMatch(iter->second.name, sFileSpec))
				continue;
		}

		char* name = ConvertFilename(iter->second.name, useUTF8);
		if (name)
		{
			addFunc(pDir, true, name, directory, 0, 0, dirToDisplayUTF8, enabledFacts);
			delete [] name;
		}
	}

	for (std::multimap<CStdString, CStdString>::const_iterator iter = user.virtualAliasNames.find(dir);
		iter != user.virtualAliasNames.end() && iter->first == dir; iter++)
	{
		t_directory directory;
		BOOL truematch = false;
		if (GetRealDirectory(dir + _T("/") + iter->second, user, directory, truematch))
			continue;
		if (!directory.bDirList)
			continue;
		if (!truematch && !directory.bDirSubdirs)
			continue;

		if (sFileSpec != _T("*.*") && sFileSpec != _T("*"))
		{
			if (!WildcardMatch(iter->second, sFileSpec))
				continue;
		}

		char* name = ConvertFilename(iter->second, useUTF8);
		if (name)
		{
			addFunc(pDir, true, name, directory, 0, 0, dirToDisplayUTF8, enabledFacts);
			delete [] name;
		}
	}

	physicalDir = directory.dir;
    if (sFileSpec != _T("*") && sFileSpec != _T("*.*"))
		physicalDir += sFileSpec;

	WIN32_FIND_DATA FindFileData;
	WIN32_FIND_DATA NextFindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(directory.dir + _T("\\") + sFileSpec, &NextFindFileData);
	while (hFind != INVALID_HANDLE_VALUE)
	{
		FindFileData = NextFindFileData;
		if (!FindNextFile(hFind, &NextFindFileData))
		{
			FindClose(hFind);
			hFind = INVALID_HANDLE_VALUE;
		}

		if (!_tcscmp(FindFileData.cFileName, _T(".")) || !_tcscmp(FindFileData.cFileName, _T("..")))
			continue;

		const CStdString& fn = FindFileData.cFileName;
		
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// Check permissions of subdir. If we don't have LIST permission,
			// don't display the subdir.
			BOOL truematch;
			t_directory subDir;
			if (GetRealDirectory(dir + _T("/") + fn, user, subDir, truematch))
				continue;

			if (subDir.bDirList)
			{
				char* utf8 = ConvertFilename(fn, useUTF8);
				if (!utf8)
					continue;
				addFunc(pDir, true, utf8, subDir, 0, &FindFileData.ftLastWriteTime, dirToDisplayUTF8, enabledFacts);
				delete [] utf8;
			}
		}
		else
		{
			char* utf8 = ConvertFilename(fn, useUTF8);
			if (!utf8)
				continue;
			addFunc(pDir, false, utf8, directory, FindFileData.nFileSizeLow + ((_int64)FindFileData.nFileSizeHigh<<32), &FindFileData.ftLastWriteTime, dirToDisplayUTF8, enabledFacts);
			delete [] utf8;
		}
	}

	delete [] dirToDisplayUTF8;

	return 0;
}

int CPermissions::CheckDirectoryPermissions(LPCTSTR username, CStdString dirname, CStdString currentdir, int op, CStdString& physicalDir, CStdString& logicalDir)
{
	//Get user from username
	CUser user;
	if (!GetUser(username, user))
		return PERMISSION_DENIED; // No user found

	CStdString dir = CanonifyServerDir(currentdir, dirname);
	if (dir == _T(""))
		return PERMISSION_INVALIDNAME;
	if (dir == _T("/"))
		return PERMISSION_NOTFOUND;

	int pos = dir.ReverseFind('/');
	if (pos == -1 || !dir[pos + 1])
		return PERMISSION_NOTFOUND;
	logicalDir = dir;
	dirname = dir.Mid(pos + 1);
	if (!pos)
		dir = _T("/");
	else
		dir = dir.Left(pos);
	
	// dir now is the absolute path (logical server path of course)
	// awhile dirname is the pure dirname without the full path

	CStdString realDir;
	CStdString realDirname;

	//Get the physical path, only of dir to get the right permissions
	t_directory directory;
	BOOL truematch;
	int res;

	CStdString dir2 = dir;
	CStdString dirname2 = dirname;
	do
	{
		res = GetRealDirectory(dir2, user, directory, truematch);
		if (res & PERMISSION_NOTFOUND && op == DOP_CREATE)
		{ //that path could not be found. Maybe more than one directory level has to be created, check that
			if (dir2 == _T("/"))
				return res;

			int pos = dir2.ReverseFind('/');
			if (pos == -1)
				return res;

			dirname2 = dir2.Mid(pos+1) + _T("/") + dirname2;
			if (pos)
				dir2 = dir2.Left(pos);
			else
				dir2 = _T("/");

			continue;
		}
		else if (res)
			return res;
		
		realDir = directory.dir;
		realDirname = dirname2;
		if (!directory.bDirDelete && op & DOP_DELETE)
			res |= PERMISSION_DENIED;
		if (!directory.bDirCreate && op & DOP_CREATE)
			res |= PERMISSION_DENIED;
		break;
	} while (TRUE);

	realDirname.Replace(_T("/"), _T("\\"));
	physicalDir = realDir + _T("\\") + realDirname;

	//Check if dir + dirname is a valid path
	int res2 = GetRealDirectory(dir + _T("/") + dirname, user, directory, truematch);
	if (!res2 && op&DOP_CREATE)
		res |= PERMISSION_DOESALREADYEXIST;
	else if (!(res2 & PERMISSION_NOTFOUND))
		return res | res2;
	
	// check dir attributes
	DWORD nAttributes = GetFileAttributes(physicalDir);
	if (nAttributes==0xFFFFFFFF && !(op&DOP_CREATE))
		res |= PERMISSION_NOTFOUND;
	else if (!(nAttributes&FILE_ATTRIBUTE_DIRECTORY))
		res |= PERMISSION_FILENOTDIR;

	//Finally, a valid path+dirname!
	return res;
}

int CPermissions::CheckFilePermissions(LPCTSTR username, CStdString filename, CStdString currentdir, int op, CStdString& physicalFile, CStdString& logicalFile)
{
	//Get user from username
	CUser user;
	if (!GetUser(username, user))
		return PERMISSION_DENIED; // No user found

	CStdString dir = CanonifyServerDir(currentdir, filename);
	if (dir == _T(""))
		return PERMISSION_INVALIDNAME;
	if (dir == _T("/"))
		return PERMISSION_NOTFOUND;

	int pos = dir.ReverseFind('/');
	if (pos == -1)
		return PERMISSION_NOTFOUND;

	logicalFile = dir;
	
	filename = dir.Mid(pos + 1);
	if (pos)
		dir = dir.Left(pos);
	else
		dir = "/";

	// dir now is the absolute path (logical server path of course)
	// while filename is the filename

	//Get the physical path
	t_directory directory;
	BOOL truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);

	if (res)
		return res;
	if (!directory.bFileRead && op&FOP_READ)
		res |= PERMISSION_DENIED;
	if (!directory.bFileDelete && op&FOP_DELETE)
		res |= PERMISSION_DENIED;
	if (!directory.bFileWrite && op&(FOP_CREATENEW|FOP_WRITE|FOP_APPEND))
		res |= PERMISSION_DENIED;
	if ((!directory.bDirList || (!directory.bDirSubdirs && !truematch)) && op&FOP_LIST)
		res |= PERMISSION_DENIED;

	physicalFile = directory.dir + "\\" + filename;
	DWORD nAttributes = GetFileAttributes(physicalFile);
	if (nAttributes == 0xFFFFFFFF)
	{
		if (!(op&(FOP_WRITE|FOP_APPEND|FOP_CREATENEW)))
			res |= PERMISSION_NOTFOUND;
	}
	else
	{
		if (nAttributes&FILE_ATTRIBUTE_DIRECTORY)
			res |= PERMISSION_DIRNOTFILE;
		if (!directory.bFileAppend && op&FOP_APPEND)
			res |= PERMISSION_DENIED;
		if (!directory.bFileDelete && op&FOP_WRITE)
			res |= PERMISSION_DENIED;
		if (op & FOP_CREATENEW)
			res |= PERMISSION_DOESALREADYEXIST;
	}

	//If res is 0 we finally have a valid path+filename!
	return res;
}

CStdString CPermissions::GetHomeDir(const CUser &user, bool physicalPath /*=false*/) const
{
	if (user.homedir == _T(""))
		return _T("");

	if (!physicalPath)
		return _T("/");
	
	CStdString path;
	path = user.homedir;
	
	user.DoReplacements(path);
	
	return path;
}

CStdString CPermissions::GetHomeDir(LPCTSTR username, bool physicalPath /*=false*/) const
{
	CUser user;
	if (!GetUser(username, user))
		return "";

	return GetHomeDir(user, physicalPath);
}

int CPermissions::GetRealDirectory(CStdString directory, const CUser &user, t_directory &ret, BOOL &truematch)
{
	/*
	 * This function translates pathnames from absolute server paths
	 * into absolute local paths.
	 * The given server directory is already an absolute canonified path, so
	 * parsing it is very quick.
	 * To find the absolute local path, we go though each segment of the server
	 * path. For the local path, we start form the homedir and append segments 
	 * sequentially or resolve aliases if required.
	 */

	// After all this is a Windows server, so ignore case
	// TODO: Use GetVolumeInformation to check case sensitivity of target volume
	directory.MakeLower();


	directory.TrimLeft(_T("/"));
	
	// Split server path
	// --------------------

	//Split dir into pieces
	std::list<CStdString> PathPieces;
	int pos;

	while((pos = directory.Find('/')) != -1)
	{
		PathPieces.push_back(directory.Left(pos));
		directory = directory.Mid(pos + 1);
	}
	if (directory != _T(""))
		PathPieces.push_back(directory);

	// Get absolute local path
	// -----------------------

	//First get the home dir
	CStdString homepath = GetHomeDir(user, true);
	if (homepath == _T("")) //No homedir found
		return PERMISSION_DENIED;
	
	// Reassamble path to get local path
	CStdString path = homepath; // Start with homedir as root

	CStdString virtualPath = _T("/");
	// Go through all pieces
	for (std::list<CStdString>::const_iterator iter = PathPieces.begin(); iter != PathPieces.end(); iter++)
	{
		//Check if piece exists
		const CStdString& piece = *iter;
		virtualPath += piece + _T("/");
		DWORD nAttributes = GetFileAttributes(path + _T("\\") + piece);
		if (nAttributes != 0xFFFFFFFF)
		{
			if (!(nAttributes&FILE_ATTRIBUTE_DIRECTORY))
				return PERMISSION_FILENOTDIR;
			path += _T("\\") + piece;
			continue;
		}
		else
		{
			// Physical path did not exist, check aliases
			path.MakeLower();
			const CStdString& target = user.GetAliasTarget(path, virtualPath, piece);

			if (target != _T(""))
			{
				if (target.Right(1) != _T(":"))
				{
					nAttributes = GetFileAttributes(target);
					if (nAttributes == 0xFFFFFFFF)
						return PERMISSION_NOTFOUND;
					else if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY))
						return PERMISSION_FILENOTDIR;
				}
				path = target;
				continue;
			}

		}
		return PERMISSION_NOTFOUND;
	}
	const CStdString realpath = path;

	// Check permissions
	// -----------------

	/* We got a valid local path, now find the closest matching path within the
	 * permissions.
	 * We do this by sequentially comparing the path with all permissions and
	 * sequentially removing the last path segment until we found a match or
	 * all path segments have been removed
	 * Distinguish the case
	 */
	truematch = TRUE;
	
	path.MakeLower();
	while (path != _T(""))
	{
		BOOL bFoundMatch = FALSE;
		unsigned int i;
	
		// Check user permissions
		for (i = 0; i < user.permissions.size(); i++)
		{
			CStdString permissionPath = user.permissions[i].dir;
			user.DoReplacements(permissionPath);
			permissionPath.MakeLower();
			if (permissionPath == path)
			{
				bFoundMatch = TRUE;
				ret = user.permissions[i];
				break;
			}
		}

		// Check owner (group) permissions
		if (!bFoundMatch && user.pOwner)
			for (i = 0; i < user.pOwner->permissions.size(); i++)
			{
				CStdString permissionPath = user.pOwner->permissions[i].dir;
				user.DoReplacements(permissionPath);
				permissionPath.MakeLower();
				if (permissionPath == path)
				{
					bFoundMatch = TRUE;
					ret = user.pOwner->permissions[i];
					break;
				}
			}

		if (!bFoundMatch)
		{
			// No match found, remove last segment and try again
			int pos = path.ReverseFind('\\');
			if (pos != -1)
				path = path.Left(pos);
			else
				return PERMISSION_DENIED;
			truematch = FALSE;
			continue;
		}
		ret.dir = realpath;

		// We can check the bDirSubdirs permission right here
		if (!truematch && !ret.bDirSubdirs)
			return PERMISSION_DENIED;

		return 0;
	}
	return PERMISSION_NOTFOUND;
}

int CPermissions::ChangeCurrentDir(LPCTSTR username, CStdString &currentdir, CStdString &dir)
{
	//Get user from username
	CUser user;
	if (!GetUser(username, user))
		return PERMISSION_DENIED; // No user found

	CStdString canonifiedDir = CanonifyServerDir(currentdir, dir);
	if (canonifiedDir == _T(""))
		return PERMISSION_INVALIDNAME;
	dir = canonifiedDir;

	//Get the physical path
	t_directory directory;
	BOOL truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);
	if (res)
		return res;
	if (!directory.bDirList)
	{
		if (!directory.bFileRead && !directory.bFileWrite)
			return PERMISSION_DENIED;
	}

	//Finally, a valid path!
	currentdir = dir; //Server paths are relative, so we can use the absolute server path

	return 0;
}

BOOL CPermissions::GetUser(CStdString username, CUser &userdata) const
{
	// Get user from username
	for (unsigned int i = 0; i < m_UsersList.size(); i++)
	{
		if (!username.CompareNoCase(m_UsersList[i].user))
		{
			userdata = m_UsersList[i];
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CPermissions::CheckUserLogin(LPCTSTR username, LPCTSTR pass, CUser &userdata, BOOL noPasswordCheck /*=FALSE*/)
{
	const char *tmp = ConvToNetwork(pass);
	if (!tmp)
		return FALSE;

	MD5 md5;
	md5.update((unsigned char *)tmp, strlen(tmp));
	md5.finalize();
	char *res = md5.hex_digest();
	CStdString hash = res;
	delete [] res;
	delete [] tmp;

	CUser user;
	if (!GetUser(username, user))
		return FALSE;

	if (noPasswordCheck || user.password == hash || user.password == _T(""))
	{
		userdata = user;
		return TRUE;
	}

	return FALSE;
}

void CPermissions::UpdateInstances()
{
	EnterCritSection(m_sync);
	for (std::list<CPermissions *>::iterator iter=m_sInstanceList.begin(); iter!=m_sInstanceList.end(); iter++)
	{
		if (*iter != this)
		{
			ASSERT((*iter)->m_pPermissionsHelperWindow);
			::PostMessage((*iter)->m_pPermissionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
		}
	}
	LeaveCritSection(m_sync);
}

void CPermissions::SetKey(CMarkupSTL *pXML, LPCTSTR name, LPCTSTR value)
{
	ASSERT(pXML);
	pXML->AddChildElem(_T("Option"), value);
	pXML->AddChildAttrib(_T("Name"), name);
}

void CPermissions::SetKey(CMarkupSTL *pXML, LPCTSTR name, int value)
{
	ASSERT(pXML);
	CStdString str;
	str.Format(_T("%d"), value);
	SetKey(pXML, name, str);
}

void CPermissions::SavePermissions(CMarkupSTL *pXML, const t_group &user)
{
	pXML->AddChildElem(_T("Permissions"));
	pXML->IntoElem();
	for (unsigned int i=0; i<user.permissions.size(); i++)
	{
		pXML->AddChildElem(_T("Permission"));
		pXML->AddChildAttrib(_T("Dir"), user.permissions[i].dir);
		pXML->IntoElem();
		if (!user.permissions[i].aliases.empty())
		{
			pXML->AddChildElem(_T("Aliases"));
			pXML->IntoElem();
			for (std::list<CStdString>::const_iterator iter = user.permissions[i].aliases.begin(); iter != user.permissions[i].aliases.end(); iter++)
			{
				pXML->AddChildElem(_T("Alias"));
				pXML->SetChildData(*iter);
			}
			pXML->OutOfElem();
		}
		SetKey(pXML, _T("FileRead"), user.permissions[i].bFileRead ? _T("1"):_T("0"));
		SetKey(pXML, _T("FileWrite"), user.permissions[i].bFileWrite ? _T("1"):_T("0"));
		SetKey(pXML, _T("FileDelete"), user.permissions[i].bFileDelete ?_T("1"):_T("0"));
		SetKey(pXML, _T("FileAppend"), user.permissions[i].bFileAppend ? _T("1"):_T("0"));
		SetKey(pXML, _T("DirCreate"), user.permissions[i].bDirCreate ? _T("1"):_T("0"));
		SetKey(pXML, _T("DirDelete"), user.permissions[i].bDirDelete ? _T("1"):_T("0"));
		SetKey(pXML, _T("DirList"), user.permissions[i].bDirList ? _T("1"):_T("0"));
		SetKey(pXML, _T("DirSubdirs"), user.permissions[i].bDirSubdirs ? _T("1"):_T("0"));
		SetKey(pXML, _T("IsHome"), user.permissions[i].bIsHome ? _T("1"):_T("0"));
		SetKey(pXML, _T("AutoCreate"), user.permissions[i].bAutoCreate ? _T("1"):_T("0"));
		pXML->OutOfElem();
	}
	pXML->OutOfElem();
}

BOOL CPermissions::GetAsCommand(char **pBuffer, DWORD *nBufferLength)
{
	// This function returns all account data as a command string which will be 
	// sent to the user interface.
	if (!pBuffer || !nBufferLength)
		return FALSE;

	EnterCritSection(m_sync);

	// First calculate the required buffer length
	DWORD len = 4;
	t_GroupsList::iterator groupiter;
	for (groupiter = m_sGroupsList.begin(); groupiter != m_sGroupsList.end(); groupiter++)
		len += groupiter->GetRequiredBufferLen();

	t_UsersList::iterator iter;
	for (iter = m_sUsersList.begin(); iter != m_sUsersList.end(); iter++)
		len += iter->GetRequiredBufferLen();

	// Allocate memory
	*pBuffer = new char[len];
	char* p  = *pBuffer;

	// Write groups to buffer
	*p++ = m_sGroupsList.size()/256;
	*p++ = m_sGroupsList.size()%256;
	for (groupiter = m_sGroupsList.begin(); groupiter != m_sGroupsList.end(); groupiter++)
	{
		p = groupiter->FillBuffer(p);
		if (!p)
		{
			delete [] *pBuffer;
			*pBuffer = NULL;
			LeaveCritSection(m_sync);
			return FALSE;
		}
	}

	// Write users to buffer
	*p++ = m_sUsersList.size()/256;
	*p++ = m_sUsersList.size()%256;
	for (iter = m_sUsersList.begin(); iter != m_sUsersList.end(); iter++)
	{
		p = iter->FillBuffer(p);
		if (!p)
		{
			delete [] *pBuffer;
			*pBuffer = NULL;
			LeaveCritSection(m_sync);
			return FALSE;
		}
	}

	LeaveCritSection(m_sync);
	*nBufferLength = len;

	return TRUE;
}

BOOL CPermissions::ParseUsersCommand(unsigned char *pData, DWORD dwDataLength)
{
	m_GroupsList.clear();
	m_UsersList.clear();
	unsigned char *p = pData;
	unsigned char* endMarker = pData + dwDataLength;

	if (dwDataLength < 2)
		return FALSE;
	int num = *p * 256 + p[1];
	p+=2;

	int i;
	for (i = 0; i < num; i++)
	{
		t_group group;
		p = group.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;
	
		if (group.group != _T(""))
		{
			//Set a home dir if no home dir could be read
			BOOL bGotHome = FALSE;
			for (unsigned int dir = 0; dir < group.permissions.size(); dir++)
				if (group.permissions[dir].bIsHome)
				{
					bGotHome = TRUE;
					break;
				}

			if (!bGotHome && !group.permissions.empty())
				group.permissions.begin()->bIsHome = TRUE;

			m_GroupsList.push_back(group);
		}
	}

	if ((endMarker - p) < 2)
		return FALSE;

	num = *p * 256 + p[1];
	p+=2;
	for (i = 0; i < num; i++)
	{
		CUser user;
	
		p = user.ParseBuffer(p, endMarker - p);
		if (!p)
			return FALSE;
	
		if (user.user != _T(""))
		{
			user.pOwner = NULL;
			if (user.group != _T(""))
			{
				for (t_GroupsList::iterator groupiter = m_GroupsList.begin(); groupiter != m_GroupsList.end(); groupiter++)
					if (groupiter->group == user.group)
					{
						user.pOwner = &(*groupiter);
						break;
					}
				if (!user.pOwner)
					user.group = "";
			}

			if (!user.pOwner)
			{
				//Set a home dir if no home dir could be read
				BOOL bGotHome = FALSE;
				for (unsigned int dir = 0; dir < user.permissions.size(); dir++)
					if (user.permissions[dir].bIsHome)
					{
						bGotHome = TRUE;
						break;
					}

				if (!bGotHome && !user.permissions.empty())
					user.permissions.begin()->bIsHome = TRUE;
			}

			std::vector<t_directory>::iterator iter;
			for (iter = user.permissions.begin(); iter != user.permissions.end(); iter++)
			{
				if (iter->bIsHome)
				{
					user.homedir = iter->dir;
					break;
				}
			}
			if (user.homedir == _T("") && user.pOwner)
			{
				for (iter = user.pOwner->permissions.begin(); iter != user.pOwner->permissions.end(); iter++)
				{
					if (iter->bIsHome)
					{
						user.homedir = iter->dir;
						break;
					}
				}
			}

			user.PrepareAliasMap();
			m_UsersList.push_back(user);
		}
	}

	// Update the account list
	EnterCritSection(m_sync);

	m_sGroupsList.clear();
	for (t_GroupsList::const_iterator groupiter=m_GroupsList.begin(); groupiter!=m_GroupsList.end(); groupiter++)
		m_sGroupsList.push_back(*groupiter);

	m_sUsersList.clear();
	for (t_UsersList::const_iterator iter=m_UsersList.begin(); iter!=m_UsersList.end(); iter++)
		m_sUsersList.push_back(*iter);

	UpdateInstances();

	LeaveCritSection(m_sync);

	// Write the new account data into xml file

	CMarkupSTL *pXML=COptions::GetXML();
	if (pXML)
	{
		while(pXML->FindChildElem(_T("Groups")))
			pXML->RemoveChildElem();
		pXML->AddChildElem(_T("Groups"));
		pXML->IntoElem();
	
		//Save the changed user details
		for (t_GroupsList::const_iterator groupiter=m_GroupsList.begin(); groupiter!=m_GroupsList.end(); groupiter++)
		{
			pXML->AddChildElem(_T("Group"));
			pXML->AddChildAttrib(_T("Name"), groupiter->group);
			pXML->IntoElem();
		
			SetKey(pXML, _T("Bypass server userlimit"), groupiter->nBypassUserLimit);
			SetKey(pXML, _T("User Limit"), groupiter->nUserLimit);
			SetKey(pXML, _T("IP Limit"), groupiter->nIpLimit);
			SetKey(pXML, _T("Enabled"), groupiter->nEnabled);
			SetKey(pXML, _T("Comments"), groupiter->comment);
			SetKey(pXML, _T("ForceSsl"), groupiter->forceSsl);

			SaveIpFilter(pXML, *groupiter);		
			SavePermissions(pXML, *groupiter);
			SaveSpeedLimits(pXML, *groupiter);

			pXML->OutOfElem();
		}
		pXML->OutOfElem();
		pXML->ResetChildPos();

		while(pXML->FindChildElem(_T("Users")))
			pXML->RemoveChildElem();
		pXML->AddChildElem(_T("Users"));
		pXML->IntoElem();
	
		//Save the changed user details
		for (t_UsersList::const_iterator iter=m_UsersList.begin(); iter!=m_UsersList.end(); iter++)
		{
			pXML->AddChildElem(_T("User"));
			pXML->AddChildAttrib(_T("Name"), iter->user);
			pXML->IntoElem();
		
			SetKey(pXML, _T("Pass"), iter->password);
			SetKey(pXML, _T("Group"), iter->group);
			SetKey(pXML, _T("Bypass server userlimit"), iter->nBypassUserLimit);
			SetKey(pXML, _T("User Limit"), iter->nUserLimit);
			SetKey(pXML, _T("IP Limit"), iter->nIpLimit);
			SetKey(pXML, _T("Enabled"), iter->nEnabled);
			SetKey(pXML, _T("Comments"), iter->comment);
			SetKey(pXML, _T("ForceSsl"), iter->forceSsl);
		
			SaveIpFilter(pXML, *iter);
			SavePermissions(pXML, *iter);
			SaveSpeedLimits(pXML, *iter);

			pXML->OutOfElem();
		}
		if (!COptions::FreeXML(pXML))
			return FALSE;
	}
	else
		return FALSE;

	return TRUE;
}

bool CPermissions::Init()
{
	EnterCritSection(m_sync);
	if (m_sInstanceList.empty() && m_sUsersList.empty() && m_sGroupsList.empty())
	{
		// It's the first time Init gets called after application start, read
		// permissions from xml file.

		ReadSettings();
	}
	else
	{
		m_GroupsList.clear();
		for (t_GroupsList::iterator groupiter = m_sGroupsList.begin(); groupiter != m_sGroupsList.end(); groupiter++)
			m_GroupsList.push_back(*groupiter);

		m_UsersList.clear();
		for (t_UsersList::iterator iter = m_sUsersList.begin(); iter != m_sUsersList.end(); iter++)
		{
			CUser user = *iter;
			user.pOwner = NULL;
			if (user.group != _T(""))
			{
				for (t_GroupsList::iterator groupiter=m_GroupsList.begin(); groupiter!=m_GroupsList.end(); groupiter++)
					if (groupiter->group == user.group)
					{
						user.pOwner = &(*groupiter);
						break;
					}
			}
			m_UsersList.push_back(user);
		}
	}

	std::list<CPermissions *>::iterator instanceIter;
	for (instanceIter = m_sInstanceList.begin(); instanceIter != m_sInstanceList.end(); instanceIter++)
		if (*instanceIter == this)
			break;
	if (instanceIter == m_sInstanceList.end())
		m_sInstanceList.push_back(this);
	LeaveCritSection(m_sync);

	return TRUE;
}

void CPermissions::ReadPermissions(CMarkupSTL *pXML, t_group &user, BOOL &bGotHome)
{
	bGotHome = FALSE;
	pXML->ResetChildPos();
	while (pXML->FindChildElem(_T("Permissions")))
	{
		pXML->IntoElem();
		while (pXML->FindChildElem(_T("Permission")))
		{
			t_directory dir;
			dir.dir = pXML->GetChildAttrib(_T("Dir"));
			dir.dir.Replace('/', '\\');
			dir.dir.TrimRight('\\');
			if (dir.dir == _T(""))
				continue;

			pXML->IntoElem();
			while (pXML->FindChildElem(_T("Aliases")))
			{
				pXML->IntoElem();
				while (pXML->FindChildElem(_T("Alias")))
				{
					CStdString alias = pXML->GetChildData();
					if (alias[0] != '/')
					{
						alias.Replace(_T("/"), _T("\\"));
						bool uncPath = false;
						if (alias.Left(2) == _T("\\\\"))
							uncPath = true;
						while (alias.Replace(_T("\\\\"), _T("\\")));
						if (uncPath)
							alias = _T("\\") + alias;

						alias.TrimRight('\\');
						if (alias != _T(""))
							dir.aliases.push_back(alias);
					}
					else
					{
						alias.Replace(_T("\\"), _T("/"));
						while (alias.Replace(_T("//"), _T("/")));
						alias.TrimRight('/');
						if (alias != _T("") && alias != _T("/"))
							dir.aliases.push_back(alias);
					}
				}
				pXML->OutOfElem();
			}
			pXML->ResetChildPos();
			while (pXML->FindChildElem(_T("Option")))
			{
				CStdString name = pXML->GetChildAttrib(_T("Name"));
				CStdString value = pXML->GetChildData();
				if (name == _T("FileRead"))
					dir.bFileRead = value == _T("1");
				else if (name == _T("FileWrite"))
					dir.bFileWrite = value == _T("1");
				else if (name == _T("FileDelete"))
					dir.bFileDelete = value == _T("1");
				else if (name == _T("FileAppend"))
					dir.bFileAppend = value == _T("1");
				else if (name == _T("DirCreate"))
					dir.bDirCreate = value == _T("1");
				else if (name == _T("DirDelete"))
					dir.bDirDelete = value == _T("1");
				else if (name == _T("DirList"))
					dir.bDirList = value == _T("1");
				else if (name == _T("DirSubdirs"))
					dir.bDirSubdirs = value == _T("1");
				else if (name == _T("IsHome"))
					dir.bIsHome = value == _T("1");
				else if (name == _T("AutoCreate"))
					dir.bAutoCreate = value == _T("1");
			}

			//Avoid multiple home dirs
			if (dir.bIsHome)
				if (!bGotHome)
					bGotHome = TRUE;
				else
					dir.bIsHome = FALSE;

			if (user.permissions.size() < 20000)
				user.permissions.push_back(dir);
			pXML->OutOfElem();			
		}
		pXML->OutOfElem();
	}
}

void CPermissions::AutoCreateDirs(LPCTSTR username)
{
	// Create missing directores after a user has logged on
	CUser user;
	if (!GetUser(username, user))
		return;
	for (std::vector<t_directory>::iterator permissioniter = user.permissions.begin(); permissioniter != user.permissions.end(); permissioniter++)
		if (permissioniter->bAutoCreate)
		{
			CStdString dir = permissioniter->dir;
			user.DoReplacements(dir);
			
			dir += _T("\\");
			CStdString str;
			while (dir != _T(""))
			{
				int pos = dir.Find(_T("\\"));
				CStdString piece = dir.Left(pos + 1);
				dir = dir.Mid(pos + 1);
				
				str += piece;
				CreateDirectory(str, 0);
			}
		}
	if (user.pOwner)
		for (std::vector<t_directory>::iterator permissioniter = user.pOwner->permissions.begin(); permissioniter != user.pOwner->permissions.end(); permissioniter++)
			if (permissioniter->bAutoCreate)
			{
				CStdString dir = permissioniter->dir;
				user.DoReplacements(dir);

				dir += _T("\\");
				CStdString str;
				while (dir != _T(""))
				{
					int pos = dir.Find(_T("\\"));
					CStdString piece = dir.Left(pos + 1);
					dir = dir.Mid(pos + 1);
				
					str += piece;
					CreateDirectory(str, 0);
				}
			}
}

void CPermissions::ReadSpeedLimits(CMarkupSTL *pXML, t_group &group)
{
	pXML->ResetChildPos();
	
	const CStdString prefixes[] = { _T("Dl"), _T("Ul") };
	const CStdString names[] = { _T("Download"), _T("Upload") };

	while (pXML->FindChildElem(_T("SpeedLimits")))
	{
		CStdString str;
		int n;

		for (int i = 0; i < 2; i++)
		{
			str = pXML->GetChildAttrib(prefixes[i] + _T("Type"));
			n = _ttoi(str);
			if (n >= 0 && n < 4)
				group.nSpeedLimitType[i] = n;
			str = pXML->GetChildAttrib(prefixes[i] + _T("Limit"));
			n = _ttoi(str);
			if (n > 0 && n < 65536)
				group.nSpeedLimit[i] = n;

			str = pXML->GetChildAttrib(_T("Server") + prefixes[i] + _T("LimitBypass"));
			n = _ttoi(str);
			if (n >= 0 && n < 4)
				group.nBypassServerSpeedLimit[i] = n;

			pXML->IntoElem();

			while (pXML->FindChildElem(names[i]))
			{
				pXML->IntoElem();

				while (pXML->FindChildElem(_T("Rule")))
				{
					CSpeedLimit limit;
					str = pXML->GetChildAttrib(_T("Speed"));
					n = _ttoi(str);
					if (n < 0 || n > 65535)
						n = 10;
					limit.m_Speed = n;
			
					pXML->IntoElem();
			
					if (pXML->FindChildElem(_T("Days")))
					{
						str = pXML->GetChildData();
						if (str != _T(""))
							n = _ttoi(str);
						else
							n = 0x7F;
						limit.m_Day = n & 0x7F;
					}
					pXML->ResetChildPos();
			
					limit.m_DateCheck = FALSE;
					if (pXML->FindChildElem(_T("Date")))
					{
						limit.m_DateCheck = TRUE;
						str = pXML->GetChildAttrib(_T("Year"));
						n = _ttoi(str);
						if (n < 1900 || n > 3000)
							n = 2003;
						limit.m_Date.y = n;
						str = pXML->GetChildAttrib(_T("Month"));
						n = _ttoi(str);
						if (n < 1 || n > 12)
							n = 1;
						limit.m_Date.m = n;
						str = pXML->GetChildAttrib(_T("Day"));
						n = _ttoi(str);
						if (n < 1 || n > 31)
							n = 1;
						limit.m_Date.d = n;
					}
					pXML->ResetChildPos();
			
					limit.m_FromCheck = FALSE;
					if (pXML->FindChildElem(_T("From")))
					{
						limit.m_FromCheck = TRUE;
						str = pXML->GetChildAttrib(_T("Hour"));
						n = _ttoi(str);
						if (n < 0 || n > 23)
							n = 0;
						limit.m_FromTime.h = n;
						str = pXML->GetChildAttrib(_T("Minute"));
						n = _ttoi(str);
						if (n < 0 || n > 59)
							n = 0;
						limit.m_FromTime.m = n;
						str = pXML->GetChildAttrib(_T("Second"));
						n = _ttoi(str);
						if (n < 0 || n > 59)
							n = 0;
						limit.m_FromTime.s = n;
					}
					pXML->ResetChildPos();

					limit.m_ToCheck = FALSE;
					if (pXML->FindChildElem(_T("To")))
					{
						limit.m_ToCheck = TRUE;
						str = pXML->GetChildAttrib(_T("Hour"));
						n = _ttoi(str);
						if (n < 0 || n > 23)
							n = 0;
						limit.m_ToTime.h = n;
						str = pXML->GetChildAttrib(_T("Minute"));
						n = _ttoi(str);
						if (n < 0 || n > 59)
							n = 0;
						limit.m_ToTime.m = n;
						str = pXML->GetChildAttrib(_T("Second"));
						n = _ttoi(str);
						if (n < 0 || n > 59)
							n = 0;
						limit.m_ToTime.s = n;
					}
					pXML->ResetChildPos();
			
					pXML->OutOfElem();

					if (group.SpeedLimits[i].size() < 20000)
						group.SpeedLimits[i].push_back(limit);
				}
				pXML->OutOfElem();
			}
			pXML->ResetChildPos();
			pXML->OutOfElem();
		}
	}
}

void CPermissions::SaveSpeedLimits(CMarkupSTL *pXML, const t_group &group)
{
	pXML->AddChildElem(_T("SpeedLimits"));

	CStdString str;

	const CStdString prefixes[] = { _T("Dl"), _T("Ul") };
	const CStdString names[] = { _T("Download"), _T("Upload") };

	for (int i = 0; i < 2; i++)
	{
		pXML->SetChildAttrib(prefixes[i] + _T("Type"), group.nSpeedLimitType[i]);
		pXML->SetChildAttrib(prefixes[i] + _T("Limit"), group.nSpeedLimit[i]);
		pXML->SetChildAttrib(_T("Server") + prefixes[i] + _T("LimitBypass"), group.nBypassServerSpeedLimit[i]);
	
		pXML->IntoElem();

		pXML->AddChildElem(names[i]);
		pXML->IntoElem();
		for (unsigned int j = 0; j < group.SpeedLimits[i].size(); j++)
		{
			CSpeedLimit limit = group.SpeedLimits[i][j];
			pXML->AddChildElem(_T("Rule"));

			pXML->SetChildAttrib(_T("Speed"), limit.m_Speed);

			pXML->IntoElem();

			str.Format(_T("%d"), limit.m_Day);
			pXML->AddChildElem(_T("Days"), str);

			if (limit.m_DateCheck)
			{
				pXML->AddChildElem(_T("Date"));
				pXML->SetChildAttrib(_T("Year"), limit.m_Date.y);
				pXML->SetChildAttrib(_T("Month"), limit.m_Date.m);
				pXML->SetChildAttrib(_T("Day"), limit.m_Date.d);
			}

			if (limit.m_FromCheck)
			{
				pXML->AddChildElem(_T("From"));
				pXML->SetChildAttrib(_T("Hour"), limit.m_FromTime.h);
				pXML->SetChildAttrib(_T("Minute"), limit.m_FromTime.m);
				pXML->SetChildAttrib(_T("Second"), limit.m_FromTime.s);
			}
	
			if (limit.m_ToCheck)
			{
				pXML->AddChildElem(_T("To"));
				pXML->SetChildAttrib(_T("Hour"), limit.m_ToTime.h);
				pXML->SetChildAttrib(_T("Minute"), limit.m_ToTime.m);
				pXML->SetChildAttrib(_T("Second"), limit.m_ToTime.s);
			}

			pXML->OutOfElem();
		}
		pXML->OutOfElem();
		pXML->OutOfElem();
	}
}

void CPermissions::ReloadConfig()
{
	m_UsersList.clear();
	m_GroupsList.clear();

	EnterCritSection(m_sync);

	ReadSettings();

	UpdateInstances();

	LeaveCritSection(m_sync);

	return;
}

void CPermissions::ReadIpFilter(CMarkupSTL *pXML, t_group &group)
{
	pXML->ResetChildPos();
	while (pXML->FindChildElem(_T("IpFilter")))
	{
		pXML->IntoElem();
		while (pXML->FindChildElem(_T("Disallowed")))
		{
			pXML->IntoElem();
			while (pXML->FindChildElem(_T("IP")))
			{
				CStdString ip = pXML->GetChildData();
				if (group.disallowedIPs.size() >= 20000)
					break;

				if (ip == _T("*"))
					group.disallowedIPs.push_back(ip);
				else
				{
					if (IsValidAddressFilter(ip))
						group.disallowedIPs.push_back(ip);
				}
			}
			pXML->OutOfElem();
		}
		pXML->ResetChildPos();
		while (pXML->FindChildElem(_T("Allowed")))
		{
			pXML->IntoElem();
			while (pXML->FindChildElem(_T("IP")))
			{
				CStdString ip = pXML->GetChildData();
				if (group.allowedIPs.size() >= 20000)
					break;

				if (ip == _T("*"))
					group.allowedIPs.push_back(ip);
				else
				{
					if (IsValidAddressFilter(ip))
						group.allowedIPs.push_back(ip);
				}
			}
			pXML->OutOfElem();
		}
		pXML->OutOfElem();
	}
}

void CPermissions::SaveIpFilter(CMarkupSTL *pXML, const t_group &group)
{
	pXML->AddChildElem(_T("IpFilter"));
	pXML->IntoElem();
	pXML->AddChildElem(_T("Disallowed"));
	pXML->IntoElem();
	std::list<CStdString>::const_iterator iter;
	for (iter = group.disallowedIPs.begin(); iter != group.disallowedIPs.end(); iter++)
	{
		pXML->AddChildElem(_T("IP"));
		pXML->SetChildData(*iter);
	}
	pXML->OutOfElem();

	pXML->AddChildElem(_T("Allowed"));
	pXML->IntoElem();
	for (iter = group.allowedIPs.begin(); iter != group.allowedIPs.end(); iter++)
	{
		pXML->AddChildElem(_T("IP"));
		pXML->SetChildData(*iter);
	}
	pXML->OutOfElem();

	pXML->OutOfElem();
}

CStdString CPermissions::CanonifyServerDir(CStdString currentDir, CStdString newDir) const
{
	/*
	 * CanonifyPath takes the current and the new server dir as parameter,
	 * concats the paths if neccessary and canonifies the dir:
	 * - remove dot-segments
	 * - convert backslashes into slashes
	 * - remove double slashes
	 */

	if (newDir == _T(""))
		return currentDir;

	// Make segment separators pretty
	newDir.Replace(_T("\\"), _T("/"));
	while (newDir.Replace(_T("//"), _T("/")));
	
	if (newDir == _T("/"))
		return newDir;
	
	// This list will hold the individual path segments
	std::list<CStdString> piecelist;
	
	/*
	 * Check the type of the path: Absolute or relative?
	 * On relative paths, use currentDir as base, else use
	 * only dir.
	 */
	if (newDir.Left(1) != _T("/"))
	{
		// New relative path, split currentDir and add it to the piece list.
		currentDir.TrimLeft(_T("/"));
		int pos;
		while((pos = currentDir.Find(_T("/"))) != -1)
		{
			piecelist.push_back(currentDir.Left(pos));
			currentDir = currentDir.Mid(pos + 1);
		}
		if (currentDir != _T(""))
			piecelist.push_back(currentDir);
	}
	
	/*
	 * Now split up the new dir into individual segments. Here we
	 * check for dot segments and remove the proper number of segments
	 * from the piece list on dots.
	 */
	
	int pos;
	newDir.TrimLeft(_T("/"));
	if (newDir.Right(1) != _T("/"))
		newDir += _T("/");
	while ((pos = newDir.Find(_T("/"))) != -1)
	{
		CStdString piece = newDir.Left(pos);
		newDir = newDir.Mid(pos + 1);

		if (piece == _T(""))
			continue;

		bool allDots = true;
		int dotCount = 0;
		for (int i = 0; i < piece.GetLength(); i++)
			if (piece[i] != '.')
			{
				allDots = false;
				break;
			}
			else
				dotCount++;

		if (allDots)
		{
			while (--dotCount)
			{
				if (!piecelist.empty())
					piecelist.pop_back();
			}
		}
		else
			piecelist.push_back(piece);
	}

	// Reassemble the directory
	CStdString result;

	if (piecelist.empty())
		return _T("/");

	// List of reserved filenames which may not be used on a Windows system
	static LPCTSTR reservedNames[] = {	_T("CON"),	_T("PRN"),	_T("AUX"),	_T("CLOCK$"), _T("NUL"),
										_T("COM1"), _T("COM2"), _T("COM3"), _T("COM4"), _T("COM5"),
										_T("COM6"), _T("COM7"), _T("COM8"), _T("COM9"),
										_T("LPT1"), _T("LPT2"), _T("LPT3"), _T("LPT4"), _T("LPT5"),
										_T("LPT6"), _T("LPT7"), _T("LPT8"), _T("LPT9"),
										_T("") };

	for (std::list<CStdString>::iterator iter = piecelist.begin(); iter != piecelist.end(); iter++)
	{
		// Check for reserved filenames
		CStdString piece = *iter;
		piece.MakeUpper();
		for (LPCTSTR *reserved = reservedNames; **reserved; reserved++)
		{
			if (piece == *reserved)
				return _T("");
		}
		int pos = piece.Find(_T("."));
		if (pos >= 3)
		{
			piece = piece.Left(pos);
			for (LPCTSTR *reserved = reservedNames; **reserved; reserved++)
			{
				if (piece == *reserved)
					return _T("");
			}
		}

		result += _T("/") + *iter;
	}

	// Now dir is the canonified absolute server path.
	return result;
}

int CPermissions::GetFact(LPCTSTR username, CStdString currentDir, CStdString file, CStdString& fact, CStdString& logicalName, bool enabledFacts[3])
{
	// Get user from username
	CUser user;
	if (!GetUser(username, user))
		return PERMISSION_DENIED; // No user found

	CStdString dir = CanonifyServerDir(currentDir, file);
	if (dir == _T(""))
		return PERMISSION_INVALIDNAME;
	logicalName = dir;

	t_directory directory;
	BOOL bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	if (res == PERMISSION_FILENOTDIR)
	{
		if (dir == _T("/"))
			return res;

		int pos = dir.ReverseFind('/');
		if (pos == -1)
			return res;

		CStdString dir2;
		if (pos)
			dir2 = dir.Left(pos);
		else
			dir2 = _T("/");

		CStdString fn = dir.Mid(pos + 1);
		int res = GetRealDirectory(dir2, user, directory, bTruematch);
		if (res)
			return res | PERMISSION_FILENOTDIR;

		if (!directory.bFileRead)
			return PERMISSION_DENIED;

		file = directory.dir + _T("\\") + fn;
		
		if (enabledFacts[0])
			fact = _T("type=file;");
		else
			fact = _T("");
	}
	else if (res)
		return res;
	else
	{
		if (!directory.bDirList)
			return PERMISSION_DENIED;

		if (!bTruematch && !directory.bDirSubdirs)
			return PERMISSION_DENIED;

		file = directory.dir;

		if (enabledFacts[0])
			fact = _T("type=dir;");
		else
			fact = _T("");
	}

	CFileStatus64 status;
	if (GetStatus64(file, status))
	{
		if (enabledFacts[1] && !(status.m_attribute & FILE_ATTRIBUTE_DIRECTORY))
		{
			CStdString str;
			str.Format(_T("size=%I64d;"), status.m_size);
			fact += str;
		}

		if (enabledFacts[2])
		{
			// Get last modification time
			FILETIME ftime = status.m_mtime;
			if (!ftime.dwHighDateTime && !ftime.dwLowDateTime)
				ftime = status.m_ctime;
			if (ftime.dwHighDateTime || ftime.dwLowDateTime)
			{
				SYSTEMTIME time;
				FileTimeToSystemTime(&ftime, &time);
				CStdString str;
				str.Format(_T("modify=%04d%02d%02d%02d%02d%02d;"),
					time.wYear,
					time.wMonth,
					time.wDay,
					time.wHour,
					time.wMinute,
					time.wSecond);

				fact += str;
			}
		}
	}

	fact += _T(" ") + logicalName;

	return 0;
}

void CUser::PrepareAliasMap()
{
	/*
	 * Prepare the alias map.
	 * For fast access, aliases are stored as key/value pairs.
	 * The key is the folder part of the alias.
	 * The value is a structure containing the name of the alias
	 * and the target folder.
	 * Example:
	 * Shared folder c:\myfolder, alias d:\myotherfolder\myalias
	 * Key: d:\myotherfolder, Value = myalias, c:\myfolder
	 */

	aliasMap.clear();
	virtualAliases.clear();
	std::vector<t_directory>::const_iterator permIter;
	std::list<CStdString>::const_iterator aliasIter;
	for (permIter = permissions.begin(); permIter != permissions.end(); permIter++)
	{
		for (aliasIter = permIter->aliases.begin(); aliasIter != permIter->aliases.end(); aliasIter++)
		{
			CStdString alias = *aliasIter;
			DoReplacements(alias);

			if (alias[0] == '/')
			{
				int pos = alias.ReverseFind('/');
				CStdString dir = alias.Left(pos);
				if (dir == _T(""))
					dir = _T("/");
				dir.MakeLower();
				virtualAliasNames.insert(std::pair<CStdString, CStdString>(dir, alias.Mid(pos + 1)));
				alias.MakeLower();
				virtualAliases[alias + _T("/")] = permIter->dir;
				DoReplacements(virtualAliases[alias + _T("/")]);
				continue;
			}

			int pos = alias.ReverseFind('\\');
			if (pos == -1)
				continue;
			t_alias aliasStruct;
			aliasStruct.name = alias.Mid(pos + 1);
			if (aliasStruct.name == _T(""))
				continue;
			alias = alias.Left(pos);

			alias.MakeLower();
			aliasStruct.targetFolder = permIter->dir;
			aliasStruct.targetFolder.MakeLower();
			DoReplacements(aliasStruct.targetFolder);

			aliasMap.insert(std::pair<CStdString, t_alias>(alias, aliasStruct));
		}
	}

	if (!pOwner)
		return;

	for (permIter = pOwner->permissions.begin(); permIter != pOwner->permissions.end(); permIter++)
	{
		for (aliasIter = permIter->aliases.begin(); aliasIter != permIter->aliases.end(); aliasIter++)
		{
			CStdString alias = *aliasIter;
			DoReplacements(alias);

			if (alias[0] == '/')
			{
				int pos = alias.ReverseFind('/');
				CStdString dir = alias.Left(pos);
				if (dir == _T(""))
					dir = _T("/");
				dir.MakeLower();
				virtualAliasNames.insert(std::pair<CStdString, CStdString>(dir, alias.Mid(pos + 1)));
				alias.MakeLower();
				virtualAliases[alias + _T("/")] = permIter->dir;
				DoReplacements(virtualAliases[alias + _T("/")]);
				continue;
			}

			int pos = alias.ReverseFind('\\');
			if (pos == -1)
				continue;
			t_alias aliasStruct;
			aliasStruct.name = alias.Mid(pos + 1);
			if (aliasStruct.name == _T(""))
				continue;
			alias = alias.Left(pos);
			
			alias.MakeLower();
			aliasStruct.targetFolder = permIter->dir;
			aliasStruct.targetFolder.MakeLower();
			DoReplacements(aliasStruct.targetFolder);
		
			aliasMap.insert(std::pair<CStdString, t_alias>(alias, aliasStruct));
		}
	}
}

CStdString CUser::GetAliasTarget(const CStdString& path, const CStdString& virtualPath, const CStdString& name) const
{
	// Find the target for the alias with the specified path and name

	for (std::multimap<CStdString, CUser::t_alias>::const_iterator iter = aliasMap.find(path); iter != aliasMap.end() && iter->first == path; iter++)
	{
		if (!iter->second.name.CompareNoCase(name))
			return iter->second.targetFolder;
	}

	{ // braces needed because VC++7.1 complains about perfectly valid code
		std::map<CStdString, CStdString>::const_iterator iter = virtualAliases.find(virtualPath);
		if (iter != virtualAliases.end())
			return iter->second;
	}

	return _T("");
}

void CPermissions::ReadSettings()
{
	CMarkupSTL *pXML = COptions::GetXML();
	if (!pXML)
		return;
    
	if (!pXML->FindChildElem(_T("Groups")))
		pXML->AddChildElem(_T("Groups"));
	pXML->IntoElem();
	while (pXML->FindChildElem(_T("Group")))
	{
		t_group group;
		group.nIpLimit = group.nIpLimit = group.nUserLimit = 0;
		group.nBypassUserLimit = 2;
		group.group = pXML->GetChildAttrib(_T("Name"));
		if (group.group != _T(""))
		{
			pXML->IntoElem();
		
			while (pXML->FindChildElem(_T("Option")))
			{
				CStdString name = pXML->GetChildAttrib(_T("Name"));
				CStdString value = pXML->GetChildData();
				if (name == _T("Bypass server userlimit"))
					group.nBypassUserLimit = _ttoi(value);
				else if (name == _T("User Limit"))
					group.nUserLimit = _ttoi(value);
				else if (name == _T("IP Limit"))
					group.nIpLimit = _ttoi(value);
				else if (name == _T("Enabled"))
					group.nEnabled = _ttoi(value);
				else if (name == _T("Comments"))
					group.comment = value;
				else if (name == _T("ForceSsl"))
					group.forceSsl = _ttoi(value);

				if (group.nUserLimit<0 || group.nUserLimit>999999999)
					group.nUserLimit=0;
				if (group.nIpLimit<0 || group.nIpLimit>999999999)
					group.nIpLimit=0;
			}

			ReadIpFilter(pXML, group);
				
			BOOL bGotHome = FALSE;
			ReadPermissions(pXML, group, bGotHome);
			//Set a home dir if no home dir could be read
			if (!bGotHome && !group.permissions.empty())
				group.permissions.begin()->bIsHome = TRUE;

			ReadSpeedLimits(pXML, group);
				
			if (m_GroupsList.size() < 20000)
				m_GroupsList.push_back(group);
			pXML->OutOfElem();
		}
	}
	pXML->OutOfElem();
	pXML->ResetChildPos();
		
	if (!pXML->FindChildElem(_T("Users")))
		pXML->AddChildElem(_T("Users"));
	pXML->IntoElem();

	while (pXML->FindChildElem(_T("User")))
	{
		CUser user;
		user.nIpLimit = user.nIpLimit = user.nUserLimit = 0;
		user.nBypassUserLimit = 2;
		user.user=pXML->GetChildAttrib(_T("Name"));
		if (user.user == _T(""))
			continue;

		pXML->IntoElem();

		while (pXML->FindChildElem(_T("Option")))
		{
			CStdString name = pXML->GetChildAttrib(_T("Name"));
			CStdString value = pXML->GetChildData();
			if (name == _T("Pass"))
			{
				// If provided password is not a MD5 has, convert it into a MD5 hash
				if (value != _T("") && value.GetLength() != 32)
				{
					char *tmp = ConvToNetwork(value);
					if (!tmp)
					{
						tmp = new char[1];
						tmp[0] = 0;
					}
					MD5 md5;
					md5.update((unsigned char *)tmp, strlen(tmp));
					md5.finalize();
					char *res = md5.hex_digest();
#ifdef _UNICODE
					pXML->SetChildData(ConvFromNetwork(res));
#else
					pXML->SetChildData(res);
#endif
					user.password = res;
					delete [] tmp;
					delete [] res;
				}
				else
					user.password = value;
			}
			else if (name == _T("Bypass server userlimit"))
				user.nBypassUserLimit = _ttoi(value);
			else if (name == _T("User Limit"))
				user.nUserLimit = _ttoi(value);
			else if (name == _T("IP Limit"))
				user.nIpLimit = _ttoi(value);
			else if (name == _T("Group"))
				user.group = value;
			else if (name == _T("Enabled"))
				user.nEnabled = _ttoi(value);
			else if (name == _T("Comments"))
				user.comment = value;
			else if (name == _T("ForceSsl"))
				user.forceSsl = _ttoi(value);

			if (user.nUserLimit < 0 || user.nUserLimit > 999999999)
				user.nUserLimit = 0;
			if (user.nIpLimit < 0 || user.nIpLimit > 999999999)
				user.nIpLimit = 0;
		}

		if (user.group != _T(""))
		{
			for (t_GroupsList::iterator groupiter = m_GroupsList.begin(); groupiter != m_GroupsList.end(); groupiter++)
				if (groupiter->group == user.group)
				{
					user.pOwner = &(*groupiter);
					break;
				}
		
			if (!user.pOwner)
				user.group = _T("");
		}
			
		ReadIpFilter(pXML, user);

		BOOL bGotHome = FALSE;
		ReadPermissions(pXML, user, bGotHome);
		user.PrepareAliasMap();
				
		//Set a home dir if no home dir could be read
		if (!bGotHome && !user.pOwner)
		{
			if (!user.permissions.empty())
				user.permissions.begin()->bIsHome = TRUE;
		}
			
		std::vector<t_directory>::iterator iter;
		for (iter = user.permissions.begin(); iter != user.permissions.end(); iter++)
		{
			if (iter->bIsHome)
			{
				user.homedir = iter->dir;
				break;
			}
		}
		if (user.homedir == _T("") && user.pOwner)
		{
			for (iter = user.pOwner->permissions.begin(); iter != user.pOwner->permissions.end(); iter++)
			{
				if (iter->bIsHome)
				{
					user.homedir = iter->dir;
					break;
				}
			}
		}
			
		ReadSpeedLimits(pXML, user);

		if (m_UsersList.size() < 20000)
			m_UsersList.push_back(user);
		pXML->OutOfElem();
	}
	COptions::FreeXML(pXML);

	EnterCritSection(m_sync);

	m_sGroupsList.clear();
	for (t_GroupsList::iterator groupiter = m_GroupsList.begin(); groupiter != m_GroupsList.end(); groupiter++)
		m_sGroupsList.push_back(*groupiter);

	m_sUsersList.clear();
	for (t_UsersList::iterator iter = m_UsersList.begin(); iter != m_UsersList.end(); iter++)
	{
		CUser user = *iter;
		user.pOwner = NULL;
		if (user.group != _T(""))
		{
			for (t_GroupsList::iterator groupiter = m_GroupsList.begin(); groupiter != m_GroupsList.end(); groupiter++)
				if (groupiter->group == user.group)
				{
					user.pOwner = &(*groupiter);
					break;
				}
		}
		m_sUsersList.push_back(user);
	}

	LeaveCritSection(m_sync);
}

// Replace :u and :g (if a group it exists)
void CUser::DoReplacements(CStdString& path) const
{
	path.Replace(_T(":u"), user);
	path.Replace(_T(":U"), user);
	if (group != _T(""))
	{
		path.Replace(_T(":g"), group);
		path.Replace(_T(":G"), group);
	}
}

bool CPermissions::WildcardMatch(CStdString string, CStdString pattern) const
{
	if (pattern == _T("*") || pattern == _T("*.*"))
		return true;

	// Do a really primitive wildcard check, does even ignore ?
	string.MakeLower();
	pattern.MakeLower();

	bool starFirst = false;
	while (pattern != _T(""))
	{
		int pos = pattern.Find('*');
		if (pos == -1)
		{
			if (starFirst)
			{
				if (string.GetLength() > pattern.GetLength())
					string = string.Right(pattern.GetLength());
			}
			if (pattern != string)
				return false;
			else
				return true;
		}
		else if (!pos)
		{
			starFirst = true;
			pattern = pattern.Mid(1);
		}
		else
		{
			int npos = string.Find(pattern.Left(pos));
			if (npos == -1)
				return false;
			if (npos && !starFirst)
				return false;
			pattern = pattern.Mid(pos + 1);
			string = string.Mid(npos + pos);

			starFirst = true;
		}
	}
	return true;
}

char* CPermissions::ConvertFilename(const CStdString& filename, bool useUTF8)
{
	if (useUTF8)
		return ConvToNetwork(filename);
	
	const CStdStringA& tmp = ConvToLocal(filename);
    
	char* str = new char[strlen(tmp) + 1];
	strcpy(str, tmp);
	return str;
}

void CPermissions::DestroyDirlisting(struct t_dirlisting* pListing)
{
	while (pListing)
	{
		t_dirlisting *pPrev = pListing;
		pListing = pListing->pNext;
		delete pPrev;
	}
}
