/*
This file is part of SoundPad.

SoundPad is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

SoundPad is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SoundPad; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <windows.h>
#include <assert.h>
#include "resource.h"

/* Function prototypes */

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK AboutDlgProc(HWND , UINT, WPARAM, LPARAM);

VOID UpdateUI (VOID);
VOID ProcessPendingAction (VOID);
VOID PlayNext(PWAVEHDR pWaveHdr);
VOID InitWaveFormatEx(PWAVEFORMATEX pWaveFormatEx);
VOID InitWaveHdr (PWAVEHDR pWaveHdr, PBYTE pWaveBuffer);

/* UI Events */
VOID OnRecord (VOID);
VOID OnPlay (VOID);
VOID OnStop (VOID);
VOID OnLoop(VOID);
VOID OnControlLoop(VOID);
VOID OnSave(VOID);

/* Multimedia Events */
VOID OnWimOpen (VOID);
VOID OnWimData (PWAVEHDR pWaveHdr);
VOID OnWimClose (VOID);
VOID OnWomOpen (VOID);
VOID OnWomDone (PWAVEHDR pWaveHdr);
VOID OnWomClose (VOID);

/* Error */
VOID OnMmError (MMRESULT mmResult, BOOL bIn);

/* SoundPad state */

typedef enum {
	SP_IDLE,
	SP_OPENING,
	SP_ACTIVE,
	SP_CLOSING
} SP_STATE;

typedef enum {
	SPA_NONE,
	SPA_STOP,
	SPA_RECORD,
	SPA_PLAY
} SNDPADACT;

/* Variables for UI */

static TCHAR szAppName[] = TEXT ("SoundPad");

static HWND hMainWnd;
static HINSTANCE hMainInstance;

/* Parameters for PCM sound */

#define SAMPLES_PER_SEC     (22050) /* 22.05kHz */
#define AUDIO_BUFFER_LENGTH (120 * 60) /* 120min */
#define BYTES_PER_SAMPLE    (2) /* 1 channel, 16bits */
#define BYTES_PER_SEC       (SAMPLES_PER_SEC * BYTES_PER_SAMPLE)
#define AUDIO_BUFFER_SIZE   (BYTES_PER_SEC * AUDIO_BUFFER_LENGTH)
#define MARGIN_LENGTH       (10) /* 10s */
#define MARGIN_SIZE         (BYTES_PER_SEC * MARGIN_LENGTH)
#define WAVE_BUFFER_SIZE    (BYTES_PER_SEC * 500 / 1000) /* 500ms */
#define AUDIO_FILENAME      "tempfile.wav"

/* Variables for audio recording/playing */

static WAVEFORMATEX waveFormEx;
static HWAVEIN  hWaveIn;
static HWAVEOUT hWaveOut;
static WAVEHDR waveHdrIn1;
static WAVEHDR waveHdrIn2;
static WAVEHDR waveHdrOut1;
static WAVEHDR waveHdrOut2;
static BYTE waveBufferIn1[WAVE_BUFFER_SIZE];
static BYTE waveBufferIn2[WAVE_BUFFER_SIZE];
static BYTE waveBufferOut1[WAVE_BUFFER_SIZE];
static BYTE waveBufferOut2[WAVE_BUFFER_SIZE];

static SP_STATE WomState = SP_IDLE;
static SP_STATE WimState = SP_IDLE;
static SNDPADACT NextAct = SPA_STOP;

static PBYTE pWaveBuffer;
static DWORD dwWaveLength;
static DWORD dwWaveIndex;
static DWORD dwWaveFinished;
static DWORD dwWaveRecording;
static BOOL bWaveExists;
static BOOL bLoop;

