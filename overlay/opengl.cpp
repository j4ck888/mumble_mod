/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "lib.h"
#include <time.h>
#include <GL/gl.h>
#include "../3rdparty/GL/glext.h"

#define TDEF(ret, name, arg) typedef ret (__stdcall * t##name) arg
#define GLDEF(ret, name, arg) TDEF(ret, name, arg); t##name o##name = NULL

GLDEF(HGLRC, wglCreateContext, (HDC));
GLDEF(void, glGenTextures, (GLsizei, GLuint *));
GLDEF(void, glDeleteTextures, (GLsizei, GLuint *));
GLDEF(void, glEnable, (GLenum));
GLDEF(void, glDisable, (GLenum));
GLDEF(void, glBlendFunc, (GLenum, GLenum));
GLDEF(void, glColorMaterial, (GLenum, GLenum));
GLDEF(void, glViewport, (GLint, GLint, GLsizei, GLsizei));
GLDEF(void, glMatrixMode, (GLenum));
GLDEF(void, glLoadIdentity, (void));
GLDEF(void, glOrtho, (GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble));
GLDEF(void, glBindTexture, (GLenum, GLuint));
GLDEF(void, glPushMatrix, (void));
GLDEF(void, glColor4ub, (GLubyte, GLubyte, GLubyte, GLubyte));
GLDEF(void, glTranslatef, (GLfloat, GLfloat, GLfloat));
GLDEF(void, glBegin, (GLenum));
GLDEF(void, glEnd, (void));
GLDEF(void, glTexCoord2f, (GLfloat, GLfloat));
GLDEF(void, glVertex2f, (GLfloat, GLfloat));
GLDEF(void, glPopMatrix, (void));
GLDEF(void, glTexParameteri, (GLenum, GLenum, GLint));
GLDEF(void, glTexEnvi, (GLenum, GLenum, GLint));
GLDEF(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *));
GLDEF(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *));
GLDEF(void, glPixelStorei, (GLenum, GLint));
GLDEF(void, wglMakeCurrent, (HDC, HGLRC));
GLDEF(HGLRC, wglGetCurrentContext, (void));
GLDEF(HDC, wglGetCurrentDC, (void));
GLDEF(int, GetDeviceCaps, (HDC, int));

#define INJDEF(ret, name, arg) GLDEF(ret, name, arg); static HardHook hh##name
#define INJECT(name) { o##name = reinterpret_cast<t##name>(GetProcAddress(hGL, #name)); if (o##name) { hh##name.setup(reinterpret_cast<voidFunc>(o##name), reinterpret_cast<voidFunc>(my##name)); o##name = (t##name) hh##name.call; } else { ods("OpenGL: No GetProc for %s", #name);} }

INJDEF(BOOL, wglSwapLayerBuffers, (HDC, UINT));
INJDEF(BOOL, wglSwapBuffers, (HDC));
INJDEF(BOOL, SwapBuffers, (HDC));

static bool bHooked = false;

class Context : protected Pipe {
	public:
		Context(HDC hdc);
		void draw(HDC hdc);

	protected:
		virtual void blit(unsigned int x, unsigned int y, unsigned int w, unsigned int h);
		virtual void setRect();
		virtual void newTexture(unsigned int width, unsigned int height);

	private:
		HGLRC ctx;
		GLuint texture;

		clock_t timeT;
		unsigned int frameCount;

		void initContext();
		void doDraw(HDC hdc);
};

Context::Context(HDC hdc) {
	timeT = clock();
	frameCount = 0;

	texture = ~0;
	ctx = owglCreateContext(hdc);

	HGLRC oldctx = owglGetCurrentContext();
	HDC oldhdc = owglGetCurrentDC();
	owglMakeCurrent(hdc, ctx);

	initContext();

	owglMakeCurrent(oldhdc, oldctx);
}

