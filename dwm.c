/* see LICENSE file for copyright and license details. */

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <fribidi.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/XF86keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD__ */

#if defined(__GLIBC__) && __GLIBC_PREREQ(2, 1)
#define BACKTRACE
#endif /* __GLIBC__ */

#ifdef BACKTRACE
#include <execinfo.h>
#endif /* BACKTRACE */

#include "dwm.h"

/* ======================
 * = Public Data Types
 * ====================== */

enum {
	ColorFG, ColorBG, ColorBorder, ColorBorderBG,
ColorLast };

enum {
	SchemeNorm, SchemeSel, SchemeUrg, SchemeTitle,
	SchemeStatus, SchemeStatusSep, SchemeWinButton,
SchemeLast };

enum {
	ClickInvalid, ClickTagBar, ClickLtSymbol, ClickWinTitle, ClickStatusText,
	ClickWinButton, ClickWinButtonDouble, ClickClientWin,
	ClickRootWin, ClickWinArea,
};

enum {
	UnicodeGeneric,
	UnicodeFarsi,
	UnicodeEmoji,
	UnicodeNerd,
};

struct FontDef {
	const char *name;
	int block;
};

struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
};

struct Rule {
	const char *class;
	const char *instance;
	const char *title;
	int isfloating;
	int compfullscreen;
	int noautofocus;
	int noswallow;
	int isterminal;
	int nojitter;
	unsigned int tags;
	int monitor;
};

union Arg {
	int i;
	unsigned int ui;
	float f;
	void (*lt)(Monitor *m);
	void (*adj)(const Arg *arg);
	const void *v;
};

struct Key {
	KeySym keysym;
	unsigned int mod;
	void (*func)(const Arg *);
	const Arg arg;
};

struct Button {
	int click;
	unsigned int button;
	unsigned int mod;
	void (*func)(const Arg *arg);
	const Arg arg;
};

struct StatusClick {
	const char *module;
	unsigned int button;
	unsigned int mod;
	void (*func)(const Arg *arg);
	const Arg arg;
};

/* ============
 * = config.h
 * ============ */

#include "config.h"

/* =============
 * = Constants
 * ============= */

/* utf8decode return values */
enum {
	UTF8Init = 0,
	UTF8Accept = 0,
	UTF8Reject = 12,
	UTF8Invalid = 0xFFFD, /* replacement character */
	UTF8ZWNBS = 0xFEFF,   /* zero-width non-breaking space */
};

/* systray and Xembed constants */
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ  0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT  1
#define SYSTEM_TRAY_REQUEST_DOCK           0
#define SYSTEM_TRAY_BEGIN_MESSAGE          1
#define SYSTEM_TRAY_CANCEL_MESSAGE         2
#define XEMBED_EMBEDDED_NOTIFY             0
#define XEMBED_MAPPED                      (1 << 0)
#define XEMBED_VERSION                     0

/* buffer sizes */
#define UTF8CacheSize      2048
#define StatusSize         1024
#define WinTitleSize       256
#define LtSymbolSize       16
#define ClassNameSize      32
#define PertagStackSize    16
#define BackTraceSize      16

/* ==================
 * = Utility Macros
 * ================== */

/* utility macros */
#define BUTTONMASK            (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK             (BUTTONMASK|PointerMotionMask)
#define TAGMASK               ((1 << LENGTH(tags)) - 1)
#define STATUSMON             (*statusmonptr)
#define MAX(A, B)             ((A) > (B) ? (A) : (B))
#define MIN(A, B)             ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)      ((A) <= (X) && (X) <= (B))
#define LENGTH(A)             (sizeof(A) / sizeof((A)[0]))
#define ISVISIBLEONTAG(C, T)  ((C)->tags & (T))
#define ISVISIBLE(C)          ISVISIBLEONTAG(C, (C)->mon->tagset[(C)->mon->seltags])
#define WIDTH(C)              ((C)->w + 2 * (C)->bw)
#define HEIGHT(C)             ((C)->h + 2 * (C)->bw)
#define GEOM(C)               (C)->x, (C)->y, (C)->w, (C)->h

/* draw text with padding */
#define RENDERTEXTWP(SCM, STR, X, W, INV) \
	(rendertext(SCM, STR, X, 0, W, barheight, fontheight / 2, INV))

#define ISUTILWIN(W) \
	(W == root || W == selmon->barwin || (systray && W == systray->win))

#define CLEANMASK(MASK) \
	(MASK & ~(numlockmask|LockMask) \
	& (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

#define INTERSECT(X, Y, W, H, M) \
	 (MAX(0, MIN((X)+(W),(M)->wx+(M)->ww) - MAX((X),(M)->wx)) \
	* MAX(0, MIN((Y)+(H),(M)->wy+(M)->wh) - MAX((Y),(M)->wy)))

/* X macro */
#define X1(A, B) A,
#define X2(A, B) B,

/* ======================
 * = Private Data Types
 * ====================== */

struct Pertag {
	float mfact;
	int nmaster;
	unsigned int sellt;
	const Layout *lt[2];
};

struct BarState {
	int isselmon, isstatusmon, isfloating, bdw;
	unsigned int tags, occtags, urgtags, nclients, selpos;
	char statustext[StatusSize], title[WinTitleSize], ltsymbol[LtSymbolSize];
};

struct ButtonPos {
	int tags[LENGTH(tags)];
	int tagsend;
	int ltsymbol;
	struct {
		int exists;
		int start;
		int end;
	} modules[LENGTH(statusclick)];
	int statusstart;
};

struct Monitor {
	char ltsymbol[LtSymbolSize];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int bdw;              /* bar draw width (barwidth minus systraywidth) */
	int mx, my, mw, mh;   /* monitor geometry */
	int wx, wy, ww, wh;   /* window area geometry */
	int gappx;
	unsigned int seltags, sellt, tagset[2];
	int showbar, topbar;
	Client *clients;
	Client *sel;
	Client *stack;
	Monitor *next;
	Window barwin;
	const Layout *lt[2];
	Pertag pertag[LENGTH(tags)];
	unsigned int pertagstack[PertagStackSize];
	unsigned int pertagtop;
	BarState bs;
	ButtonPos bp;
};

struct Client {
	char title[WinTitleSize], class[ClassNameSize], instance[ClassNameSize];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags, pid, xkblayout;
	int isfixed, isfloating, isurgent, neverfocus, oldfloating, isfullscreen;
	int compfullscreen, isterminal, noswallow, nojitter, origcompfullscreen;
	int desktop, geomvalid, noautofocus, isfocused;
	Client *next;
	Client *snext;
	Client *swallow;
	Monitor *mon;
	Window win, origwin, buttonwin;
	int ismapped; /* mapped state for systray icons */
};

struct Systray {
	Window win;
	Client *icons;
};

struct ClickEv {
	int isvalid;
	Time time;
	Window win;
	unsigned int button;
};

struct XFont {
	int height, block;
	XftFont *xftfont;
	FcPattern *pattern;
	XFont *next;
};

struct UTF8Cache {
	struct {
		unsigned long codepoint;
		XFont *font;
		int width;
	} list[UTF8CacheSize];
	int idx, len;
};

struct UnicodeBlockDef {
	int block;
	unsigned long start;
	unsigned long end;
};

/* =============
 * = Atoms
 * ============= */

#define ATOMS(X) \
	X( WMProtocols,                "WM_PROTOCOLS"       ) \
	X( WMState,                    "WM_STATE"           ) \
	X( WMTakeFocus,                "WM_TAKE_FOCUS"      ) \
	X( WMDelete,                   "WM_DELETE_WINDOW"   ) \
	X( Manager,                    "MANAGER"            ) \
	X( Xembed,                     "_XEMBED"            ) \
	X( XembedInfo,                 "_XEMBED_INFO"       ) \
	X( DWMSwallow,                 "_DWM_SWALLOW"       ) \
	X( DWMSwallower,               "_DWM_SWALLOWER"     ) \
	X( DWMSwallowed,               "_DWM_SWALLOWED"     ) \
	X( DWMMonDesktop,              "_DWM_MON_DESKTOP"   ) \
	X( DWMMonSel,                  "_DWM_MON_SEL"       )

#define NETATOMS(X) \
	X( NetSupported,               "_NET_SUPPORTED"                 ) \
	X( NetWMCheck,                 "_NET_SUPPORTING_WM_CHECK"       ) \
	X( NetActiveWindow,            "_NET_ACTIVE_WINDOW"             ) \
	X( NetWMName,                  "_NET_WM_NAME"                   ) \
	X( NetWMState,                 "_NET_WM_STATE"                  ) \
	X( NetWMFullscreen,            "_NET_WM_STATE_FULLSCREEN"       ) \
	X( NetWMUserTime,              "_NET_WM_USER_TIME"              ) \
	X( NetWMUserTimeWindow,        "_NET_WM_USER_TIME_WINDOW"       ) \
	X( NetWMWindowType,            "_NET_WM_WINDOW_TYPE"            ) \
	X( NetWMWindowTypeDialog,      "_NET_WM_WINDOW_TYPE_DIALOG"     ) \
	X( NetWMWindowTypeDock,        "_NET_WM_WINDOW_TYPE_DOCK"       ) \
	X( NetWMPID,                   "_NET_WM_PID"                    ) \
	X( NetWMDesktop,               "_NET_WM_DESKTOP"                ) \
	X( NetWMWindowOpacity,         "_NET_WM_WINDOW_OPACITY"         ) \
	X( NetClientList,              "_NET_CLIENT_LIST"               ) \
	X( NetCurrentDesktop,          "_NET_CURRENT_DESKTOP"           ) \
	X( NetNumberOfDesktops,        "_NET_NUMBER_OF_DESKTOPS"        ) \
	X( NetDesktopNames,            "_NET_DESKTOP_NAMES"             ) \
	X( NetSystemTray,              "_NET_SYSTEM_TRAY"               ) \
	X( NetSystemTrayOP,            "_NET_SYSTEM_TRAY_OPCODE"        ) \
	X( NetSystemTrayOrientation,   "_NET_SYSTEM_TRAY_ORIENTATION"   )

enum { ATOMS(X1) };
enum { NETATOMS(X1) };
static char *atom_names[] = { ATOMS(X2) };
static char *netatom_names[] = { NETATOMS(X2) };
static Atom atoms[LENGTH(atom_names)], netatoms[LENGTH(netatom_names)];
#undef ATOMS
#undef NETATOMS

/* =============
 * = Cursors
 * ============= */

#define CURSORS(X) \
	X( CurNormal,  XC_left_ptr ) \
	X( CurResize,  XC_sizing   ) \
	X( CurMove,    XC_fleur    )

enum { CURSORS(X1) };
static int cursor_shapes[] = { CURSORS(X2) };
static Cursor cursors[LENGTH(cursor_shapes)];
#undef CURSORS

/* ==================
 * = Unicode Blocks
 * ================== */

static const UnicodeBlockDef blockdefs[] = {
	/* block          start      end    */
	{ UnicodeFarsi,   0x600,     0x6FF    },
	{ UnicodeFarsi,   0x750,     0x77F    },
	{ UnicodeFarsi,   0x8A0,     0x8FF    },
	{ UnicodeFarsi,   0xFB50,    0xFDFF   },
	{ UnicodeFarsi,   0xFE70,    0xFEFF   },
	{ UnicodeFarsi,   0x1EE00,   0x1EEFF  },

	{ UnicodeEmoji,   0x2600,    0x26FF   },
	{ UnicodeEmoji,   0x2700,    0x27BF   },
	{ UnicodeEmoji,   0x1F300,   0x1F5FF  },
	{ UnicodeEmoji,   0x1F600,   0x1F64F  },
	{ UnicodeEmoji,   0x1F680,   0x1F6FF  },
	{ UnicodeEmoji,   0x1F900,   0x1F9FF  },

	{ UnicodeNerd,    0xE000,    0xE00D,  },
	{ UnicodeNerd,    0xE0A0,    0xE0A2,  },
	{ UnicodeNerd,    0xE0B0,    0xE0B3,  },
	{ UnicodeNerd,    0xE0A3,    0xE0A3,  },
	{ UnicodeNerd,    0xE0B4,    0xE0C8,  },
	{ UnicodeNerd,    0xE0CC,    0xE0D2,  },
	{ UnicodeNerd,    0xE0D4,    0xE0D4,  },
	{ UnicodeNerd,    0xE5FA,    0xE62B,  },
	{ UnicodeNerd,    0xE700,    0xE7C5,  },
	{ UnicodeNerd,    0xF000,    0xF2E0,  },
	{ UnicodeNerd,    0xE200,    0xE2A9,  },
	{ UnicodeNerd,    0xF400,    0xF4A8,  },
	{ UnicodeNerd,    0x2665,    0x2665,  },
	{ UnicodeNerd,    0x26A1,    0x26A1,  },
	{ UnicodeNerd,    0xF27C,    0xF27C,  },
	{ UnicodeNerd,    0xF300,    0xF313,  },
	{ UnicodeNerd,    0x23FB,    0x23FE,  },
	{ UnicodeNerd,    0x2B58,    0x2B58,  },
	{ UnicodeNerd,    0xF500,    0xFD46,  },
	{ UnicodeNerd,    0xE300,    0xE3EB,  },
};

/* ===================
 * = Global Variables
 * =================== */

static char statustext[StatusSize];
static const char broken[] = "broken";

static Display *dpy;
static xcb_connection_t *xcon;
static int screen;
static int sw, sh; /* screen width, height */
static int depth;
static Visual *visual;
static Colormap colormap;
static Pixmap pixmap;
static GC gc;
static XFont *fonts;
static XftDraw *xftdraw;
static UTF8Cache utf8cache = { .len = 0, .idx = 0 };

static Window root, wmcheckwin, ignoreenterwin = 0;
static Monitor *mons, *selmon, **statusmonptr;
static Systray *systray = NULL;
static XftColor schemes[SchemeLast][ColorLast];
static int barheight, fontheight;
static unsigned int numlockmask = 0;
static volatile int running = 1, mustrestart = 0;
static int startup = 0;
static int currentdesktop = -1;
static int (*xerrorxlib)(Display *, XErrorEvent *);

/* event handlers */
static void (*handler[LASTEvent]) (XEvent *) = {
	[MapRequest]        =  maprequest,
	[DestroyNotify]     =  destroynotify,
	[UnmapNotify]       =  unmapnotify,
	[EnterNotify]       =  enternotify,
	[LeaveNotify]       =  leavenotify,
	[MotionNotify]      =  motionnotify,
	[ConfigureNotify]   =  configurenotify,
	[ConfigureRequest]  =  configurerequest,
	[PropertyNotify]    =  propertynotify,
	[ClientMessage]     =  clientmessage,
	[ButtonPress]       =  buttonpress,
	[KeyPress]          =  keypress,
	[Expose]            =  expose,
	[FocusIn]           =  focusin,
	[ResizeRequest]     =  resizerequest,
	[MappingNotify]     =  mappingnotify,
};

/* =============
 * = Functions
 * ============= */

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);

	if (!(dpy = XOpenDisplay(NULL)))
		die("cannot open display");

	if (!(xcon = XGetXCBConnection(dpy)))
		die("cannot get xcb connection");

	checkotherwm();
	startup = 1;
	setup();
	scan();
	startup = 0;
	run(); /* event loop */

	cleanup();
	XCloseDisplay(dpy);
	if (mustrestart)
		execvp(argv[0], argv);

	return EXIT_SUCCESS;
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, 0);
	XSetErrorHandler(xerror);
	XSync(dpy, 0);
}