/* Functions */

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					PSTR szCmdLine, int iCmdShow)
{
	MSG msg;
	WNDCLASS wndclass;
	HACCEL hAccel;

	hMainInstance = hInstance;

	pWaveBuffer = VirtualAlloc (NULL, AUDIO_BUFFER_SIZE,
								MEM_COMMIT, PAGE_READWRITE);
	if (pWaveBuffer == NULL)
	{
		MessageBox (NULL, TEXT ("Cannot allocate memory"),
					szAppName, MB_ICONERROR);
		return 0;
	}

	dwWaveLength    = 0;
	dwWaveIndex     = 0;
	dwWaveFinished  = 0;
	dwWaveRecording = 0;
	bWaveExists      = FALSE;

	InitWaveHdr (&waveHdrIn1,  waveBufferIn1);
	InitWaveHdr (&waveHdrIn2,  waveBufferIn2);
	InitWaveHdr (&waveHdrOut1, waveBufferOut1);
	InitWaveHdr (&waveHdrOut2, waveBufferOut2);
	
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = DLGWINDOWEXTRA;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon (hInstance, (LPCSTR)IDI_SOUNDPAD);
	wndclass.hCursor = LoadCursor (NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = szAppName;

	if (!RegisterClass (&wndclass))
	{
		MessageBox (NULL, TEXT ("This program requires Windows NT!"),
					szAppName, MB_ICONERROR);
		return 0;
	}
	
	hMainWnd = CreateDialog (hInstance, (LPCSTR) IDD_SOUNDPAD, 0, NULL);

	UpdateUI ();
	ShowWindow (hMainWnd, iCmdShow);
	UpdateWindow (hMainWnd);

	hAccel = LoadAccelerators (hInstance, (LPCSTR)IDC_SOUNDPAD);

	while (GetMessage (&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator (hMainWnd, hAccel, &msg))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}

	VirtualFree (pWaveBuffer, AUDIO_BUFFER_SIZE, MEM_RELEASE);

	return (int) msg.wParam;
}

LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam , LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY :
		PostQuitMessage (0);
		return 0;
	case WM_COMMAND :
		switch (LOWORD(wParam))
		{
		case ID_FILE_SAVE:
		    OnSave();
			break;
		case ID_FILE_EXIT:
			PostQuitMessage (0);
			break;
		case ID_CONTROL_RECORD :
			OnRecord ();
			break;
		case ID_CONTROL_PLAY :
			OnPlay ();
			break;
		case ID_CONTROL_STOP :
			OnStop ();
			break;
		case ID_CONTROL_LOOP :
			OnControlLoop();
			break;
		case ID_HELP_ABOUT:
			DialogBox (hMainInstance, (LPCSTR)IDD_ABOUTBOX,
					   hMainWnd, AboutDlgProc);
			break;
		case IDC_LOOP:
			OnLoop();
			break;
		default :
			break;
		}
		return 0;
	case MM_WIM_OPEN :
		OnWimOpen ();
		return 0;
	case MM_WIM_DATA :
		OnWimData ((PWAVEHDR)lParam);
		return 0;
	case MM_WIM_CLOSE :
		OnWimClose ();
		return 0;
	case MM_WOM_OPEN :
		OnWomOpen ();
		return 0;
	case MM_WOM_DONE :
		OnWomDone ((PWAVEHDR)lParam);
		return 0;
	case MM_WOM_CLOSE :
		OnWomClose ();
		return 0;
	}
	return DefWindowProc (hwnd, message, wParam, lParam);
}

BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT message,
						   WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG :
		return TRUE;
	case WM_COMMAND:
		EndDialog(hDlg, 0);
		return TRUE;
	}
	return FALSE;
}

VOID UpdateUI (VOID)
{
	HMENU hMenu;
	TCHAR szStr[16];
	BOOL t;
	UINT uId;

	static UINT StatusIdTable[4] = {
		IDS_IDLE, IDS_PLAYING, IDS_RECORDING, IDS_ECHOING
	};

	hMenu = GetMenu (hMainWnd);

	t = WomState == SP_IDLE || WimState == SP_IDLE;
	EnableWindow (GetDlgItem(hMainWnd, ID_CONTROL_RECORD), t);
	EnableMenuItem (hMenu, ID_CONTROL_RECORD, t ? MF_ENABLED : MF_GRAYED);

	t = bWaveExists && (!bLoop || WomState == SP_IDLE || WimState == SP_IDLE);
	EnableWindow (GetDlgItem(hMainWnd, ID_CONTROL_PLAY), t);
	EnableMenuItem (hMenu, ID_CONTROL_PLAY, t ? MF_ENABLED : MF_GRAYED);

	t = WimState != SP_IDLE || WomState != SP_IDLE;
	EnableWindow (GetDlgItem(hMainWnd, ID_CONTROL_STOP), t);
	EnableMenuItem (hMenu, ID_CONTROL_STOP, t ? MF_ENABLED : MF_GRAYED);

	t = WimState == SP_IDLE && WomState == SP_IDLE && bWaveExists;
	EnableMenuItem (hMenu, ID_FILE_SAVE, t ? MF_ENABLED : MF_GRAYED);

	uId = StatusIdTable[(WimState != SP_IDLE ? 2 : 0) |
						(WomState != SP_IDLE ? 1 : 0)];
	LoadString (hMainInstance, uId, szStr, 16);
	SetWindowText (GetDlgItem (hMainWnd, IDC_STATUS), szStr);
}

