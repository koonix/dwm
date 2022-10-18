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
#include <fribidi.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD__ */

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define ISVISIBLEONTAG(C, T)    ((C->tags & T))
#define ISVISIBLE(C)            ISVISIBLEONTAG(C, C->mon->tagset[C->mon->seltags])
#define LENGTH(X)               (sizeof(X) / sizeof(X[0]))
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TEXTWR(X)               (drw_fontset_getwidth(drw, (X)))
#define TEXTW(X)                (TEXTWR(X) + lrpad)
#define NSECPERMSEC             1000000
#define MSECPERSEC              1000

#define CLEANMASK(mask) \
	(mask & ~(numlockmask|LockMask) \
	& (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

#define INTERSECT(x,y,w,h,m) \
	 (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
	* MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))

/* systray macros */
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ  0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT  1
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON          10
#define XEMBED_MAPPED               (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define XEMBED_VERSION              0

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeUrg, SchemeTitle,
       SchemeBlockSel, SchemeBlockNorm }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType, NetWMDesktop, NetWMPID,
       NetWMWindowTypeDialog, NetWMWindowTypeDock, NetClientList, NetDesktopNames,
       NetNumberOfDesktops, NetCurrentDesktop, NetSystemTray, NetSystemTrayOP,
       NetSystemTrayOrientation, NetSystemTrayOrientationHorz, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XembedLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { DWMSwallow, DWMSwallower, DWMSwallowed, DWMLast }; /* dwm atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum { TimerUnblock, TimerLast }; /* timers */

/* include the tags array from config.h */
#define TAGS
#include "config.h"
#undef TAGS

/* data types */
typedef struct Monitor Monitor;
typedef struct Layout Layout;
typedef struct Pertag Pertag;
typedef struct BarState BarState;
typedef struct ButtonPos ButtonPos;
typedef struct Client Client;
typedef struct Systray Systray;
typedef struct Button Button;
typedef struct Key Key;
typedef struct Rule Rule;
typedef union Arg Arg;

struct Pertag {
	float mfact;
	int nmaster;
	unsigned int sellt;
	const Layout *lt[2];
};

struct BarState {
	int isselmon, isfloating, stw;
	unsigned int tags, occ, urg, nclients, selpos;
	char stext[1024], name[256], ltsymbol[16];
};

struct ButtonPos {
	unsigned int tag[LENGTH(tags)];
	unsigned int tagend;
	unsigned int ltsymbol;
	unsigned int status;
};

struct Monitor {
	char ltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
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
	unsigned int pertagstack[16];
	unsigned int pertagtop;
	BarState bs;
	ButtonPos bp;
};

struct Layout {
	const char *symbol;
	void (*arrange)(Monitor *);
};

struct Client {
	char name[256], class[32], instance[32];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
	int bw, oldbw;
	unsigned int tags, blockinput, pid, xkblayout;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	int barfullscreen, isterminal, noswallow, nojitter, origbarfullscreen;
	int desktop;
	Client *next;
	Client *snext;
	Client *swallow;
	Monitor *mon;
	Window win, origwin;
	Client *sametagnext;
	unsigned int sametagid, sametagparentid;
	int ismapped; /* mapped state for systray icons */
};

struct Systray {
	Window win;
	Client *icons;
};

union Arg {
	int i;
	unsigned int ui;
	float f;
	void (*lt)(Monitor *m);
	void (*adj)(const Arg *arg);
	const void *v;
};


struct Button {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
};

struct Key {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
};

