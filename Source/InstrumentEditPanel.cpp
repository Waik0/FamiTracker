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
#include <algorithm>
#include <iterator> 
#include <string>
#include <sstream>
#include "stdafx.h"
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "FamiTrackerView.h"
#include "InstrumentEditPanel.h"
#include "InstrumentEditDlg.h"
#include "SequenceEditor.h"

// CInstrumentEditPanel dialog
//
// Base class for instrument editors
//

IMPLEMENT_DYNAMIC(CInstrumentEditPanel, CDialog)
CInstrumentEditPanel::CInstrumentEditPanel(UINT nIDTemplate, CWnd* pParent) : CDialog(nIDTemplate, pParent)//, m_bShow(false)
{
}

CInstrumentEditPanel::~CInstrumentEditPanel()
{
}

void CInstrumentEditPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CInstrumentEditPanel, CDialog)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()

//COLORREF m_iBGColor = 0xFF0000;

BOOL CInstrumentEditPanel::OnEraseBkgnd(CDC* pDC)
{
	return FALSE;
}

HBRUSH CInstrumentEditPanel::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO: Find a proper way to get the background color
	//m_iBGColor = GetPixel(pDC->m_hDC, 2, 2);

	if (!theApp.IsThemeActive())
		return hbr;
	
	switch (nCtlColor) {
		case CTLCOLOR_STATIC:
//		case CTLCOLOR_DLG:
			pDC->SetBkMode(TRANSPARENT);
			// TODO: this might fail on some themes?
			//return NULL;
			return GetSysColorBrush(COLOR_3DHILIGHT);
			//return CreateSolidBrush(m_iBGColor);
	}

	return hbr;
}

BOOL CInstrumentEditPanel::PreTranslateMessage(MSG* pMsg)
{
	TCHAR ClassName[256];

	switch (pMsg->message) {
		case WM_KEYDOWN:
			switch (pMsg->wParam) {
				case VK_RETURN:	// Return
					pMsg->wParam = 0;
					OnKeyReturn();
					return TRUE;
				case VK_ESCAPE:	// Esc, close the dialog
					static_cast<CInstrumentEditDlg*>(GetParent())->DestroyWindow();
					return TRUE;
				case VK_TAB:
				case VK_DOWN:
				case VK_UP:
				case VK_LEFT:
				case VK_RIGHT:
				case VK_SPACE:
					// Do nothing
					break;
				default:	// Note keys
					// Make sure the dialog is selected when previewing
					GetClassName(pMsg->hwnd, ClassName, 256);
					if (_tcscmp(ClassName, _T("Edit"))) {
						// Remove repeated keys
						if ((pMsg->lParam & (1 << 30)) == 0)
							PreviewNote((unsigned char)pMsg->wParam);
						return TRUE;
					}
			}
			break;
		case WM_KEYUP:
			PreviewRelease((unsigned char)pMsg->wParam);
			return TRUE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void CInstrumentEditPanel::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Set focus on mouse clicks to enable note preview from keyboard
	SetFocus();
	CDialog::OnLButtonDown(nFlags, point);
}

void CInstrumentEditPanel::OnKeyReturn()
{
	// Empty
}

void CInstrumentEditPanel::OnSetFocus(CWnd* pOldWnd)
{	
	// Kill the default handler to avoid setting focus to a child control
	//Invalidate();
	CDialog::OnSetFocus(pOldWnd);
}

CFamiTrackerDoc *CInstrumentEditPanel::GetDocument() const
{
	// Return selected document
	return CFamiTrackerDoc::GetDoc();
}

void CInstrumentEditPanel::PreviewNote(unsigned char Key)
{
	CFamiTrackerView::GetView()->PreviewNote(Key);
}

void CInstrumentEditPanel::PreviewRelease(unsigned char Key)
{
	CFamiTrackerView::GetView()->PreviewRelease(Key);
}

//
// CSequenceInstrumentEditPanel
// 
// For dialog panels with sequence editors. Can translate MML strings 
//

IMPLEMENT_DYNAMIC(CSequenceInstrumentEditPanel, CInstrumentEditPanel)

CSequenceInstrumentEditPanel::CSequenceInstrumentEditPanel(UINT nIDTemplate, CWnd* pParent) : 
	CInstrumentEditPanel(nIDTemplate, pParent), 
	m_pSequenceEditor(NULL),
	m_pSequence(NULL),
	m_pParentWin(pParent),
	m_iSelectedSetting(0)
{
}

CSequenceInstrumentEditPanel::~CSequenceInstrumentEditPanel()
{
}

void CSequenceInstrumentEditPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSequenceInstrumentEditPanel, CInstrumentEditPanel)
	ON_NOTIFY(NM_RCLICK, IDC_INSTSETTINGS, OnRClickInstSettings)
