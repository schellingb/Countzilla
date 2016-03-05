/*
  Countzilla
  Copyright (C) 2013,2016 Bernhard Schelling

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Surface.h>
#include <ZL_Signal.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>
#include <ZL_Scene.h>
#include <ZL_Math.h>
#include <ZL_SynthImc.h>
#include <vector>
using namespace std;

extern ZL_SynthImcTrack imcEffect, imcHektik;
#define SCENE_GAME 5

struct sNumber : ZL_Vector
{
	int num;
	unsigned int hit;
	sNumber(scalar x, scalar y, int num = -1, unsigned int hit = 0) : ZL_Vector(x, y), num(num), hit(hit) { }
};
static vector<sNumber> numbers;
static int round_num = 0, hitnumber = 0, misses = 0;
static enum ePhase { PHASE_TITLE, PHASE_PREVIEW, PHASE_PLAY, PHASE_DONE, PHASE_GAMEOVER } phase;
static unsigned int phase_start = 0;
static ZL_Font fntNumbers;
static ZL_Surface srfCircle;
enum { MISSES_MAX = 3 };

static void SetPhase(ePhase now) { phase = now; phase_start = ZLTICKS; }

static void StartNextRound()
{
	round_num++;
	numbers.clear();
	for (int i = 1; i <= round_num; i++)
	{
		sNumber n(RAND_RANGE(0.2,0.8), RAND_RANGE(0.2,0.8), i);
		bool conflict = false;
		for (vector<sNumber>::iterator it = numbers.begin(); it != numbers.end(); ++it)
			if (sabs(it->x - n.x) < 0.1 && sabs(it->y - n.y) < 0.1) { conflict = true; break; }
		if (conflict) { i--; continue; }
		numbers.push_back(n);
		ZL_LOG2("GAME", "Num at %f - %f", n.x, n.y);
	}
	SetPhase(PHASE_TITLE);
	hitnumber = 0;
	imcHektik.SetSongBPM(100.0f + round_num*10).Play();
}

static void StartGame()
{
	round_num = 0;
	misses = 0;
	StartNextRound();
}

static struct sSceneGame : public ZL_Scene
{
	sSceneGame() : ZL_Scene(SCENE_GAME) { }

	void InitGlobal()
	{
		fntNumbers = ZL_Font("Data/typomoderno.ttf", s(100)).SetDrawOrigin(ZL_Origin::Center).SetDrawAtBaseline(false);
		srfCircle = ZL_Surface("Data/circle.png").SetDrawOrigin(ZL_Origin::Center);
	}

	void InitAfterTransition()
	{
		ZL_Display::sigPointerDown.connect(this, &sSceneGame::OnPointerDown);
		ZL_Display::sigKeyDown.connect(this, &sSceneGame::OnKeyDown);
		StartGame();
	}

	void OnPointerDown(ZL_PointerPressEvent& e)
	{
		if (phase != PHASE_PLAY || hitnumber == round_num || misses == MISSES_MAX) return;
		sNumber &n = numbers[hitnumber];
		ZL_Vector p = ZL_Display::Size() * n;
		static int chan = 0;
		if (p.GetDistance(e) < 100)
		{
			imcEffect.NoteOn((chan++)%8, 60+(hitnumber*3));
			n.hit = ZLTICKS;
			hitnumber++;
		}
		else
		{
			imcEffect.NoteOn((chan++)%8, 48);
			imcEffect.NoteOn((chan++)%8, 50);
			numbers.push_back(sNumber(e.x/ZLWIDTH, e.y/ZLHEIGHT, -(++misses), ZLTICKS));
		}
	}

	void OnKeyDown(ZL_KeyboardEvent& e)
	{
		if (e.key == ZLK_ESCAPE) ZL_Application::Quit();
	}

	void Draw()
	{
		ZL_Display::ClearFill(ZL_Color::Black);
		if      (phase == PHASE_TITLE && ZLTICKS - phase_start >= 1000) SetPhase(PHASE_PREVIEW);
		else if (phase == PHASE_TITLE)
		{
			scalar e = s(ZLTICKS - phase_start)/s(1000);
			scalar a = s(1)-sabs(e-s(.5))*s(2);
			fntNumbers.Draw(ZLHALFW, ZLHALFH, ZL_String("ROUND ")<<round_num, s(1)+a*2, ZLRGBA(1,1,.1,a));
		}
		else if (phase == PHASE_PREVIEW && ZLTICKS - phase_start >= (unsigned)round_num*500) SetPhase(PHASE_PLAY);
		else if (phase == PHASE_PREVIEW)
		{
			scalar e = s(ZLTICKS - phase_start)/s(500);
			sNumber n = numbers[(int)e];
			scalar a = s(1)-sabs((e-((int)e))-s(0.5))*s(2);
			ZL_Vector p = ZL_Display::Size() * n;
			fntNumbers.Draw(p, ZL_String(n.num), s(.6)+a, ZLRGBA(1,1,1,a));
			srfCircle.Draw(p, a, a, ZLRGBA(1,1,1,a));
		}
		else if (phase == PHASE_PLAY)
		{
			if (ZLTICKS - phase_start < 500)
			{
				scalar e = s(ZLTICKS - phase_start)/s(500);
				scalar a = s(1)-sabs(e-s(.5))*s(2);
				fntNumbers.Draw(ZLHALFW, ZLHALFH, "GO!!", s(1)+a*2, ZLRGBA(1,1,1,a));
			}
			for (vector<sNumber>::iterator it = numbers.begin(); it != numbers.end(); ++it)
			{
				if (it->hit && it->num == round_num && ZLTICKS - it->hit >= 1000)
				{
					SetPhase(PHASE_DONE);
					break;
				}
				if (it->num == -MISSES_MAX && it->hit && ZLTICKS - it->hit >= 1000)
				{
					SetPhase(PHASE_GAMEOVER);
					imcHektik.SetSongBPM(60);
					break;
				}
				if (!it->hit || ZLTICKS - it->hit >= 1000) continue;
				scalar e = s(ZLTICKS - it->hit)/s(1000);
				scalar a = s(1)-sabs((e-((int)e))-s(0.5))*s(2);
				if (it->num < 0)
					fntNumbers.Draw(ZL_Display::Size() * (*it), "X", s(.6)+a, ZLRGBA(1,.1,.1,a));
				else
					fntNumbers.Draw(ZL_Display::Size() * (*it), ZL_String(it->num), s(.6)+a, ZLRGBA(.1,1,.1,a));
			}
		}
		else if (phase == PHASE_DONE && ZLTICKS - phase_start >= 1000) StartNextRound();
		else if (phase == PHASE_DONE)
		{
			scalar e = s(ZLTICKS - phase_start)/s(1000);
			scalar a = s(1)-sabs(e-s(.5))*s(2);
			fntNumbers.Draw(ZLHALFW, ZLHALFH, "OK!", s(1)+a*2, ZLRGBA(.1,.1,1,a));
		}
		else if (phase == PHASE_GAMEOVER && ZLTICKS - phase_start >= 2000) StartGame();
		else if (phase == PHASE_GAMEOVER)
		{
			scalar e = s(ZLTICKS - phase_start)/s(2000);
			scalar a = s(1)-sabs(e-s(.5))*s(2);
			for (vector<sNumber>::iterator it = numbers.begin(); it != numbers.end(); ++it)
				if (it->num < 0)  continue;
				else if (it->hit) fntNumbers.Draw(ZL_Display::Size() * (*it), ZL_String(it->num), s(.6)+a, ZLRGBA(.1,.8,.1,a));
				else              fntNumbers.Draw(ZL_Display::Size() * (*it), ZL_String(it->num), s(.6)+a, ZLRGBA(.4,.4,.4,a));

			fntNumbers.Draw(ZLHALFW, ZLHALFH, "GAMEOVER", s(1)+a*s(1.5), ZLRGBA(.5,.1,.1,a));
			fntNumbers.Draw(20, ZLFROMH(43), "XXX", s(1)+a*s(.1), ZLRGBA(1,0,0,.5*a), ZL_Origin::CenterLeft);
		}
		fntNumbers.Draw(20, ZLFROMH(43), "XXX", ZLRGB(.3,.3,.3), ZL_Origin::CenterLeft);
		fntNumbers.Draw(20, ZLFROMH(43), ZL_String("X")*misses, ZLRGB(1,0,0), ZL_Origin::CenterLeft);
	}
} SceneGame;

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct sCountzilla : public ZL_Application
{
	sCountzilla() : ZL_Application(60) { }

	virtual void Load(int argc, char *argv[])
	{
		if (!ZL_Application::LoadReleaseDesktopDataBundle()) return;
		if (!ZL_Display::Init("Countzilla", 800, 480, ZL_DISPLAY_ALLOWRESIZEHORIZONTAL)) return;
		ZL_Audio::Init();
		ZL_SceneManager::Init(SCENE_GAME);
	}
} Countzilla;

//sound effect and song data
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static TImcSongEnvelope ImcSongEnvList[] = { { 0, 256, 39, 0, 24, 255, true, 255, }, { 0, 256, 173, 24, 16, 255, true, 255, } };
static TImcSongEnvelopeCounter ImcSongEnvCounterList[] = { { 0, 0, 128 }, { 1, 0, 0 }, { -1, -1, 256 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 128 }, { 1, 1, 0 }, { 1, 1, 0 },
  { 1, 1, 0 }, { 0, 2, 128 }, { 1, 2, 0 }, { 1, 2, 0 }, { 1, 2, 0 }, { 0, 3, 128 }, { 1, 3, 0 }, { 1, 3, 0 }, { 1, 3, 0 }, { 0, 4, 128 }, { 1, 4, 0 }, { 1, 4, 0 },
  { 1, 4, 0 }, { 0, 5, 128 }, { 1, 5, 0 }, { 1, 5, 0 }, { 1, 5, 0 }, { 0, 6, 128 }, { 1, 6, 0 }, { 1, 6, 0 }, { 1, 6, 0 }, { 0, 7, 128 }, { 1, 7, 0 }, { 1, 7, 0 }, { 1, 7, 0 } };
static TImcSongOscillator ImcSongOscillatorList[] = { { 8, 0, IMCSONGOSCTYPE_SINE, 0, -1, 100, 1, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 0, -1, 100, 3, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 0, -1, 100, 4, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 100, 6, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 1, -1, 100, 7, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 1, -1, 100, 8, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 10, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 2, -1, 100, 11, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 2, -1, 100, 12, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 14, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 3, -1, 100, 15, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 3, -1, 100, 16, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 18, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 4, -1, 100, 19, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 4, -1, 100, 20, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 22, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 5, -1, 100, 23, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 5, -1, 100, 24, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 6, -1, 100, 26, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 6, -1, 100, 27, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 6, -1, 100, 28, 2 }, { 8, 0, IMCSONGOSCTYPE_SINE, 7, -1, 100, 30, 2 }, { 8, 66, IMCSONGOSCTYPE_SINE, 7, -1, 100, 31, 2 },
  { 8, 127, IMCSONGOSCTYPE_SINE, 7, -1, 100, 32, 2 } };
static TImcSongEffect ImcSongEffectList[] = { { 32385, 258, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 }, { 32385, 258, 1, 1, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 },
  { 32385, 258, 1, 2, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 }, { 32385, 258, 1, 3, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 }, { 32385, 258, 1, 4, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 },
  { 32385, 258, 1, 5, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 }, { 32385, 258, 1, 6, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 }, { 32385, 258, 1, 7, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 2 } };
static unsigned char ImcSongChannelVol[8] = {100, 100, 100, 100, 100, 100, 100, 100 };
static unsigned char ImcSongChannelEnvCounter[8] = {0, 5, 9, 13, 17, 21, 25, 29 };
static bool ImcSongChannelStopNote[8] = {true, true, true, true, true, true, true, true };
static TImcSongData imcEffectData = { 0, 0, 2, 33, 24, 8, 100, NULL, NULL, NULL, ImcSongEnvList, ImcSongEnvCounterList,
	ImcSongOscillatorList, ImcSongEffectList, ImcSongChannelVol, ImcSongChannelEnvCounter, ImcSongChannelStopNote };
ZL_SynthImcTrack imcEffect(&imcEffectData);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static unsigned int HEKTIK_ImcSongOrderTable[] = { 0x000000011, 0x020000011, 0x011000011, 0x021000022, 0x011000011, 0x021000012, 0x011000021, 0x011000012,
  0x011000011, 0x011000011, 0x010000000, 0x010000000, 0x011000000, 0x011000000, 0x011000011, 0x011000011, 0x011000011, 0x011000011 };
static unsigned char HEKTIK_ImcSongPatternData[] = { 0x50, 0, 0, 0, 0, 0, 0, 0, 0x50, 0, 0x52, 0, 0, 0x54, 0, 0, 0x50, 0, 0x50, 0, 0, 0, 0, 0, 0x50, 0, 0x50,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0x50, 0, 0, 0, 0, 0, 0, 0, 0x55, 0, 0, 0, 0, 0, 0, 0, 0x52, 0x42, 0, 0, 0, 0, 0, 0, 0x50, 0, 0, 0, 0x50, 0x40, 0x50, 0x40, 0x50,
	0x40, 0x50, 0x40, 0x50, 0x40, 0x50, 0x40, 0x50, 0x62, 0x74, 0x74, 0x50, 0, 0x50, 0, 0x54, 0, 0x54, 0, 0x52, 0, 0x52, 0, 0x51, 0, 0x50, 0x50,
	0, 0, 0, 0, 0, 0, 0, 0, 0x50, 0, 0x50, 0, 0x54, 0, 0x54, 0 };
static unsigned char HEKTIK_ImcSongPatternLookupTable[] = { 0, 2, 4, 4, 4, 4, 4, 5, };
static TImcSongEnvelope HEKTIK_ImcSongEnvList[] = { { 0, 256, 209, 1, 23, 255, true, 255, }, { 128, 256, 174, 8, 16, 16, true, 255, },
  { 0, 256, 871, 8, 16, 16, true, 255, }, { 0, 256, 523, 8, 16, 255, true, 255, }, { 0, 256, 10, 8, 255, 255, true, 255, },
  { 0, 256, 373, 8, 16, 255, true, 255, }, { 64, 256, 261, 8, 15, 255, true, 255, }, { 0, 256, 108, 8, 16, 255, true, 255, },
  { 0, 256, 523, 8, 15, 255, true, 255, }, { 0, 256, 3269, 8, 16, 255, true, 255, }, { 0, 256, 523, 8, 16, 255, true, 255, },
  { 32, 256, 196, 8, 16, 255, true, 255, }, { 0, 256, 108, 0, 24, 255, true, 255, }, { 0, 256, 476, 0, 24, 255, true, 255, } };
static TImcSongEnvelopeCounter HEKTIK_ImcSongEnvCounterList[] = { { 0, 0, 158 }, { -1, -1, 256 }, { 1, 0, 256 }, { 2, 0, 256 },
  { 3, 0, 256 }, { 4, 1, 256 }, { 5, 1, 256 }, { 6, 1, 256 }, { 7, 1, 256 }, { 8, 1, 256 }, { 9, 1, 256 }, { 10, 6, 256 },
  { 11, 6, 256 }, { 12, 7, 128 }, { 13, 7, 128 } };
static TImcSongOscillator HEKTIK_ImcSongOscillatorList[] = { { 5, 15, IMCSONGOSCTYPE_SINE, 0, -1, 72, 1, 2 }, { 8, 0, IMCSONGOSCTYPE_NOISE, 0, -1, 204, 3, 1 },
	{ 5, 227, IMCSONGOSCTYPE_SINE, 0, -1, 126, 4, 1 }, { 6, 0, IMCSONGOSCTYPE_SINE, 1, -1, 114, 6, 7 }, { 8, 0, IMCSONGOSCTYPE_SINE, 1, -1, 12, 8, 9 },
  { 8, 0, IMCSONGOSCTYPE_NOISE, 1, -1, 152, 10, 1 }, { 7, 0, IMCSONGOSCTYPE_NOISE, 1, 3, 96, 1, 1 }, { 8, 0, IMCSONGOSCTYPE_SINE, 2, -1, 100, 0, 0 },
  { 8, 0, IMCSONGOSCTYPE_SINE, 3, -1, 100, 0, 0 }, { 8, 0, IMCSONGOSCTYPE_SINE, 4, -1, 100, 0, 0 }, { 8, 0, IMCSONGOSCTYPE_SINE, 5, -1, 100, 0, 0 },
  { 8, 0, IMCSONGOSCTYPE_NOISE, 6, -1, 127, 1, 12 }, { 4, 150, IMCSONGOSCTYPE_SINE, 7, -1, 254, 14, 1 } };
static TImcSongEffect HEKTIK_ImcSongEffectList[] = { { 7620, 1100, 1, 0, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 }, { 228, 0, 1, 0, IMCSONGEFFECTTYPE_LOWPASS, 1, 0 },
  { 9398, 833, 1, 1, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 }, { 134, 216, 1, 1, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 }, { 122, 0, 3675, 6, IMCSONGEFFECTTYPE_DELAY, 0, 0 },
  { 255, 156, 1, 6, IMCSONGEFFECTTYPE_RESONANCE, 1, 1 }, { 227, 0, 1, 6, IMCSONGEFFECTTYPE_HIGHPASS, 1, 0 }, { 508, 8320, 1, 7, IMCSONGEFFECTTYPE_OVERDRIVE, 0, 1 } };
static unsigned char HEKTIK_ImcSongChannelVol[8] = {255, 255, 100, 100, 100, 100, 140, 255 };
static unsigned char HEKTIK_ImcSongChannelEnvCounter[8] = {0, 5, 0, 0, 0, 0, 11, 13 };
static bool HEKTIK_ImcSongChannelStopNote[8] = {true, true, false, false, false, false, true, true };
static TImcSongData imcHektikData = { 0x12, 3675, 14, 15, 13, 8, 117, HEKTIK_ImcSongOrderTable,HEKTIK_ImcSongPatternData, HEKTIK_ImcSongPatternLookupTable, HEKTIK_ImcSongEnvList,
	HEKTIK_ImcSongEnvCounterList, HEKTIK_ImcSongOscillatorList, HEKTIK_ImcSongEffectList, HEKTIK_ImcSongChannelVol, HEKTIK_ImcSongChannelEnvCounter, HEKTIK_ImcSongChannelStopNote };
ZL_SynthImcTrack imcHektik(&imcHektikData);
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