void
setup(void)
{
	XSetWindowAttributes swa;
	char trayatom_name[32];

	/* clean up any zombies immediately */
	sigchld(0);

#ifdef BACKTRACE
	/* signal handler for printing a backtrace on segfault */
	if (signal(SIGSEGV, sigbacktrace) == SIG_ERR)
		die("can't install SIGSEGV handler:");
#endif /* BACKTRACE */

	/* signal handler for restarting */
	if (signal(SIGHUP, sigrestart) == SIG_ERR)
		die("can't install SIGHUP handler:");

	/* init vars */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	depth = DefaultDepth(dpy, screen);
	visual = DefaultVisual(dpy, screen);
	colormap = DefaultColormap(dpy, screen);

	/* init atoms */
	XInternAtoms(dpy, atom_names, LENGTH(atom_names), 0, atoms);
	XInternAtoms(dpy, netatom_names, LENGTH(netatom_names), 0, netatoms);
	snprintf(trayatom_name, sizeof(trayatom_name), "_NET_SYSTEM_TRAY_S%d", screen);
	netatoms[NetSystemTray] = XInternAtom(dpy, trayatom_name, 0);

	/* init graphics */
	renderinit();

	/* init monitors */
	updatestatustext();
	updatemons();
	loadmonsettings();

	/* init NetWMCheck window */
	wmcheckwin = createsimplewin();
	setwinprop(root, netatoms[NetWMCheck], wmcheckwin);
	setwinprop(wmcheckwin, netatoms[NetWMCheck], wmcheckwin);
	XChangeProperty(dpy, wmcheckwin, netatoms[NetWMName],
		XInternAtom(dpy, "UTF8_STRING", 0), 8,
		PropModeReplace, (unsigned char *)"dwm", 3);

	/* init EWMH props */
	XDeleteProperty(dpy, root, netatoms[NetClientList]);
	XChangeProperty(dpy, root, netatoms[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)netatoms, LENGTH(netatoms));

	/* select events */
	swa.cursor = cursors[CurNormal];
	swa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask|KeyPressMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &swa);
	XSelectInput(dpy, root, swa.event_mask);

	grabkeys();

#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
}

void
scan(void)
{
	int i;
	unsigned int num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num))
		return;

	/* regular windows */
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1)
		|| getatomprop(wins[i], atoms[DWMSwallow]) != None)
			continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
			manage(wins[i], wa);
	}

	/* swallowed windows */
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (getatomprop(wins[i], atoms[DWMSwallow]) == atoms[DWMSwallowed])
			manage(wins[i], wa);
	}

	/* swallower windows */
	for (i = 0; i < num; i++) { /* now swallower windows */
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (getatomprop(wins[i], atoms[DWMSwallow]) == atoms[DWMSwallower])
			manage(wins[i], wa);
	}

	/* transient windows */
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa))
			continue;
		if (XGetTransientForHint(dpy, wins[i], &d1)
		&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
			manage(wins[i], wa);
	}

	XFree(wins);

	focus(NULL);
	arrange(NULL);
}

void
run(void)
{
	XEvent ev;

	XSync(dpy, 0);
	while (running && !XNextEvent(dpy, &ev)) {
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
	}
}

void
maprequest(XEvent *e)
{
	XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;

	if (!wintoclient(ev->window))
		manage(ev->window, wa);
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if (systray && ev->window == systray->win) {
		free(systray);
		systray = NULL;
	}
	else if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = origwintoclient(ev->window)))
		unmanageswallowed(c);
	else if ((c = wintosystrayicon(ev->window)))
		systrayremoveicon(c);
}

void
unmapnotify(XEvent *e)
{
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (c->swallow)
			return;
		else if (ev->send_event)
			setclientstate(c, WithdrawnState);
		else
			unmanage(c, 0);
	} else if ((c = wintosystrayicon(ev->window))) {
		c->ismapped = 0;
		systrayupdate();
	}
}

void
enternotify(XEvent *e)
{
	Client *c, *s;
	/* Monitor *m; */
	XCrossingEvent *ev = &e->xcrossing;

	if (ev->window != root && (ev->mode != NotifyNormal || ev->detail == NotifyInferior))
		return;

	/* ignore incorrect and parasitic enternotify events  */
	if (isenterignored(ev->window))
		return;

	if ((c = wintoclient(ev->window)))
		for (s = c->mon->clients; s; s = s->next)
			setcardprop(s->buttonwin, netatoms[NetWMWindowOpacity], 0);
	else if ((c = winbuttontoclient(ev->window)))
		setcardprop(c->buttonwin, netatoms[NetWMWindowOpacity], 0xFFFFFFFF);

	selmon = c ? c->mon : wintomon(ev->window);
	focus(c);
}

void
leavenotify(XEvent *e)
{
	Client *c;
	XCrossingEvent *ev = &e->xcrossing;

	if (ev->window != root && (ev->mode != NotifyNormal || ev->detail == NotifyInferior))
		return;

	if ((c = winbuttontoclient(ev->window)))
		setcardprop(c->buttonwin, netatoms[NetWMWindowOpacity], 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *prev = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;

	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != prev && prev)
		focusmon(m);

	prev = m;
}

void
configurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;
	int updated;

	if (ev->window != root)
		return;

	updated = (sw != ev->width || sh != ev->height);
	sw = ev->width;
	sh = ev->height;

	if (updatemons() || updated) {
		renderupdatesize();
		focus(NULL);
		arrange(NULL);
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if (!(c = wintoclient(ev->window))) {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
		/* XSync(dpy, 0); */
		return;
	}

	if (ev->value_mask & CWBorderWidth)
		c->bw = MIN(ev->border_width, MIN(c->mon->ww, c->mon->wh) / 3);

	if (c->isfullscreen || !afloat(c)) {
		sendconfigurenotify(c);
		/* XSync(dpy, 0); */
		return;
	}

	m = c->mon;
	ev->value_mask &= c->nojitter ? (unsigned)~(CWX|CWY) : ev->value_mask;
	c->x = (ev->value_mask & CWX)      ? m->mx + ev->x : c->x;
	c->y = (ev->value_mask & CWY)      ? m->my + ev->y : c->y;
	c->w = (ev->value_mask & CWWidth)  ? ev->width     : c->w;
	c->h = (ev->value_mask & CWHeight) ? ev->height    : c->h;

	if ((c->x + c->w) > m->mx + m->mw)
		c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */

	if ((c->y + c->h) > m->my + m->mh)
		c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */

	if (ISVISIBLE(c)) {
		c->geomvalid = 0;
		resize(c, GEOM(c), 0);
		XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
	}
	else if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
		sendconfigurenotify(c);

	/* XSync(dpy, 0); */
}