struct Rule {
	const char *class;
	const char *instance;
	const char *title;
	int isfloating;
	int barfullscreen;
	int blockinput;
	unsigned int sametagid;
	unsigned int sametagparentid;
	int noswallow;
	int isterminal;
	int nojitter;
	unsigned int tags;
	int monitor;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachabove(Client *c) __attribute__((unused));
static void attachaside(Client *c) __attribute__((unused));
static void attachbelow(Client *c) __attribute__((unused));
static void attachbottom(Client *c) __attribute__((unused));
static void attachtop(Client *c) __attribute__((unused));
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatus(Monitor *m);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void focusstacktiled(const Arg *arg);
static void fribidi(char *in, char *out);
static void setcardprop(Window w, Atom prop, long value);
static void setatomprop(Window w, Atom prop, Atom value);
static int getcardprop(Window w, Atom prop, long *ret);
static Atom getatomprop(Window w, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void loadclienttagsandmon(Client *c);
static void loadmontagset(Monitor *m);
static void gotourgent(const Arg *arg);
static void grabbuttons(Window w, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void push(const Arg *arg);
static void quit(const Arg *arg) __attribute__((unused));
static void restart(const Arg *arg);
static void sigrestart(int unused);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizefullscreen(Client *c, int barfullscreen);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendeventraw(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setwindowstate(Window w, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setfullscreenprop(Window w, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void stairs(Monitor *m);
static void switchcol(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tagreduced(Client *c, int unmanage, unsigned int newtags);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void transfer(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updateclientdesktop(Client *c);
static void updatecurrentdesktop(void);
static void updatedesktops(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static void appendtoclientlist(Window w);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updateclass(Client *c);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* pertag functions */
static void pertagload(Monitor *m, unsigned int tags, unsigned int newtags);
static void pertagpush(Monitor *m, unsigned int newtags);
static void pertagpop(Monitor *m);

/* timer functions */
static void timerstart(int task, unsigned int msec);
static int timerexec(Window win);

/* blockinput functions */
static void blockinput(Window w, int msec);
static void unblockinput(void);

/* dualborders functions */
static void drawborder(Window win, int scm);
static void updateborder(Client *c);

/* sametag functions */
static void sametagapply(Client *c);
static int sametagisattached(Client *c);
static void sametagattach(Client *c);
static void sametagdetach(Client *c);

/* swallow functions */
static unsigned int getparentpid(unsigned int pid);
static int isdescprocess(unsigned int parent, unsigned int child);
static Client *origwintoclient(Window w);
static int swallow(Client *c);
static void unswallow(Client *c, int destroyed);
static void unmanageswallowed(Client *c);
static Client *getparentterminal(Client *c);
static unsigned int getwinpid(Window w);

/* systray functions */
static int systrayinit(void);
static void systrayaddicon(Window w);
static void systrayremoveicon(Client *c);
static void systrayupdate(void);
static void systrayupdateicon(Client *c, XResizeRequestEvent *ev);
static Client *wintosystrayicon(Window w);
static int getxembedinfoprop(Window w, unsigned int *version, unsigned int *flags);
static void sendxembedevent(Window w, long message, long detail, long data1, long data2);
static void resizerequest(XEvent *e);

/* auxiliary functions */
static unsigned int gettagnum(unsigned int tags);
static int numtiled(Monitor *m) __attribute__((unused));
static int numtiledontag(Client *c);
static int numtiledcore(Client *c, unsigned int tags);
static int ismaster(Client *c);
static int ismasterontag(Client *c);
static int ismastercore(Client *c, unsigned int tags);
static Client *nexttiledloop(Client *c);
static Client *prevtiledloop(Client *c);
static Client *nexttiled(Client *c);
static Client * nexttiledontag(Client *c) __attribute__((unused));
static Client * nexttiledcore(Client *c, unsigned int tags);
static Client *prevtiled(Client *c);
static Client *firsttiled(Monitor *m) __attribute__((unused));
static Client * firsttiledontag(Client *c);
static Client * firsttiledcore(Client *c, unsigned int tags);
static Client *lasttiled(Monitor *m);

/* variables */
static const char broken[] = "broken";
static char stext[1024];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height */
static int stw = 0;          /* systray width */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XembedLast], dwmatom[DWMLast];
static volatile int running = 1;
static volatile int mustrestart = 0;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Systray *systray = NULL;
static Window root, wmcheckwin;
static Window blockedwin = 0;
static Window swallowwin = 0;
static Client *sametagstacks[128];
static xcb_connection_t *xcon;
static Window timerwin[TimerLast];
static int starting = 0;
static int currentdesktop = -1;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */

void
applyrules(Client *c)
{
	unsigned int i;
	const Rule *r;
	Monitor *m;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(c->class, r->class))
		&& (!r->instance || strstr(c->instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->barfullscreen = r->barfullscreen >= 0 ? r->barfullscreen : barfullscreen;
			c->blockinput = r->blockinput >= 0 ? r->blockinput : blockinputmsec;
			c->sametagid  = r->sametagid;
			c->sametagparentid = r->sametagparentid;
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
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
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
		if (baseismin) { /* increment calculation requires this */
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
arrange(Monitor *m)
{
	Client *c;

	if (m) {
		showhide(m->stack);
	} else {
		for (m = mons; m; m = m->next)
			showhide(m->stack);
	}
	if (m) {
		arrangemon(m);
		restack(m);
	} else {
		for (m = mons; m; m = m->next)
			arrangemon(m);
	}
	if (m) {
		for (c = m->clients; c; c = c->next)
			updateclientdesktop(c);
	} else {
		for (m = mons; m; m = m->next)
			for (c = m->clients; c; c = c->next)
				updateclientdesktop(c);
	}
}

void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
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
	if (c->mon->sel == NULL || c->mon->sel == c->mon->clients || c->mon->sel->isfloating) {
		attach(c);
		return;
	}

	Client *at;
	for (at = c->mon->clients; at->next != c->mon->sel; at = at->next);
	c->next = at->next;
	at->next = c;
}

void
attachaside(Client *c) {
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
	if (c->mon->sel == NULL || c->mon->sel == c || c->mon->sel->isfloating) {
		attach(c);
		return;
	}
	c->next = c->mon->sel->next;
	c->mon->sel->next = c;
}

void
attachbottom(Client *c)
{
	Client *below = c->mon->clients;
	for (; below && below->next; below = below->next);
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
		below && below->next && (below->isfloating || !ISVISIBLEONTAG(below, c->tags) || n != m->nmaster);
		n = below->isfloating || !ISVISIBLEONTAG(below, c->tags) ? n + 0 : n + 1, below = below->next);
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
blockinput(Window w, int msec)
{
	unblockinput();
	XSetErrorHandler(xerrordummy);
	XGrabKey(dpy, AnyKey, AnyModifier, w, False, GrabModeAsync, GrabModeAsync);
	XSetErrorHandler(xerror);
	blockedwin = w;
	timerstart(TimerUnblock, msec);
}

void
unblockinput(void)
{
	int scm;
	Client *c;

	if (!blockedwin)
		return;

	XSetErrorHandler(xerrordummy);
	XUngrabKey(dpy, AnyKey, AnyModifier, blockedwin);
	XSetErrorHandler(xerror);

	if (selmon->sel && blockedwin == selmon->sel->win)
		scm = SchemeSel;
	else if ((c = wintoclient(blockedwin)) && c->isurgent)
		scm = SchemeUrg;
	else
		scm = SchemeNorm;
	drawborder(blockedwin, scm);

	blockedwin = 0;
}

void
buttonpress(XEvent *e)
{
	unsigned int i = 0, click = ClkRootWin;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}

	if (ev->window == root) {
		click = ClkRootWin;
	} else if (ev->window == m->barwin) {
		if (ev->x < m->bp.tagend) {
			for (i = 0; i < LENGTH(tags); i++) {
				if (m->bp.tag[i] && ev->x < m->bp.tag[i]) {
					click = ClkTagBar;
					arg.ui = 1 << i;
					break;
				}
			}
		} else if (ev->x < m->bp.ltsymbol) {
			click = ClkLtSymbol;
		} else if (ev->x > m->bp.status) {
			click = ClkStatusText;
		} else {
			click = ClkWinTitle;
		}
	} else if ((c = wintoclient(ev->window))) {
		/* if modkey is pressed down, do not focus the window
		 * under the cursor. this enables eg. mod+scroll to
		 * be used to change focus between windows. */
		if (!(ev->state & Mod)) {
			focus(c);
			restack(selmon);
		}
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}

	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click
			&& buttons[i].func
			&& buttons[i].button == ev->button
			&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
		{
			buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
		}
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
cleanup(void)
{
	Layout foo = { "", NULL };
	XWindowChanges wc;
	Monitor *m;
	Client *c, *f;
	size_t i;

	selmon->tagset[selmon->seltags] = ~0 & TAGMASK;
	selmon->lt[selmon->sellt] = &foo;
	arrange(selmon);

	XGrabServer(dpy);
	XSetErrorHandler(xerrordummy);
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			wc.border_width = c->oldbw;
			XSelectInput(dpy, c->win, NoEventMask);
			XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
			XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
			setclientstate(c, WithdrawnState);
			if (c->swallow) {
				c->swallow->win = c->origwin;
				c->swallow->next = c->next;
				c->next = c->swallow;
			}
		}
	}
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XUngrabServer(dpy);

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c;) {
			f = c;
			c = c->next;
			free(f);
		}
	}

	XUngrabKey(dpy, AnyKey, AnyModifier, root);

	while (mons)
		cleanupmon(mons);

	if (systray) {
		XUnmapWindow(dpy, systray->win);
		XDestroyWindow(dpy, systray->win);
		free(systray);
	}

	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);

	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);

	free(scheme);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
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

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c;

	if (showsystray
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
configure(Client *c)
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

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizefullscreen(c, c->barfullscreen);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX && !c->nojitter) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY && !c->nojitter) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating && !c->nojitter)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating && !c->nojitter)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c)) {
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
				if (ev->value_mask & (CWWidth|CWHeight))
					updateborder(c);
			}
		} else
			configure(c);
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;
	int i;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = m->pertagstack[0] = 1;
	m->pertagtop = 0;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);

	/* m->pertag = ecalloc(LENGTH(tags), sizeof(Pertag)); */
	for (i = 0; i < LENGTH(tags); i++) {
		m->pertag[i].mfact = m->mfact;
		m->pertag[i].nmaster = m->nmaster;
		m->pertag[i].lt[0] = m->lt[0];
		m->pertag[i].lt[1] = m->lt[1];
	}

	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if (timerexec(ev->window))
		return;

	if (ev->window == systray->win) {
		free(systray);
		systray = NULL;
	} else if ((c = wintoclient(ev->window)))
		unmanage(c, 1);
	else if ((c = origwintoclient(ev->window)))
		unmanageswallowed(c);
	else if ((c = wintosystrayicon(ev->window)))
		systrayremoveicon(c);
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

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

