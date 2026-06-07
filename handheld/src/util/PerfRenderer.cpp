#include "PerfRenderer.h"
#include "PerfTimer.h"

#include "Mth.h"
#include "../client/gui/Font.h"
#include "../client/renderer/gles.h"
#include "../client/renderer/Tesselator.h"
#include "../client/Minecraft.h"

PerfRenderer::PerfRenderer( Minecraft* mc, Font* font )
:   _mc(mc),
	_font(font),
	_debugPath("root"),
	frameTimePos(0),
	lastTimer(-1)
{
	for (int i = 0; i < 512; ++i) {
		frameTimes.push_back(0);
		tickTimes.push_back(0);
	}
}

void PerfRenderer::debugFpsMeterKeyPress( int key )
{
	std::vector<PerfTimer::ResultField> list = PerfTimer::getLog(_debugPath);
	if (list.empty()) return;

	PerfTimer::ResultField node = list[0];
	list.erase(list.begin());
	if (key == 0) {
		if (node.name.length() > 0) {
			int pos = _debugPath.rfind(".");
			if (pos != std::string::npos) _debugPath = _debugPath.substr(0, pos);
		}
	} else {
		key--;
		if (key < (int)list.size() && list[key].name != "unspecified") {
			if (_debugPath.length() > 0) _debugPath += ".";
			_debugPath += list[key].name;
		}
	}
}

void PerfRenderer::renderFpsMeter( float tickTime )
{
	std::vector<PerfTimer::ResultField> list = PerfTimer::getLog(_debugPath);
	if (list.empty())
		return;

	PerfTimer::ResultField node = list[0];
	list.erase(list.begin());

	long usPer60Fps = 1000000l / 60;
	if (lastTimer == -1) {
		lastTimer = getTimeS();
	}
	float now = getTimeS();
	tickTimes[ frameTimePos ] = tickTime;
	frameTimes[frameTimePos ] = now - lastTimer;
	lastTimer = now;

	if (++frameTimePos >= (int)frameTimes.size())
		frameTimePos = 0;

	glClear(GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glEnable2(GL_COLOR_MATERIAL);
	glLoadIdentity2();

//#ifdef __3DS__
//	glOrtho(0, (GLfloat)_mc->width, (GLfloat)_mc->height, 0, 1000, 3000);
//#else
	glOrthof(0, (GLfloat)_mc->width, (GLfloat)_mc->height, 0, 1000, 3000);
//#endif
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity2();
	glTranslatef2(0, 0, -2000);
	glLineWidth(1);

	glDisable2(GL_TEXTURE_2D);
	Tesselator& t = Tesselator::instance;

	t.begin(GL_TRIANGLES);
	int hh1 = (int) (usPer60Fps / 200);
	float count = (float)frameTimes.size();
	t.color(0x20000000);
	t.vertex(0, (float)(_mc->height - hh1), 0);
	t.vertex(0, (float)_mc->height, 0);
	t.vertex(count, (float)_mc->height, 0);
	t.vertex(count, (float)(_mc->height - hh1), 0);

	t.color(0x20200000);
	t.vertex(0, (float)(_mc->height - hh1 * 2), 0);
	t.vertex(0, (float)(_mc->height - hh1), 0);
	t.vertex(count, (float)(_mc->height - hh1), 0);
	t.vertex(count, (float)(_mc->height - hh1 * 2), 0);

	t.draw();
	float totalTime = 0;
	for (unsigned int i = 0; i < frameTimes.size(); i++) {
		totalTime += frameTimes[i];
	}
	int hh = (int) (totalTime / 200 / frameTimes.size());
	glEnable(GL_TEXTURE_2D);

	{
		std::stringstream msg;
		if (node.name != "unspecified") {
			msg << "[0] ";
		}
		if (node.name.length() == 0) {
			msg << "ROOT ";
		} else {
			msg << node.name << " ";
		}
		int col = 0xffffff;
		_font->drawShadow(msg.str(), 10.0f, 10.0f, col);
		std::string msg2 = toPercentString(node.globalPercentage);
		_font->drawShadow(msg2, 100.0f, 10.0f, col);
	}

	for (unsigned int i = 0; i < list.size(); i++) {
		PerfTimer::ResultField& result = list[i];
		std::stringstream msg;
		if (result.name != "unspecified") {
			msg << "[" << (i + 1) << "] ";
		} else {
			msg << "[?] ";
		}

		msg << result.name;
		float xx = 10.0f;
		float yy = 20.0f + i * 10.0f;
		
		int color = result.getColor();
		if (result.percentage > 30.0f) {
			color = 0xff0000; // Red for bottleneck
		}
		
		_font->drawShadow(msg.str(), xx, yy, color);
		std::string msg2 = toPercentString(result.percentage);
		_font->drawShadow(msg2, xx + 100.0f, yy, color);
		msg2 = toPercentString(result.globalPercentage);
		_font->drawShadow(msg2, xx + 150.0f, yy, color);
	}
}

std::string PerfRenderer::toPercentString( float percentage )
{
	char buf[32] = {0};
	sprintf(buf, "%3.2f%%", percentage);
	return buf;
}