void
propertynotify(XEvent *e)
{
	Client *c;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		updatestatustext();
		drawbar(STATUSMON);
	}
	else if (ev->state == PropertyDelete)
		return;

	/* return early if property is not supported */
	if (ev->atom != atoms[XembedInfo]
	 && ev->atom != netatoms[NetWMWindowType]
	 && ev->atom != netatoms[NetWMName]
	 && ev->atom != XA_WM_HINTS
	 && ev->atom != XA_WM_NORMAL_HINTS
	 && ev->atom != XA_WM_TRANSIENT_FOR)
		return;

	if (ev->atom == atoms[XembedInfo] && (c = wintosystrayicon(ev->window))) {
		systrayupdateicon(c, NULL);
		return;
	}

	if (!(c = wintoclient(ev->window)))
		return;

	if (ev->atom == netatoms[NetWMWindowType])
		updatewindowtype(c);

	if (ev->atom == netatoms[NetWMName]
	 || ev->atom == XA_WM_NAME)
	{
		updatetitle(c);
		if (c == c->mon->sel)
			drawbar(c->mon);
	}

	if (ev->atom == XA_WM_HINTS) {
		updatewmhints(c);
		drawbar(NULL);
	}
	else if (ev->atom == XA_WM_NORMAL_HINTS) {
		c->hintsvalid = 0;
		arrange(c->mon);
	}
	else if (ev->atom == XA_WM_TRANSIENT_FOR
		&& !c->isfloating
		&& gettransientfor(c->win, NULL))
	{
		c->isfloating = 1;
		arrange(c->mon);
	}
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c;

	if (systray
		&& cme->window == systray->win
		&& cme->message_type == netatoms[NetSystemTrayOP])
	{
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK && cme->data.l[2])
			systrayaddicon(cme->data.l[2]);
		return;
	}

	if (!(c = wintoclient(cme->window)))
		return;

	if (cme->message_type == netatoms[NetWMState]) {
		if (cme->data.l[1] == netatoms[NetWMFullscreen]
		|| cme->data.l[2] == netatoms[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatoms[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
keypress(XEvent *e)
{
	int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);

	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
		{
			keys[i].func(&(keys[i].arg));
		}
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		if (m == STATUSMON)
			systrayupdate();
	}
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		focusinput(selmon->sel);
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *c;

	if ((c = wintosystrayicon(ev->window)))
		systrayupdateicon(c, ev);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
manage(Window w, XWindowAttributes wa)
{
	Client *c, *t = NULL;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->mon = selmon;
	c->desktop = -1;
	c->oldbw = wa.border_width;
	c->xkblayout  = xkblayout;
	c->noautofocus = noautofocus;
	c->w = c->oldw = wa.width;
	c->h = c->oldh = wa.height;
	c->pid = getwinpid(c->win);

	updateclass(c);
	updatetitle(c);
	applyrules(c);

	c->bw = MIN(borderpx, MIN(c->mon->ww, c->mon->wh) / 3);
	if ((c->isfloating = gettransientfor(c->win, &t) || c->isfloating) && t) {
		c->mon = t->mon;
		c->tags = t->tags;
		c->x = t->x + (t->w - WIDTH(c)) / 2;
		c->y = t->y + (t->h - HEIGHT(c)) / 2;
	} else {
		c->x = c->mon->wx + (c->mon->ww - WIDTH(c)) / 2;
		c->y = c->mon->wy + (c->mon->wh - HEIGHT(c)) / 2;
	}

	if (startup)
		loadclienttagsandmon(c);

	XSetWindowBorderWidth(dpy, c->win, c->bw);
	drawborder(c->win, SchemeNorm);

	/* updatewinbutton(c); */
	updatesizehints(c);
	updatewindowtype(c);
	updatewmhints(c);
	appendtoclientlist(c->win);
	XSelectInput(dpy, c->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);

#ifdef __linux__
	if (c->pid)
		setcardprop(c->win, netatoms[NetWMPID], c->pid);
#endif /* __linux__ */

	if (swallow(c))
		return;

	setclientstate(c, NormalState);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */

	/* only send configure notify to hidden windows.
	 * visible windows will be configured in arrange() -> resize(). */
	if (!ISVISIBLE(c))
		sendconfigurenotify(c);

	if (startup) {
		attach(c);
		attachstack(c);
	} else if (!ISVISIBLE(c)) {
		if (c->noautofocus)
			attachbottom(c);
		else {
			attachdirection(c);
			seturgent(c, 1);
		}
		attachstackbottom(c);
		XMapWindow(dpy, c->win);
		restack(c->mon);
	} else if (c->noautofocus) {
		attachbottom(c);
		attachstackbottom(c);
		arrange(c->mon);
		XMapWindow(dpy, c->win);
		ignoreenter(c->win);
	} else {
		attachdirection(c);
		c->mon->sel = c;
		attachstack(c);
		arrange(c->mon);
		XMapWindow(dpy, c->win);
		focus(NULL);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;

	XkbLockGroup(dpy, XkbUseCoreKbd, xkblayout);

	if (c->swallow) {
		unswallow(c, destroyed, 0);
		return;
	}

	if (!destroyed) {
		/* XGrabServer(dpy); /1* avoid race conditions *1/ */
		/* XSetErrorHandler(xerrordummy); */
		XSelectInput(dpy, c->win, NoEventMask);
		XSetWindowBorderWidth(dpy, c->win, c->oldbw);
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		/* XSync(dpy, 0); */
		/* XSetErrorHandler(xerror); */
		/* XUngrabServer(dpy); */
	}

	XDestroyWindow(dpy, c->buttonwin);
	tagreduced(c, 1, 0);
	detach(c);
	detachstack(c);
	free(c);
	updateclientlist();
	arrange(m);
	focus(NULL);
}

void
cleanup(void)
{
	Layout lt = { "", NULL };
	Monitor *m;
	Client *c, *f;

	selmon->tagset[selmon->seltags] = ~0 & TAGMASK;
	selmon->lt[selmon->sellt] = &lt;
	arrange(selmon);

	/* unmanage clients */
	/* XGrabServer(dpy); */
	/* XSetErrorHandler(xerrordummy); */

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next) {
			XSelectInput(dpy, c->win, NoEventMask);
			XSetWindowBorderWidth(dpy, c->win, c->oldbw);
			XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
			setclientstate(c, WithdrawnState);
			if (c->swallow) {
				c->swallow->win = c->origwin;
				c->swallow->mon = c->mon;
				c->swallow->tags = c->tags;
				updateclientdesktop(c->swallow);
				c->swallow->next = c->next;
				c->next = c->swallow;
			}
		}

	/* XSync(dpy, 0); */
	/* XSetErrorHandler(xerror); */
	/* XUngrabServer(dpy); */

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c;) {
			f = c;
			c = c->next;
			free(f);
		}
	}

	while (mons)
		cleanupmon(mons);

	systraycleanup();
	renderfree();

	XDestroyWindow(dpy, wmcheckwin);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	/* XSync(dpy, 0); */

	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatoms[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

int
swallow(Client *c)
{
	Client *t;

	if (c->noswallow || c->isterminal
		|| (c->isfloating && !c->isfullscreen && !swallowfloating)
		|| !(t = getparentterminal(c)))
	{
		return 0;
	}

	/* XSetErrorHandler(xerrordummy); */
	XSelectInput(dpy, t->win, NoEventMask);
	XUngrabButton(dpy, AnyButton, AnyModifier, t->win);
	setclientstate(t, WithdrawnState);
	XUnmapWindow(dpy, t->win);
	/* XSync(dpy, 0); */
	/* XSetErrorHandler(xerror); */

	t->swallow = c;
	c->mon = t->mon;
	c->tags = t->tags;

	t->origwin = t->win;
	t->win = c->win;

	t->origcompfullscreen = t->compfullscreen;
	t->compfullscreen = c->compfullscreen;

	t->geomvalid = 0;
	t->isfocused = 0;

	updatetitle(t);
	setatomprop(t->win, atoms[DWMSwallow], atoms[DWMSwallower]);
	setatomprop(t->origwin, atoms[DWMSwallow], atoms[DWMSwallowed]);
	setfullscreenprop(t->win, t->isfullscreen);
	arrange(t->mon);
	XMapWindow(dpy, t->win);

	if (t->mon->stack == t)
		focus(t);
	else
		ignoreenter(t->win);

	return 1;
}

void
unswallow(Client *c, int destroyed, int reattach)
{
	if (!destroyed && !reattach) {
		/* XGrabServer(dpy); /1* avoid race conditions *1/ */
		/* XSetErrorHandler(xerrordummy); */
		XSelectInput(dpy, c->win, NoEventMask);
		XSetWindowBorderWidth(dpy, c->win, c->swallow->oldbw);
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		/* XSync(dpy, 0); */
		/* XSetErrorHandler(xerror); */
		/* XUngrabServer(dpy); */
	}

	if (reattach) {
		c->swallow->mon = c->mon;
		c->swallow->tags = c->tags;
		XDeleteProperty(dpy, c->swallow->win, atoms[DWMSwallow]);
		setclientstate(c->swallow, NormalState);
		updatetitle(c->swallow);
		updateclientdesktop(c->swallow);
		attachdirection(c->swallow);
		attachstack(c->swallow);
		XMapWindow(dpy, c->swallow->win);
		c->swallow = NULL;
	} else {
		free(c->swallow);
		c->swallow = NULL;
		updateclientlist();
	}

	c->win = c->origwin;
	c->compfullscreen = c->origcompfullscreen;
	c->geomvalid = 0;
	c->isfocused = 0;

	grabbuttons(c, 0);
	XSelectInput(dpy, c->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	XDeleteProperty(dpy, c->win, atoms[DWMSwallow]);
	setfullscreenprop(c->win, c->isfullscreen);
	setclientstate(c, NormalState);
	updatetitle(c);
	updateclientdesktop(c);
	XMapWindow(dpy, c->win);
	focus(NULL);
	arrange(c->mon);
}

void
unmanageswallowed(Client *c)
{
	XDeleteProperty(dpy, c->win, atoms[DWMSwallow]);
	free(c->swallow);
	c->swallow = NULL;
	updateclientlist();
}

void
ignoreenter(Window w)
{
	Window win, wtmp;
	unsigned int uitmp;
	int itmp;

	XQueryPointer(dpy, root, &wtmp, &win, &itmp, &itmp, &itmp, &itmp, &uitmp);
	if (!w)
		ignoreenterwin = win;
	else if (win == w)
		ignoreenterwin = w;
}

/* ignore incorrect enternotify events related to swallowing */
int
isenterignored(Window w)
{
	int ignored = (w == ignoreenterwin)
		   /* ignore enternotify events caused by an unswallow */
		|| (selmon->sel && selmon->sel->swallow && !ismapped(selmon->sel->win));
	ignoreenterwin = 0;
	return ignored;
}

void
tagreduced(Client *c, int unmanage, unsigned int newtags) {
	unsigned int targettags = unmanage ? c->tags : ~(~c->tags|newtags);

	if (!targettags)
		return;

	pertagpush(c->mon, targettags);

	if (resettag && numtiledontag(c) == 1) {
		c->mon->nmaster = nmaster;
		c->mon->mfact = mfact;
		c->mon->sellt ^= 1;
		c->mon->lt[c->mon->sellt] = &layouts[0];
	} else if (nmasterbias >= 0 && c->mon->nmaster > nmasterbias && ismasterontag(c)) {
		c->mon->nmaster = MAX(c->mon->nmaster - 1, 0);
	}

	pertagpop(c->mon);
}

int
updatemons(void)
{
	int updated = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, moncount, xsicount;
		Client *c;
		Monitor *m, **mp;
		XineramaScreenInfo *xsiall = XineramaQueryScreens(dpy, &xsicount);
		XineramaScreenInfo *xsi = NULL;

		for (moncount = 0, m = mons; m; m = m->next, moncount++);

		/* only consider unique geometries as separate monitors */
		xsi = ecalloc(xsicount, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < xsicount; i++)
			if (isxsiunique(xsiall[i], xsi, j))
				xsi[j++] = xsiall[i];
		XFree(xsiall);
		xsicount = j;

		/* new monitors if xsicount > moncount */
		for (i = moncount; i < xsicount; i++) {
			for (mp = &mons; *mp; mp = &(*mp)->next);
			*mp = createmon();
		}
		for (m = mons, i = 0; m; m = m->next, i++)
			if (i >= moncount || isnewmon(m, xsi[i])) {
				updated = 1;
				m->num = i;
				m->mx = m->wx = xsi[i].x_org;
				m->my = m->wy = xsi[i].y_org;
				m->mw = m->ww = xsi[i].width;
				m->mh = m->wh = xsi[i].height;
				m->bdw = m->ww;
				m->gappx = MIN(gappx, MIN(m->ww, m->wh) / 3);
			}

		/* removed monitors if moncount > xsicount */
		for (i = xsicount; i < moncount; i++) {
			updated = 1;
			for (m = mons; m && m->next; m = m->next);
			for (c = m->clients; c; c = c->next) {
				c->mon = mons;
				attachdirection(c);
				attachstackbottom(c);
			}
			/* while ((c = m->clients)) { */
			/* 	m->clients = c->next; */
			/* 	detachstack(c); */
			/* 	c->mon = mons; */
			/* 	attachdirection(c); */
			/* 	attachstackbottom(c); */
			/* } */
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}

		free(xsi);
	} else
#endif /* XINERAMA */

	{
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			updated = 1;
			mons->mw = mons->ww = mons->bdw = sw;
			mons->mh = mons->wh = sh;
		}
	}

	if (updated) {
		selmon = mons;
		selmon = wintomon(root);
		updatestatusmonptr();
		updatedesktops();
		updatebarwin(NULL);
	}

	return updated;
}

#ifdef XINERAMA
int
isxsiunique(XineramaScreenInfo xsi, XineramaScreenInfo *list, size_t nlist)
{
	while (nlist--)
		if (xsi.x_org  == list[nlist].x_org
		 && xsi.y_org  == list[nlist].y_org
		 && xsi.width  == list[nlist].width
		 && xsi.height == list[nlist].height)
			return 0;
	return 1;
}

int
isnewmon(Monitor *m, XineramaScreenInfo xsi)
{
	if (xsi.x_org  != m->mx
	 || xsi.y_org  != m->my
	 || xsi.width  != m->mw
	 || xsi.height != m->mh)
		return 1;
	return 0;
}
#endif /* XINERAMA */

void
updatestatusmonptr(void)
{
	Monitor *m;

	if (statusmonnum < 0)
		statusmonptr = &selmon;
	else {
		for (m = mons; m && m->next && m->num != statusmonnum; m = m->next);
		statusmonptr = &m;
	}
}

Monitor *
createmon(void)
{
	Monitor *m;
	int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = m->pertagstack[0] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	/* strscpy(m->ltsymbol, layouts[0].symbol, sizeof(m->ltsymbol)); */

	for (i = 0; i < LENGTH(tags); i++) {
		m->pertag[i].mfact = m->mfact;
		m->pertag[i].nmaster = m->nmaster;
		m->pertag[i].lt[0] = m->lt[0];
		m->pertag[i].lt[1] = m->lt[1];
	}

	return m;
}

void
applyrules(Client *c)
{
	const Rule *r;
	Monitor *m;
	int i;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->title, r->title))
		 && (!r->class || strstr(c->class, r->class))
		 && (!r->instance || strstr(c->instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->compfullscreen = r->compfullscreen;
			c->noautofocus = r->noautofocus;
			c->noswallow  = r->noswallow;
			c->isterminal = r->isterminal;
			c->nojitter   = r->nojitter;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

void
focus(Client *c)
{
	Client *f;
	Monitor *m;
	XkbStateRec xkbstate;

	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);

	if (c && c->isfocused)
		return;

	for (m = mons; m; m = m->next)
		for (f = m->clients; f; f = f->next)
			if (f->isfocused) {
				f->isfocused = 0;
				grabbuttons(f, 0);
				drawborder(f->win, SchemeNorm);
				XkbGetState(dpy, XkbUseCoreKbd, &xkbstate);
				f->xkblayout = xkbstate.group;
				XkbLockGroup(dpy, XkbUseCoreKbd, xkblayout);
				break;
			}

	if (c) {
		c->isfocused = 1;
		selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		XkbLockGroup(dpy, XkbUseCoreKbd, c->xkblayout);
		grabbuttons(c, 1);
		detachstack(c);
		attachstack(c);
		selmon->sel = c;
		focusinput(c);
		updateborder(c);
	} else {
		selmon->sel = NULL;
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatoms[NetActiveWindow]);
	}

	drawbar(NULL);
	updatecurrentdesktop();
}

void
focusmon(Monitor *m)
{
	selmon = m;
	focus(NULL);
}

void
focusinput(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		setwinprop(root, netatoms[NetActiveWindow], c->win);
	}
	sendevent(c, atoms[WMTakeFocus]);
}

void
arrange(Monitor *m)
{
	Client *c;

	if (!m) {
		for (m = mons; m; m = m->next)
			arrange(m);
		return;
	}

	showhide(m->stack);

	strscpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof(m->ltsymbol));
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);

	restack(m);
	for (c = m->clients; c; c = c->next)
		updateclientdesktop(c);
}

void
showhide(Client *c)
{
	if (!c)
		return;

	if (ISVISIBLE(c))
	{
		if (c->isfullscreen)
		{
			if (c->compfullscreen)
				resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
			else
				resizeclient(c, c->mon->wx, c->mon->wy, c->mon->ww, c->mon->wh);
		}
		else if (afloat(c))
			resize(c, GEOM(c), 0);

		/* show clients top down */
		showhide(c->snext);
	}
	else
	{
		/* hide clients bottom up */
		showhide(c->snext);

		updatewinbutton(c);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
		c->geomvalid = 0;
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XWindowChanges wc;
	XEvent ev;

	drawbar(m);

	if (!m->sel)
		return;

	if (m->lt[m->sellt]->arrange)
	{
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		if (m->sel->isfloating && !m->sel->isfullscreen) {
			XConfigureWindow(dpy, m->sel->win, CWSibling|CWStackMode, &wc);
			wc.sibling = m->sel->win;
		}
		for (c = m->stack; c; c = c->snext)
			if (ISVISIBLE(c) && c->isfullscreen) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
		/* XConfigureWindow(dpy, m->barwin, CWSibling|CWStackMode, &wc); */
		/* wc.sibling = m->barwin; */
		for (c = m->stack; c; c = c->snext)
			if (ISVISIBLE(c) && c->isfloating && !c->isfullscreen && c != m->sel) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
		for (c = m->stack; c; c = c->snext) {
			if (ISVISIBLE(c) && !c->isfloating) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
		}
	}

	for (c = m->stack; c; c = c->snext)
		updatewinbutton(c);

	/* if (m->sel->isfloating || !m->lt[m->sellt]->arrange) { */
	/* 	XRaiseWindow(dpy, m->sel->win); */
	/* 	XRaiseWindow(dpy, m->sel->buttonwin); */
	/* } */

	if (m->sel && (m->sel->isfullscreen || !afloat(m->sel)))
		for (c = m->stack; c; c = c->snext)
			if (c->isfullscreen && c != m->sel && ISVISIBLE(c))
				setfullscreen(c, 0);

	/* XSync(dpy, 0); */
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact) || !c->geomvalid)
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->geomvalid = 1;
	c->x = wc.x = x;
	c->y = wc.y = y;
	c->w = wc.width = w;
	c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	sendconfigurenotify(c);
	updateborder(c);
	updatewinbutton(c);
	/* XSync(dpy, 0); */
}

