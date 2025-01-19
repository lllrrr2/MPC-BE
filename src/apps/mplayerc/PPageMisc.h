/*
 * (C) 2006-2024 see Authors.txt
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "PPageBase.h"

// CPPageMisc dialog

class CPPageMisc : public CPPageBase
{
	DECLARE_DYNAMIC(CPPageMisc)

private:
	int m_nJumpDistS = 0;
	int m_nJumpDistM = 0;
	int m_nJumpDistL = 0;

	BOOL m_fDontUseSearchInFolder    = FALSE;
	BOOL m_fPreventMinimize          = FALSE;
	BOOL m_bHideWindowedMousePointer = FALSE;
	BOOL m_bShowZeroHours            = FALSE;
	int  m_nMinMPlsDuration          = 3;
	CSpinButtonCtrl m_spnMinMPlsDuration;
	BOOL m_fLCDSupport       = FALSE;
	BOOL m_bWinMediaControls = FALSE;
	BOOL m_fMiniDump         = FALSE;
	CComboBox m_cbFFmpegExePath;

	CButton m_updaterAutoCheckCtrl;
	CEdit m_updaterDelayCtrl;
	CSpinButtonCtrl m_updaterDelaySpin;
	int m_nUpdaterDelay = 7;

public:
	CPPageMisc();
	virtual ~CPPageMisc();

	enum { IDD = IDD_PPAGEMISC };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnUpdateDelayEditBox(CCmdUI* pCmdUI);
	afx_msg void OnResetSettings();
	afx_msg void OnExportSettings();
};
