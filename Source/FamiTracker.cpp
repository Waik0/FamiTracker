/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

#include "stdafx.h"
#include "Exception.h"
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "FamiTrackerView.h"
#include "MainFrm.h"
#include "AboutDlg.h"
#include "TrackerChannel.h"
#include "MIDI.h"
#include "SoundGen.h"
#include "Accelerator.h"
#include "Settings.h"
#include "ChannelMap.h"
#include "CustomExporters.h"
#include "CommandLineExport.h"

#ifdef EXPORT_TEST
#include "ExportTest/ExportTest.h"
#endif /* EXPORT_TEST */

// Single instance-stuff
const TCHAR FT_SHARED_MUTEX_NAME[]	= _T("FamiTrackerMutex");	// Name of global mutex
const TCHAR FT_SHARED_MEM_NAME[]	= _T("FamiTrackerWnd");		// Name of global memory area
const DWORD	SHARED_MEM_SIZE			= 256;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef RELEASE_BUILD
#pragma message("Building SVN release build...")
#endif /* RELEASE_BUILD */

// CFamiTrackerApp

BEGIN_MESSAGE_MAP(CFamiTrackerApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
#ifdef UPDATE_CHECK
	ON_COMMAND(ID_HELP_CHECKFORNEWVERSIONS, CheckNewVersion)
#endif
#ifdef EXPORT_TEST
	ON_COMMAND(ID_MODULE_TEST_EXPORT, OnTestExport)
#endif
END_MESSAGE_MAP()

// Include this for windows xp style in visual studio 2005 or later
#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df' language='*'\"")

// CFamiTrackerApp construction

CFamiTrackerApp::CFamiTrackerApp() :
	m_bThemeActive(false),
	m_pMIDI(NULL),
	m_pAccel(NULL),
	m_pSettings(NULL),
	m_pSoundGenerator(NULL),
	m_pChannelMap(NULL),
	m_customExporters(NULL),
	m_hWndMapFile(NULL),
#ifdef SUPPORT_TRANSLATIONS
	m_hInstResDLL(NULL),
#endif
#ifdef EXPORT_TEST
	m_bExportTesting(false),
#endif
	m_pInstanceMutex(NULL)
{
	// Place all significant initialization in InitInstance
	EnableHtmlHelp();

#ifdef ENABLE_CRASH_HANDLER
	// This will cover the whole process
	InstallExceptionHandler();
#endif /* ENABLE_CRASH_HANDLER */
}


// The one and only CFamiTrackerApp object
CFamiTrackerApp	theApp;

// CFamiTrackerApp initialization

BOOL CFamiTrackerApp::InitInstance()
{
	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	InitCommonControls();
#ifdef SUPPORT_TRANSLATIONS
	LoadLocalization();
#endif
	CWinApp::InitInstance();

	TRACE("App: InitInstance\n");

	if (!AfxOleInit()) {
		TRACE0("OLE initialization failed\n");
	}

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T(""));
	LoadStdProfileSettings(8);  // Load standard INI file options (including MRU)

	// Load program settings
	m_pSettings = CSettings::GetObject();
	m_pSettings->LoadSettings();

	// Parse command line for standard shell commands, DDE, file open + some custom ones
	CFTCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if (CheckSingleInstance(cmdInfo))
		return FALSE;

	//who: added by Derek Andrews <derek.george.andrews@gmail.com>
	//why: Load all custom exporter plugins on startup.
	
	TCHAR pathToPlugins[MAX_PATH];
	GetModuleFileName(NULL, pathToPlugins, MAX_PATH);
	PathRemoveFileSpec(pathToPlugins);
	PathAppend(pathToPlugins, _T("\\Plugins"));
	m_customExporters = new CCustomExporters( pathToPlugins );

	// Load custom accelerator
	m_pAccel = new CAccelerator();
	m_pAccel->LoadShortcuts(m_pSettings);
	m_pAccel->Setup();

	// Create the MIDI interface
	m_pMIDI = new CMIDI();

	// Create sound generator
	m_pSoundGenerator = new CSoundGen();

	// Create channel map
	m_pChannelMap = new CChannelMap();

	// Start sound generator thread, initially suspended
	if (!m_pSoundGenerator->CreateThread(CREATE_SUSPENDED)) {
		// If failed, restore and save default settings
		m_pSettings->DefaultSettings();
		m_pSettings->SaveSettings();
		// Show message and quit
		AfxMessageBox(IDS_START_ERROR, MB_ICONERROR);
		return FALSE;
	}

	// Check if the application is themed
	CheckAppThemed();

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME, 
		RUNTIME_CLASS(CFamiTrackerDoc), 
		RUNTIME_CLASS(CMainFrame), 
		RUNTIME_CLASS(CFamiTrackerView));

	if (!pDocTemplate)
		return FALSE;
	
	AddDocTemplate(pDocTemplate);

	// Determine windows version
    OSVERSIONINFO osvi;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);

	// Work-around to enable file type registration in windows vista/7
	if (osvi.dwMajorVersion >= 6) { 
		HKEY HKCU;
		long res_reg = ::RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Classes"), &HKCU);
		if(res_reg == ERROR_SUCCESS)
			RegOverridePredefKey(HKEY_CLASSES_ROOT, HKCU);
	}

	// Enable DDE Execute open
	EnableShellOpen();

	// Skip this if in wip/beta mode