void
sendconfigurenotify(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = 0;
	XSendEvent(dpy, c->win, 0, StructureNotifyMask, (XEvent *)&ce);
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin, iw, ih;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);

	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	}
	else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}

	if (*h < barheight)
		*h = barheight;
	if (*w < barheight)
		*w = barheight;

	if (resizehints || afloat(c)) {

		iw = *w;
		ih = *h;

		if (!c->hintsvalid)
			updatesizehints(c);

		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = (c->basew == c->minw && c->baseh == c->minh);
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}

		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}

		/* increment calculation requires this */
		if (baseismin) {
			*w -= c->basew;
			*h -= c->baseh;
		}

		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;

		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);

		if (hintcenter) {
			if (*w < iw)
				*x += ((iw - *w) / 2);
			if (*h < ih)
				*y += ((ih - *h) / 2);
		}
	}

	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (c->swallow)
		return;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		size.flags = PSize; /* size is uninitialized, ensure that size.flags aren't used */

	/* base size */
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	}
	else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	}
	else
		c->basew = c->baseh = 0;

	/* min size */
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	}
	else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	}
	else
		c->minw = c->minh = 0;

	/* max size */
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	}
	else
		c->maxw = c->maxh = 0;

	/* resize increments */
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	}
	else
		c->incw = c->inch = 0;

	/* aspect ratio */
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	}
	else
		c->maxa = c->mina = 0.0;

	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->isfloating = c->isfloating || c->isfixed;
	c->hintsvalid = 1;
}

void
loadclienttagsandmon(Client *c)
{
	unsigned int desktop, monnum;
	Monitor *m;

	if (!getcardprop(c->win, netatoms[NetWMDesktop], (long *)&desktop))
		return;

	c->tags = (1 << (desktop % LENGTH(tags))) & TAGMASK;
	monnum = desktop / LENGTH(tags);
	for (m = mons; m && m->num != monnum; m = m->next);
	if (m)
		c->mon = m;
}

void
loadmonsettings(void)
{
	Atom type;
	int format, selnum, i = 0;
	unsigned long nitems, remaining;
	long *value = NULL;
	Monitor *m;

	/* load selected monitor */
	getcardprop(root, atoms[DWMMonSel], (long *)&selnum);

	/* load tags of monitors */
	while (XGetWindowProperty(dpy, root, atoms[DWMMonDesktop], i, 1, 0,
		XA_CARDINAL, &type, &format, &nitems, &remaining,
		(unsigned char **)&value) == Success
		&& value
		&& nitems)
	{
		for (m = mons; m && m->num != i; m = m->next);
		if (!m)
			break;
		m->tagset[0] = m->tagset[1] = m->pertagstack[0] =
			(1 << (unsigned int)*value) & TAGMASK;
		if (m->num == selnum)
			selmon = m;
		i++;
		XFree(value);
	}

	XFree(value);
}

void
updateclass(Client *c)
{
	XClassHint ch = { NULL, NULL };

	XGetClassHint(dpy, c->win, &ch);
	strscpy(c->class, ch.res_class ? ch.res_class : broken, sizeof(c->class));
	strscpy(c->instance, ch.res_name ? ch.res_name : broken, sizeof(c->instance));
	XFree(ch.res_class);
	XFree(ch.res_name);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatoms[NetWMName], c->title, sizeof(c->title)))
		gettextprop(c->win, XA_WM_NAME, c->title, sizeof(c->title));
	if (c->title[0] == '\0') /* hack to mark broken clients */
		strscpy(c->title, broken, sizeof(c->title));
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c->win, netatoms[NetWMState]);
	Atom wtype = getatomprop(c->win, netatoms[NetWMWindowType]);

	if (wtype == netatoms[NetWMWindowTypeDialog])
		c->isfloating = 1;
	if (state == netatoms[NetWMFullscreen])
		setfullscreen(c, 1);
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;

	if (c == selmon->sel && wmh->flags & XUrgencyHint) {
		wmh->flags &= ~XUrgencyHint;
		XSetWMHints(dpy, c->win, wmh);
	}
	else {
		c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (c->isurgent)
			drawborder(c->win, SchemeUrg);
	}

	if (wmh->flags & InputHint)
		c->neverfocus = !wmh->input;
	else
		c->neverfocus = 0;

	XFree(wmh);
}

void
appendtoclientlist(Window w)
{
	XChangeProperty(dpy, root, netatoms[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char *)&w, 1);
}

void
updateclientlist(void)
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatoms[NetClientList]);

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next) {
			appendtoclientlist(c->win);
			if (c->swallow)
				appendtoclientlist(c->origwin);
		}
}

void
updateclientdesktop(Client *c)
{
	unsigned int desktop = gettagnum(c->tags) + (c->mon->num * LENGTH(tags));

	if (desktop != c->desktop) {
		c->desktop = desktop;
		setcardprop(c->win, netatoms[NetWMDesktop], desktop);
	}
}

void
updatecurrentdesktop(void)
{
	Monitor *m;
	unsigned int desktop;

	setcardprop(root, atoms[DWMMonSel], selmon->num);

	desktop = gettagnum(selmon->tagset[selmon->seltags]) + (selmon->num * LENGTH(tags));
	if (desktop == currentdesktop)
		return;

	currentdesktop = desktop;
	setcardprop(root, netatoms[NetCurrentDesktop], desktop);

	XDeleteProperty(dpy, root, atoms[DWMMonDesktop]);

	for (m = mons; m; m = m->next) {
		desktop = gettagnum(m->tagset[m->seltags]);
		XChangeProperty(dpy, root, atoms[DWMMonDesktop], XA_CARDINAL, 32,
			PropModeAppend, (unsigned char *)&desktop, 1);
	}
}

void
updatedesktops(void)
{
	int n, i;
	Monitor *m;

	for (n = 0, m = mons; m; m = m->next, n++);

	/* set number of desktops */
	setcardprop(root, netatoms[NetNumberOfDesktops], n * LENGTH(tags));

	/* set desktop names */
	const char *names[n * LENGTH(tags)];
	for (i = 0; i < LENGTH(names); i++)
		names[i] = tags[i % LENGTH(tags)];
	XTextProperty prop;
	Xutf8TextListToTextProperty(dpy, (char **)names, LENGTH(names),
		XUTF8StringStyle, &prop);
	XSetTextProperty(dpy, root, &prop, netatoms[NetDesktopNames]);
}

void
updatewinbutton(Client *c)
{
	int width = 20;
	int height = 15;
	int border = 2;
	XWindowChanges wc;
	XClassHint ch = { "winbutton", "dwm" };
	XSetWindowAttributes swa = {
		.override_redirect = 1,
		.background_pixel = schemes[SchemeWinButton][ColorFG].pixel,
		.border_pixel = schemes[SchemeWinButton][ColorBorder].pixel,
	};

	if (!c->buttonwin) {
		c->buttonwin = XCreateWindow(dpy, root, WIDTH(c) * -2, c->y, width, height,
			border, depth, CopyFromParent, visual,
			CWOverrideRedirect|CWBackPixel|CWBorderPixel, &swa);
		XSetClassHint(dpy, c->buttonwin, &ch);
		XDefineCursor(dpy, c->buttonwin, cursors[CurNormal]);
		XSelectInput(dpy, c->buttonwin, ButtonPressMask|EnterWindowMask|LeaveWindowMask);
		XMapWindow(dpy, c->buttonwin);
		setcardprop(c->buttonwin, netatoms[NetWMWindowOpacity], 0);
	}

	if (ISVISIBLE(c)) {
		wc.stack_mode = Above;
		wc.sibling = c->win;
		XConfigureWindow(dpy, c->buttonwin, CWSibling|CWStackMode, &wc);
		XMoveWindow(dpy, c->buttonwin, c->x + WIDTH(c) - width, c->y - (height / 2));
	} else
		XMoveWindow(dpy, c->buttonwin, WIDTH(c) * -2, c->y - 10);
}

void
updatebarwin(Monitor *m)
{
	XClassHint ch = { "bar", "dwm" };
	XSetWindowAttributes swa = {
		.override_redirect = 1,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};

	if (!m) {
		for (m = mons; m; m = m->next)
			updatebarwin(m);
		return;
	}

	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= barheight;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + barheight : m->wy;
	} else {
		m->by = -barheight;
	}

	if (m->barwin) {
		XMoveResizeWindow(dpy, selmon->barwin,
			selmon->wx, selmon->by, selmon->ww, barheight);
	} else {
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, barheight, 0,
			depth, CopyFromParent, visual,
			CWOverrideRedirect|CWBackPixmap|CWEventMask, &swa);
		XSetClassHint(dpy, m->barwin, &ch);
		XDefineCursor(dpy, m->barwin, cursors[CurNormal]);
		setatomprop(m->barwin, netatoms[NetWMWindowType], netatoms[NetWMWindowTypeDock]);
		XMapRaised(dpy, m->barwin);
	}

	if (m == STATUSMON)
		systrayupdate(); /* systrayupdate() calls drawbar() as well */
	else
		drawbar(m);
}

void
updatestatustext(void)
{
	if (!gettextprop(root, XA_WM_NAME, statustext, sizeof(statustext)))
		strscpy(statustext, "dwm-"VERSION, sizeof(statustext));
}

void
drawbar(Monitor *m)
{
	const int boxs = fontheight / 9;
	const int boxw = fontheight / 6 + 2;
	int i, j, w, x = 0, status_x, tmpx, issel, scheme;
	int cindpx = fontheight * cindfact;
	unsigned int occ = 0; /* occupied tags */
	unsigned int urg = 0; /* tags containing urgent clients */
	char biditext[StatusSize];
	Client *c;

	if (!m) {
		for (m = mons; m; m = m->next)
			drawbar(m);
		return;
	}

	if (!m->showbar || barunchanged(m))
		return;

	/* get occupied and urgent tags */
	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}

	/* reset button positions to 0 */
	m->bp = (const ButtonPos){0};

	/* draw status first so it can be overdrawn by tags later */
	status_x = (m == STATUSMON) ? drawstatus(m) : m->bdw;

	/* draw tags */
	for (i = 0; i < LENGTH(tags); i++)
	{
		issel = m->tagset[m->seltags] & (1 << i);

		/* skip vacant tags */
		if (!issel && !(occ & (1 << i)))
			continue;

		/* draw tag names */
		tmpx = x;
		scheme = issel ? SchemeSel : SchemeNorm;
		x = m->bp.tags[i] = RENDERTEXTWP(scheme, tags[i], x, m->bdw, urg & (1 << i));

		/* draw client indicators */
		for (j = 0, c = m->clients; c; c = c->next)
		{
			if (c->tags & (1 << i))
			{
				int ismonocle = (c == m->sel && m->lt[m->sellt]->arrange == monocle);
				int gap = MAX(cindpx / 2, 1);
				renderrect(scheme, tmpx + gap, (j * cindpx * 2) + gap + 1,
					cindpx * (ismonocle ? 2.5 : 1), cindpx, 1, urg & (1 << i));
				j++;
			}
		}
	}
	m->bp.tagsend = x;

	/* draw layout symbol */
	x = m->bp.ltsymbol = RENDERTEXTWP(SchemeNorm, m->ltsymbol, x, m->bdw, 0);

	/* draw window title if it fits */
	if ((w = status_x - x) > barheight)
	{
		if (m->sel)
		{
			scheme = (m == selmon) ? SchemeTitle : SchemeNorm;
			fribidi(biditext, m->sel->title, sizeof(m->sel->title));
			RENDERTEXTWP(scheme, biditext, x, w, 0);

			/* draw a floating indicator */
			if (m->sel->isfloating && !m->sel->isfullscreen)
				renderrect(scheme, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		}
		else
			renderrect(SchemeNorm, x, 0, w, barheight, 1, 1);
	}

	rendermap(m, status_x, 0);
}

int
barunchanged(Monitor *m)
{
	unsigned int n, selpos = 0, occ = 0, urg = 0;
	Client *c;

	for (n = 0, c = m->clients; c; c = c->next, n++) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
		if (c == m->sel)
			selpos = n;
	}

	if (m->bs.nclients == n
		&& m->bs.tags == m->tagset[m->seltags]
		&& m->bs.occtags == occ
		&& m->bs.urgtags == urg
		&& m->bs.bdw == m->bdw
		&& !!m->bs.isselmon == !!(m == selmon)
		&& !!m->bs.isstatusmon == !!(m == STATUSMON)
		&& !!m->bs.title[0] == !!(m->sel)
		&& (!m->sel || m->bs.isfloating == m->sel->isfloating)
		&& (m->lt[m->sellt]->arrange != monocle || m->bs.selpos == selpos)
		&& strncmp(m->bs.ltsymbol, m->ltsymbol, sizeof(m->ltsymbol)) == 0
		&& (!m->sel || strncmp(m->bs.title, m->sel->title, sizeof(m->sel->title)) == 0)
		&& (m != STATUSMON || strncmp(m->bs.statustext, statustext, sizeof(statustext)) == 0))
	{
		return 1;
	}

	m->bs.nclients = n;
	m->bs.tags = m->tagset[m->seltags];
	m->bs.occtags = occ;
	m->bs.urgtags = urg;
	m->bs.selpos = selpos;
	m->bs.bdw = m->bdw;
	m->bs.isselmon = (m == selmon);
	m->bs.isstatusmon = (m == STATUSMON);
	m->bs.isfloating = (m->sel && m->sel->isfloating);
	strscpy(m->bs.ltsymbol, m->ltsymbol, sizeof(m->bs.ltsymbol));
	strscpy(m->bs.statustext, statustext, sizeof(m->bs.statustext));
	strscpy(m->bs.title, m->sel ? m->sel->title : "\0", sizeof(m->bs.title));

	return 0;
}