/* TODO: analyze how drawstatus() works */
int
drawstatus(Monitor *m)
{
	int statusx, i, w, x, len, iscode = 0;
	char *text, *origtext;
	Clr oldbg, oldfg;

	len = strlen(stext) + 1;
	origtext = text = ecalloc(len, sizeof(char));
	memcpy(text, stext, len);

	/* compute width of the status text */
	w = 0;
	i = -1;
	while (text[++i]) {
		if (text[i] != '^')
			continue;
		if (!iscode) {
			iscode = 1;
			text[i] = '\0';
			w += TEXTWR(text);
			text[i] = '^';
			if (text[++i] == 'f')
				w += atoi(&text[++i]);
		} else {
			iscode = 0;
			text = &text[i + 1];
			i = -1;
		}
	}
	if (!iscode)
		w += TEXTWR(text);

	text = origtext;
	w += bh / 5;
	statusx = x = m->ww - MIN(w + stw, m->ww / 2);

	drw_setscheme(drw, scheme[LENGTH(colors)]);
	oldfg = drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
	oldbg = drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
	drw_rect(drw, x, 0, w, bh, 1, 1);

	/* process status text */
	i = -1;
	while (text[++i]) {

		if (text[i] != '^')
			continue;

		text[i] = '\0';
		w = TEXTWR(text);
		drw_text(drw, x, 0, w, bh, 0, text, 0);
		x += w;

		/* process code */
		while (text[++i] != '^') {

			/* set foreground color */
			if (text[i] == 'c') {
				char buf[8];
				memcpy(buf, (char *)&text[i + 1], 7);
				buf[7] = '\0';
				drw_clr_create(drw, &drw->scheme[ColFg], buf);
				i += 7;
			}

			/* set background color */
			else if (text[i] == 'b') {
				char buf[8];
				memcpy(buf, (char*)&text[i + 1], 7);
				buf[7] = '\0';
				drw_clr_create(drw, &drw->scheme[ColBg], buf);
				i += 7;
			}

			/* set foreground color to the statuscolor with the given number */
			else if (text[i] == 'C') {
				int c = atoi(&text[++i]);
				drw_clr_create(drw, &drw->scheme[ColFg], statuscolors[c]);
			}

			/* set background color to the statuscolor with the given number */
			else if (text[i] == 'B') {
				int c = atoi(&text[++i]);
				drw_clr_create(drw, &drw->scheme[ColBg], statuscolors[c]);
			}

			/* reset colors to SchemeNorm */
			else if (text[i] == 'd') {
				drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
				drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
			}

			/* swap the current foreground and background colors */
			else if (text[i] == 'w') {
				Clr swp;
				swp = drw->scheme[ColFg];
				drw->scheme[ColFg] = drw->scheme[ColBg];
				drw->scheme[ColBg] = swp;
			}

			/* save the current colorscheme so it can be restored with ^t^ */
			else if (text[i] == 'v') {
				oldfg = drw->scheme[ColFg];
				oldbg = drw->scheme[ColBg];
			}

			/* restore the colorscheme saved by ^v^ */
			else if (text[i] == 't') {
				drw->scheme[ColFg] = oldfg;
				drw->scheme[ColBg] = oldbg;
			}

			/* draw rectangle */
			else if (text[i] == 'r') {
				int rx = atoi(&text[++i]);
				while (text[++i] != ',');
				int ry = atoi(&text[++i]);
				while (text[++i] != ',');
				int rw = atoi(&text[++i]);
				while (text[++i] != ',');
				int rh = atoi(&text[++i]);
				drw_rect(drw, rx + x, ry, rw, rh, 1, 0);
			}

			/* forward the cursor by the given number of pixels */
			else if (text[i] == 'f') {
				x += atoi(&text[++i]);
			}
		}

		text = &text[i + 1];
		i = -1;
	}

	w = TEXTWR(text);
	drw_text(drw, x, 0, w, bh, 0, text, 0);

	drw_setscheme(drw, scheme[SchemeNorm]);
	free(origtext);

	return statusx;
}

