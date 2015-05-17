// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2015 - Tim Kosse <tim.kosse@filezilla-project.org>

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

#if !defined(AFX_OPTIONSSECURITYPAGE_H__59102206_048F_4DA0_B0F9_FAF29E56F1D1__INCLUDED_)
#define AFX_OPTIONSSECURITYPAGE_H__59102206_048F_4DA0_B0F9_FAF29E56F1D1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OptionsSecurityPage.h : Header-Datei
//

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsSecurityPage

class COptionsDlg;
class COptionsSecurityPage : public COptionsPage
{
// Konstruktion
public:
	COptionsSecurityPage(COptionsDlg *pOptionsDlg, CWnd* pParent = NULL);   // Standardkonstruktor

	virtual void SaveData();
	virtual void LoadData();

// Dialogfelddaten
	//{{AFX_DATA(COptionsSecurityPage)
	enum { IDD = IDD_OPTIONS_SECURITY };
	BOOL	m_exact{};
	BOOL	m_relaxed{};
	BOOL	m_none{};
	//}}AFX_DATA


// �berschreibungen
	// Vom Klassen-Assistenten generierte virtuelle Funktions�berschreibungen
	//{{AFX_VIRTUAL(COptionsSecurityPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV-Unterst�tzung
	//}}AFX_VIRTUAL

// Implementierung
protected:

	// Generierte Nachrichtenzuordnungsfunktionen
	//{{AFX_MSG(COptionsSecurityPage)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
public:
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ f�gt unmittelbar vor der vorhergehenden Zeile zus�tzliche Deklarationen ein.

#endif // AFX_OPTIONSSECURITYPAGE_H__59102206_048F_4DA0_B0F9_FAF29E56F1D1__INCLUDED_