void
buttonpress(XEvent *e)
{
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;
	unsigned int i = 0, click = ClickInvalid, clickalt = ClickInvalid;
	static ClickEv lastclick = { .isvalid = 0 };

	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon)
		focusmon(m);

	if (ev->window == root)
		click = ClickRootWin;

	else if (ev->window == m->barwin)
	{
		/* tag buttons */
		if (ev->x < m->bp.tagsend) {
			for (i = 0; i < LENGTH(tags); i++)
				if (m->bp.tags[i] && ev->x < m->bp.tags[i]) {
					click = ClickTagBar;
					arg.ui = 1 << i;
					break;
				}
		}

		/* layout symbol */
		else if (ev->x < m->bp.ltsymbol)
			click = ClickLtSymbol;

		/* status text */
		else if (ev->x > m->bp.statusstart) {
			click = ClickStatusText;
			handlestatusclick(m, ev);
			}

		/* window title */
		else
			click = ClickWinTitle;
	}

	else if ((c = wintoclient(ev->window)))
	{
		click = ClickClientWin;

		/* if any modkeys are pressed down, do not focus the window under the cursor.
		 * this enables eg. super+scroll to be used to change focus between windows. */
		if (!(ev->state & (Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))) {
			focus(c);
			restack(selmon);
		}

		XAllowEvents(dpy, ReplayPointer, CurrentTime);
	}

	else if ((c = winbuttontoclient(ev->window)))
	{
		click = ClickWinButton;

		if (lastclick.isvalid
		 && ev->button == lastclick.button
		 && ev->window == lastclick.win
		 && ev->time - lastclick.time < 300)
		{
			click = ClickWinButtonDouble;
			if (c != selmon->sel)
				focus(c);
		}

		lastclick.isvalid = 1;
		lastclick.button = ev->button;
		lastclick.win = ev->window;
		lastclick.time = ev->time;
	}

	if (click == ClickClientWin || click == ClickRootWin)
		clickalt = ClickWinArea;

	for (i = 0; i < LENGTH(buttons); i++)
		if ((buttons[i].click == click || buttons[i].click == clickalt)
			&& buttons[i].func
			&& buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mod) == CLEANMASK(ev->state))
		{
			lastclick.isvalid = 0;
			buttons[i].func(click == ClickTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
		}
}

void
handlestatusclick(Monitor *m, XButtonPressedEvent *ev)
{
	for (int i = 0; i < LENGTH(statusclick); i++)
		if (m->bp.modules[i].exists
		&& ev->x > m->bp.modules[i].start
		&& ev->x < m->bp.modules[i].end
		&& statusclick[i].func
		&& statusclick[i].button == ev->button
		&& CLEANMASK(statusclick[i].mod) == CLEANMASK(ev->state))
		{
			statusclick[i].func(&statusclick[i].arg);
		}
}

int
drawstatus(Monitor *m)
{
	char c, modulename[64] = {'\0'}, normaltext[LENGTH(statustext)] = {'\0'};
	int x = 0, pos = 0, normalstartpos = 0, tagstartpos = 0, skiptag = 0;
	int i, islastchar, status_w, status_x, modulestart_x = 0;
	enum Mode { Normal, Tag, };
	enum Mode mode = Normal;

	for (i = 0; i < LENGTH(statusclick); i++)
		m->bp.modules[i].exists = 0;

	for (c = statustext[0]; c; c = statustext[++pos])
	{
		islastchar = !statustext[pos + 1];

		/* start of a tag */
		if (mode == Normal && c == '<' && !skiptag) {
			mode = Tag;
			tagstartpos = pos;
		}

		/* end of the started tag */
		else if (mode == Tag && c == '>')
		{
			mode = Normal;

			/* save any normaltext up to here */
			if (tagstartpos - normalstartpos > 0)
				strncat(normaltext, &statustext[normalstartpos], tagstartpos - normalstartpos);
			normalstartpos = pos + 1;

			/* record modules's name */
			strsscpy(modulename, &statustext[tagstartpos + 1],
				sizeof(modulename), pos - (tagstartpos + 1));
		}

		/* no end of tag found;
		 * break tag, go back and interpret the tag start char as a normal char */
		else if (mode == Tag && (islastchar || !isalnum(c) || isseparator(c))) {
			mode = Normal;
			pos = tagstartpos - 1;
			skiptag = 1;
			continue;
		}

		/* reached a separator char */
		else if (mode == Normal && isseparator(c))
		{
finaldraw:
			/* save any normaltext up to here */
			if (pos - normalstartpos > 0)
				strncat(normaltext, &statustext[normalstartpos], pos - normalstartpos);
			normalstartpos = pos + 1;

			/* draw any normaltext up to here */
			if (normaltext[0])
				x = rendertext(SchemeStatus, normaltext, x, 0, m->bdw, barheight, 0, 0);

			/* record module's start and end x coord */
			for (i = 0; i < LENGTH(statusclick); i++)
				if (*modulename && strcmp(statusclick[i].module, modulename) == 0) {
					m->bp.modules[i].exists = 1;
					m->bp.modules[i].start = modulestart_x;
					m->bp.modules[i].end = x;
				}

			if (!c)
				goto finaldraw_end;

			/* draw the seaprator char */
			normaltext[0] = c;
			normaltext[1] = '\0';
			x = rendertext(SchemeStatusSep, normaltext, x, 0, m->bdw, barheight, 0, 0);
			modulestart_x = x;

			/* reset the normaltext buffer */
			normaltext[0] = '\0';
		}

		skiptag = 0;
	}

	goto finaldraw;
finaldraw_end:

	status_w = MIN(x + (fontheight / 5), m->bdw / 1.5);
	status_x = m->bp.statusstart = m->bdw - status_w;
	renderrect(SchemeNorm, status_x + status_w, 0, m->bdw, barheight, 1, 0);
	rendermap(m, status_w, status_x);

	for (i = 0; i < LENGTH(statusclick); i++)
		if (m->bp.modules[i].exists) {
			m->bp.modules[i].start += status_x;
			m->bp.modules[i].end += status_x;
		}

	return status_x;
}

int
isseparator(char c)
{
	int i = 0;
	for (; i < LENGTH(statusseparators); i++)
		if (c == statusseparators[i])
			return 1;
	return 0;
}

void
drawborder(Window win, int scm)
{
	int w, h;   /* window width and height */
	int pw, ph; /* pixmap width and height */
	int innerpx, inneroffset, innersum;
	XWindowAttributes wa;
	Pixmap borderpixmap;
	GC bordergc;

	if (!XGetWindowAttributes(dpy, win, &wa) || !wa.border_width)
		return;

	w = wa.width;
	h = wa.height;
	pw = w + (wa.border_width * 2);
	ph = h + (wa.border_width * 2);
	innerpx = innerborderpx;
	inneroffset = innerborderoffsetpx;
	innersum = innerpx + inneroffset;

	/* the border pixmap's origin is the same as the window's origin (not the
	 * border's origin), but pixmaps tile in all directions. so to draw rectangles
	 * in the window border that are above or to the left of the window, we need
	 * to draw them with respect to where they appear when the pixmap is tiled
	 * above and to the left of the window. for example, if we want a rectangle
	 * to appear on the left side of the window, we need to draw it on the right
	 * edge of the pixmap. */
	XRectangle rectangles[] = {
		{ 0,              h+inneroffset,  w+inneroffset, innerpx     }, /* 1 - below the window */
		{ 0,              ph-innersum,    w+innersum,    innerpx     }, /* 2 - above the window */
		{ w+inneroffset,  0,              innerpx,       h+innersum  }, /* 3 - right of the window */
		{ pw-innersum,    0,              innerpx,       h+innersum  }, /* 4 - left of the window */
		{ pw-innersum,    ph-innersum,    innersum,      innerpx     }, /* 5 - up and to the left of the window, left of 1 and above 7 */
		{ w+inneroffset,  ph-inneroffset, innerpx,       inneroffset }, /* 6 - up and to the right of the window, above 3 and below 2 */
		{ pw-innersum,    ph-inneroffset, innerpx,       inneroffset }, /* 7 - up and to the left of the window, above 4 and below 5 */
		{ pw-inneroffset, h+inneroffset,  inneroffset,   innerpx     }, /* 8 - down and to the left of the window, left of 1 and right of 4 */
	};

	borderpixmap = XCreatePixmap(dpy, win, pw, ph, wa.depth);
	bordergc = XCreateGC(dpy, borderpixmap, 0, NULL);

	/* fill the area with the border background color */
	XSetForeground(dpy, bordergc, schemes[scm][ColorBorderBG].pixel);
	XFillRectangle(dpy, borderpixmap, bordergc, 0, 0, pw, ph);

	/* draw the inner border on top of the previous fill */
	XSetForeground(dpy, bordergc, schemes[scm][ColorBorder].pixel);
	XFillRectangles(dpy, borderpixmap, bordergc, rectangles, LENGTH(rectangles));

	XSetWindowBorderPixmap(dpy, win, borderpixmap);
	XFreePixmap(dpy, borderpixmap);
	XFreeGC(dpy, bordergc);
}

void
updateborder(Client *c)
{
	int scm;

	if (c->isfullscreen || !ISVISIBLE(c))
		return;

	if (c == selmon->sel)
		scm = SchemeSel;
	else if (c->isurgent)
		scm = SchemeUrg;
	else
		scm = SchemeNorm;

	drawborder(c->win, scm);
}

void
systrayupdate(void)
{
	XWindowAttributes wa;
	int x = 0, systraywidth;
	int lastpad = fontheight / 5;
	Client *c;

	if (!systrayinit())
		return;

	for (c = systray->icons; c; c = c->next) {
		if (!c->ismapped)
			continue;
		if (!XGetWindowAttributes(dpy, c->win, &wa) || !wa.width || !wa.height) {
			wa.width = fontheight;
			wa.height = fontheight;
		}
		c->x = x + lastpad;
		c->y = barheight - fontheight / 2; /* TODO: abs() ? */
		c->h = fontheight;
		c->w = c->h * ((float)wa.width / wa.height);
		XMoveResizeWindow(dpy, c->win, GEOM(c));
		XSetWindowBackground(dpy, c->win, schemes[SchemeNorm][ColorBG].pixel);
		sendconfigurenotify(c);
		x += c->w + (fontheight / 5);
	}

	systraywidth = MIN(MAX(x, 1), STATUSMON->ww / 3);
	STATUSMON->bdw = STATUSMON->ww - systraywidth;
	STATUSMON->bdw -= x ? lastpad : 0;

	XWindowChanges wc = {
		.x = STATUSMON->wx + STATUSMON->bdw,
		.y = STATUSMON->by,
		.width = systraywidth,
		.height = barheight,
		.stack_mode = Above,
		.sibling = STATUSMON->barwin,
	};
	XConfigureWindow(dpy, systray->win,
		CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);

	drawbar(STATUSMON);
}

void
systrayaddicon(Window w)
{
	unsigned int version, flags;
	Client *c;

	if (!systrayinit())
		return;

	c = ecalloc(1, sizeof(Client));
	c->next = systray->icons;
	systray->icons = c;

	c->win = w;
	c->mon = STATUSMON;

	XAddToSaveSet(dpy, c->win);
	XSelectInput(dpy, c->win, StructureNotifyMask|PropertyChangeMask|ResizeRedirectMask);
	XReparentWindow(dpy, c->win, systray->win, 0, 0);

	if (getxembedinfoprop(c->win, &version, &flags) && (flags & XEMBED_MAPPED)) {
		c->ismapped = 1;
		XMapRaised(dpy, c->win);
		setclientstate(c, NormalState);
		sendxembedevent(c->win, XEMBED_EMBEDDED_NOTIFY, 0,
			systray->win, MIN(version, XEMBED_VERSION));
	} else {
		setclientstate(c, WithdrawnState);
	}

	systrayupdate();
}

void
systrayupdateicon(Client *c, XResizeRequestEvent *resize)
{
	unsigned int version, flags;

	if (resize) {
		XMoveResizeWindow(dpy, c->win, c->x, c->y, resize->width, resize->height);
		/* XSync(dpy, 0); */
		systrayupdate();
		return;
	}

	if (!getxembedinfoprop(c->win, &version, &flags))
		return;

	if (flags & XEMBED_MAPPED) {
		c->ismapped = 1;
		XMapRaised(dpy, c->win);
		setclientstate(c, NormalState);
	}
	else {
		c->ismapped = 0;
		XUnmapWindow(dpy, c->win);
		setclientstate(c, WithdrawnState);
	}

	systrayupdate();
}

void
systrayremoveicon(Client *c)
{
	Client **tc;

	for (tc = &systray->icons; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;

	free(c);
	systrayupdate();
}

int
systrayinit(void)
{
	XClassHint ch = { "systray", "dwm" };
	unsigned int orientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
	XSetWindowAttributes swa = {
		.override_redirect = 1,
		.background_pixel = schemes[SchemeNorm][ColorBG].pixel,
		.event_mask = ButtonPressMask|ExposureMask,
	};

	if (!showsystray)
		return 0;

	if (systray)
		return 1;

	systray = ecalloc(1, sizeof(Systray));
	systray->win = createsimplewin();
	XSelectInput(dpy, systray->win, SubstructureNotifyMask);
	XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &swa);
	XSetClassHint(dpy, systray->win, &ch);
	XDefineCursor(dpy, systray->win, cursors[CurNormal]);
	setatomprop(systray->win, netatoms[NetWMWindowType], netatoms[NetWMWindowTypeDock]);
	setcardprop(systray->win, netatoms[NetSystemTrayOrientation], orientation);
	XMapRaised(dpy, systray->win);

	XSetSelectionOwner(dpy, netatoms[NetSystemTray], systray->win, CurrentTime);
	if (XGetSelectionOwner(dpy, netatoms[NetSystemTray]) == systray->win) {
		sendeventraw(root, atoms[Manager], StructureNotifyMask, CurrentTime,
			netatoms[NetSystemTray], systray->win, 0, 0);
	} else {
		fprintf(stderr, "unable to obtain system tray.\n");
		XDestroyWindow(dpy, systray->win);
		free(systray);
		systray = NULL;
		return 0;
	}

	return 1;
}

void
systraycleanup(void)
{
	Client *c, *f;

	if (!systray)
		return;

	XUnmapWindow(dpy, systray->win);
	XDestroyWindow(dpy, systray->win);

	for (c = systray->icons; c;) {
		f = c;
		c = c->next;
		free(f);
	}

	free(systray);
}