void Context::initContext() {
	// Here we go. From the top. Where is glResetState?
	oglDisable(GL_ALPHA_TEST);
	oglDisable(GL_AUTO_NORMAL);
	oglEnable(GL_BLEND);
	// Skip clip planes, there are thousands of them.
	oglDisable(GL_COLOR_LOGIC_OP);
	oglDisable(GL_COLOR_MATERIAL);
	oglDisable(GL_COLOR_TABLE);
	oglDisable(GL_CONVOLUTION_1D);
	oglDisable(GL_CONVOLUTION_2D);
	oglDisable(GL_CULL_FACE);
	oglDisable(GL_DEPTH_TEST);
	oglDisable(GL_DITHER);
	oglDisable(GL_FOG);
	oglDisable(GL_HISTOGRAM);
	oglDisable(GL_INDEX_LOGIC_OP);
	oglDisable(GL_LIGHTING);
	// Skip line smmooth
	// Skip map
	oglDisable(GL_MINMAX);
	// Skip polygon offset
	oglDisable(GL_SEPARABLE_2D);
	oglDisable(GL_SCISSOR_TEST);
	oglDisable(GL_STENCIL_TEST);
	oglEnable(GL_TEXTURE_2D);
	oglDisable(GL_TEXTURE_GEN_Q);
	oglDisable(GL_TEXTURE_GEN_R);
	oglDisable(GL_TEXTURE_GEN_S);
	oglDisable(GL_TEXTURE_GEN_T);

	oglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void Context::blit(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
	ods("OpenGL: Blit %d %d %d %d -- %d %d : %d", x, y, w, h, uiWidth, uiHeight, texture);

	if (texture == ~0)
		return;

	oglBindTexture(GL_TEXTURE_2D, texture);

	if ((x == 0) && (y == 0) && (w == uiWidth) && (h == uiHeight)) {
		oglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, uiWidth, uiHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, a_ucTexture);
	} else {
		if (w != uiWidth)
			oglPixelStorei(GL_UNPACK_ROW_LENGTH, uiWidth);
		oglTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA, GL_UNSIGNED_BYTE, a_ucTexture + 4 * (y * uiWidth + x));
		if (w != uiWidth)
			oglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
}

void Context::setRect() {
	ods("OpenGL: setRect");
}

void Context::newTexture(unsigned int width, unsigned int height) {
	ods("OpenGL: newTex");

	if (texture == ~0) {
		oglBindTexture(GL_TEXTURE_2D, 0);
		oglDeleteTextures(1, &texture);
		texture = ~0;
	}
	oglGenTextures(1, &texture);

	oglBindTexture(GL_TEXTURE_2D, texture);
	oglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	oglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	oglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	oglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	unsigned char *ptr = new unsigned char[width*height*4];
	memset(ptr, 0, width*height*4);
	oglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, ptr);
	delete [] ptr;
}

void Context::draw(HDC hdc) {
	HGLRC oldctx = owglGetCurrentContext();
	HDC oldhdc = owglGetCurrentDC();
	owglMakeCurrent(hdc, ctx);

	doDraw(hdc);

	owglMakeCurrent(oldhdc, oldctx);
}

void Context::doDraw(HDC hdc) {
	// DEBUG
	//sm->bDebug = true;

	clock_t t = clock();
	float elapsed = static_cast<float>(t - timeT) / CLOCKS_PER_SEC;
	++frameCount;
	if (elapsed > OVERLAY_FPS_INTERVAL) {
		OverlayMsg om;
		om.omh.uiMagic = OVERLAY_MAGIC_NUMBER;
		om.omh.uiType = OVERLAY_MSGTYPE_FPS;
		om.omh.iLength = sizeof(OverlayMsgFps);
		om.omf.fps = frameCount / elapsed;

		sendMessage(om);

		frameCount = 0;
		timeT = t;
	}

	unsigned int width, height;

	width = oGetDeviceCaps(hdc, HORZRES);
	height = oGetDeviceCaps(hdc, VERTRES);

	HWND hwnd = WindowFromDC(hdc);
	if (hwnd) {
		RECT r;
		if (GetClientRect(hwnd, &r)) {
			width = r.right - r.left;
			height = r.bottom - r.top;
		}
	}

	ods("OpenGL: DrawStart: Screen is %d x %d", width, height);

	checkMessage(width, height);

	oglViewport(0, 0, width, height);

	oglMatrixMode(GL_PROJECTION);
	oglLoadIdentity();
	oglOrtho(0, width, height, 0, -100.0, 100.0);

	oglMatrixMode(GL_MODELVIEW);

	oglBindTexture(GL_TEXTURE_2D, texture);
	oglPushMatrix();
	oglLoadIdentity();

	float w = static_cast<float>(uiWidth);
	float h = static_cast<float>(uiHeight);

	float left   = static_cast<float>(uiLeft);
	float top    = static_cast<float>(uiTop);
	float right  = static_cast<float>(uiRight);
	float bottom = static_cast<float>(uiBottom);

	float xm = (left) / w;
	float ym = (top) / h;
	float xmx = (right) / w;
	float ymx = (bottom) / h;

	oglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	oglBegin(GL_QUADS);

	oglTexCoord2f(xm, ymx);
	oglVertex2f(left, bottom);

	oglTexCoord2f(xm, ym);
	oglVertex2f(left, top);

	oglTexCoord2f(xmx, ym);
	oglVertex2f(right, top);

	oglTexCoord2f(xmx, ymx);
	oglVertex2f(right, bottom);
	oglEnd();

	oglPopMatrix();

}