END_MESSAGE_MAP()

void CSequenceInstrumentEditPanel::SetupDialog(LPCTSTR *pListItems)
{
	// Instrument settings
	CListCtrl *pList = static_cast<CListCtrl*>(GetDlgItem(IDC_INSTSETTINGS));
	
	pList->DeleteAllItems();
	pList->InsertColumn(0, _T(""), LVCFMT_LEFT, 26);
	pList->InsertColumn(1, _T("#"), LVCFMT_LEFT, 30);
	pList->InsertColumn(2, _T("Effect name"), LVCFMT_LEFT, 84);
	pList->SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
	
	for (int i = SEQ_COUNT - 1; i > -1; i--) {
		pList->InsertItem(0, _T(""), 0);
		pList->SetCheck(0, 0);
		pList->SetItemText(0, 1, _T("0"));
		pList->SetItemText(0, 2, pListItems[i]);
	}
	
	pList->SetItemState(m_iSelectedSetting, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

	SetDlgItemInt(IDC_SEQ_INDEX, m_iSelectedSetting);

	CSpinButtonCtrl *pSequenceSpin = static_cast<CSpinButtonCtrl*>(GetDlgItem(IDC_SEQUENCE_SPIN));
	pSequenceSpin->SetRange(0, MAX_SEQUENCES - 1);

	CRect rect(190 - 2, 30 - 2, CSequenceEditor::SEQUENCE_EDIT_WIDTH, CSequenceEditor::SEQUENCE_EDIT_HEIGHT);
	
	m_pSequenceEditor = new CSequenceEditor(GetDocument());	
	m_pSequenceEditor->CreateEditor(this, rect);
	m_pSequenceEditor->ShowWindow(SW_SHOW);
}

int CSequenceInstrumentEditPanel::ReadStringValue(const std::string &str)
{
	// Translate a string number to integer, accepts '$' and 'x' for hexadecimal notation
	std::stringstream ss;
	if (str[0] == '$')
		ss << std::hex << str.substr(1);
	else if (str[0] == 'x')
		ss << std::hex << str.substr(1);
	else
		ss << str;
	int value = 0;
	ss >> value;
	return value;
}

void CSequenceInstrumentEditPanel::PreviewNote(unsigned char Key)
{
	// Skip if MML window has focus
	if (GetDlgItem(IDC_SEQUENCE_STRING) != GetFocus())
		CFamiTrackerView::GetView()->PreviewNote(Key);
}

void CSequenceInstrumentEditPanel::PreviewRelease(unsigned char Key)
{
	CFamiTrackerView::GetView()->PreviewRelease(Key);
}

void CSequenceInstrumentEditPanel::TranslateMML(CString String, CSequence *pSequence, int Max, int Min) const
{
	// Takes a string and translates it into a sequence

	int AddedItems = 0;

	// Reset loop points
	pSequence->SetLoopPoint(-1);
	pSequence->SetReleasePoint(-1);

	std::string str;
	str.assign(CStringA(String));
	std::istringstream values(str);
	std::istream_iterator<std::string> begin(values);
	std::istream_iterator<std::string> end;

	while (begin != end && AddedItems < MAX_SEQUENCE_ITEMS) {
		std::string item = *begin++;

		if (item[0] == '|') {
			// Set loop point
			pSequence->SetLoopPoint(AddedItems);
		}
		else if (item[0] == '/') {
			// Set release point
			pSequence->SetReleasePoint(AddedItems);
		}
		else {
			// Convert to number
			int value = ReadStringValue(item);
			// Check for invalid chars
			if (!(value == 0 && item[0] != '0')) {
				value = std::min<int>(std::max<int>(value, Min), Max);
				pSequence->SetItem(AddedItems++, value);
			}
		}
	}

	pSequence->SetItemCount(AddedItems);
}

void CSequenceInstrumentEditPanel::OnRClickInstSettings(NMHDR* pNMHDR, LRESULT* pResult)
{
	POINT oPoint;
	GetCursorPos(&oPoint);

	if (m_pSequence == NULL)
		return;

	// Display clone menu
	CMenu contextMenu;
	contextMenu.LoadMenu(IDR_SEQUENCE_POPUP);
	CMenu *pMenu = contextMenu.GetSubMenu(0);
	pMenu->EnableMenuItem(ID_CLONE_SEQUENCE, (m_pSequence->GetItemCount() != 0) ? FALSE : TRUE);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, oPoint.x, oPoint.y, this);
}