#if /*!defined(WIP) &&*/ !defined(_DEBUG)
	// Add shell options
	RegisterShellFileTypes(TRUE);
	// Add an option to play files
	CString strPathName, strTemp, strFileTypeId;
	AfxGetModuleShortFileName(AfxGetInstanceHandle(), strPathName);
	CString strOpenCommandLine = strPathName;
	strOpenCommandLine += _T(" /play \"%1\"");
	if (pDocTemplate->GetDocString(strFileTypeId, CDocTemplate::regFileTypeId) && !strFileTypeId.IsEmpty()) {
		strTemp.Format(_T("%s\\shell\\play\\%s"), (LPCTSTR)strFileTypeId, _T("command"));
		AfxRegSetValue(HKEY_CLASSES_ROOT, strTemp, REG_SZ, strOpenCommandLine, lstrlen(strOpenCommandLine) * sizeof(TCHAR));
	}
#endif

	// Handle command line export
	if (cmdInfo.m_bExport) {
		CCommandLineExport exporter;
		exporter.CommandLineExport(cmdInfo.m_strFileName, cmdInfo.m_strExportFile, cmdInfo.m_strExportLogFile, cmdInfo.m_strExportDPCMFile);
		ExitProcess(0);
	}

	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo)) {
		if (cmdInfo.m_nShellCommand == CCommandLineInfo::AppUnregister) {
			// Also clear settings from registry when unregistering
			m_pSettings->DeleteSettings();
		}
		return FALSE;
	}

	// Move root key back to default
	if (osvi.dwMajorVersion >= 6) { 
		::RegOverridePredefKey(HKEY_CLASSES_ROOT, NULL);
	}

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(m_nCmdShow);
	m_pMainWnd->UpdateWindow();
	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand
	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();
	
	// Initialize the sound interface, also resumes the thread
	if (!m_pSoundGenerator->InitializeSound(m_pMainWnd->m_hWnd)) {
		// If failed, restore and save default settings
		m_pSettings->DefaultSettings();
		m_pSettings->SaveSettings();
		// Quit program
		AfxMessageBox(IDS_START_ERROR, MB_ICONERROR);
		return FALSE;
	}
	
	// Initialize midi unit
	m_pMIDI->Init();
	
	if (cmdInfo.m_bPlay)
		theApp.StartPlayer(MODE_PLAY);