static map<HDC, Context *> contexts;

static void doSwap(HDC hdc) {
	Context *c = contexts[hdc];

	if (!c) {
		ods("OpenGL: New context for device %p", hdc);
		c = new Context(hdc);
		contexts[hdc] = c;
	} else {
		ods("OpenGL: Reusing old context");
	}
	c->draw(hdc);
}

static BOOL __stdcall mywglSwapBuffers(HDC hdc) {
	ods("OpenGL: wglSwapBuffers");
	doSwap(hdc);

	hhwglSwapBuffers.restore();
	BOOL ret=owglSwapBuffers(hdc);
	hhwglSwapBuffers.inject();

	return ret;
}

static BOOL __stdcall mySwapBuffers(HDC hdc) {
	ods("OpenGL: SwapBuffers");

	hhSwapBuffers.restore();
	BOOL ret=oSwapBuffers(hdc);
	hhSwapBuffers.inject();

	return ret;
}

static BOOL __stdcall mywglSwapLayerBuffers(HDC hdc, UINT fuPlanes) {
	ods("OpenGL: SwapLayerBuffers %x",fuPlanes);

	hhwglSwapLayerBuffers.restore();
	BOOL ret=owglSwapLayerBuffers(hdc, fuPlanes);
	hhwglSwapLayerBuffers.inject();

	return ret;
}


#undef GLDEF
#define GLDEF(name) o##name = reinterpret_cast<t##name>(GetProcAddress(hGL, #name))

void checkOpenGLHook() {
	static bool bCheckHookActive = false;
	if (bCheckHookActive) {
		ods("OpenGL: Recursion in checkOpenGLHook");
		return;
	}

	bCheckHookActive = true;

	HMODULE hGL = GetModuleHandle("OpenGL32.DLL");

	if (hGL != NULL) {
		if (! bHooked) {
			char procname[1024];
			GetModuleFileName(NULL, procname, 1024);
			ods("OpenGL: Unhooked OpenGL App %s", procname);
			bHooked = true;

			// Add a ref to ourselves; we do NOT want to get unloaded directly from this process.
			HMODULE hTempSelf = NULL;
			GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<char *>(&checkOpenGLHook), &hTempSelf);

			INJECT(wglSwapBuffers);
			// INJECT(wglSwapLayerBuffers);

			GLDEF(wglCreateContext);
			GLDEF(glGenTextures);
			GLDEF(glDeleteTextures);
			GLDEF(glEnable);
			GLDEF(glDisable);
			GLDEF(glBlendFunc);
			GLDEF(glColorMaterial);
			GLDEF(glViewport);
			GLDEF(glMatrixMode);
			GLDEF(glLoadIdentity);
			GLDEF(glOrtho);
			GLDEF(glBindTexture);
			GLDEF(glPushMatrix);
			GLDEF(glColor4ub);
			GLDEF(glTranslatef);
			GLDEF(glBegin);
			GLDEF(glEnd);
			GLDEF(glTexCoord2f);
			GLDEF(glVertex2f);
			GLDEF(glPopMatrix);
			GLDEF(glTexParameteri);
			GLDEF(glTexEnvi);
			GLDEF(glTexImage2D);
			GLDEF(glTexSubImage2D);
			GLDEF(glPixelStorei);
			GLDEF(wglMakeCurrent);
			GLDEF(wglGetCurrentContext);
			GLDEF(wglGetCurrentDC);

			hGL = GetModuleHandle("GDI32.DLL");
			if (hGL) {
				// INJECT(SwapBuffers);
				GLDEF(GetDeviceCaps);
			} else {
				ods("OpenGL: Failed to find GDI32");
			}
		} else {
			hhwglSwapBuffers.check();
		}
	}

	bCheckHookActive = false;
}