unsigned int
getwinpid(Window w)
{
	int pid = 0;

#ifdef __OpenBSD__
	getcardprop(w, netatoms[NetWMPID], (long *)&pid);
#endif /* __OpenBSD__ */

#ifdef __linux__
	xcb_res_query_client_ids_cookie_t cookie;
	xcb_res_query_client_ids_reply_t *reply;
	xcb_res_client_id_value_iterator_t iter;
	xcb_generic_error_t *error = NULL;
	xcb_res_client_id_spec_t spec = {0};

	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;
	cookie = xcb_res_query_client_ids(xcon, 1, &spec);
	reply = xcb_res_query_client_ids_reply(xcon, cookie, &error);

	if (!reply)
		return 0;

	iter = xcb_res_query_client_ids_ids_iterator(reply);
	for (; iter.rem; xcb_res_client_id_value_next(&iter)) {
		spec = iter.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			pid = *(int *)xcb_res_client_id_value_value(iter.data);
			break;
		}
	}

	free(reply);
#endif /* __linux__ */

	return pid < 0 ? 0 : (unsigned int)pid;
}

Client *
getparentterminal(Client *c)
{
	Client *t;
	Monitor *m;

	if (!c->pid || c->isterminal)
		return NULL;

	if ((t = selmon->sel) && t->isterminal &&
		!t->swallow && t->pid && isdescprocess(t->pid, c->pid))
	{
		return t;
	}

	for (m = mons; m; m = m->next)
		for (t = m->clients; t; t = t->next)
			if (t != selmon->sel && t->isterminal &&
			    !t->swallow && t->pid && isdescprocess(t->pid, c->pid))
				return t;

	return NULL;
}

int
isdescprocess(unsigned int parent, unsigned int child)
{
	while (child != parent && child != 0)
		child = getparentpid(child);

	return !!child;
}

unsigned int
getparentpid(unsigned int pid)
{
	int ppid = 0;

#ifdef __linux__
	char path[32];
	FILE *stat;

	snprintf(path, sizeof(path), "/proc/%u/stat", pid);
	if (!(stat = fopen(path, "r")))
		return 0;
	fscanf(stat, "%*u (%*[^)]) %*c %u", (unsigned int *)&ppid);
	fclose(stat);
#endif /* __linux__ */

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	if (!(kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL))
		return 0;
	kp = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof(*kp), &n);
	ppid = kp->p_ppid;
#endif /* __OpenBSD__ */

	return ppid < 0 ? 0 : (unsigned int)ppid;
}

void
pertagload(Monitor *m, unsigned int tags, unsigned int newtags)
{
	if (!pertag)
		return;

	m->pertagstack[m->pertagtop] = newtags;

	if (newtags == tags)
		return;

	unsigned int tagnum = gettagnum(tags);
	unsigned int newtagnum = gettagnum(newtags);

	m->pertag[tagnum].mfact = m->mfact;
	m->pertag[tagnum].nmaster = m->nmaster;
	m->pertag[tagnum].sellt = m->sellt;
	m->pertag[tagnum].lt[0] = m->lt[0];
	m->pertag[tagnum].lt[1] = m->lt[1];

	m->mfact = m->pertag[newtagnum].mfact;
	m->nmaster = m->pertag[newtagnum].nmaster;
	m->sellt = m->pertag[newtagnum].sellt;
	m->lt[0] = m->pertag[newtagnum].lt[0];
	m->lt[1] = m->pertag[newtagnum].lt[1];
}

void
pertagpush(Monitor *m, unsigned int newtags)
{
	if (!pertag)
		return;

	if (m->pertagtop < LENGTH(m->pertagstack) - 1)
		pertagload(m, m->pertagstack[m->pertagtop++], newtags);
}

void
pertagpop(Monitor *m)
{
	if (!pertag)
		return;

	if (m->pertagtop >= 1) {
		m->pertagtop--;
		pertagload(m, m->pertagstack[m->pertagtop + 1], m->pertagstack[m->pertagtop]);
	}
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	if (selmon->sel && selmon->sel->win == w)
		return selmon->sel;

	if (ISUTILWIN(w))
		return NULL;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;

	return NULL;
}

Client *
origwintoclient(Window w)
{
	Client *c;
	Monitor *m;

	if ((c = selmon->sel) && c->swallow && c->origwin == w)
		return c;

	if (ISUTILWIN(w))
		return NULL;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->swallow && c->origwin == w)
				return c;

	return NULL;
}

Client *
winbuttontoclient(Window w)
{
	Client *c;
	Monitor *m;

	if (selmon->sel && selmon->sel->buttonwin == w)
		return selmon->sel;

	if (ISUTILWIN(w))
		return NULL;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->buttonwin == w)
				return c;

	return NULL;
}

Client *
wintosystrayicon(Window w)
{
	Client *c;

	if (!systray || ISUTILWIN(w))
		return NULL;

	for (c = systray->icons; c && c->win != w; c = c->next);
	return c;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);

	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;

	if ((c = wintoclient(w)) || (c = winbuttontoclient(w)))
		return c->mon;

	return selmon;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}

	return r;
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	}
	else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);

	return m;
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;

	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	}
	else {
		for (i = selmon->clients; i != selmon->sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}

	if (c) {
		focus(c);
		restack(selmon);
	}
}

void
focusstacktile(const Arg *arg)
{
	Client *c = NULL;

	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;

	if (arg->i > 0)
		c = nexttiledloop(selmon->sel);
	else
		c = prevtiledloop(selmon->sel);

	if (c) {
		focus(c);
		restack(selmon);
	}
}

void
cycleview(const Arg *arg)
{
	int tagnum = gettagnum(selmon->tagset[selmon->seltags]);
	
	switch(arg->i) {
		case +1: tagnum = MIN(tagnum + 1, LENGTH(tags) - 1); break; /* next tag */
		case -1: tagnum = MAX(tagnum - 1, 0); break; /* previous tag */
		case +2: tagnum = tagnum >= LENGTH(tags) - 1 ? 0 : tagnum + 1; break; /* next tag, wrap around */
		case -2: tagnum = tagnum <= 0 ? LENGTH(tags) - 1 : tagnum - 1; break; /* previous tag, wrap around */
		default: return;
	}

	tagnum = 1 << tagnum & TAGMASK;
	pertagload(selmon, selmon->tagset[selmon->seltags], tagnum);
	selmon->tagset[selmon->seltags] = tagnum;

	focus(NULL);
	arrange(selmon);
}

void
view(const Arg *arg)
{
	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;

	pertagload(selmon, selmon->tagset[selmon->seltags], arg->ui & TAGMASK);

	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;

	focus(NULL);
	arrange(selmon);
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		pertagload(selmon, selmon->tagset[selmon->seltags], newtagset);
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		tagreduced(selmon->sel, 0, arg->ui & TAGMASK);
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		tagreduced(selmon->sel, 0, newtags);
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
viewmon(const Arg *arg)
{
	Monitor *m;

	if (mons->next && (m = dirtomon(arg->i)) != selmon)
		focusmon(m);
}

void
tagmon(const Arg *arg)
{
	if (selmon->sel && mons->next)
		setclientmon(selmon->sel, dirtomon(arg->i));
}

void
setclientmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = m->tagset[m->seltags];
	attachdirection(c);
	attachstackbottom(c);
	focus(NULL);
	arrange(NULL);
}

void
push(const Arg *arg)
{
	Client *sel = selmon->sel, *c;

	if (!sel || afloat(sel))
		return;

	if (arg->i > 0)
	{
		if ((c = nexttiled(sel->next))) {
			detach(sel);
			sel->next = c->next;
			c->next = sel;
		}
		else {
			detach(sel);
			attach(sel);
		}
	}

	else if (arg->i < 0)
	{
		if ((c = prevtiled(sel))) {
			detach(sel);
			sel->next = c;
			if (selmon->clients == c)
				selmon->clients = sel;
			else {
				for (c = selmon->clients; c->next != sel->next; c = c->next);
				c->next = sel;
			}
		}
		else {
			for (c = sel; c->next; c = c->next);
			detach(sel);
			sel->next = NULL;
			c->next = sel;
		}
	}

	focus(sel);
	arrange(selmon);
}

void
switchcol(const Arg *arg)
{
	Client *c, *t;
	int col = 0;
	int i;

	if (!selmon->sel)
		return;

	for (i = 0, c = nexttiled(selmon->clients); c;
	     c = nexttiled(c->next), i++)
	{
		if (c == selmon->sel)
			col = (i + 1) > selmon->nmaster;
	}

	if (i <= selmon->nmaster)
		return;

	for (c = selmon->stack; c; c = c->snext) {
		if (!ISVISIBLE(c))
			continue;
		for (i = 0, t = nexttiled(selmon->clients); t && t != c;
		     t = nexttiled(t->next), i++);
		if (t && (i + 1 > selmon->nmaster) != col) {
			focus(c);
			restack(selmon);
			break;
		}
	}
}

void
togglefloating(const Arg *arg)
{
	if (selmon->sel && !selmon->sel->isfullscreen) {
		selmon->sel->isfloating ^= 1;
		arrange(selmon);
	}
}

void
togglefullscreen(const Arg *arg)
{
	if (selmon->sel)
		setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void
setlayout(const Arg *arg)
{
	const Layout *lt = NULL;
	int i;

	if (arg && arg->lt)
		for (i = 0; i < LENGTH(layouts); i++)
			if (layouts[i].arrange == arg->lt) {
				lt = &layouts[i];
				break;
			}

	if (!lt) {
		for (i = 0; (i < LENGTH(layouts)) && (&layouts[i] != selmon->lt[selmon->sellt]); i++);
		switch(arg->i) {
			case +1: lt = &layouts[MIN(i + 1, LENGTH(layouts) - 1)]; break; /* next layout */
			case -1: lt = &layouts[MAX(i - 1, 0)]; break; /* previous layout */
			case +2: lt = &layouts[i >= LENGTH(layouts) - 1 ? 0 : i + 1]; break; /* next layout, wrap around */
			case -2: lt = &layouts[i <= 0 ? LENGTH(layouts) - 1 : i - 1]; break; /* previous layout, wrap around */
		}
	}
	
	if (!lt || lt != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (lt)
		selmon->lt[selmon->sellt] = lt;

	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel || sendevent(selmon->sel, atoms[WMDelete]))
		return;

	/* XGrabServer(dpy); */
	/* XSetErrorHandler(xerrordummy); */
	XSetCloseDownMode(dpy, DestroyAll);
	XKillClient(dpy, selmon->sel->win);
	/* XSync(dpy, 0); */
	/* XSetErrorHandler(xerror); */
	/* XUngrabServer(dpy); */
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarwin(selmon);
	arrange(selmon);
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!c || afloat(c) || ismasterontag(c))
		return;

	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
transfer(const Arg *arg)
{
	Client *c, *mtail = selmon->clients, *stail = NULL, *insertafter;
	int transfertostack = 0, nmasterclients = 0, i;

	for (i = 0, c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || c->isfloating)
			continue;
		if (selmon->sel == c)
			transfertostack = i < selmon->nmaster && selmon->nmaster != 0;
		if (i < selmon->nmaster) {
			nmasterclients++;
			mtail = c;
		}
		stail = c;
		i++;
	}

	if (!selmon->sel || selmon->sel->isfloating || i == 0) {
		return;
	}
	else if (transfertostack) {
		selmon->nmaster = MIN(i, selmon->nmaster) - 1;
		insertafter = stail;
	}
	else {
		selmon->nmaster++;
		insertafter = mtail;
	}

	if (insertafter != selmon->sel) {
		detach(selmon->sel);
		if (selmon->nmaster == 1 && !transfertostack) {
			attach(selmon->sel); /* head prepend case */
		}
		else {
			selmon->sel->next = insertafter->next;
			insertafter->next = selmon->sel;
		}
	}

	arrange(selmon);
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		int fd = open("/dev/null", O_WRONLY);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("execvp '%s' failed:", ((char **)arg->v)[0]);
	}
}

void
quit(const Arg *arg)
{
	running = 0;
}

void
restart(const Arg *arg)
{
	mustrestart = 1;
	running = 0;
}

void
sigrestart(int unused)
{
	restart(NULL);
	/* generate a destroynotify so that the event loop realizes it has to quit */
	XDestroyWindow(dpy, createsimplewin());
	XFlush(dpy);
}

