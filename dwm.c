/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

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

/* TODO: fix input focus on unswallow */
/* TODO: focus all windows by default, except if we specify specifically with rules,
 *       AND when we're sure (through whatever method we can find) a window is
 *       impolite and has opened up out of nowhere.
 *       also, blockinput can be done with time. duh. */
/* TODO: make resettag work with floating and fullscreen clients too */
/* TODO: do sth like damage tracking for drawbar(), arrange(), restack(), focus(), etc.
 * TODO: at the end of manage(), handle attaching a noautofocus client and stuff
 *       ideally, we want new functions or new code to attach not at the top of the stack;
 *       also we need to see wether we should set `c->mon->sel = c` or not. */
/* TODO: center size-hinted windows in their resizal */
/* TODO: finish togglelayout */
/* TODO: scroll action for tagbar and ltsymbol */
/* TODO: fuck with setting environment variables in spawn and getting it in winpid to do some trickery shit */
/* TODO: implement some sort of scratchpads somehow. like for dictionaries and calculators and man pages and stuff. */
/* TODO: fix swallowing windows getting unmapped not restoring the terminal */
/* TODO: merge https://github.com/cdown/dwm/commit/62cd7e9 after a while once it's stable */
/* TODO: fix Arg functions on when they should exit early (floating, layout, etc.) */
/* TODO: can come up with a solution to call updatebar only at the end of the event loop */
/* TODO: probably can come up with a solution to call focus/arrange/restack only at the end of the event loop */
/* TODO: track where updatebar/updatebars is called */

/* ======================
 * = Public Data Types
 * ====================== */

/* colors */
enum {
	ColorFG, ColorBG, ColorBorder, ColorBorderBG,
ColorLast };

/* color schemes */
enum {
	SchemeNorm, SchemeSel, SchemeUrg, SchemeTitle, SchemeStatus, SchemeStatusSep,
SchemeLast };

/* clicks */
enum {
	ClickTagBar, ClickLtSymbol, ClickWinTitle, ClickClientWin, ClickRootWin,
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
	unsigned int click;
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
 * = Config.h
 * ============ */

#include "config.h"

/* =============
 * = Macros
 * ============= */

/* systray and Xembed macros */
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ  0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT  1
#define SYSTEM_TRAY_REQUEST_DOCK           0
#define SYSTEM_TRAY_BEGIN_MESSAGE          1
#define SYSTEM_TRAY_CANCEL_MESSAGE         2
#define XEMBED_EMBEDDED_NOTIFY             0
#define XEMBED_MAPPED                      (1 << 0)
#define XEMBED_VERSION                     0

/* buffer sizes */
#define StatusSize         1024
#define WinTitleSize       256
#define LtSymbolSize       16
#define ClassNameSize      32
#define PertagStackSize    16
#define BackTraceSize      16
#define DrwNoMatchSize     1024

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
#define DRWTEXTWP(STR, X, W, INV) \
	(drwtext(STR, X, 0, W, barheight, drw.fonts->h / 2, INV))

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

/* UTF-8 macros */
#define UTF_SIZ      4
#define UTF_INVALID  0xFFFD
static const unsigned char utfbyte[UTF_SIZ + 1] = { 0x80,    0, 0xC0, 0xE0, 0xF0  };
static const unsigned char utfmask[UTF_SIZ + 1] = { 0xC0, 0x80, 0xE0, 0xF0, 0xF8  };
static const long utfmax[UTF_SIZ + 1] = { 0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF };
static const long utfmin[UTF_SIZ + 1] = { 0, 0, 0x80, 0x800, 0x10000 };

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
static Atom atom[LENGTH(atom_names)], netatom[LENGTH(netatom_names)];
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
	int isselmon, isstatusmon, isfloating;
	unsigned int tags, occtags, urgtags, nclients, selpos, bdw;
	char statustext[StatusSize], title[WinTitleSize], ltsymbol[LtSymbolSize];
};

struct ButtonPos {
	unsigned int tags[LENGTH(tags)];
	unsigned int tagsend;
	unsigned int ltsymbol;
	struct {
		int exists;
		unsigned int start;
		unsigned int end;
	} modules[LENGTH(statusclick)];
	unsigned int statusstart;
};

struct Monitor {
	char ltsymbol[LtSymbolSize];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int bdw;     /* bar draw width (barwidth minus systraywidth) */
	unsigned int gappx;
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	int showbar;
	int topbar;
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
	Window win, origwin;
	int ismapped; /* mapped state for systray icons */
};

struct Systray {
	Window win;
	Client *icons;
};

struct Drw {
	Pixmap pixmap;
	GC gc;
	DrwFont *fonts;
	int scheme;
	unsigned int ellipsiswidth;
	struct {
		long codepoints[DrwNoMatchSize];
		unsigned int idx;
		unsigned int max;
	} nomatches;
};

struct DrwFont {
	unsigned int h;
	XftFont *xfont;
	FcPattern *pattern;
	DrwFont *next;
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

static Window root, wmcheckwin, ignoreenterwin = 0;
static Monitor *mons, *selmon, **statusmonptr;
static Systray *systray = NULL;
static XftColor schemes[SchemeLast][ColorLast];
static Drw drw;
static unsigned int numlockmask = 0;
static int barheight;
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

/* configuration, allows nested code to access above variables */
/* #include "config.h" */

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* =============
 * = Functions
 * ============= */

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
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
setup(void)
{
	XSetWindowAttributes wa;
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
	XInternAtoms(dpy, atom_names, LENGTH(atom_names), False, atom);
	XInternAtoms(dpy, netatom_names, LENGTH(netatom_names), False, netatom);
	snprintf(trayatom_name, sizeof(trayatom_name), "_NET_SYSTEM_TRAY_S%d", screen);
	netatom[NetSystemTray] = XInternAtom(dpy, trayatom_name, False);

	/* init graphics */
	drwinit();

	/* init monitors */
	updatestatustext();
	updatemons();
	loadmonsettings();

	/* init NetWMCheck window */
	wmcheckwin = mksimplewin();
	setwinprop(root, netatom[NetWMCheck], wmcheckwin);
	setwinprop(wmcheckwin, netatom[NetWMCheck], wmcheckwin);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName],
		XInternAtom(dpy, "UTF8_STRING", False), 8,
		PropModeReplace, (unsigned char *)"dwm", 3);

	/* init EWMH props */
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)netatom, LENGTH(netatom));

	/* select events */
	wa.cursor = cursors[CurNormal];
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask|KeyPressMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);

	grabkeys();