void
drawbar(Monitor *m)
{
	int i, j, w, x = 0, issel;
	int boxs   = drw->fonts->h / 9;
	int boxw   = drw->fonts->h / 6 + 2;
	int cindpx = drw->fonts->h * cindfact;
	unsigned int occ = 0; /* occupied tags */
	unsigned int urg = 0; /* tags containing urgent clients */
	unsigned int selpos = 0;
	char biditext[1024];
	Client *c;

	if (!m->showbar)
		return;

	/* get occupied and urgent tags */
	for (i = 0, c = m->clients; c; c = c->next, i++) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
		if (c == m->sel)
			selpos = i;
	}

	/* don't redraw the bar if it's content hasn't changed */
	if (m->bs.nclients == i
		&& m->bs.tags == m->tagset[m->seltags]
		&& m->bs.occ == occ
		&& m->bs.urg == urg
		&& m->bs.stw == stw
		&& !!m->bs.isselmon == !!(m == selmon)
		&& !!m->bs.name[0] == !!(m->sel)
		&& (!m->sel || m->bs.isfloating == m->sel->isfloating)
		&& (m->lt[m->sellt]->arrange != monocle || m->bs.selpos == selpos)
		&& strncmp(m->bs.ltsymbol, m->ltsymbol, sizeof(m->ltsymbol)) == 0
		&& (!m->sel || strncmp(m->bs.name, m->sel->name, sizeof(m->sel->name)) == 0)
		&& strncmp(m->bs.stext, stext, sizeof(stext)) == 0)
	{
		return;
	}
	m->bs.nclients = i;
	m->bs.tags = m->tagset[m->seltags];
	m->bs.occ = occ;
	m->bs.urg = urg;
	m->bs.stw = stw;
	m->bs.selpos = selpos;
	m->bs.isselmon = (m == selmon);
	m->bs.isfloating = (m->sel && m->sel->isfloating);
	strncpy(m->bs.ltsymbol, m->ltsymbol, sizeof(m->ltsymbol));
	strncpy(m->bs.stext, stext, sizeof(stext));
	if (m->sel)
		strncpy(m->bs.name, m->sel->name, sizeof(m->sel->name));
	else
		m->bs.name[0] = '\0';

	/* reset button positions to 0 */
	m->bp = (const ButtonPos){0};

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon)   /* status is only drawn on selected monitor */
		m->bp.status = drawstatus(m);

	/* draw tags */
	for (i = 0; i < LENGTH(tags); i++) {

		issel = m->tagset[m->seltags] & (1 << i);

		/* skip vacant tags */
		if (!issel && !(occ & (1 << i)))
			continue;

		/* draw tag names */
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[issel ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & (1 << i));

		/* draw client indicators */
		j = 0;
		for (c = m->clients; c; c = c->next) {
			if (c->tags & (1 << i)) {
				drw_rect(drw,
					x + MAX(cindpx / 2, 1),
					(j * cindpx * 2) + MAX(cindpx / 2, 1) + 1,
					cindpx * (c == m->sel && m->lt[m->sellt]->arrange == monocle ? 2.5 : 1),
					cindpx, 1, urg & (1 << i));
				j++;
			}
		}

		x += w;
		m->bp.tag[i] = x;
	}
	m->bp.tagend = x;

	/* draw layout symbol */
	w = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
	m->bp.ltsymbol = x;

	/* draw window name */
	if ((w = (m == selmon ? m->bp.status : m->ww) - x) > bh) {
		if (m->sel) {
			drw_setscheme(drw, scheme[m == selmon ? SchemeTitle : SchemeNorm]);
			fribidi(m->sel->name, biditext);
			drw_text(drw, x, 0, w, bh, lrpad / 2, biditext, 0);
			/* draw floating indicator */
			if (m->sel->isfloating && !m->sel->isfullscreen)
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}

	drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
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

	if (!XGetWindowAttributes(dpy, win, &wa))
		return;

	if (!wa.border_width)
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

	/* fill the area with the border color */
	XSetForeground(dpy, gc, scheme[scm][ColBorder].pixel);
	XFillRectangle(dpy, pixmap, gc, 0, 0, pw, ph);

	/* draw the inner border on top of the previous fill */
	XSetForeground(dpy, gc, scheme[scm][ColInnerBorder].pixel);
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
	if (c->win == blockedwin && c == selmon->sel)
		scm = SchemeBlockSel;
	else if (c->win == blockedwin)
		scm = SchemeBlockNorm;
	else if (c == selmon->sel)
		scm = SchemeSel;
	else if (c->isurgent)
		scm = SchemeUrg;
	else
		scm = SchemeNorm;
	drawborder(c->win, scm);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	/* the cursor entering swallowwin means it was already on the window
	 * when the swallowing window opened, so this event must be ignored */
	if (ev->window == swallowwin) {
		swallowwin = 0;
		return;
	}
	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		if (m == selmon)
			systrayupdate();
	}
}