#ifdef BACKTRACE
void
sigbacktrace(int sig)
{
	void *bt[BackTraceSize];
	int size;

	size = backtrace(bt, BackTraceSize);
	fprintf(stderr, "dwm: %s:\n", strsignal(sig));
	backtrace_symbols_fd(bt, size, STDERR_FILENO);
	exit(EXIT_FAILURE);
}
#endif /* BACKTRACE */

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel) || c->isfullscreen)
		return;

	restack(selmon);
	ocx = c->x;
	ocy = c->y;

	if (XGrabPointer(dpy, root, 0, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursors[CurMove], CurrentTime) != GrabSuccess
		|| !getrootptr(&x, &y))
	{
		return;
	}

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);

		switch(ev.type) {
			case ConfigureRequest:
			case MapRequest:
			case Expose:
				handler[ev.type](&ev);
				continue;
			case MotionNotify:
				break;
			default:
				continue;
		}

		/* don't run more than 60 times per second */
		if ((ev.xmotion.time - lasttime) <= (1000 / 60))
			continue;
		lasttime = ev.xmotion.time;

		if (c != selmon->sel)
			focus(c);

		nx = ocx + (ev.xmotion.x - x);
		ny = ocy + (ev.xmotion.y - y);

		/* apply x snap */
		if (abs(selmon->wx - nx) < snap)
			nx = selmon->wx;
		else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
			nx = selmon->wx + selmon->ww - WIDTH(c);

		/* apply y snap */
		if (abs(selmon->wy - ny) < snap)
			ny = selmon->wy;
		else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
			ny = selmon->wy + selmon->wh - HEIGHT(c);

		if (afloat(c))
			resize(c, nx, ny, c->w, c->h, 1);
		else if (abs(nx - c->x) > snap || abs(ny - c->y) > snap)
			togglefloating(NULL);

	} while (ev.type != ButtonRelease);

	XUngrabPointer(dpy, CurrentTime);

	if ((m = recttomon(GEOM(c))) != selmon) {
		setclientmon(c, m);
		focusmon(m);
	}
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel) || c->isfullscreen)
		return;

	restack(selmon);
	ocx = c->x;
	ocy = c->y;

	if (XGrabPointer(dpy, root, 0, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursors[CurResize], CurrentTime) != GrabSuccess)
	{
		return;
	}

	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);

		switch(ev.type) {
			case ConfigureRequest:
			case MapRequest:
			case Expose:
				handler[ev.type](&ev);
				continue;
			case MotionNotify:
				break;
			default:
				continue;
		}

		/* don't run more than 60 times per second */
		if ((ev.xmotion.time - lasttime) <= (1000 / 60))
			continue;
		lasttime = ev.xmotion.time;

		if (c != selmon->sel)
			focus(c);

		nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
		nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);

		if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
		{
			if (!afloat(c) && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
				togglefloating(NULL);
		}

		if (afloat(c))
			resize(c, c->x, c->y, nw, nh, 1);

	} while (ev.type != ButtonRelease);

	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);

	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));

	if ((m = recttomon(GEOM(c))) != selmon) {
		setclientmon(c, m);
		focusmon(m);
	}
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		c->oldfloating = c->isfloating;
		c->isfloating = 1;
		c->oldbw = c->bw;
		c->bw = 0;
		c->oldx = c->x;
		c->oldy = c->y;
		c->oldw = c->w;
		c->oldh = c->h;
		setfullscreenprop(c->win, 1);
		if (!ISVISIBLE(c))
			seturgent(c, 1);
		arrange(c->mon);
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		c->isfloating = c->oldfloating;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		c->geomvalid = 0;
		setfullscreenprop(c->win, 0);
		arrange(c->mon);
	}
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();

	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };

		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);

		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, 0,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);

		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClickClientWin
			 || buttons[i].click == ClickWinArea)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mod|modifiers[j],
						c->win, 0, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachabove(Client *c)
{
	if (c->mon->sel == NULL
	 || c->mon->sel == c->mon->clients
	 || c->mon->sel->isfloating)
	{
		attach(c);
		return;
	}

	Client *t;
	for (t = c->mon->clients; t->next != c->mon->sel; t = t->next);
	c->next = t->next;
	t->next = c;
}

void
attachbelow(Client *c)
{
	if (c->mon->sel == NULL
	 || c->mon->sel == c
	 || c->mon->sel->isfloating)
	{
		attach(c);
		return;
	}

	c->next = c->mon->sel->next;
	c->mon->sel->next = c;
}

void
attachtop(Client *c)
{
	Client **p;
	Client *n;

	for (p = &c->mon->clients; *p && ismasterontag(*p); p = &(*p)->next);
	n = nexttiled(*p);
	*p = c;
	c->next = n;
}

void
attachbottom(Client *c)
{
	Client **t;

	for (t = &c->mon->clients; *t; t = &(*t)->next);
	*t = c;
	c->next = NULL;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
attachstackbottom(Client *c)
{
	Client *below;
	for (below = c->mon->stack; below && below->snext; below = below->snext);
	c->snext = NULL;
	if (below)
		below->snext = c;
	else
		c->mon->stack = c;
}

void
detach(Client *c)
{
	Client **tc;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

void
grabkeys(void)
{
	updatenumlockmask();

	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);

		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod|modifiers[j], root,
						1, GrabModeAsync, GrabModeAsync);
	}
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);

	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
			{
				numlockmask = (1 << i);
			}

	XFreeModifiermap(modmap);
}

void
setcardprop(Window w, Atom prop, long value)
{
	XChangeProperty(dpy, w, prop, XA_CARDINAL, 32,
		PropModeReplace, (unsigned char *)&value, 1);
}

void
setatomprop(Window w, Atom prop, Atom value)
{
	XChangeProperty(dpy, w, prop, XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&value, 1);
}

void
setwinprop(Window w, Atom prop, Window value)
{
	XChangeProperty(dpy, w, prop, XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&value, 1);
}

void
setfullscreenprop(Window w, int fullscreen)
{
	if (fullscreen)
		setatomprop(w, netatoms[NetWMState], netatoms[NetWMFullscreen]);
	else
		XChangeProperty(dpy, w, netatoms[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char *)0, 0);
}

void
setwindowstate(Window w, long state)
{
	long data[] = { state, None };
	XChangeProperty(dpy, w, atoms[WMState], atoms[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
getcardprop(Window w, Atom prop, long *ret)
{
	Atom type;
	int format;
	unsigned long nitems, remaining;
	long *value = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, 1, 0, XA_CARDINAL,
		&type, &format, &nitems, &remaining, (unsigned char **)&value) != Success
		|| !value)
	{
		return 0;
	}

	*ret = *value;
	XFree(value);
	return 1;
}

Atom
getatomprop(Window w, Atom prop)
{
	int format;
	unsigned long nitems, remaining;
	long *value = NULL;
	Atom type, atom = None;

	if (XGetWindowProperty(dpy, w, prop, 0L, sizeof atom, 0, XA_ATOM,
		&type, &format, &nitems, &remaining, (unsigned char **)&value) == Success
		&& value)
	{
		atom = (Atom)*value;
		XFree(value);
	}

	return atom;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	XTextProperty prop;
	char **list = NULL;
	int n;

	text[0] = '\0';
	if (!size || !XGetTextProperty(dpy, w, &prop, atom) || !prop.nitems)
		return 0;

	if (prop.encoding == XA_STRING) {
		strscpy(text, (char *)prop.value, size);
	}
	else if (XmbTextPropertyToTextList(dpy, &prop, &list, &n) >= Success
		&& n > 0 && *list)
	{
		strscpy(text, *list, size);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(prop.value);
	return 1;
}

int
getxembedinfoprop(Window w, unsigned int *version, unsigned int *flags)
{
	Atom type;
	int format;
	unsigned long nitems, remaining;
	long *prop = NULL;
	int success = 0;

	if (XGetWindowProperty(dpy, w, atoms[XembedInfo], 0, 2, 0, atoms[XembedInfo],
		&type, &format, &nitems, &remaining, (unsigned char **)&prop) == Success
		&& prop
		&& nitems == 2)
	{
		*version = prop[0];
		*flags = prop[1];
		success = 1;
	}

	XFree(prop);
	return success;
}

long
getstate(Window w)
{
	int format;
	long *prop = NULL, result = -1;
	unsigned long nitems, remaining;
	Atom type;

	if (XGetWindowProperty(dpy, w, atoms[WMState], 0, 2, 0, atoms[WMState],
		&type, &format, &nitems, &remaining, (unsigned char **)&prop) == Success
		&& prop
		&& nitems)
	{
		result = prop[0];
	}

	XFree(prop);
	return result;
}

int
gettransientfor(Window w, Client **transfor)
{
	int istrans;
	Window transforwin;
	if ((istrans = XGetTransientForHint(dpy, w, &transforwin)) && transfor)
		*transfor = wintoclient(transforwin);
	return istrans;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

int
ismapped(Window w)
{
	XWindowAttributes wa;
	return XGetWindowAttributes(dpy, w, &wa) && wa.map_state == IsViewable;
}

void
setclientstate(Client *c, long state)
{
	setwindowstate(c->win, state);
}

int
sendevent(Client *c, Atom proto)
{
	XEvent ev;
	Atom *protocols;
	int nproto, exists = 0;

	if (XGetWMProtocols(dpy, c->win, &protocols, &nproto)) {
		while (!exists && nproto--)
			exists = (protocols[nproto] == proto);
		XFree(protocols);
	}

	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = atoms[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, 0, NoEventMask, &ev);
	}

	return exists;
}

void
sendeventraw(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	XEvent ev = {
		.type = ClientMessage,
		.xclient.window = w,
		.xclient.message_type = proto,
		.xclient.format = 32,
		.xclient.data.l[0] = d0,
		.xclient.data.l[1] = d1,
		.xclient.data.l[2] = d2,
		.xclient.data.l[3] = d3,
		.xclient.data.l[4] = d4,
	};
	XSendEvent(dpy, w, 0, mask, &ev);
}

void
sendxembedevent(Window w, long message, long detail, long data1, long data2)
{
	sendeventraw(w, netatoms[Xembed], NoEventMask,
		CurrentTime, message, detail, data1, data2);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags|XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;
	for (i = 0, my = ty = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2*c->bw) - m->gappx, h - (2*c->bw), 0);
			if (my + HEIGHT(c) + m->gappx < m->wh)
				my += HEIGHT(c) + m->gappx;
		} else {
			h = (m->wh - ty) / (n - i) - m->gappx;
			resize(c, m->wx + mw + m->gappx, m->wy + ty, m->ww - mw - (2*c->bw) - 2*m->gappx, h - (2*c->bw), 0);
			if (ty + HEIGHT(c) + m->gappx < m->wh)
				ty += HEIGHT(c) + m->gappx;
		}
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
stairs(Monitor *m)
{
	int i, n, h, mw, my;
	int ox, oy, ow, oh; /* offset values for stairs */
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;
	for (i = 0, my = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (i < m->nmaster) {
			h = (m->wh - my) / (MIN(n, m->nmaster) - i) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2 * c->bw) - m->gappx, h - (2 * c->bw), 0);
			if (my + HEIGHT(c) + m->gappx < m->wh)
				my += HEIGHT(c) + m->gappx;
		} else {
			oy = i - m->nmaster;
			ox = stairsdirection ? n - i - 1 : (stairssamesize ? i - m->nmaster : 0);
			ow = stairssamesize ? n - m->nmaster - 1 : n - i - 1;
			oh = stairssamesize ? ow : i - m->nmaster;
			resize(c,
				   m->wx + mw + (ox * stairpx) + m->gappx,
				   m->wy + (oy * stairpx) + m->gappx,
				   m->ww - mw - (2 * c->bw) - (ow * stairpx) - (2 * m->gappx),
				   m->wh - (2 * c->bw) - (oh * stairpx) - (2 * m->gappx),
				   0);
		}
	}
}

int
afloat(Client *c)
{
	int isfloating;

	if (c->isfloating)
		 return 1;

	pertagpush(c->mon, c->tags);
	isfloating = !c->mon->lt[c->mon->sellt]->arrange;
	pertagpop(c->mon);

	return isfloating;
}

unsigned int
gettagnum(unsigned int tags)
{
	unsigned int i;
	for (i = 0; tags && !(tags & 1); tags >>= 1, i++);
	return i;
}

int
numtiledontag(Client *c)
{
	int i = 0;
	c = nexttiledcore(c->mon->clients, c->tags);
	for (; c; c = nexttiledcore(c->next, c->tags), i++);
	return i;
}

int
ismasterontag(Client *c)
{
	Client *t;
	int i, ret;

	for (i = 0, t = nexttiledcore(c->mon->clients, c->tags);
	     t && t != c;
	     t = nexttiledcore(t->next, c->tags), i++);

	pertagpush(c->mon, c->tags);
	ret = i < c->mon->nmaster;
	pertagpop(c->mon);
	return ret;
}

Client *
nexttiledloop(Client *c)
{
	Client *t;
	if (!(t = nexttiled(c->next)))
		t = nexttiled(c->mon->clients);
	return t;
}

Client *
prevtiledloop(Client *c)
{
	Client *t;
	if (!(t = prevtiled(c)))
		t = lasttiled(c->mon);
	return t;
}

Client *
nexttiled(Client *c)
{
	return c ? nexttiledcore(c, c->mon->tagset[c->mon->seltags]) : c;
}

Client *
nexttiledontag(Client *c)
{
	return nexttiledcore(c, c->tags);
}

Client *
nexttiledcore(Client *c, unsigned int tags)
{
	for (; c && (c->isfloating || !ISVISIBLEONTAG(c, tags)); c = c->next);
	return c;
}

Client *
prevtiled(Client *c)
{
	Client *p = c->mon->clients, *r = NULL;
	for (; p && p != c; p = p->next)
		if (!p->isfloating && ISVISIBLE(p))
			r = p;
	return r;
}

Client *
lasttiled(Monitor *m)
{
	Client *p = m->clients, *r = NULL;
	for (; p; p = p->next)
		if (!p->isfloating && ISVISIBLE(p))
			r = p;
	return r;
}

void
fribidi(char *dest, char *src, size_t size)
{
	FriBidiStrIndex len;
	FriBidiCharSet charset;
	FriBidiChar unicodestr[size * 3];
	FriBidiChar visstr[size * 3];
	FriBidiParType partype = FRIBIDI_PAR_ON;

	dest[0] = '\0';
	if (!(len = strnlen(src, size)))
		return;
	charset = fribidi_parse_charset("UTF-8");
	len = fribidi_charset_to_unicode(charset, src, len, unicodestr);
	fribidi_log2vis(unicodestr, len, &partype, visstr, NULL, NULL, NULL);
	fribidi_unicode_to_charset(charset, visstr, len, dest);
}