#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (!XQueryTree(dpy, root, &d1, &d2, &wins, &num))
		return;

	/* regular windows */
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1)
		|| getatomprop(wins[i], atom[DWMSwallow]) != None)
			continue;
		if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
			manage(wins[i], wa);
	}

	/* swallowed windows */
	for (i = 0; i < num; i++) {
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (getatomprop(wins[i], atom[DWMSwallow]) == atom[DWMSwallowed])
			manage(wins[i], wa);
	}

	/* swallower windows */
	for (i = 0; i < num; i++) { /* now swallower windows */
		if (!XGetWindowAttributes(dpy, wins[i], &wa)
		|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
			continue;
		if (getatomprop(wins[i], atom[DWMSwallow]) == atom[DWMSwallower])
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

	XSync(dpy, False);
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
	Client *c;
	/* Monitor *m; */
	XCrossingEvent *ev = &e->xcrossing;

	if (ev->window != root && (ev->mode != NotifyNormal || ev->detail == NotifyInferior))
		return;

	/* ignore incorrect and parasitic enternotify events  */
	if (isenterignored(ev->window))
		return;

	c = wintoclient(ev->window);
	selmon = c ? c->mon : wintomon(ev->window);
	focus(c);

	/* if (m != selmon) { */
	/* 	unfocus(selmon->sel, 1); */
	/* 	selmon = m; */
	/* } else if (!c || c == selmon->sel) */
	/* 	return; */
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
		/* XSync(dpy, False); */
		return;
	}

	if (ev->value_mask & CWBorderWidth)
		c->bw = ev->border_width;

	if (c->isfullscreen || !afloat(c)) {
		sendconfigurenotify(c);
		/* XSync(dpy, False); */
		return;
	}

	m = c->mon;
	ev->value_mask &= c->nojitter ? ~(CWX|CWY) : ev->value_mask;
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

	/* XSync(dpy, False); */
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
	if (ev->atom != atom[XembedInfo]
	 && ev->atom != netatom[NetWMWindowType]
	 && ev->atom != netatom[NetWMName]
	 && ev->atom != XA_WM_HINTS
	 && ev->atom != XA_WM_NORMAL_HINTS
	 && ev->atom != XA_WM_TRANSIENT_FOR)
		return;

	if (ev->atom == atom[XembedInfo] && (c = wintosystrayicon(ev->window))) {
		systrayupdateicon(c, NULL);
		return;
	}

	if (!(c = wintoclient(ev->window)))
		return;

	if (ev->atom == netatom[NetWMWindowType])
		updatewindowtype(c);

	if (ev->atom == netatom[NetWMName]
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
		&& cme->message_type == netatom[NetSystemTrayOP])
	{
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK && cme->data.l[2])
			systrayaddicon(cme->data.l[2]);
		return;
	}

	if (!(c = wintoclient(cme->window)))
		return;

	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);

	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
		{
			/* keypresstime = ev->time; */
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
	c->pid = getwinpid(c->win);
	c->bw = borderpx;
	c->xkblayout  = xkblayout;
	c->noautofocus = noautofocus;
	c->desktop = -1;

	c->oldbw = wa.border_width;
	c->x = c->oldx = wa.x;
	c->y = c->oldy = wa.y;
	c->w = c->oldw = wa.width;
	c->h = c->oldh = wa.height;

	updateclass(c);
	updatetitle(c);

	if ((c->isfloating = gettransientfor(c->win, &t)) && t) {
		c->mon = t->mon;
		c->tags = t->tags;
		c->x = t->x + (t->w - WIDTH(c)) / 2;
		c->y = t->y + (t->h - HEIGHT(c)) / 2;
	} else {
		c->mon = selmon;
		applyrules(c);
		c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
		c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	}

	if (startup)
		loadclienttagsandmon(c);

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);

	XSetWindowBorderWidth(dpy, c->win, c->bw);
	drawborder(c->win, SchemeNorm);

	updatesizehints(c);
	updatewindowtype(c);
	updatewmhints(c);
	appendtoclientlist(c->win);
	XSelectInput(dpy, c->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);

#ifdef __linux__
	if (c->pid)
		setcardprop(c->win, netatom[NetWMPID], c->pid);
