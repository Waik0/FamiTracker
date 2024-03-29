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

#ifndef VRC6_H
#define VRC6_H

#include "external.h"
#include "channel.h"

class CVRC6_Pulse : public CExChannel {
public:
	CVRC6_Pulse(CMixer *pMixer, int ID);
	void Reset();
	void Write(uint16 Address, uint8 Value);
	void Process(int Time);

private:
	uint8	m_iDutyCycle, 
			m_iVolume, 
			m_iGate, 
			m_iEnabled;
	uint32	m_iPeriod;
	uint8	m_iPeriodLow, 
			m_iPeriodHigh;
	int32	m_iCounter;
	uint8	m_iDutyCycleCounter;
};

class CVRC6_Sawtooth : public CExChannel {
public:
	CVRC6_Sawtooth(CMixer *pMixer, int ID);
	void Reset();
	void Write(uint16 Address, uint8 Value);
	void Process(int Time);

private:
	uint8	m_iPhaseAccumulator, 
			m_iPhaseInput, 
			m_iEnabled, 
			m_iResetReg;
	uint32	m_iPeriod;
	uint8	m_iPeriodLow, 
			m_iPeriodHigh;
	int32	m_iCounter;
};

class CVRC6 : public CExternal {
public:
	CVRC6(CMixer *pMixer);
	virtual ~CVRC6();
	void Reset();
	void Write(uint16 Address, uint8 Value);
	uint8 Read(uint16 Address, bool &Mapped);
	void EndFrame();
	void Process(uint32 Time);

private:
	CVRC6_Pulse	*m_pPulse1, *m_pPulse2;
	CVRC6_Sawtooth *m_pSawtooth;
};

#endif /* VRC6_H */