#ifdef EXPORT_TEST
	if (cmdInfo.m_bVerifyExport) {
		m_bExportTesting = true;
		VerifyExport(cmdInfo.m_strVerifyFile);
	}
	else {
		// Append menu option
		m_pMainWnd->GetMenu()->GetSubMenu(2)->AppendMenu(MF_SEPARATOR);
		m_pMainWnd->GetMenu()->GetSubMenu(2)->AppendMenu(MF_STRING, ID_MODULE_TEST_EXPORT, _T("Test exporter"));
	}
#endif

	// Save the main window handle
	RegisterSingleInstance();

#ifndef _DEBUG
	// WIP
	m_pMainWnd->GetMenu()->GetSubMenu(2)->RemoveMenu(ID_MODULE_CHANNELS, MF_BYCOMMAND);
#endif

	// Initialization is done
	TRACE0("App: InitInstance done\n");

	return TRUE;
}

int CFamiTrackerApp::ExitInstance()
{
	// Close program
	// The document is already closed at this point (and detached from sound player)

	TRACE("App: Begin ExitInstance\n");

	UnregisterSingleInstance();

	ShutDownSynth();

	if (m_pMIDI) {
		m_pMIDI->Shutdown();
		delete m_pMIDI;
		m_pMIDI = NULL;
	}

	if (m_pAccel) {
		m_pAccel->SaveShortcuts(m_pSettings);
		m_pAccel->Shutdown();
		delete m_pAccel;
		m_pAccel = NULL;
	}

	if (m_pSettings) {
		m_pSettings->SaveSettings();
		m_pSettings = NULL;
	}

	if (m_customExporters) {
		delete m_customExporters;
		m_customExporters = NULL;
	}

	if (m_pChannelMap) {
		delete m_pChannelMap;
		m_pChannelMap = NULL;
	}

#ifdef SUPPORT_TRANSLATIONS
	if (m_hInstResDLL) {
		// Revert back to internal resources
		AfxSetResourceHandle(m_hInstance);
		// Unload DLL
		::FreeLibrary(m_hInstResDLL);
		m_hInstResDLL = NULL;
	}
#endif

	TRACE0("App: End ExitInstance\n");

	return CWinApp::ExitInstance();
}

BOOL CFamiTrackerApp::PreTranslateMessage(MSG* pMsg)
{
	if (CWinApp::PreTranslateMessage(pMsg)) {
		return TRUE;
	}
	else if (m_pMainWnd != NULL && m_pAccel != NULL) {
		if (m_pAccel->Translate(m_pMainWnd->m_hWnd, pMsg)) {
			return TRUE;
		}
	}

	return FALSE;
}

void CFamiTrackerApp::CheckAppThemed()
{
	HMODULE hinstDll = ::LoadLibrary(_T("UxTheme.dll"));
	
	if (hinstDll) {
		typedef BOOL (*ISAPPTHEMEDPROC)();
		ISAPPTHEMEDPROC pIsAppThemed;
		pIsAppThemed = (ISAPPTHEMEDPROC) ::GetProcAddress(hinstDll, "IsAppThemed");

		if(pIsAppThemed)
			m_bThemeActive = (pIsAppThemed() == TRUE);

		::FreeLibrary(hinstDll);
	}
}

bool CFamiTrackerApp::IsThemeActive() const
{ 
	return m_bThemeActive;
}

bool GetFileVersion(LPCTSTR Filename, WORD &Major, WORD &Minor, WORD &Revision, WORD &Build)
{
	DWORD Handle;
	DWORD Size = GetFileVersionInfoSize(Filename, &Handle);
	bool Success = true;

	Major = 0;
	Minor = 0;
	Revision = 0;
	Build = 0;

	if (Size > 0) {
		TCHAR *pData = new TCHAR[Size];
		if (GetFileVersionInfo(Filename, NULL, Size, pData) != 0) {
			UINT size;
			VS_FIXEDFILEINFO *pFileinfo;
			if (VerQueryValue(pData, _T("\\"), (LPVOID*)&pFileinfo, &size) != 0) {
				Major = pFileinfo->dwProductVersionMS >> 16;
				Minor = pFileinfo->dwProductVersionMS & 0xFFFF;
				Revision = pFileinfo->dwProductVersionLS >> 16;
				Build = pFileinfo->dwProductVersionLS & 0xFFFF;
			}
			else
				Success = false;
		}
		else 
			Success = false;

		SAFE_RELEASE_ARRAY(pData);
	}
	else
		Success = false;

	return Success;
}