#endif /* __linux__ */

	if (swallow(c))
		return;

	attachdirection(c);
	attachstack(c);
	setclientstate(c, NormalState);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */

	/* only send configure notify to hidden windows.
	 * visible windows will be configured in arrange() -> resize(). */
	if (!ISVISIBLE(c))
		sendconfigurenotify(c);

	if (startup)
		return;

	if (!ISVISIBLE(c)) {
		c->mon->sel = c;
		XMapWindow(dpy, c->win);
		drawbar(c->mon);
		restack(c->mon);
	} else if (c->noautofocus) {
		arrange(c->mon);
		XMapWindow(dpy, c->win);
		ignoreenter(c->win);
		restack(c->mon);
		focus(c->mon->sel);
	} else {
		/* if (c->mon == selmon) */
		/* 	unfocus(selmon->sel, 0); */
		c->mon->sel = c;
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
		/* XSync(dpy, False); */
		/* XSetErrorHandler(xerror); */
		/* XUngrabServer(dpy); */
	}

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
	for (m = mons; m; m = m->next) {
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
	}
	/* XSync(dpy, False); */
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
	drwfree();

	XDestroyWindow(dpy, wmcheckwin);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	/* XSync(dpy, False); */

	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
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
	/* XSync(dpy, False); */
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
	setatomprop(t->win, atom[DWMSwallow], atom[DWMSwallower]);
	setatomprop(t->origwin, atom[DWMSwallow], atom[DWMSwallowed]);
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
		/* XSync(dpy, False); */
		/* XSetErrorHandler(xerror); */
		/* XUngrabServer(dpy); */
	}

	if (reattach) {
		c->swallow->mon = c->mon;
		c->swallow->tags = c->tags;
		XDeleteProperty(dpy, c->swallow->win, atom[DWMSwallow]);
		setclientstate(c->swallow, NormalState);
		updatetitle(c->swallow);
		updateclientdesktop(c->swallow);
		attachstack(c->swallow);
		attachdirection(c->swallow);
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
	XDeleteProperty(dpy, c->win, atom[DWMSwallow]);
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
	XDeleteProperty(dpy, c->win, atom[DWMSwallow]);
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
		int i, j, nmon, nxsi;
		Client *c;
		Monitor *m, **mp;
		XineramaScreenInfo *xsiall = XineramaQueryScreens(dpy, &nxsi);
		XineramaScreenInfo *xsi = NULL;

		for (nmon = 0, m = mons; m; m = m->next, nmon++);

		/* only consider unique geometries as separate monitors */
		xsi = ecalloc(nxsi, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nxsi; i++)
			if (isxsiunique(xsiall[i], xsi, j))
				xsi[j++] = xsiall[i];
		XFree(xsiall);
		nxsi = j;

		/* new monitors if nxsi > nmon */
		for (i = nmon; i < nxsi; i++) {
	 		for (mp = &mons; *mp; mp = &(*mp)->next);
			*mp = createmon();
		}
		for (m = mons, i = 0; m && i < nxsi; m = m->next, i++)
			if (i >= nmon || isnewmon(m, xsi[i])) {
				updated = 1;
				m->num = i;
				m->mx = m->wx = xsi[i].x_org;
				m->my = m->wy = xsi[i].y_org;
				m->mw = m->ww = xsi[i].width;
				m->mh = m->wh = xsi[i].height;
				m->bdw = m->ww;
			}

		/* removed monitors if nmon > nxsi */
		for (i = nxsi; i < nmon; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				updated = 1;
				m->clients = c->next;
				detachstack(c);
				c->mon = mons;
				attach(c);
				attachdirection(c);
			}
			if (m == selmon)
				selmon = mons;
			cleanupmon(m);
		}

		free(xsi);
	} else
#endif /* XINERAMA */

	{ /* default monitor setup */
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
		drwupdatesize();
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
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof(m->ltsymbol) - 1);

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
	unsigned int i;
	Monitor *m;

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
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}

	if (c && (c->isfullscreen || !afloat(c)))
		for (f = selmon->clients; f; f = f->next)
			if (f != c && f->isfullscreen && ISVISIBLE(f))
				setfullscreen(f, 0);

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
		setwinprop(root, netatom[NetActiveWindow], c->win);
	}
	sendevent(c, atom[WMTakeFocus]);
}

void
arrange(Monitor *m)
{
	Client *c;

	if (m)
		showhide(m->stack);
	else
		for (m = mons; m; m = m->next)
			showhide(m->stack);

	if (m) {
		arrangemon(m);
		restack(m);
	} else
		for (m = mons; m; m = m->next)
			arrangemon(m);

	if (m)
		for (c = m->clients; c; c = c->next)
			updateclientdesktop(c);
	else
		for (m = mons; m; m = m->next)
			for (c = m->clients; c; c = c->next)
				updateclientdesktop(c);
}

void
showhide(Client *c)
{
	if (!c)
		return;

	if (ISVISIBLE(c)) {
		/* show clients top down */
		if (c->isfullscreen) {
			if (c->compfullscreen)
				resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
			else
				resizeclient(c, c->mon->wx, c->mon->wy, c->mon->ww, c->mon->wh);
		}
		else if (afloat(c))
			resize(c, GEOM(c), 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
		c->geomvalid = 0;
	}
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof(m->ltsymbol) - 1);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (afloat(m->sel))
		XRaiseWindow(dpy, m->sel->win);
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext) {
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
		}
	}
	/* XSync(dpy, False); */
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
	/* XSync(dpy, False); */
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
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
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

		if (!c->hintsvalid)
			updatesizehints(c);

		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
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

	if (!getcardprop(c->win, netatom[NetWMDesktop], (long *)&desktop))
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
	getcardprop(root, atom[DWMMonSel], (long *)&selnum);

	/* load tags of monitors */
	while (XGetWindowProperty(dpy, root, atom[DWMMonDesktop], i, 1, False,
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
	strncpy(c->class, ch.res_class ? ch.res_class : broken, sizeof(c->class));
	strncpy(c->instance, ch.res_name ? ch.res_name : broken, sizeof(c->instance));
	XFree(ch.res_class);
	XFree(ch.res_name);
}

void
updatetitle(Client *c)
{
	if (!gettextprop(c->win, netatom[NetWMName], c->title, sizeof(c->title)))
		gettextprop(c->win, XA_WM_NAME, c->title, sizeof(c->title));
	if (c->title[0] == '\0') /* hack to mark broken clients */
		strcpy(c->title, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c->win, netatom[NetWMState]);
	Atom wtype = getatomprop(c->win, netatom[NetWMWindowType]);

	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
	if (state == netatom[NetWMFullscreen])
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
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char *)&w, 1);
}

void
updateclientlist()
{
	Client *c;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);

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
		setcardprop(c->win, netatom[NetWMDesktop], desktop);
	}
}