void
focus(Client *c)
{
	Client *f;

	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);

	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);

	if (c) {
		selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		XkbLockGroup(dpy, XkbUseCoreKbd, c->xkblayout);
		detachstack(c);
		attachstack(c);
		grabbuttons(c->win, 1);
		setfocus(c);
		drawborder(c->win, c->win == blockedwin ? SchemeBlockSel : SchemeSel);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}

	selmon->sel = c;

	if (c && !c->isfloating)
		for (f = selmon->clients; f; f = f->next)
			if (f != c && f->isfullscreen && ISVISIBLE(f))
				setfullscreen(f, 0);

	drawbars();
	updatecurrentdesktop();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
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
	} else {
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
fribidi(char *in, char *out)
{
	FriBidiStrIndex len;
	FriBidiCharSet charset;
	FriBidiChar logical[1024];
	FriBidiChar visual[1024];
	FriBidiParType base = FRIBIDI_PAR_ON;

	out[0] = '\0';
	if (!(len = strlen(in)))
		return;
	charset = fribidi_parse_charset("UTF-8");
	len = fribidi_charset_to_unicode(charset, in, len, logical);
	fribidi_log2vis(logical, len, &base, visual, NULL, NULL, NULL);
	fribidi_unicode_to_charset(charset, visual, len, out);
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
getxembedinfoprop(Window w, unsigned int *version, unsigned int *flags)
{
	Atom type;
	int format;
	unsigned long nitems, remaining;
	long *prop = NULL;
	int success = 0;

	if (XGetWindowProperty(dpy, w, xatom[XembedInfo], 0, 2, False, xatom[XembedInfo],
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

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long *prop = NULL, result = -1;
	unsigned long nitems, remaining;
	Atom type;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0, 2, False, wmatom[WMState],
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
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING) {
		strncpy(text, (char *)name.value, size - 1);
	} else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
		strncpy(text, *list, size - 1);
		XFreeStringList(list);
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
loadclienttagsandmon(Client *c)
{
	unsigned int desktop, monnum;
	Monitor *m;

	if (getcardprop(c->win, netatom[NetWMDesktop], (long *)&desktop)) {
		c->tags = (1 << (desktop % LENGTH(tags))) & TAGMASK;
		monnum = desktop / LENGTH(tags);
		for (m = mons; m && m->num != monnum; m = m->next);
		if (m)
			c->mon = m;
	}
}

void
loadmontagset(Monitor *m)
{
	unsigned int tagset;

	if (getcardprop(root, netatom[NetCurrentDesktop], (long *)&tagset)) {
		tagset -= m->num * LENGTH(tags);
		tagset = (1 << tagset) & TAGMASK;
		m->tagset[0] = m->tagset[1] = m->pertagstack[0] = tagset;
	}
}

void
gotourgent(const Arg *arg)
{
	Monitor *m;
	Client *c;
	Arg a;
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->isurgent && !c->neverfocus) {
				a.ui = c->tags;
				if (c->mon != selmon) {
					unfocus(selmon->sel, 0);
					selmon = c->mon;
					focus(NULL);
				}
				view(&a);
				focus(c);
				restack(c->mon);
				return;
			}
		}
	}
}

void
grabbuttons(Window w, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, w);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, w, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						w, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
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
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

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
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel || selmon->sel->win == blockedwin)
		return;
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;
	/* unsigned int wintags; */

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = getwinpid(c->win);
	c->bw = borderpx;
	c->barfullscreen = barfullscreen;
	c->blockinput = blockinputmsec;
	c->xkblayout  = xkblayout;
	c->desktop = -1;

	c->oldbw = wa->border_width;
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;

	updateclass(c);
	updatetitle(c);

	if (XGetTransientForHint(dpy, c->win, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
		c->x = t->x + (t->w - WIDTH(c)) / 2;
		c->y = t->y + (t->h - HEIGHT(c)) / 2;
	} else {
		c->mon = selmon;
		applyrules(c);
		sametagapply(c);
		c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
		c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	}

	if (starting)
		loadclienttagsandmon(c);

	if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
		c->x = c->mon->wx + c->mon->ww - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
		c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->wx);
	c->y = MAX(c->y, c->mon->wy);

	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
	drawborder(c->win, SchemeNorm);

	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatewmhints(c);
	appendtoclientlist(c->win);
	XSelectInput(dpy, c->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c->win, 0);

	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;

#ifdef __linux__
	if (c->pid)
		setcardprop(c->win, netatom[NetWMPID], c->pid);
#endif /* __linux__ */

	if (swallow(c))
		return;

	if (c->blockinput && ISVISIBLE(c))
		blockinput(c->win, c->blockinput);

	attachdirection(c);
	attachstack(c);
	updatesizehints(c);
	setclientstate(c, NormalState);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */

	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
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
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
		return;
	if (!wintoclient(ev->window))
		manage(ev->window, &wa);
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
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}


void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if (ev->atom == xatom[XembedInfo] && (c = wintosystrayicon(ev->window)))
		systrayupdateicon(c, NULL);
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			c->hintsvalid = 0;
			arrange(c->mon);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