/* there's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify). other types of errors call Xlib's
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow || ee->error_code == BadDrawable
	||  ee->error_code == BadPixmap || ee->error_code == BadGC
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	/* || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) */
	/* || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable) */
	/* || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) */
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	/* || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable)*/)
		return 0;
	fprintf(stderr, "fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("another window manager is already running");
	return -1;
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

Window
createsimplewin(void)
{
	return XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
}

void
renderinit(void)
{
	XFont *font, *prevfont = NULL;
	int i, j;

	pixmap = XCreatePixmap(dpy, root, sw, sh, depth);
	gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
	xftdraw = XftDrawCreate(dpy, pixmap, visual, colormap);

	/* init fonts */
	for (i = 1; i <= LENGTH(fontdefs); i++)
		if ((font = createfont(fontdefs[LENGTH(fontdefs) - i].name, NULL))) {
			font->block = fontdefs[LENGTH(fontdefs) - i].block;
			font->next = prevfont;
			prevfont = font;
		}
	if (!(fonts = prevfont))
		die("no fonts could be loaded");

	fontheight = fonts->height;
	barheight = fontheight;
	barheight *= barheightfact;

	/* init colors */
	for (i = 0; i < LENGTH(colors); i++)
		for (j = 0; j < LENGTH(colors[i]); j++)
			if (colors[i][j]) {
				if (!XftColorAllocName(dpy, visual, colormap, colors[i][j], &schemes[i][j]))
					die("cannot allocate color '%s'", colors[i][j]);
				/* remove any transparency from the pixel */
				schemes[i][j].pixel |= 0xFFU << 24;
			}

	/* init cursors */
	for (i = 0; i < LENGTH(cursors); i++)
		cursors[i] = XCreateFontCursor(dpy, cursor_shapes[i]);
}

void
renderfree(void)
{
	int i, j;

	XFreePixmap(dpy, pixmap);
	XFreeGC(dpy, gc);
	XftDrawDestroy(xftdraw);
	renderfreefonts(fonts);

	/* free colors */
	for (i = 0; i < LENGTH(colors); i++)
		for (j = 0; j < LENGTH(colors[i]); j++)
			if (colors[i][j])
				XftColorFree(dpy, visual, colormap, &schemes[i][j]);

	/* free cursors */
	for (i = 0; i < LENGTH(cursors); i++)
		XFreeCursor(dpy, cursors[i]);
}

void
renderfreefonts(XFont *font)
{
	if (font) {
		renderfreefonts(font->next);
		freefont(font);
	}
}

void
renderupdatesize(void)
{
	XFreePixmap(dpy, pixmap);
	pixmap = XCreatePixmap(dpy, root, sw, barheight, depth);

	/* clear the pixmap */
	/* XSetForeground(dpy, gc, schemes[SchemeNorm][ColorBG].pixel); */
	/* XFillRectangle(dpy, pixmap, gc, 0, 0, sw, barheight); */

	XftDrawDestroy(xftdraw);
	xftdraw = XftDrawCreate(dpy, pixmap, visual, colormap);
}

void
rendermap(Monitor *m, int w, int dest_x)
{
	XCopyArea(dpy, pixmap, m->barwin, gc, 0, 0, w, barheight, dest_x, 0);
	/* XSetForeground(dpy, gc, schemes[SchemeNorm][ColorBG].pixel); */
	/* XFillRectangle(dpy, pixmap, gc, 0, 0, sw, barheight); */
	/* XSync(dpy, 0); */
}

void
renderrect(int scheme, int x, int y, int w, int h, int filled, int invert)
{
	XSetForeground(dpy, gc, schemes[scheme][invert ? ColorBG : ColorFG].pixel);
	if (filled)
		XFillRectangle(dpy, pixmap, gc, x, y, w, h);
	else
		XDrawRectangle(dpy, pixmap, gc, x, y, w - 1, h - 1);
}

int
rendergettextwidth(const char *string)
{
	return rendertext(0, string, 0, 0, 0, 0, 0, 0);
}

int
rendertext(int scheme, const char *string, int x, int y, int w, int h,
	int pad, int invert)
{
	int render, render_y, tmpwidth = 0, width = 0;
	unsigned int tmpsize = 0, size = 0;
	XFont *font = NULL, *prevfont = NULL;
	const char *prevstring = string;
	XftColor fgcolor, bgcolor;

	render = x || y || w || h;

	if (render) {
		fgcolor = schemes[scheme][invert ? ColorBG : ColorFG];
		bgcolor = schemes[scheme][invert ? ColorFG : ColorBG];
		XSetForeground(dpy, gc, bgcolor.pixel);
		XFillRectangle(dpy, pixmap, gc, x, y, pad, h);
	}

	x += pad;
	w -= pad;

	while (1)
	{
		if (*string)
			getfirstcharinfo(string, &font, &tmpsize, &tmpwidth);

		if (!prevfont)
			prevfont = font;

		if (size && (!*string || font != prevfont))
		{
			if (render && width > 0)
			{
				XFillRectangle(dpy, pixmap, gc, x, y, width, h);
				render_y = y + (h - prevfont->height) / 2 + prevfont->xftfont->ascent;
				XftDrawStringUtf8(xftdraw, &fgcolor, prevfont->xftfont,
					x, render_y, (XftChar8 *)prevstring, size);
			}

			x += width;
			w -= width;
			width = size = 0;
			prevfont = font;
			prevstring = string;
		}

		if (!*string)
			break;

		string += tmpsize;
		size += tmpsize;
		width += tmpwidth;
	}

	if (render)
		XFillRectangle(dpy, pixmap, gc, x, y, w, h);

	return x + pad;
}

void
getfirstcharinfo(const char* string, XFont **font_ret, unsigned int *size_ret, int *width_ret)
{
	unsigned long codepoint;
	int i;

	utf8decodefirst(string, &codepoint, size_ret);

	for (i = 0; i < utf8cache.len; i++)
		if (codepoint == utf8cache.list[i].codepoint) {
			*font_ret = utf8cache.list[i].font;
			*width_ret = utf8cache.list[i].width;
			return;
		}

	*font_ret = getcharfont(codepoint);
	*width_ret = (codepoint == UTF8ZWNBS)
		? 0 : getcharwidth(*font_ret, string, *size_ret);

	utf8cache.idx %= LENGTH(utf8cache.list);
	utf8cache.list[utf8cache.idx].codepoint = codepoint;
	utf8cache.list[utf8cache.idx].font = *font_ret;
	utf8cache.list[utf8cache.idx].width = *width_ret;
	utf8cache.len = MAX(utf8cache.len, utf8cache.idx++);
}

XFont *
getcharfont(unsigned long codepoint)
{
	XFont *font, *lastfont;
	FcCharSet *fccharset;
	FcPattern *fcpattern, *match;
	XftResult result;
	int i;

	for (font = fonts; font; font = font->next)
		if (font->block != UnicodeGeneric)
			for (i = 0; i < LENGTH(blockdefs); i++)
				if (blockdefs[i].block == font->block
				 && codepoint >= blockdefs[i].start
				 && codepoint <= blockdefs[i].end
				 && XftCharExists(dpy, font->xftfont, codepoint))
				{
					return font;
				}

	for (font = fonts; font; font = font->next)
		if (XftCharExists(dpy, font->xftfont, codepoint))
			return font;

	fccharset = FcCharSetCreate();
	FcCharSetAddChar(fccharset, codepoint);

	/* refer to the comment in createfont for more information. */
	if (!fonts->pattern)
		die("the first font in the cache must be loaded from a font string.");

	fcpattern = FcPatternDuplicate(fonts->pattern);
	FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
	FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);
	if (!allowcolorfonts)
		FcPatternAddBool(fcpattern, FC_COLOR, FcFalse);

	FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
	FcDefaultSubstitute(fcpattern);
	match = XftFontMatch(dpy, screen, fcpattern, &result);

	FcCharSetDestroy(fccharset);
	FcPatternDestroy(fcpattern);

	if (!match)
		return fonts;

	font = createfont(NULL, match);
	if (font && XftCharExists(dpy, font->xftfont, codepoint)) {
		/* TODO: add the character to the list of matched fonts */
		for (lastfont = fonts; lastfont->next; lastfont = lastfont->next);
		lastfont->next = font;
	}
	else {
		freefont(font);
		font = fonts;
		/* TODO: add the character to the list of nomatch fonts */
	}

	return font;
}

int
getcharwidth(XFont *font, const char *string, unsigned int size)
{
	XGlyphInfo info;

	XftTextExtentsUtf8(dpy, font->xftfont, (XftChar8 *)string, size, &info);
	return info.xOff;
}

XFont *
createfont(const char *fontname, FcPattern *fontpattern)
{
	XFont *font;
	XftFont *xftfont = NULL;
	FcPattern *pattern = NULL;
	FcBool color;

	if (fontname)
	{
		/*
		 * using the pattern found at font->xftfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts.
		 */
		if (!(xftfont = XftFontOpenName(dpy, screen, fontname))) {
			fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
			return NULL;
		}
		if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
			fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n", fontname);
			XftFontClose(dpy, xftfont);
			return NULL;
		}
	}
	else if (fontpattern)
	{
		if (!(xftfont = XftFontOpenPattern(dpy, fontpattern))) {
			fprintf(stderr, "error, cannot load font from pattern.\n");
			return NULL;
		}
	}
	else
		die("no font specified for createfont().");

	if (!allowcolorfonts
		&& FcPatternGetBool(xftfont->pattern, FC_COLOR, 0, &color) == FcResultMatch
		&& color)
	{
		XftFontClose(dpy, xftfont);
		return NULL;
	}

	font = ecalloc(1, sizeof(XFont));
	font->xftfont = xftfont;
	font->pattern = pattern;
	font->height = xftfont->ascent + xftfont->descent;

	return font;
}

void
freefont(XFont *font)
{
	if (font->pattern)
		FcPatternDestroy(font->pattern);
	XftFontClose(dpy, font->xftfont);
	free(font);
}

void
utf8decodefirst(const char *string, unsigned long *codepoint_ret, unsigned int *size_ret)
{
	unsigned long state, prevstate;
	const char *start = string;

	*codepoint_ret = UTF8Invalid;

	for (state = prevstate = UTF8Init; *string; string++, prevstate = state)
	{
		utf8decode(&state, *string, codepoint_ret);

		if (state == UTF8Accept)
			break;

		if (state == UTF8Reject) {
			*codepoint_ret = UTF8Invalid;
			if (prevstate == UTF8Init)
				break;
			state = UTF8Init;
			string--;
		}
	}

	*size_ret = string - start + (*start ? 1 : 0);
}

/* Bjoern Hoehrmann's UTF-8 decoder (bjoern@hoehrmann.de)
 * for details, see http://bjoern.hoehrmann.de/utf-8/decoder/dfa */
void
utf8decode(unsigned long* state, unsigned char byte, unsigned long* codepoint_ret)
{
	const long utf8d[] = {
		/* map bytes to character classes to create bitmasks */
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		 8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,
		/* map a combination of a state of the automaton and a character class to a state */
		 0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	unsigned long type = utf8d[byte];

	*codepoint_ret = (*state == UTF8Init)
		? (0xFF >> type) & (byte)
		: (byte & 0x3FU) | (*codepoint_ret << 6);

	*state = utf8d[256 + *state + type];
}

/* copy src (or as much of it as fits) into dest. */
void
strscpy(char *dest, const char *src, size_t size)
{
	if (size <= 0)
		return;
	size_t len = strnlen(src, size - 1);
	memcpy(dest, src, len);
	dest[len] = '\0';
}

/* copy src (or as much of it as fits) into dest.
 * won't truncate if src is not null-terminated and dest has enough space. */
void
strsscpy(char *dest, const char *src, size_t destsize, size_t srcsize)
{
	if (destsize <= 0)
		return;
	size_t len = MIN(destsize - 1, strnlen(src, srcsize));
	memcpy(dest, src, len);
	dest[len] = '\0';
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

void
die(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "dwm: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

/* TODO: make sure borderpx, gappx, stairpx etc. won't cause any negative width or height values
         also make sure and prove widths and heights are always positive */
/* TODO: any new clients opening should un-fullscreen the currently fullscreened
 *       window, even if it is noautofocus. */
/* TODO: investigate the exact purpose of expose events on the bar,
 *       specially when a compositor starts/stops */
/* TODO: if you decide to keep the winbutton thingy:
 *       - make it smaller
 *       - perhaps hide it completely if there is no compositor */
/* TODO: DECIDE ON AND FIX THE FUCKING STACKING ORDER OF WINDOWS */
/* TODO: give functions (yes, all of them) (and variables?) more sensible names */
/* TODO: either fix the backtrace or get rid of it */
/* TODO: rewrite drwtext in a non-stupid way, then add farsi font support to it,
 *       (maybe then do the same thing to dmenu to draw icons with a mono nerd font?) */
/* TODO: see if we can move to a circular doubly-linked list */
/* TODO: maybe let's not define all the fucking variables of a function at the top, shall we? */
/* TODO: analyze and simplify switchcol and push */
/* TODO: analyze in what functions should restack(), focus(), arrange(), etc. be called  */
/* TODO: instead of having a c->isfullscreen and c->isfloating, we should have a c->state = {Fullscreen,Float,Tile} and a c->oldstate.
 *       after we do this, we can track the previous state of a client, and do some cool stuff. for example, if a client
 *       becomes floating from a state of being tiled, we can center it in the monitor. */
/* TODO: have a c->isinfloatlt for clients that are in a tag with a floating layout. */
/* TODO: focus all windows by default, except if we specify specifically with rules,
 *       AND when we're sure (through whatever method we can find) a window is
 *       impolite and has opened up out of nowhere.
 *       also, blockinput can be done with time. duh. */
/* TODO: make resettag work with floating and fullscreen clients too */
/* TODO: do sth like damage tracking for drawbar(), arrange(), restack(), focus(), etc.
 * TODO: at the end of manage(), handle attaching a noautofocus client and stuff
 *       ideally, we want new functions or new code to attach not at the top of the stack;
 *       also we need to see wether we should set `c->mon->sel = c` or not. */
/* TODO: fuck with setting environment variables in spawn and getting it in winpid to do some trickery shit */
/* TODO: implement some sort of scratchpads somehow. like for dictionaries and calculators and man pages and stuff. */
/* TODO: fix swallowing windows getting unmapped not restoring the terminal */
/* TODO: merge https://github.com/cdown/dwm/commit/62cd7e9 after a while once it's stable */
/* TODO: fix Arg functions on when they should exit early (floating, layout, etc.) */
/* TODO: can come up with a solution to call updatebar only at the end of the event loop */
/* TODO: probably can come up with a solution to call focus/arrange/restack only at the end of the event loop */
/* TODO: track where updatebar/updatebars is called */
/* TODO: dick-hardening and possibly completely useless and ridiculous feature:
 *       highlight bar buttons on mouse-over */

// vim:noexpandtab