void
updatecurrentdesktop(void)
{
	Monitor *m;
	unsigned int desktop;

	setcardprop(root, atom[DWMMonSel], selmon->num);

	desktop = gettagnum(selmon->tagset[selmon->seltags]) + (selmon->num * LENGTH(tags));
	if (desktop == currentdesktop)
		return;

	currentdesktop = desktop;
	setcardprop(root, netatom[NetCurrentDesktop], desktop);

	XDeleteProperty(dpy, root, atom[DWMMonDesktop]);

	for (m = mons; m; m = m->next) {
		desktop = gettagnum(m->tagset[m->seltags]);
		XChangeProperty(dpy, root, atom[DWMMonDesktop], XA_CARDINAL, 32,
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
	setcardprop(root, netatom[NetNumberOfDesktops], n * LENGTH(tags));

	/* set desktop names */
	const char *names[n * LENGTH(tags)];
	for (i = 0; i < LENGTH(names); i++)
		names[i] = tags[i % LENGTH(tags)];
	XTextProperty prop;
	Xutf8TextListToTextProperty(dpy, (char **)names, LENGTH(names),
		XUTF8StringStyle, &prop);
	XSetTextProperty(dpy, root, &prop, netatom[NetDesktopNames]);
}

void
updatebarwin(Monitor *m)
{
	XClassHint ch = { "bar", "dwm" };
	XSetWindowAttributes wa = {
		.override_redirect = True,
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
			CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XSetClassHint(dpy, m->barwin, &ch);
		XDefineCursor(dpy, m->barwin, cursors[CurNormal]);
		setatomprop(m->barwin, netatom[NetWMWindowType], netatom[NetWMWindowTypeDock]);
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
		strcpy(statustext, "dwm-"VERSION);
}

void
drawbar(Monitor *m)
{
	int i, j, w, x = 0, tmpx, issel;
	int boxs   = drw.fonts->h / 9;
	int boxw   = drw.fonts->h / 6 + 2;
	int cindpx = drw.fonts->h * cindfact;
	unsigned int occ = 0; /* occupied tags */
	unsigned int urg = 0; /* tags containing urgent clients */
	unsigned int status_x;
	char biditext[StatusSize];
	Client *c;

	if (!m) {
		for (m = mons; m; m = m->next)
			drawbar(m);
		return;
	}

	if (!m->showbar || barunchanged(m))
		return;

	/* clear barwin */
	/* drw.scheme = SchemeNorm; */
	/* drwrect(0, 0, sw, barheight, 1, 1); */
	/* drwmap(m, sw, 0); */

	/* get occupied and urgent tags */
	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}

	/* reset button positions to 0 */
	m->bp = (const ButtonPos){0};

	/* draw status first so it can be overdrawn by tags later */
	status_x = m == STATUSMON ? drawstatus(m) : m->bdw;

	/* draw tags */
	for (i = 0; i < LENGTH(tags); i++)
	{
		issel = m->tagset[m->seltags] & (1 << i);

		/* skip vacant tags */
		if (!issel && !(occ & (1 << i)))
			continue;

		/* draw tag names */
		tmpx = x;
		drw.scheme = issel ? SchemeSel : SchemeNorm;
		x = m->bp.tags[i] = DRWTEXTWP(tags[i], x, m->bdw, urg & (1 << i));

		/* draw client indicators */
		for (j = 0, c = m->clients; c; c = c->next)
		{
			if (c->tags & (1 << i))
			{
				int ismonocle = (c == m->sel && m->lt[m->sellt]->arrange == monocle);
				int gap = MAX(cindpx / 2, 1);
				drwrect(
					tmpx + gap,
					(j * cindpx * 2) + gap + 1,
					cindpx * (ismonocle ? 2.5 : 1),
					cindpx,
					1, urg & (1 << i));
				j++;
			}
		}
	}
	m->bp.tagsend = x;

	/* draw layout symbol */
	drw.scheme = SchemeNorm;
	x = m->bp.ltsymbol = DRWTEXTWP(m->ltsymbol, x, m->bdw, 0);

	/* draw window title if it fits */
	if ((w = status_x - x) > barheight)
	{
		if (m->sel)
		{
			drw.scheme = m == selmon ? SchemeTitle : SchemeNorm;
			fribidi(m->sel->title, biditext);
			DRWTEXTWP(biditext, x, w, 0);

			/* draw a floating indicator */
			if (m->sel->isfloating && !m->sel->isfullscreen)
				drwrect(x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		}
		else
		{
			drw.scheme = SchemeNorm;
			drwrect(x, 0, w, barheight, 1, 1);
		}
	}

	drwmap(m, status_x, 0);
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
	strncpy(m->bs.ltsymbol, m->ltsymbol, sizeof(m->ltsymbol));
	strncpy(m->bs.statustext, statustext, sizeof(statustext));
	strncpy(m->bs.title, m->sel ? m->sel->title : "\0", sizeof(m->sel->title));

	return 0;
}

void
buttonpress(XEvent *e)
{
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;
	unsigned int i = 0, click = ClickRootWin;

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

		/* status modules */
		else if (ev->x > m->bp.statusstart) {
			for (i = 0; i < LENGTH(statusclick); i++)
				if (m->bp.modules[i].exists
				&& ev->x > m->bp.modules[i].start
				&& ev->x < m->bp.modules[i].end
				&& statusclick[i].func
				&& statusclick[i].button == ev->button
				&& CLEANMASK(statusclick[i].mod) == CLEANMASK(ev->state))
				{
					statusclick[i].func(&statusclick[i].arg);
					return;
				}
		}

		/* title */
		else
			click = ClickWinTitle;
	}

	else if ((c = wintoclient(ev->window))) {
		/* if modkey is pressed down, do not focus the window under the cursor.
		 * this enables eg. mod+scroll to be used to change focus between windows. */
		if (!(ev->state & Mod)) {
			focus(c);
			restack(selmon);
		}
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClickClientWin;
	}

	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click
			&& buttons[i].func
			&& buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mod) == CLEANMASK(ev->state))
		{
			buttons[i].func(click == ClickTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
		}
}

int
drawstatus(Monitor *m)
{
	char c, normaltext[LENGTH(statustext)] = {'\0'};
	int i, x = 0, pos = 0, normalstartpos = 0, tagstartpos = 0, skiptag = 0;
	unsigned int status_w, status_x, modulestart_x = 0;
	int islastchar, modulefound[LENGTH(statusclick)];
	enum modes { Normal, Tag, };
	enum modes mode = Normal;

	for (i = 0; i < LENGTH(statusclick); i++) {
		m->bp.modules[i].exists = 0;
		modulefound[i] = 0;
	}

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

			/* record modules's start x coord */
			for (i = 0; i < LENGTH(statusclick); i++)
				if (strncmp(&statustext[tagstartpos + 1], statusclick[i].module,
					pos - (tagstartpos + 1)) == 0)
				{
					m->bp.modules[i].exists = 1;
					m->bp.modules[i].start = modulestart_x;
					modulefound[i] = 1;
				}
		}

		/* no end of tag found;
		 * break tag, go back and interpret the tag start char as a normal char */
		else if (mode == Tag && (islastchar || !isalnum(c) || isseparator(c))) {
			mode = Normal;
			pos = tagstartpos - 1;
			skiptag = 1;
		}

		/* reached a separator char or the last char */
		else if (mode == Normal && isseparator(c))
		{
finaldraw:
			/* save any normaltext up to here */
			if (pos - normalstartpos > 0)
				strncat(normaltext, &statustext[normalstartpos], pos - normalstartpos);
			normalstartpos = pos + 1;

			/* draw any normaltext up to here */
			if (normaltext[0]) {
				drw.scheme = SchemeStatus;
				x = drwtext(normaltext, x, 0, m->bdw, barheight, 0, 0);
			}

			/* record module's end x coord */
			for (i = 0; i < LENGTH(statusclick); i++)
				if (modulefound[i]) {
					m->bp.modules[i].end = x;
					modulefound[i] = 0;
				}

			if (!c)
				goto finaldraw_end;

			/* draw the seaprator char */
			drw.scheme = SchemeStatusSep;
			normaltext[0] = c;
			normaltext[1] = '\0';
			x = drwtext(normaltext, x, 0, m->bdw, barheight, 0, 0);
			modulestart_x = x;

			/* reset the normaltext buffer */
			normaltext[0] = '\0';
		}

		if (mode == Normal && islastchar)

		if (c == '<' && skiptag)
			skiptag = 0;
	}

	goto finaldraw;