#ifdef SUPPORT_TRANSLATIONS
void CFamiTrackerApp::LoadLocalization()
{
	LPCTSTR DLL_NAME = _T("language.dll");
	WORD Major, Minor, Build, Revision;

	if (GetFileVersion(DLL_NAME, Major, Minor, Revision, Build)) {
		if (Major != VERSION_MAJ || Minor != VERSION_MIN || Revision != VERSION_REV)
			return;

		m_hInstResDLL = ::LoadLibrary(DLL_NAME);

		if (m_hInstResDLL != NULL) {
			TRACE0("App: Loaded localization DLL\n");
			AfxSetResourceHandle(m_hInstResDLL);
		}
	}
}
#endif

void CFamiTrackerApp::ShutDownSynth()
{
	// Shut down sound generator
	if (m_pSoundGenerator == NULL) {
		TRACE0("App: Sound generator object was not available\n");
		return;
	}

	// Save a handle to the thread since the object will delete itself
	HANDLE hThread = m_pSoundGenerator->m_hThread;

	if (hThread == NULL) {
		// Object was found but thread not created
		delete m_pSoundGenerator;
		m_pSoundGenerator = NULL;
		TRACE0("App: Sound generator object was found but no thread created\n");
		return;
	}

	TRACE0("App: Waiting for sound player thread to close\n");

	// Resume if thread was suspended
	if (m_pSoundGenerator->ResumeThread() == 0) {
		// Thread was not suspended, send quit message
		// Note that this object may be deleted now!
		m_pSoundGenerator->PostThreadMessage(WM_QUIT, 0, 0);
	}
	// If thread was suspended then it will auto-terminate, because sound hasn't been initialized

	// Wait for thread to exit
	DWORD dwResult = ::WaitForSingleObject(hThread, CSoundGen::AUDIO_TIMEOUT + 1000);

	if (dwResult != WAIT_OBJECT_0 && m_pSoundGenerator != NULL) {
		TRACE0("App: Closing the sound generator thread failed\n");
#ifdef _DEBUG
		AfxMessageBox(_T("Error: Could not close sound generator thread"));
#endif
		// Unclean exit
		return;
	}

	// Object should be auto-deleted
	ASSERT(m_pSoundGenerator == NULL);

	TRACE0("App: Sound generator has closed\n");
}

void CFamiTrackerApp::RemoveSoundGenerator()
{
	// Sound generator object has been deleted, remove reference
	m_pSoundGenerator = NULL;
}

CCustomExporters* CFamiTrackerApp::GetCustomExporters(void) const
{
	return m_customExporters;
}

void CFamiTrackerApp::RegisterSingleInstance()
{
	// Create a memory area with this app's window handle
	if (!GetSettings()->General.bSingleInstance)
		return;

	m_hWndMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHARED_MEM_SIZE, FT_SHARED_MEM_NAME);

	if (m_hWndMapFile != NULL) {
		LPTSTR pBuf = (LPTSTR) MapViewOfFile(m_hWndMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
		if (pBuf != NULL) { 
			// Create a string of main window handle
			_itot_s((int)GetMainWnd()->m_hWnd, pBuf, SHARED_MEM_SIZE, 10);
			UnmapViewOfFile(pBuf);
		}
	}
}

void CFamiTrackerApp::UnregisterSingleInstance()
{	
	// Close shared memory area
	if (m_hWndMapFile) {
		CloseHandle(m_hWndMapFile);
		m_hWndMapFile = NULL;
	}

	SAFE_RELEASE(m_pInstanceMutex);
}