push(const Arg *arg)
{
	Client *sel = selmon->sel, *c;

	if (!sel || sel->isfloating)
		return;

	if (arg->i > 0) {
		if ((c = nexttiled(sel->next))) {
			detach(sel);
			sel->next = c->next;
			c->next = sel;
		} else {
			detach(sel);
			attach(sel);
		}
	} else if (arg->i < 0) {
		if ((c = prevtiled(sel))) {
			detach(sel);
			sel->next = c;
			if (selmon->clients == c)
				selmon->clients = sel;
			else {
				for (c = selmon->clients; c->next != sel->next; c = c->next);
				c->next = sel;
			}
		} else {
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
quit(const Arg *arg)
{
	running = 0;
}

void
restart(const Arg *arg)
{
	mustrestart = 1;
	running = 0;
	XDestroyWindow(dpy, XCreateSimpleWindow(dpy, root, 1, 1, 1, 1, 0, 0, 0));
	XFlush(dpy);
}

void
sigrestart(int unused)
{
	restart(NULL);
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

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	updateborder(c);
	XSync(dpy, False);
}

void
resizefullscreen(Client *c, int barfullscreen)
{
	if (barfullscreen)
		resizeclient(c, c->mon->wx, c->mon->wy, c->mon->ww, c->mon->wh);
	else
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
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
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
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
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
sametagapply(Client *c)
{
	const Client *p = NULL;

	if (c->sametagparentid && (p = sametagstacks[c->sametagparentid])) {
		c->mon  = p->mon;
		c->tags = p->tags;
		if (!ISVISIBLE(c))
			seturgent(c, 1);
	}

	if (c->sametagid && !sametagisattached(c))
		sametagattach(c);
}

int
sametagisattached(Client *c)
{
	Client *i;
	for (i = sametagstacks[c->sametagid]; i && i != c; i = i->sametagnext);
	return !!i;
}

void
sametagattach(Client *c)
{
	c->sametagnext = sametagstacks[c->sametagid];
	sametagstacks[c->sametagid] = c;
}

void
sametagdetach(Client *c)
{
	if (c->sametagid)
		return;
	Client **tc;
	for (tc = &sametagstacks[c->sametagid]; *tc && *tc != c; tc = &(*tc)->sametagnext);
	*tc = c->sametagnext;
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1)
			|| getatomprop(wins[i], dwmatom[DWMSwallow]) != None)
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now swallowed windows */
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (getatomprop(wins[i], dwmatom[DWMSwallow]) == dwmatom[DWMSwallowed])
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now swallower windows */
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (getatomprop(wins[i], dwmatom[DWMSwallow]) == dwmatom[DWMSwallower])
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
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
setclientstate(Client *c, long state)
{
	setwindowstate(c->win, state);
}

void
setwindowstate(Window w, long state)
{
	long data[] = { state, None };
	XChangeProperty(dpy, w, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
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
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}

	return exists;
}

void
sendxembedevent(Window w, long message, long detail, long data1, long data2)
{
	sendeventraw(w, netatom[Xembed], NoEventMask,
		CurrentTime, message, detail, data1, data2);
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
			PropModeReplace, (unsigned char *)&(c->win), 1);
	}
	sendevent(c, wmatom[WMTakeFocus]);
}

/* TODO: fix the whole fullscreen situation; oldx, oldbw, the whole 9 yards
 *       as in windows that become fullscreen or swallowing clients that
 *       switch from being fullscreen to barfullscreen (or vice-versa),
 *       if they're on another tag, shouldn't move to the current tag.
 *       they should just become urgent. */
void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		setfullscreenprop(c->win, 1);
		resizefullscreen(c, c->barfullscreen);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen) {
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		setfullscreenprop(c->win, 0);
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setfullscreenprop(Window w, int fullscreen)
{
	if (fullscreen)
		setatomprop(w, netatom[NetWMState], netatom[NetWMFullscreen]);
	else
		setatomprop(w, netatom[NetWMState], None);
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
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/* TODO: finish togglelayout */
/* void */
/* togglelayout(const Arg *arg) */
/* { */
/* 	for (i = 0; i < LENGTH(layouts) && layouts[i].arrange) */
/* } */

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
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;
	char trayatom[32];

	/* clean up any zombies immediately */
	sigchld(0);

	/* signal handler for restarting */
	if (signal(SIGHUP, sigrestart) == SIG_ERR)
		die("can't install SIGHUP handler:");

	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetWMWindowTypeDock] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
	netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
	netatom[NetWMDesktop] = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	netatom[NetWMPID] = XInternAtom(dpy, "_NET_WM_PID", False);
	snprintf(trayatom, sizeof(trayatom), "_NET_SYSTEM_TRAY_S%d", screen);
	netatom[NetSystemTray] = XInternAtom(dpy, trayatom, False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	dwmatom[DWMSwallow] = XInternAtom(dpy, "_DWM_SWALLOW", False);
	dwmatom[DWMSwallower] = XInternAtom(dpy, "_DWM_SWALLOWER", False);
	dwmatom[DWMSwallowed] = XInternAtom(dpy, "_DWM_SWALLOWED", False);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, sw, sh);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = drw->fonts->h + 2;
	updategeom();

	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);

	/* init appearance */
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], 4);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], 4);

	/* init bars */
	updatebars();
	updatestatus();

	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *)"dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *)&wmcheckwin, 1);

	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *)netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);

	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask|KeyPressMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);

	grabkeys();
	focus(NULL);
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
	}
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

void
timerstart(int task, unsigned int msec)
{
	timerwin[task] = XCreateSimpleWindow(dpy, root, 1, 1, 1, 1, 0, 0, 0);

	if (!msec) {
		XDestroyWindow(dpy, timerwin[task]);
		XFlush(dpy);
		return;
	}

	if (fork() == 0) {

		if (dpy)
			close(ConnectionNumber(dpy));

		struct timespec sleep = {
			.tv_sec = (time_t)msec / MSECPERSEC,
			.tv_nsec = ((long)msec % MSECPERSEC) * NSECPERMSEC
		};
		nanosleep(&sleep, NULL);

		if ((dpy = XOpenDisplay(NULL))) {
			XDestroyWindow(dpy, timerwin[task]);
			XCloseDisplay(dpy);
		}

		exit(EXIT_SUCCESS);
	}
}

int
timerexec(Window win)
{
	if (win == timerwin[TimerUnblock]) {
		timerwin[TimerUnblock] = 0;
		unblockinput();
	} else {
		return 0;
	}

	return 1;
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
	     c = nexttiled(c->next), i++) {
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
tagreduced(Client *c, int unmanage, unsigned int newtags) {
	unsigned int targettags = unmanage ? c->tags : ~(~c->tags|newtags);

	if (!targettags)
		return;

	pertagpush(c->mon, targettags);

	if (resettag && !c->isfloating && numtiledontag(c) == 1) {
		c->mon->nmaster = nmaster;
		c->mon->mfact = mfact;
		c->mon->sellt ^= 1;
		c->mon->lt[c->mon->sellt] = &layouts[0];
	} else if (nmasterbias >= 0 && c->mon->nmaster > nmasterbias && ismasterontag(c)) {
		c->mon->nmaster = MAX(c->mon->nmaster - 1, 0);
	}

	pertagpop(c->mon);
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
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
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	if (selmon->sel && selmon->sel->isfullscreen && selmon->sel->barfullscreen)
		resizefullscreen(selmon->sel, 1);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void
togglefullscr(const Arg *arg)
{
	if (selmon->sel)
		setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
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
unfocus(Client *c, int setfocus)
{
	XkbStateRec xkbstate;

	if (!c)
		return;

	grabbuttons(c->win, 0);
	drawborder(c->win, c->win == blockedwin ? SchemeBlockNorm : SchemeNorm);

	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}

	XkbGetState(dpy, XkbUseCoreKbd, &xkbstate);
	c->xkblayout = xkbstate.group;
	XkbLockGroup(dpy, XkbUseCoreKbd, xkblayout);
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	XWindowChanges wc;

	XkbLockGroup(dpy, XkbUseCoreKbd, xkblayout);

	if (c->swallow) {
		unswallow(c, destroyed);
		return;
	}

	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}

	tagreduced(c, 1, 0);
	sametagdetach(c);
	detach(c);
	detachstack(c);
	free(c);
	updateclientlist();
	arrange(m);
	focus(NULL);
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
updatebars(void)
{
	Monitor *m;
	XClassHint ch = { "bar", "dwm" };
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};

	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0,
			DefaultDepth(dpy, screen),
			CopyFromParent, DefaultVisual(dpy, screen),
			CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XSetClassHint(dpy, m->barwin, &ch);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		setatomprop(m->barwin, netatom[NetWMWindowType], netatom[NetWMWindowTypeDock]);
		XMapRaised(dpy, m->barwin);
	}

	systrayupdate();
}