finaldraw_end:

	drw.scheme = SchemeNorm;
	status_w = MIN(x + (drw.fonts->h / 5), m->bdw);
	status_x = m->bp.statusstart = m->bdw - status_w;
	drwrect(status_x + status_w, 0, m->bdw, barheight, 1, 0);
	drwmap(m, status_w, status_x);

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
	XSetWindowAttributes swa;
	XWindowAttributes wa;
	Pixmap pixmap;
	GC gc;

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

	pixmap = XCreatePixmap(dpy, win, pw, ph, wa.depth);
	gc = XCreateGC(dpy, pixmap, 0, NULL);

	/* fill the area with the border background color */
	XSetForeground(dpy, gc, schemes[scm][ColorBorderBG].pixel);
	XFillRectangle(dpy, pixmap, gc, 0, 0, pw, ph);

	/* draw the inner border on top of the previous fill */
	XSetForeground(dpy, gc, schemes[scm][ColorBorder].pixel);
	XFillRectangles(dpy, pixmap, gc, rectangles, LENGTH(rectangles));

	swa.border_pixmap = pixmap;
	XChangeWindowAttributes(dpy, win, CWBorderPixmap, &swa);

	XFreePixmap(dpy, pixmap);
	XFreeGC(dpy, gc);
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
	unsigned int x = 0, systraywidth;
	int lastpad = drw.fonts->h / 5;
	Client *c;

	if (!systrayinit())
		return;

	for (c = systray->icons; c; c = c->next) {
		if (!c->ismapped)
			continue;
		if (!XGetWindowAttributes(dpy, c->win, &wa) || !wa.width || !wa.height) {
			wa.width = drw.fonts->h;
			wa.height = drw.fonts->h;
		}
		c->x = x + lastpad;
		c->y = (barheight - drw.fonts->h) / 2;
		c->h = drw.fonts->h;
		c->w = c->h * ((float)wa.width / wa.height);
		XMoveResizeWindow(dpy, c->win, GEOM(c));
		XSetWindowBackground(dpy, c->win, schemes[SchemeNorm][ColorBG].pixel);
		sendconfigurenotify(c);
		x += c->w + (drw.fonts->h / 5);
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
		/* XSync(dpy, False); */
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
		.override_redirect = True,
		.background_pixel = schemes[SchemeNorm][ColorBG].pixel,
		.event_mask = ButtonPressMask|ExposureMask,
	};

	if (!showsystray)
		return 0;

	if (systray)
		return 1;

	systray = ecalloc(1, sizeof(Systray));
	systray->win = mksimplewin();
	XSelectInput(dpy, systray->win, SubstructureNotifyMask);
	XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &swa);
	XSetClassHint(dpy, systray->win, &ch);
	XDefineCursor(dpy, systray->win, cursors[CurNormal]);
	setatomprop(systray->win, netatom[NetWMWindowType], netatom[NetWMWindowTypeDock]);
	setcardprop(systray->win, netatom[NetSystemTrayOrientation], orientation);
	XMapRaised(dpy, systray->win);

	XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
	if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
		sendeventraw(root, atom[Manager], StructureNotifyMask, CurrentTime,
			netatom[NetSystemTray], systray->win, 0, 0);
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
systraycleanup()
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
	getcardprop(w, netatom[NetWMPID], (long *)&pid);
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

	if ((c = wintoclient(w)))
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
focusstacktiled(const Arg *arg)
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
	c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
	attachdirection(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

void
push(const Arg *arg)
{
	Client *sel = selmon->sel, *c;

	if (!sel || afloat(sel))
		return;

	if (arg->i > 0) {
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
	else if (arg->i < 0) {
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

	if (arg && arg->lt) {
		for (i = 0; i < LENGTH(layouts) && layouts[i].arrange != arg->lt; i++);
		lt = &layouts[i];
	}
	if (!arg || !arg->lt || lt != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->lt)
		selmon->lt[selmon->sellt] = lt;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof(selmon->ltsymbol) - 1);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
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
	if (!selmon->sel || sendevent(selmon->sel, atom[WMDelete]))
		return;

	/* XGrabServer(dpy); */
	/* XSetErrorHandler(xerrordummy); */
	XSetCloseDownMode(dpy, DestroyAll);
	XKillClient(dpy, selmon->sel->win);
	/* XSync(dpy, False); */
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

	if (!c || afloat(c) || ismaster(c))
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
	XDestroyWindow(dpy, mksimplewin());
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

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
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

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
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
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);

		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClickClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mod|modifiers[j],
						c->win, False, BUTTONMASK,
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

	Client *at;
	for (at = c->mon->clients; at->next != c->mon->sel; at = at->next);
	c->next = at->next;
	at->next = c;
}

void
attachaside(Client *c)
{
	Client *at = firsttiledontag(c);

	if (!at) {
		attach(c);
		return;
	}

	c->next = at->next;
	at->next = c;
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
attachbottom(Client *c)
{
	Client *below;

	for (below = c->mon->clients; below && below->next; below = below->next);
	c->next = NULL;
	if (below)
		below->next = c;
	else
		c->mon->clients = c;
}

void
attachtop(Client *c)
{
	int n;
	Monitor *m = selmon;
	Client *below;

	for (n = 1, below = c->mon->clients;
		below && below->next &&
			(below->isfloating
			 || !ISVISIBLEONTAG(below, c->tags)
			 || n != m->nmaster);
		n = below->isfloating || !ISVISIBLEONTAG(below, c->tags) ? n + 0 : n + 1,
			below = below->next);

	c->next = NULL;

	if (below) {
		c->next = below->next;
		below->next = c;
	}
	else
		c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
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
						True, GrabModeAsync, GrabModeAsync);
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
		setatomprop(w, netatom[NetWMState], netatom[NetWMFullscreen]);
	else
		XChangeProperty(dpy, w, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char *)0, 0);
}