VOID OnRecord (VOID)
{
	MMRESULT mmResult;
	
	if (WimState != SP_IDLE || WomState != SP_IDLE)
	{
		/* We have to stop recording and/or playing before starting
		   recording */
		OnStop ();
		NextAct = SPA_RECORD;
		return;
	}
	NextAct = SPA_NONE;

	VirtualFree (pWaveBuffer, AUDIO_BUFFER_SIZE, MEM_DECOMMIT);
	VirtualAlloc (pWaveBuffer, AUDIO_BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
	dwWaveRecording = 0;
	dwWaveLength    = 0;
	bWaveExists      = TRUE;

	WimState = SP_OPENING;

	InitWaveFormatEx(&waveFormEx);
	mmResult = waveInOpen (&hWaveIn, WAVE_MAPPER, &waveFormEx,
						   (DWORD) hMainWnd, 0, CALLBACK_WINDOW);
	if (mmResult != MMSYSERR_NOERROR)
	{
		OnMmError (mmResult, TRUE);
		WimState = SP_IDLE;
		return;
	}
}

VOID OnPlay (VOID)
{
	MMRESULT mmResult;
	
	if ((!bLoop && WimState != SP_IDLE) || WomState != SP_IDLE)
	{
		/* We have to stop recording and/or playing before starting
		   playing */
		OnStop ();
		NextAct = SPA_PLAY;
		return;
	}
	NextAct = SPA_NONE;

	if (bLoop && WimState == SP_ACTIVE)
	{
	    dwWaveLength = min (dwWaveLength + MARGIN_SIZE, AUDIO_BUFFER_SIZE);
	}
	dwWaveIndex    = 0;
	dwWaveFinished = 0;

	WomState = SP_OPENING;

	InitWaveFormatEx(&waveFormEx);
	mmResult = waveOutOpen (&hWaveOut, WAVE_MAPPER, &waveFormEx,
							 (DWORD) hMainWnd, 0, CALLBACK_WINDOW);
	if (mmResult != MMSYSERR_NOERROR)
	{
		OnMmError (mmResult, FALSE);
		WomState = SP_IDLE;
		return;
	}
}

VOID OnStop (VOID)
{
	MMRESULT mmResult;
	SP_STATE OldWomState;
	MMTIME mmt;

	NextAct = SPA_NONE;
	OldWomState = WomState;
	switch (WimState)
	{
	case SP_OPENING:
		WimState = SP_CLOSING;
		break;
	case SP_ACTIVE:
		WimState = SP_CLOSING;
		mmResult = waveInStop (hWaveIn);
		assert(mmResult == MMSYSERR_NOERROR);
		mmt.wType = TIME_SAMPLES;
		mmResult = waveInGetPosition(hWaveIn, &mmt, sizeof (mmt));
		assert(mmResult == MMSYSERR_NOERROR);
		dwWaveLength = mmt.u.sample * BYTES_PER_SAMPLE;
		if (dwWaveLength == 0) bWaveExists = FALSE;
		mmResult = waveInReset (hWaveIn);
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveInUnprepareHeader(hWaveIn, &waveHdrIn1,
										 sizeof (waveHdrIn1));
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveInUnprepareHeader(hWaveIn, &waveHdrIn2,
										 sizeof (waveHdrIn2));
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveInClose(hWaveIn);
		assert(mmResult == MMSYSERR_NOERROR);
		break;
	default:
		break;
	}
	
	switch (OldWomState)
	{
	case SP_OPENING:
		WomState = SP_CLOSING;
		break;
	case SP_ACTIVE:
		WomState = SP_CLOSING;
		mmResult = waveOutReset(hWaveOut);
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveOutUnprepareHeader(hWaveOut, &waveHdrOut1,
										 sizeof (waveHdrOut1));
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveOutUnprepareHeader(hWaveOut, &waveHdrOut2,
										 sizeof (waveHdrOut2));
		assert(mmResult == MMSYSERR_NOERROR);
		mmResult = waveOutClose(hWaveOut);
		assert(mmResult == MMSYSERR_NOERROR);
		break;
	default:
		break;
	}
}

VOID OnControlLoop(VOID)
{
	bLoop = !bLoop;
	SendMessage(GetDlgItem(hMainWnd, IDC_LOOP),
		BM_SETCHECK, bLoop ? BST_CHECKED : BST_UNCHECKED, 0);
	CheckMenuItem(GetMenu (hMainWnd), ID_CONTROL_LOOP,
		bLoop ? MF_CHECKED : MF_UNCHECKED);
}

VOID OnLoop(VOID)
{
	bLoop = SendMessage(GetDlgItem(hMainWnd, IDC_LOOP),
		BM_GETCHECK, 0, 0) ? TRUE : FALSE;
	CheckMenuItem(GetMenu (hMainWnd), ID_CONTROL_LOOP,
		bLoop ? MF_CHECKED : MF_UNCHECKED);
}

VOID OnWimOpen (VOID)
{
	MMRESULT mmResult;
	
	switch (WimState)
	{
	case SP_CLOSING:
		mmResult = waveInClose (hWaveIn);
		assert (mmResult == MMSYSERR_NOERROR);
		break;
	case SP_OPENING:
		WimState = SP_ACTIVE;
		waveHdrIn1.dwFlags = 0;
		mmResult = waveInPrepareHeader (hWaveIn, &waveHdrIn1,
										sizeof (*&waveHdrIn1));
		assert (mmResult == MMSYSERR_NOERROR);
		waveHdrIn2.dwFlags = 0;
		mmResult = waveInPrepareHeader (hWaveIn, &waveHdrIn2,
										sizeof (*&waveHdrIn2));
		assert (mmResult == MMSYSERR_NOERROR);
		mmResult = waveInAddBuffer (hWaveIn, &waveHdrIn1, sizeof (waveHdrIn1));
		assert (mmResult == MMSYSERR_NOERROR);
		mmResult = waveInAddBuffer (hWaveIn, &waveHdrIn2, sizeof (waveHdrIn2));
		assert (mmResult == MMSYSERR_NOERROR);
		mmResult = waveInStart (hWaveIn);
		assert (mmResult == MMSYSERR_NOERROR);
		UpdateUI ();
		break;
	default:
		break;
	}
}

VOID OnWimData (PWAVEHDR pWaveHdr)
{
	MMRESULT mmResult;
	PBYTE lpData;
	DWORD dwLen;
	DWORD dwMaxLen;

	if (WimState != SP_CLOSING)
	{
		mmResult = waveInUnprepareHeader(hWaveIn, pWaveHdr,
										 sizeof (*pWaveHdr));
		assert (mmResult == MMSYSERR_NOERROR);
	}

	lpData = pWaveHdr->lpData;
	dwLen  = pWaveHdr->dwBytesRecorded;

	dwMaxLen = ((bLoop && WomState != SP_IDLE)
		? dwWaveLength : AUDIO_BUFFER_SIZE) - dwWaveRecording;
	if (dwLen > dwMaxLen)
	{
		dwLen = dwMaxLen;
	}
	CopyMemory(pWaveBuffer + dwWaveRecording, lpData, dwLen);
	dwWaveRecording += dwLen;
	if (bLoop && WomState != SP_IDLE && dwWaveRecording >= dwWaveLength)
	{
		/* Copy rest of the recording for looping */
		lpData += dwLen;
		dwLen -= pWaveHdr->dwBytesRecorded - dwLen;
		CopyMemory(pWaveBuffer, lpData, dwLen);
		dwWaveRecording = dwLen;
	}

	if (WimState == SP_ACTIVE)
	{
		pWaveHdr->dwFlags = 0;
		mmResult = waveInPrepareHeader(hWaveIn, pWaveHdr,
									   sizeof (*pWaveHdr));
		assert (mmResult == MMSYSERR_NOERROR);
		mmResult = waveInAddBuffer (hWaveIn, pWaveHdr, sizeof (*pWaveHdr));
		assert (mmResult == MMSYSERR_NOERROR);
	}
}

VOID OnWimClose (VOID)
{
	WimState = SP_IDLE;

//	dwWaveLength = dwWaveRecording;
	ProcessPendingAction ();
	UpdateUI ();
}

VOID OnWomOpen ()
{
	MMRESULT mmResult;
	
	switch (WomState)
	{
	case SP_CLOSING:
		mmResult = waveOutClose(hWaveOut);
		assert (mmResult == MMSYSERR_NOERROR);
		break;
	case SP_OPENING:
		WomState = SP_ACTIVE;
		PlayNext (&waveHdrOut1);
		PlayNext (&waveHdrOut2);
		UpdateUI();
		break;
	default:
		break;
	}
}

VOID OnWomDone (PWAVEHDR pWaveHdr)
{
	MMRESULT mmResult;

	if (WomState != SP_CLOSING)
	{
		mmResult = waveOutUnprepareHeader (hWaveOut, pWaveHdr,
										   sizeof (*pWaveHdr));
		assert (mmResult == MMSYSERR_NOERROR);
	}

	if (WomState != SP_ACTIVE) return;

	dwWaveFinished += pWaveHdr->dwBufferLength;
	if (bLoop && dwWaveFinished >= dwWaveLength)
	{
		dwWaveFinished = 0;
	}
	if (dwWaveFinished >= dwWaveLength)
	{
		WomState = SP_CLOSING;
		mmResult = waveOutClose (hWaveOut);
		assert (mmResult == MMSYSERR_NOERROR);
	}
	else
	{
		PlayNext(pWaveHdr);
	}
}

VOID PlayNext(PWAVEHDR pWaveHdr)
{
	MMRESULT mmResult;
	DWORD dwLen;

	if (dwWaveLength <= dwWaveIndex) return;
	dwLen = min(dwWaveLength - dwWaveIndex, WAVE_BUFFER_SIZE);
	if (bLoop && WimState != SP_IDLE
		&& dwWaveRecording > dwWaveIndex
		&& dwWaveRecording < dwWaveIndex + dwLen)
	{
		dwLen = (dwWaveRecording - dwWaveIndex + 1) / 2;
	}
	CopyMemory(pWaveHdr->lpData, pWaveBuffer + dwWaveIndex, dwLen); 
	pWaveHdr->dwBufferLength = dwLen;
	pWaveHdr->dwFlags = 0;
	mmResult = waveOutPrepareHeader (hWaveOut, pWaveHdr,
									 sizeof (*pWaveHdr));
	assert (mmResult == MMSYSERR_NOERROR);
	mmResult = waveOutWrite (hWaveOut, pWaveHdr, sizeof (*pWaveHdr));
	assert (mmResult == MMSYSERR_NOERROR);
	dwWaveIndex += dwLen;
	if (bLoop && dwWaveIndex >= dwWaveLength)
	{
		dwWaveIndex = 0;
	}

	return;
}

VOID OnWomClose (VOID)
{
	WomState = SP_IDLE;

	ProcessPendingAction ();
	UpdateUI ();
}

VOID ProcessPendingAction (VOID)
{

	switch (NextAct)
	{
	case SPA_PLAY:
		OnPlay();
		break;
	case SPA_RECORD:
		OnRecord();
		break;
	default:
		break;
	}
}

VOID InitWaveFormatEx(PWAVEFORMATEX pWaveFormatEx)
{
	pWaveFormatEx->wFormatTag      = WAVE_FORMAT_PCM;
	pWaveFormatEx->nChannels       = 1;
	pWaveFormatEx->nSamplesPerSec  = SAMPLES_PER_SEC;
	pWaveFormatEx->nAvgBytesPerSec = BYTES_PER_SEC;
	pWaveFormatEx->nBlockAlign     = BYTES_PER_SAMPLE;
	pWaveFormatEx->wBitsPerSample  = 16;
	pWaveFormatEx->cbSize          = 0;
}

VOID InitWaveHdr (PWAVEHDR pWaveHdr, PBYTE pWaveBuffer)
{
	pWaveHdr->lpData          = pWaveBuffer;
	pWaveHdr->dwBufferLength  = WAVE_BUFFER_SIZE;
	pWaveHdr->dwBytesRecorded = 0;
	pWaveHdr->dwUser          = 0;
	pWaveHdr->dwFlags         = 0;
	pWaveHdr->dwLoops         = 1;
	pWaveHdr->lpNext          = NULL;
	pWaveHdr->reserved        = 0;
}

typedef struct
{
	DWORD dwHead;
	DWORD dwLength;
	DWORD dwType;
} RiffChunk;

typedef struct
{
	DWORD dwHead;
	DWORD dwLength;
	WORD wDummy;
	WORD wChannels;
	DWORD dwSamplesPerSec;
	DWORD dwBytesPerSec;
	WORD wBytesPerSample;
	WORD wBitsPerSample;
} FormatChunk;

typedef struct
{
	DWORD dwHead;
	DWORD dwLength;
} DataChunk;

VOID OnSave(VOID)
{
	HANDLE hFile;
	BOOL bStatus;
	DWORD i;
	DWORD dwLength;
	RiffChunk riffChunk;
	FormatChunk formatChunk;
	DataChunk dataChunk;

	hFile = CreateFile(AUDIO_FILENAME, GENERIC_WRITE, 0, NULL,
					   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == NULL) return;

	/* Write RIFF Chunk */
	riffChunk.dwHead = *((DWORD*)"RIFF");
	riffChunk.dwLength = dwWaveLength
		+ sizeof (RiffChunk) + sizeof (FormatChunk) + sizeof (DataChunk)
		- 8;
	riffChunk.dwType = *((DWORD*)"WAVE");
	bStatus = WriteFile(hFile, &riffChunk, sizeof riffChunk, &dwLength, NULL);
	if (!bStatus) goto _catch;

	/* Write Format Chunk */
	formatChunk.dwHead          = *((DWORD*)"fmt ");
	formatChunk.dwLength        = 0x10;
	formatChunk.wDummy          = 0x1;
	formatChunk.wChannels       = 0x1;
	formatChunk.dwSamplesPerSec = SAMPLES_PER_SEC;
	formatChunk.dwBytesPerSec   = BYTES_PER_SEC;
	formatChunk.wBytesPerSample = BYTES_PER_SAMPLE;
	formatChunk.wBitsPerSample  = 16;
	bStatus = WriteFile(hFile, &formatChunk, sizeof formatChunk,
						&dwLength, NULL);
	if (!bStatus) goto _catch;
	
	/* Write DATA Chunk */
	dataChunk.dwHead = *((DWORD*)"data");
	dataChunk.dwLength = dwWaveLength;
	bStatus = WriteFile(hFile, &dataChunk, sizeof dataChunk, &dwLength, NULL);
	if (!bStatus) goto _catch;

	for (i = 0; i < dwWaveLength; i += dwLength)
	{
		bStatus = WriteFile(hFile, pWaveBuffer + i,
							min(dwWaveLength - i, 4096),
							&dwLength, NULL);
		if (!bStatus) goto _catch;
		if (dwLength == 0) break;
	}

_catch:
	CloseHandle(hFile);
}

VOID OnMmError (MMRESULT mmResult, BOOL bIn)
{
	TCHAR MsgBuf[1024];
	if (bIn)
	{
		waveInGetErrorText(mmResult, MsgBuf, 1024);
	}
	else
	{
		waveOutGetErrorText(mmResult, MsgBuf, 1024);
	}
		
	MessageBox (NULL, MsgBuf, szAppName, MB_ICONERROR);
}

/* EOF */