void
updatebarpos(Monitor *m)
{
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
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
appendtoclientlist(Window w)
{
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
		PropModeAppend, (unsigned char *)&w, 1);
}

void
updatecurrentdesktop(void)
{
	unsigned int desktop =
		gettagnum(selmon->tagset[selmon->seltags]) + (selmon->num * LENGTH(tags));
	if (desktop != currentdesktop) {
		currentdesktop = desktop;
		setcardprop(root, netatom[NetCurrentDesktop], desktop);
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

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);

		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;

		/* new monitors if nn > n */
		for (i = n; i < nn; i++) {
			for (m = mons; m && m->next; m = m->next);
			if (m)
				m->next = createmon();
			else
				mons = createmon();
		}
		for (i = 0, m = mons; i < nn && m; m = m->next, i++)
			if (i >= n
			|| unique[i].x_org != m->mx || unique[i].y_org != m->my
			|| unique[i].width != m->mw || unique[i].height != m->mh)
			{
				dirty = 1;
				m->num = i;
				m->mx = m->wx = unique[i].x_org;
				m->my = m->wy = unique[i].y_org;
				m->mw = m->ww = unique[i].width;
				m->mh = m->wh = unique[i].height;
				updatebarpos(m);
				if (starting)
					loadmontagset(m);
			}
		/* removed monitors if n > nn */
		for (i = nn; i < n; i++) {
			for (m = mons; m && m->next; m = m->next);
			while ((c = m->clients)) {
				dirty = 1;
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
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
		updatedesktops();
	}
	return dirty;
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
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (c->swallow)
		return;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
	c->hintsvalid = 1;
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	drawbar(selmon);
}

void
systrayupdate(void)
{
	XWindowAttributes wa;
	unsigned int x = 0;
	Client *c;

	if (!showsystray || !systrayinit())
		return;

	for (c = systray->icons; c; c = c->next) {
		if (!c->ismapped)
			continue;
		if (!XGetWindowAttributes(dpy, c->win, &wa) || !wa.width || !wa.height) {
			wa.width = bh;
			wa.height = bh;
		}
		c->x = x;
		c->y = 0;
		c->h = bh;
		c->w = c->h * ((float)wa.width / wa.height);
		XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		XSetWindowBackground(dpy, c->win, scheme[SchemeNorm][ColBg].pixel);
		configure(c);
		x += c->w + (bh / 5);
	}

	stw = MIN(MAX(x, 1), selmon->ww / 3);

	XWindowChanges wc = {
		.x = selmon->wx + selmon->ww - stw,
		.y = selmon->by,
		.width = stw,
		.height = bh,
		.stack_mode = Above,
		.sibling = selmon->barwin,
	};
	XConfigureWindow(dpy, systray->win,
		CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);

	drawbar(selmon);
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
	c->mon = selmon;

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
systrayremoveicon(Client *c)
{
	Client **tc;
	for (tc = &systray->icons; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
	free(c);
	systrayupdate();
}

void
systrayupdateicon(Client *c, XResizeRequestEvent *resize)
{
	unsigned int version, flags;

	if (resize) {
		XMoveResizeWindow(dpy, c->win, c->x, c->y, resize->width, resize->height);
		XSync(dpy, False);
		systrayupdate();
	} else if (getxembedinfoprop(c->win, &version, &flags)) {
		if (flags & XEMBED_MAPPED) {
			c->ismapped = 1;
			XMapRaised(dpy, c->win);
			setclientstate(c, NormalState);
		} else if (!(flags & XEMBED_MAPPED)) {
			c->ismapped = 0;
			XUnmapWindow(dpy, c->win);
			setclientstate(c, WithdrawnState);
		}
		systrayupdate();
	}
}

Client *
wintosystrayicon(Window w)
{
	Client *c;

	if (!showsystray)
		return NULL;

	for (c = systray->icons; c && c->win != w; c = c->next);
	return c;
}

int
systrayinit(void)
{
	XClassHint ch = { "systray", "dwm" };
	unsigned int orientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
	XSetWindowAttributes swa = {
		.override_redirect = True,
		.background_pixel = scheme[SchemeNorm][ColBg].pixel,
		.event_mask = ButtonPressMask|ExposureMask,
	};

	if (systray)
		return 1;

	systray = ecalloc(1, sizeof(Systray));
	systray->win = XCreateSimpleWindow(dpy, root, 1, 1, 1, 1, 0, 0, 0);
	XSelectInput(dpy, systray->win, SubstructureNotifyMask);
	XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &swa);
	XSetClassHint(dpy, systray->win, &ch);
	XDefineCursor(dpy, systray->win, cursor[CurNormal]->cursor);
	setatomprop(systray->win, netatom[NetWMWindowType], netatom[NetWMWindowTypeDock]);
	setcardprop(systray->win, netatom[NetSystemTrayOrientation], orientation);
	XMapRaised(dpy, systray->win);

	XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
	if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
		sendeventraw(root, xatom[Manager], StructureNotifyMask, CurrentTime,
			netatom[NetSystemTray], systray->win, 0, 0);
	} else {
		fprintf(stderr, "dwm: unable to obtain system tray.\n");
		free(systray);
		systray = NULL;
		return 0;
	}

	return 1;
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
	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c->win, netatom[NetWMState]);
	Atom wtype = getatomprop(c->win, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else {
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

unsigned int
getparentpid(unsigned int pid)
{
	int ppid = 0;

#ifdef __linux__
	FILE *stat;
	char path[32];

	snprintf(path, sizeof(path), "/proc/%u/stat", pid);
	if (!(stat = fopen(path, "r")))
		return 0;
	fscanf(stat, "%*u (%*[^)]) %*c %u", (unsigned int *)&ppid);
	fclose(stat);
#endif /* __linux__*/

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

int
isdescprocess(unsigned int parent, unsigned int child)
{
	while (child != parent && child != 0)
		child = getparentpid(child);
	return !!child;
}

Client *
getparentterminal(Client *c)
{
	Client *t;
	Monitor *m;

	if (!c->pid || c->isterminal)
		return NULL;

	/* the chance of the selected client being the terminal
	 * we're looking for is higher, so check that first */
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

Client *
origwintoclient(Window w)
{
	Client *c;
	Monitor *m;

	/* the chance of the selected client being the client
	 * we're looking for is higher, so check that first */
	if ((c = selmon->sel) && c->swallow && c->origwin == w)
		return c;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->swallow && c->origwin == w)
				return c;

	return NULL;
}

int
swallow(Client *c)
{
	Client *t;
	Window win, wtmp;
	unsigned int uitmp;
	int itmp;
	int x, y, w, h;

	if (c->noswallow || c->isterminal || (c->isfloating && !swallowfloating)
		|| !(t = getparentterminal(c)))
	{
		return 0;
	}

	XSetErrorHandler(xerrordummy);
	XSelectInput(dpy, t->win, NoEventMask);
	XUngrabButton(dpy, AnyButton, AnyModifier, t->win);
	setclientstate(t, WithdrawnState);
	XUnmapWindow(dpy, t->win);
	XSync(dpy, False);
	XSetErrorHandler(xerror);

	t->swallow = c;
	c->mon = t->mon;
	c->tags = t->tags;

	t->origwin = t->win;
	t->win = c->win;

	t->origbarfullscreen = t->barfullscreen;
	t->barfullscreen = c->barfullscreen;

	x = t->x;
	y = t->y;
	w = t->w;
	h = t->h;

	if (ISVISIBLE(c) && t->isfullscreen && t->barfullscreen != t->origbarfullscreen) {
		if (t->barfullscreen) {
			x = t->mon->wx;
			y = t->mon->wy;
			w = t->mon->ww;
			h = t->mon->wh;
		} else {
			x = t->mon->mx;
			y = t->mon->my;
			w = t->mon->mw;
			h = t->mon->mh;
		}
	}

	updatetitle(t);
	setatomprop(t->win, dwmatom[DWMSwallow], dwmatom[DWMSwallower]);
	setatomprop(t->origwin, dwmatom[DWMSwallow], dwmatom[DWMSwallowed]);
	setfullscreenprop(t->win, t->isfullscreen);
	resizeclient(t, x, y, w, h);
	arrange(t->mon);
	XMapWindow(dpy, t->win);

	if (t->mon->stack == t) {
		focus(t);
	} else {
		/* set swallowwin which is used to ignore an enternotify() event */
		XQueryPointer(dpy, root, &wtmp, &win, &itmp, &itmp, &itmp, &itmp, &uitmp);
		if (win == t->win)
			swallowwin = win;
	}

	return 1;
}

void
unswallow(Client *c, int destroyed)
{
	int x, y, w, h;
	XWindowChanges wc;

	if (!destroyed) {
		wc.border_width = c->swallow->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XSelectInput(dpy, c->win, NoEventMask);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}

	c->win = c->origwin;

	x = c->x;
	y = c->y;
	w = c->w;
	h = c->h;

	if (ISVISIBLE(c) && c->isfullscreen && c->barfullscreen != c->origbarfullscreen) {
		if (c->origbarfullscreen) {
			x = c->mon->wx;
			y = c->mon->wy;
			w = c->mon->ww;
			h = c->mon->wh;
		} else {
			x = c->mon->mx;
			y = c->mon->my;
			w = c->mon->mw;
			h = c->mon->mh;
		}
	}

	c->barfullscreen = c->origbarfullscreen;

	free(c->swallow);
	c->swallow = NULL;
	updateclientlist();

	grabbuttons(c->win, 0);
	XSelectInput(dpy, c->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	XDeleteProperty(dpy, c->win, dwmatom[DWMSwallow]);
	setfullscreenprop(c->win, c->isfullscreen);
	setclientstate(c, NormalState);
	updatetitle(c);
	updateclientdesktop(c);
	resizeclient(c, x, y, w, h);
	XMapWindow(dpy, c->win);
	focus(NULL);
	restack(c->mon);
}

void
unmanageswallowed(Client *c)
{
	XDeleteProperty(dpy, c->win, dwmatom[DWMSwallow]);
	free(c->swallow);
	c->swallow = NULL;
	updateclientlist();
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	/* the chance of the selected client being the client
	 * we're looking for is higher, so check that first */
	if (selmon->sel && selmon->sel->win == w)
		return selmon->sel;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;

	return NULL;
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

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
		return;
	if (ismaster(c))
		return;
	if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
		return;
	pop(c);
}

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
		die("dwm: cannot open display");
	if (!(xcon = XGetXCBConnection(dpy)))
		die("dwm: cannot get xcb connection\n");
	checkotherwm();
	XrmInitialize();
	starting = 1;
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec ps", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	starting = 0;
	run();
	cleanup();
	XCloseDisplay(dpy);
	if (mustrestart)
		execvp(argv[0], argv);
	return EXIT_SUCCESS;
}

void
transfer(const Arg *arg)
{
	Client *c, *mtail = selmon->clients, *stail = NULL, *insertafter;
	int transfertostack = 0, nmasterclients = 0, i;

	for (i = 0, c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || c->isfloating)
			continue;
		if (selmon->sel == c) {
			transfertostack = i < selmon->nmaster && selmon->nmaster != 0;
		}
		if (i < selmon->nmaster) { nmasterclients++; mtail = c; }
		stail = c;
		i++;
	}
	if (!selmon->sel || selmon->sel->isfloating || i == 0) {
		return;
	} else if (transfertostack) {
		selmon->nmaster = MIN(i, selmon->nmaster) - 1;
		insertafter = stail;
	} else {
		selmon->nmaster++;
		insertafter = mtail;
	}
	if (insertafter != selmon->sel) {
		detach(selmon->sel);
		if (selmon->nmaster == 1 && !transfertostack) {
			attach(selmon->sel); /* head prepend case */
		} else {
			selmon->sel->next = insertafter->next;
			insertafter->next = selmon->sel;
		}
	}
	arrange(selmon);
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

// vim:noexpandtab