void
setwindowstate(Window w, long state)
{
	long data[] = { state, None };
	XChangeProperty(dpy, w, atom[WMState], atom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
getcardprop(Window w, Atom prop, long *ret)
{
	Atom type;
	int format;
	unsigned long nitems, remaining;
	long *value = NULL;

	if (XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
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

	if (XGetWindowProperty(dpy, w, prop, 0L, sizeof atom, False, XA_ATOM,
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
		strncpy(text, (char *)prop.value, size - 1);
	}
	else if (XmbTextPropertyToTextList(dpy, &prop, &list, &n) >= Success
		&& n > 0 && *list)
	{
		strncpy(text, *list, size - 1);
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

	if (XGetWindowProperty(dpy, w, atom[XembedInfo], 0, 2, False, atom[XembedInfo],
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

	if (XGetWindowProperty(dpy, w, atom[WMState], 0, 2, False, atom[WMState],
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
			exists = protocols[nproto] == proto;
		XFree(protocols);
	}

	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = atom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
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
	XSendEvent(dpy, w, False, mask, &ev);
}

void
sendxembedevent(Window w, long message, long detail, long data1, long data2)
{
	sendeventraw(w, netatom[Xembed], NoEventMask,
		CurrentTime, message, detail, data1, data2);
}

/* void */
/* togglelayout(const Arg *arg) */
/* { */
/* 	for (i = 0; i < LENGTH(layouts) && layouts[i].arrange) */
/* } */

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
	unsigned int i, n, h, mw, my;
	unsigned int ox, oy, ow, oh; /* offset values for stairs */
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
numtiled(Monitor *m)
{
	return numtiledcore(m->clients, m->tagset[m->seltags]);
}

int
numtiledontag(Client *c)
{
	return numtiledcore(c, c->tags);
}

int
numtiledcore(Client *c, unsigned int tags)
{
	int i = 0;
	c = nexttiledcore(c->mon->clients, tags);
	for (; c; c = nexttiledcore(c->next, tags), i++);
	return i;
}

int
ismaster(Client *c)
{
	return ismastercore(c, c->mon->tagset[c->mon->seltags]);
}

int
ismasterontag(Client *c)
{
	return ismastercore(c, c->tags);
}

int
ismastercore(Client *c, unsigned int tags)
{
	Client *t;
	int i, ret;

	for (i = 0, t = nexttiledcore(c->mon->clients, tags);
	     t && t != c;
	     t = nexttiledcore(t->next, tags), i++);

	pertagpush(c->mon, tags);
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
firsttiled(Monitor *m)
{
	return firsttiledcore(m->clients, m->tagset[m->seltags]);
}

Client *
firsttiledontag(Client *c)
{
	return firsttiledcore(c, c->tags);
}

Client *
firsttiledcore(Client *c, unsigned int tags)
{
	Client *t = c->mon->clients;
	for (; t && (t->isfloating || !ISVISIBLEONTAG(t, tags)); t = t->next);
	return t;
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
fribidi(char *in, char *out)
{
	FriBidiStrIndex len;
	FriBidiCharSet charset;
	FriBidiChar logical[StatusSize];
	FriBidiChar visual[StatusSize];
	FriBidiParType base = FRIBIDI_PAR_ON;

	out[0] = '\0';
	if (!(len = strlen(in)))
		return;
	charset = fribidi_parse_charset("UTF-8");
	len = fribidi_charset_to_unicode(charset, in, len, logical);
	fribidi_log2vis(logical, len, &base, visual, NULL, NULL, NULL);
	fribidi_unicode_to_charset(charset, visual, len, out);
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
mksimplewin(void)
{
	return XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
}

void
drwinit(void)
{
	DrwFont *font, *prevfont = NULL;
	int i, j;

	drw.pixmap = XCreatePixmap(dpy, root, sw, sh, depth);
	drw.gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, drw.gc, 1, LineSolid, CapButt, JoinMiter);

	/* init fonts */
	for (i = 1; i <= LENGTH(fonts); i++)
		if ((font = xfontcreate(fonts[LENGTH(fonts) - i], NULL))) {
			font->next = prevfont;
			prevfont = font;
		}
	if (!(drw.fonts = prevfont))
		die("no fonts could be loaded");

	drw.nomatches.idx = drw.nomatches.max = 0;
	drw.ellipsiswidth = drwgettextwidth("...");
	barheight = drw.fonts->h;
	barheight *= barheightfact;

	/* init colors */
	for (i = 0; i < LENGTH(colors); i++)
		for (j = 0; j < LENGTH(colors[i]); j++)
			if (colors[i][j]) {
				if (!XftColorAllocName(dpy, visual, colormap, colors[i][j], &schemes[i][j]))
					die("cannot allocate color '%s'", colors[i][j]);
				/* remove any transparency from the pixel */
				schemes[i][j].pixel |= 0xFF << 24;
			}

	/* init cursors */
	for (i = 0; i < LENGTH(cursors); i++)
		cursors[i] = XCreateFontCursor(dpy, cursor_shapes[i]);
}

void
drwmap(Monitor *m, unsigned int w, int dest_x)
{
	XCopyArea(dpy, drw.pixmap, m->barwin, drw.gc, 0, 0, w, barheight, dest_x, 0);
	XSetForeground(dpy, drw.gc, schemes[drw.scheme][ColorBG].pixel);
	XFillRectangle(dpy, drw.pixmap, drw.gc, 0, 0, sw, barheight);
	/* XSync(dpy, False); */
}

unsigned int
drwgettextwidth(const char *text)
{
	return drwtext(text, 0, 0, 0, 0, 0, 0);
}

unsigned int
drwgettextwidthclamp(const char *text, unsigned int n)
{
	unsigned int tmp = 0;
	if (text && n)
		tmp = drwtext(text, 0, 0, 0, 0, 0, n);
	return MIN(n, tmp);
}

void
drwupdatesize(void)
{
	XFreePixmap(dpy, drw.pixmap);
	drw.pixmap = XCreatePixmap(dpy, root, sw, barheight, depth);

	/* clear the pixmap */
	XSetForeground(dpy, drw.gc, schemes[drw.scheme][ColorBG].pixel);
	XFillRectangle(dpy, drw.pixmap, drw.gc, 0, 0, sw, barheight);
}

void
drwrect(int x, int y, unsigned int w, unsigned int h, int filled, int invert)
{
	XSetForeground(dpy, drw.gc,
		invert ? schemes[drw.scheme][ColorBG].pixel : schemes[drw.scheme][ColorFG].pixel);
	if (filled)
		XFillRectangle(dpy, drw.pixmap, drw.gc, x, y, w, h);
	else
		XDrawRectangle(dpy, drw.pixmap, drw.gc, x, y, w - 1, h - 1);
}


DrwFont *
xfontcreate(const char *fontname, FcPattern *fontpattern)
{
	DrwFont *font;
	XftFont *xfont = NULL;
	FcPattern *pattern = NULL;
	FcBool color;

	if (fontname) {
		/*
		 * using the pattern found at font->xfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts.
		 */
		if (!(xfont = XftFontOpenName(dpy, screen, fontname))) {
			fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
			return NULL;
		}
		if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
			fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n", fontname);
			XftFontClose(dpy, xfont);
			return NULL;
		}
	} else if (fontpattern) {
		if (!(xfont = XftFontOpenPattern(dpy, fontpattern))) {
			fprintf(stderr, "error, cannot load font from pattern.\n");
			return NULL;
		}
	} else {
		die("no font specified for xfontcreate().");
	}

	if (!allowcolorfonts
		&& FcPatternGetBool(xfont->pattern, FC_COLOR, 0, &color) == FcResultMatch
		&& color)
	{
		XftFontClose(dpy, xfont);
		return NULL;
	}

	font = ecalloc(1, sizeof(DrwFont));
	font->xfont = xfont;
	font->pattern = pattern;
	font->h = xfont->ascent + xfont->descent;

	return font;
}

void
drwfree(void)
{
	int i, j;

	XFreePixmap(dpy, drw.pixmap);
	XFreeGC(dpy, drw.gc);
	drwfreefonts(drw.fonts);

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
drwfreefonts(DrwFont *font)
{
	if (font) {
		drwfreefonts(font->next);
		xfontfree(font);
	}
}

void
xfontfree(DrwFont *font)
{
	if (font->pattern)
		FcPatternDestroy(font->pattern);
	XftFontClose(dpy, font->xfont);
	free(font);
}

int
drwtext(const char *text, int x, int y, unsigned int w, unsigned int h,
	unsigned int pad, int invert)
{
	int i, drw_y, utf8strlen, utf8charlen, render;
	int ellipsis_x = 0, charexists = 0, overflow = 0;
	unsigned int tmpextentwidth, extentwidth, ellipsis_len, ellipsis_w = 0;
	long utf8codepoint = 0;
	const char *utf8str;
	XftDraw *xftdraw = NULL;
	DrwFont *usedfont, *font, *nextfont;
	FcCharSet *fccharset;
	FcPattern *fcpattern;
	FcPattern *match;
	XftResult result;

	/* don't render and only report the width if no dimensions are provided */
	render = x || y || w || h;

	if (!text || (render && !w))
		return 0;

	if (!render)
		w = invert ? invert : ~invert;
	else {
		XSetForeground(dpy, drw.gc, schemes[drw.scheme][invert ? ColorFG : ColorBG].pixel);
		XFillRectangle(dpy, drw.pixmap, drw.gc, x, y, pad, h);
	}

	x += pad;
	usedfont = drw.fonts;

	while (1)
	{
		extentwidth = ellipsis_len = utf8strlen = 0;
		utf8str = text;
		nextfont = NULL;

		while (*text)
		{
			utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);

			for (font = drw.fonts; font; font = font->next)
			{
				charexists = charexists || XftCharExists(dpy, font->xfont, utf8codepoint);

				if (!charexists)
					continue;

				drwgetextents(font, text, utf8charlen, &tmpextentwidth, NULL);

				/* keep track of the last place we know the ellipsis still fits in */
				if (extentwidth + drw.ellipsiswidth <= w)
				{
					ellipsis_x = x + extentwidth;
					ellipsis_w = w - extentwidth;
					ellipsis_len = utf8strlen;
				}

				if (extentwidth + tmpextentwidth > w)
				{
					overflow = 1;
					/* called from drwgettextwidthclamp():
					 * it wants the width AFTER the overflow */
					if (!render)
						x += tmpextentwidth;
					else
						utf8strlen = ellipsis_len;
				}
				else if (font == usedfont)
				{
					utf8strlen += utf8charlen;
					text += utf8charlen;
					extentwidth += tmpextentwidth;
				}
				else
					nextfont = font;

				break;
			}

			if (overflow || !charexists || nextfont)
				break;
			else
				charexists = 0;
		}

		if (utf8strlen)
		{
			if (render)
			{
				XFillRectangle(dpy, drw.pixmap, drw.gc, x, y, extentwidth, h);
				if (!xftdraw)
					xftdraw = XftDrawCreate(dpy, drw.pixmap, visual, colormap);
				drw_y = y + (h - usedfont->h) / 2 + usedfont->xfont->ascent;
				XftDrawStringUtf8(xftdraw,
					&schemes[drw.scheme][invert ? ColorBG : ColorFG],
					usedfont->xfont, x, drw_y, (XftChar8 *)utf8str, utf8strlen);
			}

			x += extentwidth;
			w -= extentwidth;
		}

		if (render && overflow)
			drwtext("...", ellipsis_x, y, ellipsis_w, h, 0, invert);

		if (!*text || overflow)
			break;
		else if (nextfont)
		{
			charexists = 0;
			usedfont = nextfont;
		}
		else
		{
			/* regardless of whether or not a fallback font is found, the
			 * character must be drawn. */
			charexists = 1;

			/* avoid calling XftFontMatch if we know we won't find a match */
			for (i = 0; i <= drw.nomatches.max; ++i)
				if (utf8codepoint == drw.nomatches.codepoints[i])
					goto nomatch;

			fccharset = FcCharSetCreate();
			FcCharSetAddChar(fccharset, utf8codepoint);

			if (!drw.fonts->pattern) {
				/* refer to the comment in xfontcreate for more information. */
				die("the first font in the cache must be loaded from a font string.");
			}

			fcpattern = FcPatternDuplicate(drw.fonts->pattern);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);
			FcPatternAddBool(fcpattern, FC_COLOR, FcFalse);

			FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);
			match = XftFontMatch(dpy, screen, fcpattern, &result);

			FcCharSetDestroy(fccharset);
			FcPatternDestroy(fcpattern);

			if (match)
			{
				usedfont = xfontcreate(NULL, match);
				if (usedfont && XftCharExists(dpy, usedfont->xfont, utf8codepoint))
				{
					for (font = drw.fonts; font->next; font = font->next);
					font->next = usedfont;
				}
				else
				{
					xfontfree(usedfont);
					drw.nomatches.idx %= LENGTH(drw.nomatches.codepoints);
					drw.nomatches.codepoints[drw.nomatches.idx] = utf8codepoint;
					drw.nomatches.max = MAX(drw.nomatches.max, ++drw.nomatches.idx);
nomatch:
					usedfont = drw.fonts;
				}
			}
		}
	}

	XFillRectangle(dpy, drw.pixmap, drw.gc, x, y, pad, h);

	if (xftdraw)
		XftDrawDestroy(xftdraw);

	return x + pad;
	/* return x + (render ? w : 0); */
}

void
drwgetextents(DrwFont *font, const char *text, unsigned int len,
	unsigned int *w, unsigned int *h)
{
	XGlyphInfo info;

	if (!font || !text)
		return;

	XftTextExtentsUtf8(dpy, font->xfont, (XftChar8 *)text, len, &info);
	if (w)
		*w = info.xOff;
	if (h)
		*h = font->h;
}

size_t
utf8decode(const char *c, long *u, size_t clen)
{
	size_t i, j, len, type;
	long udecoded;

	*u = UTF_INVALID;
	if (!clen)
		return 0;
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, UTF_SIZ))
		return 1;
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type)
			return j;
	}
	if (j < len)
		return 0;
	*u = udecoded;
	utf8validate(u, len);

	return len;
}

long
utf8decodebyte(const char c, size_t *i)
{
	for (*i = 0; *i < (UTF_SIZ + 1); ++(*i))
		if (((unsigned char)c & utfmask[*i]) == utfbyte[*i])
			return (unsigned char)c & ~utfmask[*i];
	return 0;
}

size_t
utf8validate(long *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF))
		*u = UTF_INVALID;
	for (i = 1; *u > utfmax[i]; ++i);
	return i;
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

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

// vim:noexpandtab