bool CFamiTrackerApp::CheckSingleInstance(CFTCommandLineInfo &cmdInfo)
{	
	// Returns true if program should close
	
	if (!GetSettings()->General.bSingleInstance)
		return false;

	if (cmdInfo.m_bExport)
		return false;

	m_pInstanceMutex = new CMutex(FALSE, FT_SHARED_MUTEX_NAME);

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		// Another instance detected, get window handle
		HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FT_SHARED_MEM_NAME);
		if (hMapFile != NULL) {	
			LPCTSTR pBuf = (LPTSTR) MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
			if (pBuf != NULL) {
				// Get window handle
				HWND hWnd = (HWND)_ttoi(pBuf);
				if (hWnd != NULL) {
					// Get file name
					LPTSTR pFilePath = cmdInfo.m_strFileName.GetBuffer();
					// We have the window handle & file, send a message to open the file
					COPYDATASTRUCT data;
					data.dwData = cmdInfo.m_bPlay ? IPC_LOAD_PLAY : IPC_LOAD;
					data.cbData = (DWORD)((_tcslen(pFilePath) + 1) * sizeof(TCHAR));
					data.lpData = pFilePath;
					DWORD result;
					SendMessageTimeout(hWnd, WM_COPYDATA, NULL, (LPARAM)&data, SMTO_NORMAL, 100, &result);
					UnmapViewOfFile(pBuf);
					CloseHandle(hMapFile);
					TRACE("App: Found another instance, shutting down\n");
					// Then close the program
					return true;
				}

				UnmapViewOfFile(pBuf);
			}
			CloseHandle(hMapFile);
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////
//  Things that belongs to the synth are kept below!  //
////////////////////////////////////////////////////////

// Load sound configuration
void CFamiTrackerApp::LoadSoundConfig()
{
	GetSoundGenerator()->LoadSettings();
	GetSoundGenerator()->Interrupt();
	static_cast<CFrameWnd*>(GetMainWnd())->SetMessageText(IDS_NEW_SOUND_CONFIG);
}

// Silences everything
void CFamiTrackerApp::SilentEverything()
{
	GetSoundGenerator()->SilentAll();
	CFamiTrackerView::GetView()->MakeSilent();
}

int CFamiTrackerApp::GetCPUUsage() const
{
	// Calculate CPU usage
	const int THREAD_COUNT = 2;
	static FILETIME KernelLastTime[THREAD_COUNT], UserLastTime[THREAD_COUNT];
	const HANDLE hThreads[THREAD_COUNT] = {m_hThread, m_pSoundGenerator->m_hThread};
	unsigned int TotalTime = 0;

	for (int i = 0; i < THREAD_COUNT; ++i) {
		FILETIME CreationTime, ExitTime, KernelTime, UserTime;
		GetThreadTimes(hThreads[i], &CreationTime, &ExitTime, &KernelTime, &UserTime);
		TotalTime += (KernelTime.dwLowDateTime - KernelLastTime[i].dwLowDateTime) / 1000;
		TotalTime += (UserTime.dwLowDateTime - UserLastTime[i].dwLowDateTime) / 1000;
		KernelLastTime[i] = KernelTime;
		UserLastTime[i]	= UserTime;
	}

	return TotalTime;
}

void CFamiTrackerApp::ReloadColorScheme()
{
	// Notify all views
	POSITION TemplatePos = GetFirstDocTemplatePosition();
	CDocTemplate *pDocTemplate = GetNextDocTemplate(TemplatePos);
	POSITION DocPos = pDocTemplate->GetFirstDocPosition();

	while (CDocument* pDoc = pDocTemplate->GetNextDoc(DocPos)) {
		POSITION ViewPos = pDoc->GetFirstViewPosition();
		while (CView *pView = pDoc->GetNextView(ViewPos)) {
			if (pView->IsKindOf(RUNTIME_CLASS(CFamiTrackerView)))
				static_cast<CFamiTrackerView*>(pView)->SetupColors();
		}
	}

	// Main window
	CMainFrame *pMainFrm = dynamic_cast<CMainFrame*>(GetMainWnd());

	if (pMainFrm != NULL) {
		pMainFrm->SetupColors();
		pMainFrm->RedrawWindow();
	}
}

// App command to run the about dialog
void CFamiTrackerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CFamiTrackerApp message handlers

void CFamiTrackerApp::StartPlayer(play_mode_t Mode)
{
	int Track = static_cast<CMainFrame*>(GetMainWnd())->GetSelectedTrack();
	if (m_pSoundGenerator)
		m_pSoundGenerator->StartPlayer(Mode, Track);
}

void CFamiTrackerApp::StopPlayer()
{
	if (m_pSoundGenerator)
		m_pSoundGenerator->StopPlayer();

	m_pMIDI->ResetOutput();
}

void CFamiTrackerApp::StopPlayerAndWait()
{
	// Synchronized stop
	if (m_pSoundGenerator) {
		m_pSoundGenerator->StopPlayer();
		m_pSoundGenerator->WaitForStop();
	}
	m_pMIDI->ResetOutput();
}

void CFamiTrackerApp::TogglePlayer()
{
	if (m_pSoundGenerator) {
		if (m_pSoundGenerator->IsPlaying())
			StopPlayer();
		else
			StartPlayer(MODE_PLAY);
	}
}

// Player interface

bool CFamiTrackerApp::IsPlaying() const
{
	if (m_pSoundGenerator)
		return m_pSoundGenerator->IsPlaying();

	return false;
}

void CFamiTrackerApp::ResetPlayer()
{
	// Called when changing track
	int Track = static_cast<CMainFrame*>(GetMainWnd())->GetSelectedTrack();
	if (m_pSoundGenerator)
		m_pSoundGenerator->ResetPlayer(Track);
}

// File load/save

void CFamiTrackerApp::OnFileOpen() 
{
	// Overloaded in order to save the file path
	CString newName, path;

	// Get path
	path = m_pSettings->GetPath(PATH_FTM) + _T("\\");
	newName = _T("");

	if (!DoPromptFileName(newName, path, AFX_IDS_OPENFILE, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, TRUE, NULL))
		return; // open cancelled

	CFrameWnd *pFrameWnd = (CFrameWnd*)GetMainWnd();

	// Save path
	m_pSettings->SetPath(newName, PATH_FTM);
	
	if (pFrameWnd)
		pFrameWnd->SetMessageText(IDS_LOADING_FILE);
	
	AfxGetApp()->OpenDocumentFile(newName);

	if (pFrameWnd)
		pFrameWnd->SetMessageText(IDS_LOADING_DONE);
}

BOOL CFamiTrackerApp::DoPromptFileName(CString& fileName, CString& filePath, UINT nIDSTitle, DWORD lFlags, BOOL bOpenFileDialog, CDocTemplate* pTemplate)
{
	// Copied from MFC
	CFileDialog dlgFile(bOpenFileDialog, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, NULL, NULL, 0);

	CString title;
	ENSURE(title.LoadString(nIDSTitle));

	dlgFile.m_ofn.Flags |= lFlags;

	CString strFilter;
	CString strDefault;

	if (pTemplate == NULL) {
		POSITION pos = GetFirstDocTemplatePosition();
		while (pos != NULL) {
			CString strFilterExt;
			CString strFilterName;
			pTemplate = GetNextDocTemplate(pos);
			pTemplate->GetDocString(strFilterExt, CDocTemplate::filterExt);
			pTemplate->GetDocString(strFilterName, CDocTemplate::filterName);

			// Add extension
			strFilter += strFilterName;
			strFilter += (TCHAR)'\0';
			strFilter += _T("*");
			strFilter += strFilterExt;
			strFilter += (TCHAR)'\0';
			dlgFile.m_ofn.nMaxCustFilter++;
		}
	}

	// Select first filter
	dlgFile.m_ofn.nFilterIndex = 1;

	// append the "*.*" all files filter
	CString allFilter;
	VERIFY(allFilter.LoadString(AFX_IDS_ALLFILTER));
	strFilter += allFilter;
	strFilter += (TCHAR)'\0';   // next string please
	strFilter += _T("*.*");
	strFilter += (TCHAR)'\0';   // last string
	dlgFile.m_ofn.nMaxCustFilter++;

	dlgFile.m_ofn.lpstrFilter = strFilter;
	dlgFile.m_ofn.lpstrTitle = title;
	dlgFile.m_ofn.lpstrInitialDir = filePath.GetBuffer(_MAX_PATH);
	dlgFile.m_ofn.lpstrFile = fileName.GetBuffer(_MAX_PATH);

	INT_PTR nResult = dlgFile.DoModal();
	fileName.ReleaseBuffer();
	filePath.ReleaseBuffer();

	return nResult == IDOK;
}

#ifdef EXPORT_TEST

void CFamiTrackerApp::OnTestExport()
{
	VerifyExport();
}

void CFamiTrackerApp::VerifyExport() const
{
	CExportTest *pExportTest = new CExportTest();
	if (pExportTest->Setup()) {
		const CMainFrame *pMainFrame = static_cast<CMainFrame*>(m_pMainWnd);
		pExportTest->RunInit(pMainFrame->GetSelectedTrack());
		GetSoundGenerator()->PostThreadMessage(WM_USER_VERIFY_EXPORT, (WPARAM)pExportTest, pMainFrame->GetSelectedTrack());
	}
	else
		delete pExportTest;
}

void CFamiTrackerApp::VerifyExport(LPCTSTR File) const
{
	CExportTest *pExportTest = new CExportTest();

	printf("Verifying export for file %s\n", File);

	if (pExportTest->Setup(File)) {
		const CMainFrame *pMainFrame = static_cast<CMainFrame*>(m_pMainWnd);
		pExportTest->RunInit(pMainFrame->GetSelectedTrack());
		GetSoundGenerator()->PostThreadMessage(WM_USER_VERIFY_EXPORT, (WPARAM)pExportTest, pMainFrame->GetSelectedTrack());
	}
	else
		delete pExportTest;
}

bool CFamiTrackerApp::IsExportTest() const
{
	return m_bExportTesting;
}

#endif /* EXPORT_TEST */

// Used to display a messagebox on the main thread
void CFamiTrackerApp::ThreadDisplayMessage(LPCTSTR lpszText, UINT nType, UINT nIDHelp)
{
	m_pMainWnd->SendMessage(WM_USER_DISPLAY_MESSAGE_STRING, (WPARAM)lpszText, (LPARAM)nType);
}

void CFamiTrackerApp::ThreadDisplayMessage(UINT nIDPrompt, UINT nType, UINT nIDHelp)
{
	m_pMainWnd->SendMessage(WM_USER_DISPLAY_MESSAGE_ID, (WPARAM)nIDPrompt, (LPARAM)nType);
}

// Various global helper functions

CString LoadDefaultFilter(LPCTSTR Name, LPCTSTR Ext)
{
	// Loads a single filter string including the all files option
	CString filter;
	CString allFilter;
	VERIFY(allFilter.LoadString(AFX_IDS_ALLFILTER));

	filter = Name;
	filter += _T("|*");
	filter += Ext;
	filter += _T("|");
	filter += allFilter;
	filter += _T("|*.*||");

	return filter;
}

CString LoadDefaultFilter(UINT nID, LPCTSTR Ext)
{
	// Loads a single filter string including the all files option
	CString filter;
	CString allFilter;
	VERIFY(allFilter.LoadString(AFX_IDS_ALLFILTER));

	filter.LoadString(nID);
	filter += _T("|*");
	filter += Ext;
	filter += _T("|");
	filter += allFilter;
	filter += _T("|*.*||");

	return filter;
}

void AfxFormatString3(CString &rString, UINT nIDS, LPCTSTR lpsz1, LPCTSTR lpsz2, LPCTSTR lpsz3)
{
	// AfxFormatString with three arguments
	LPCTSTR arr[] = {lpsz1, lpsz2, lpsz3};
	AfxFormatStrings(rString, nIDS, arr, 3);
}

CString MakeIntString(int val, LPCTSTR format)
{
	// Turns an int into a string
	CString str;
	str.Format(format, val);
	return str;
}

CString MakeFloatString(float val, LPCTSTR format)
{
	// Turns a float into a string
	CString str;
	str.Format(format, val);
	return str;
}

/**
 * CFTCommandLineInfo, a custom command line parser
 *
 */

CFTCommandLineInfo::CFTCommandLineInfo() : CCommandLineInfo(), 
	m_bLog(false), 
	m_bExport(false), 
	m_bPlay(false),
#ifdef EXPORT_TEST
	m_bVerifyExport(false),
#endif
	m_strExportFile(_T("")),
	m_strExportLogFile(_T("")),
	m_strExportDPCMFile(_T(""))
{
}

void CFTCommandLineInfo::ParseParam(const TCHAR* pszParam, BOOL bFlag, BOOL bLast)
{
	if (bFlag) {
		// Export file (/export or /e)
		if (!_tcsicmp(pszParam, _T("export")) || !_tcsicmp(pszParam, _T("e"))) {
			m_bExport = true;
			return;
		}
		// Auto play (/play or /p)
		else if (!_tcsicmp(pszParam, _T("play")) || !_tcsicmp(pszParam, _T("p"))) {
			m_bPlay = true;
			return;
		}
		// Disable crash dumps (/nodump)
		else if (!_tcsicmp(pszParam, _T("nodump"))) { 
#ifdef ENABLE_CRASH_HANDLER
			UninstallExceptionHandler();
#endif
			return;
		}
		// Enable register logger (/log), available in debug mode only
		else if (!_tcsicmp(pszParam, _T("log"))) {
#ifdef _DEBUG
			m_bLog = true;
			return;
#endif
		}
		// Run export tester (/verify), optionally provide NSF file, otherwise NSF is created
		else if (!_tcsicmp(pszParam, _T("verify"))) {
#ifdef EXPORT_TEST
			m_bVerifyExport = true;
			return;
#endif
		}
		// Enable console output (TODO)
		// This is intended for a small helper program that avoids the problem with console on win32 programs,
		// and should remain undocumented. I'm using it for testing.
		else if (!_tcsicmp(pszParam, _T("console"))) {
			FILE *f;
			AttachConsole(ATTACH_PARENT_PROCESS);
			errno_t err = freopen_s(&f, "CON", "w", stdout);
			printf("FamiTracker v%i.%i.%i\n", VERSION_MAJ, VERSION_MIN, VERSION_REV);
			return;
		}
	}
	else {
		// Store NSF name, then log filename
		if (m_bExport == true) {
			if (m_strExportFile.GetLength() == 0)
			{
				m_strExportFile = CString(pszParam);
				return;
			}
			else if(m_strExportLogFile.GetLength() == 0)
			{
				m_strExportLogFile = CString(pszParam);
				return;
			}
			else if(m_strExportDPCMFile.GetLength() == 0)
			{
				// BIN export takes another file paramter for DPCM
				m_strExportDPCMFile = CString(pszParam);
				return;
			}
		}
#ifdef EXPORT_TEST
		else if (m_bVerifyExport) {
			if (m_strVerifyFile.GetLength() == 0)
			{
				m_strVerifyFile = CString(pszParam);
				return;
			}
		}
#endif
	}

	// Call default implementation
	CCommandLineInfo::ParseParam(pszParam, bFlag, bLast);
}
