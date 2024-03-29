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

#pragma once


// CVisualizerScope, scope style visualizer

class CVisualizerScope : public CVisualizerBase
{
public:
	CVisualizerScope(bool bBlur);
	virtual ~CVisualizerScope();

	void Create(int Width, int Height);
	void SetSampleRate(int SampleRate);
	void Draw();
	void Display(CDC *pDC, bool bPaintMsg);

private:
	void RenderBuffer();
	void ClearBackground();

private:
	COLORREF *m_pBlitBuffer;

	bool m_bBlur;
	int	 m_iWindowBufPtr;
	short *m_pWindowBuf;

#ifdef _DEBUG
	int m_iPeak;
#endif
};
