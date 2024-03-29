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

#include <memory>
#include "APU.h"
#include "VRC6.h"

// Konami VRC6 external sound chip emulation

CVRC6_Pulse::CVRC6_Pulse(CMixer *pMixer, int ID) : CExChannel(pMixer, SNDCHIP_VRC6, ID) 
{
	Reset();
}

void CVRC6_Pulse::Reset()
{
	m_iDutyCycle = m_iVolume = m_iGate = m_iEnabled = 0;
	m_iPeriod = m_iPeriodLow = m_iPeriodHigh = 0;
	m_iCounter = 0;
	m_iDutyCycleCounter = 0;
}

void CVRC6_Pulse::Write(uint16 Address, uint8 Value)
{
	switch (Address) {
		case 0x00:
            m_iGate = Value & 0x80;
            m_iDutyCycle = ((Value & 0x70) >> 4) + 1;
            m_iVolume = Value & 0x0F;
			if (m_iGate)
				Mix(m_iVolume);
  			break;
		case 0x01:
            m_iPeriodLow = Value;
            m_iPeriod = m_iPeriodLow + (m_iPeriodHigh << 8);
			break;
		case 0x02:
            m_iEnabled = (Value & 0x80);
            m_iPeriodHigh = (Value & 0x0F);
            m_iPeriod = m_iPeriodLow + (m_iPeriodHigh << 8);
			break;
	}
}

void CVRC6_Pulse::Process(int Time)
{
	if (!m_iEnabled || m_iPeriod == 0) {
		m_iTime += Time;
		return;
	}

	while (Time >= m_iCounter) {
		Time      -= m_iCounter;
		m_iTime	  += m_iCounter;
		m_iCounter = m_iPeriod + 1;
	
		m_iDutyCycleCounter = (m_iDutyCycleCounter + 1) & 0x0F;
		Mix((m_iGate || m_iDutyCycleCounter >= m_iDutyCycle) ? m_iVolume : 0);
	}

	m_iCounter -= Time;
	m_iTime += Time;
}

CVRC6_Sawtooth::CVRC6_Sawtooth(CMixer *pMixer, int ID) : CExChannel(pMixer, SNDCHIP_VRC6, ID) 
{
	Reset();
}

void CVRC6_Sawtooth::Reset()
{
	m_iPhaseAccumulator = m_iPhaseInput = m_iEnabled = m_iResetReg = 0;
	m_iPeriod = 0;
	m_iPeriodLow = m_iPeriodHigh = 0;
	m_iCounter = 0;
}

void CVRC6_Sawtooth::Write(uint16 Address, uint8 Value)
{
	switch (Address) {
		case 0x00:
            m_iPhaseInput = (Value & 0x3F);
			break;
		case 0x01:
            m_iPeriodLow = Value;
            m_iPeriod = m_iPeriodLow + (m_iPeriodHigh << 8);
			break;
		case 0x02:
            m_iEnabled = (Value & 0x80);
            m_iPeriodHigh = (Value & 0x0F);
            m_iPeriod = m_iPeriodLow + (m_iPeriodHigh << 8);
			break;
	}
}

void CVRC6_Sawtooth::Process(int Time)
{
	if (!m_iEnabled || !m_iPeriod) {
		m_iTime += Time;
		return;
	}

	while (Time >= m_iCounter) {
		Time 	  -= m_iCounter;
		m_iTime	  += m_iCounter;
		m_iCounter = m_iPeriod + 1;

		if (m_iResetReg & 1)
			m_iPhaseAccumulator = (m_iPhaseAccumulator + m_iPhaseInput) & 0xFF;

		m_iResetReg++;

		if (m_iResetReg == 14) {
			m_iPhaseAccumulator = 0;
			m_iResetReg = 0;
		}

		// The 5 highest bits of accumulator are sent to the mixer
		Mix(m_iPhaseAccumulator >> 3);
	}

	m_iCounter -= Time;
	m_iTime += Time;
}

CVRC6::CVRC6(CMixer *pMixer)
{
	m_pPulse1 = new CVRC6_Pulse(pMixer, CHANID_VRC6_PULSE1);
	m_pPulse2 = new CVRC6_Pulse(pMixer, CHANID_VRC6_PULSE2);
	m_pSawtooth = new CVRC6_Sawtooth(pMixer, CHANID_VRC6_SAWTOOTH);
}

CVRC6::~CVRC6()
{
	if (m_pPulse1)
		delete m_pPulse1;

	if (m_pPulse2)
		delete m_pPulse2;

	if (m_pSawtooth)
		delete m_pSawtooth;
}

void CVRC6::Reset()
{
	m_pPulse1->Reset();
	m_pPulse2->Reset();
	m_pSawtooth->Reset();
}

void CVRC6::Write(uint16 Address, uint8 Value)
{
	switch (Address) {
		case 0x9000:
		case 0x9001:
		case 0x9002:
			m_pPulse1->Write(Address & 3, Value);
			break;			
		case 0xA000:
		case 0xA001:
		case 0xA002:
			m_pPulse2->Write(Address & 3, Value);
			break;
		case 0xB000:
		case 0xB001:
		case 0xB002:
			m_pSawtooth->Write(Address & 3, Value);
			break;
	}
}

uint8 CVRC6::Read(uint16 Address, bool &Mapped)
{
	Mapped = false;
	return 0;
}

void CVRC6::EndFrame()
{
	m_pPulse1->EndFrame();
	m_pPulse2->EndFrame();
	m_pSawtooth->EndFrame();
}

void CVRC6::Process(uint32 Time)
{
	m_pPulse1->Process(Time);
	m_pPulse2->Process(Time);
	m_pSawtooth->Process(Time);
}
