/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "kitty"
#define TERMCLASS "kitty"

/* appearance */
static const unsigned int stairpx   = 50;       /* depth of stairs layout */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int snap      = 20;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const unsigned int gappih    = 20;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 20;       /* vert inner gap between windows */
static const unsigned int gappoh    = 20;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 20;       /* vert outer gap between windows and screen edge */
static       int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "peep:size=12" , "Symbols Nerd Font:size=10", "JoyPixels:size=16" };
static const char dmenufont[]       = "peep:size=12";

/* default colors */
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";

/* custom colors */
static const char common_bg_gray[]  = "#333333";
static const char col_null[]        = "#000000";
static const char common_bg[]       = "#000000";
static const char norm_fg[]         = "#555555";
static const char norm_border[]     = "#444444";
static const char norm_float[]      = "#222222";
static const char sel_fg[]          = "#9d9d9d";
static const char sel_border[]      = "#9d9d9d";
static const char sel_float[]       = "#005577";
static const char title_fg[]        = "#9d9d9d";

static const char *colors[][4]      = {
    /*               fg         bg         border     float */
    [SchemeNorm]  = { norm_fg,  common_bg, norm_border, norm_float },
    [SchemeSel]   = { sel_fg,   common_bg, sel_border,  sel_float },
    [SchemeTitle] = { title_fg, common_bg, col_null,    col_null },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class, instance, title, tags mask, isfloating, isterminal, noswallow, monitor */
    {"TelegramDesktop", "telegram-desktop", "Media viewer", 0, 1, 0, 0, -1}, /* don't tile telegram's media viewer */
    {"Qalculate-gtk", NULL, NULL, 0, 1, 0, 0, -1}, /* keep qalculate floating */
    {"Safeeyes", "safeeyes", "safeeyes", 0, 1, 0, 0, -1}, /* don't tile safeeyes */
    {TERMCLASS, NULL, NULL, 0, 0, 1, 0, -1},
    {NULL, NULL, "Event Tester", 0, 0, 0, 1, -1},
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */

/* hint for attachdirection
 * 0 - default
 * 1 - attach above:
 *   Make new clients attach above the selected client,
 *   instead of always becoming the new master. This behaviour is known from xmonad.
 * 2 - attach aside:
 *   Make new clients get attached and focused in the stacking area,
 *   instead of always becoming the new master. It's basically an attachabove modification.
 * 3 - attach below:
 *   Make new clients attach below the selected client,
 *   instead of always becoming the new master. Inspired heavily by attachabove.
 * 4 - attach bottom:
 *   New clients attach at the bottom of the stack instead of the top.
 * 5 - attach top:
 *   New client attaches below the last master/on top of the stack.
 *   Behavior feels very intuitive as it doesn't disrupt existing masters,
 *   no matter the amount of them, it only pushes the clients in stack down.
 *   In case of nmaster = 1 feels like attachaside
 */
static const int attachdirection = 5;


#define FORCE_VSPLIT 1  /* nrowgrid layout: force two clients to always split vertically */
#include "vanitygaps.c"

/* layout array. first entry is default. */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "[M]",      monocle },
	{ "[@]",      spiral },
	{ "[\\]",     dwindle },
	{ "H[]",      deck },
	{ "TTT",      bstack },
	{ "===",      bstackhoriz },
	{ "HHH",      grid },
	{ "###",      nrowgrid },
	{ "---",      horizgrid },
	{ ":::",      gaplessgrid },
	{ "|M|",      centeredmaster },
	{ ">M>",      centeredfloatingmaster },
	{ "H==",      stairs },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ NULL,       NULL },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }
#define TERMCMD(cmd) { .v = (const char*[]){ TERM, "-e", "sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, NULL };
static const char *terminal[] = { TERM, NULL };

/* tui */
#define NCMPCPP TERMCMD("ncmpcpp")
#define ARIA2P TERMCMD("aria2p")
#define TREMC TERMCMD("tremc")
#define HTOP TERMCMD("htop")
#define VIM TERMCMD("nvim")

/* dmenu */
static const char *sysact[] = { "sysact", NULL };
static const char *lock[] = { "sysact", "lock", NULL };
static const char *suspend[] = { "sysact", "sleep", NULL };
static const char *dmount[] = { "dmount", NULL };
static const char *dshot[] = { "dshot", NULL };
static const char *bmrun[] = { "bmrun", NULL };

/* audio */
static const char *volinc[] = { "pamixer", "--allow-boost", "-i", "10", NULL };
static const char *voldec[] = { "pamixer", "--allow-boost", "-d", "10", NULL };
static const char *mute[] = { "pamixer", "-t", NULL };
static const char *cycle[] = { "pacycle", NULL };
static const char *pauseall[] = { "playerctl", "-a", "pause", NULL };

/* music */
static const char *music[] = { "mpc", "toggle", NULL };
static const char *nextsong[] = { "mpc", "next", NULL };
static const char *prevsong[] = { "mpc", "prev", NULL };
static const char *frwd[] = { "mpc", "seek", "+10", NULL };
static const char *back[] = { "mpc", "seek", "-10", NULL };

/* other */
static const char *browser[]  = { "firefox", NULL };
static const char *tray[] = { "tray", NULL };
static const char *bar[] = { "togglebar", NULL };
#define CALCULATOR TERMCMD("echo calculator; printf '\\033[6 q'; if command -v insect >/dev/null; then insect; else bc -qi; fi")
#define GIMME SHCMD("gimme -l | dmenu -p 'choose your poison' | xargs gimme")
#define NOTIFY_MUSIC SHCMD("mpc current | xargs -0 dunstify -r 45 -u low -t 2500 Playing:")
#define FF_TAB_NEXT SHCMD("inject 'ctrl+shift+j' firefox 'ctrl+Page_Down'")
#define FF_TAB_PREV SHCMD("inject 'ctrl+shift+k' firefox 'ctrl+Page_Up'")
#define FF_TAB_PUSH SHCMD("inject 'ctrl+alt+shift+j' firefox 'ctrl+shift+Page_Down'")
#define FF_TAB_PULL SHCMD("inject 'ctrl+alt+shift+k' firefox 'ctrl+shift+Page_Up'")
#define FF_TAB_CLOSE SHCMD("inject 'ctrl+shift+m' firefox 'ctrl+w'")
#define FF_FOCUS SHCMD("isfocused firefox && fffixfocus")

/* library for XF86XK_Audio keys */
#include <X11/XF86keysym.h>

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY,                       XK_i,      spawn,          GIMME },
	{ MODKEY,                       XK_t,      spawn,          {.v = terminal } },
	{ MODKEY,                       XK_b,      spawn,          {.v = browser } },
	{ MODKEY,                       XK_q,      spawn,          CALCULATOR },

	{ MODKEY|ShiftMask,             XK_m,      spawn,          NCMPCPP },
	{ MODKEY|ShiftMask,             XK_d,      spawn,          ARIA2P },
	{ MODKEY|ControlMask,           XK_d,      spawn,          TREMC },
	{ MODKEY|ControlMask,           XK_c,      spawn,          HTOP },
	{ MODKEY,                       XK_v,      spawn,          VIM },

	{ MODKEY|ControlMask,           XK_e,      spawn,          {.v = sysact } },
	{ MODKEY,                       XK_e,      spawn,          {.v = lock } },
	{ MODKEY|ShiftMask,             XK_e,      spawn,          {.v = suspend } },
	{ MODKEY,                       XK_m,      spawn,          {.v = dmount } },
	{ MODKEY|ShiftMask,             XK_s,      spawn,          {.v = dshot } },
	{ MODKEY,                       XK_n,      spawn,          {.v = bmrun } },
	{ MODKEY|Mod1Mask,              XK_z,      quit,           {0} },

	{ MODKEY|Mod1Mask,              XK_k,      spawn,          {.v = volinc } },
	{ MODKEY|Mod1Mask,              XK_j,      spawn,          {.v = voldec } },
	{ MODKEY|Mod1Mask,              XK_m,      spawn,          {.v = mute } },
	{ MODKEY|ControlMask,           XK_s,      spawn,          {.v = cycle } },
	{ MODKEY|ControlMask,           XK_p,      spawn,          {.v = pauseall } },
	{ 0,XF86XK_AudioRaiseVolume,               spawn,          {.v = volinc } },
	{ 0,XF86XK_AudioLowerVolume,               spawn,          {.v = voldec } },
	{ 0,XF86XK_AudioMute,                      spawn,          {.v = mute   } },

	{ MODKEY|Mod1Mask,              XK_p,      spawn,          {.v = music } },
	{ MODKEY|Mod1Mask,              XK_h,      spawn,          {.v = prevsong } },
	{ MODKEY|Mod1Mask,              XK_l,      spawn,          {.v = nextsong } },
	{ MODKEY|Mod1Mask,              XK_n,      spawn,          NOTIFY_MUSIC },
	{ MODKEY|ControlMask,           XK_h,      spawn,          {.v = back } },
	{ MODKEY|ControlMask,           XK_l,      spawn,          {.v = frwd } },

	{ MODKEY,                       XK_x,      spawn,          {.v = tray } },
	{ MODKEY|ControlMask,           XK_b,      spawn,          {.v = bar } },

	{ ControlMask|ShiftMask,           XK_j,      spawn,          FF_TAB_NEXT },
	{ ControlMask|ShiftMask,           XK_k,      spawn,          FF_TAB_PREV },
	{ ControlMask|ShiftMask,           XK_m,      spawn,          FF_TAB_CLOSE },
	{ ControlMask|ShiftMask,           XK_b,      spawn,          FF_FOCUS },
	{ ControlMask|ShiftMask|Mod1Mask,  XK_j,      spawn,          FF_TAB_PUSH },
	{ ControlMask|ShiftMask|Mod1Mask,  XK_k,      spawn,          FF_TAB_PULL },

	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      pushdown,       {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_k,      pushup,         {.i = -1 } },
	{ MODKEY,                       XK_s,      switchcol,      {0} },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.025} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.025} },
	{ MODKEY|ShiftMask,             XK_h,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,             XK_l,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,             XK_o,      setcfact,       {.f =  0.00} },
	{ MODKEY|ShiftMask,             XK_Return, transfer,       {0} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_w,      killclient,     {0} },
	{ MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_d,      setlayout,      {.v = &layouts[13]} },
	{ MODKEY,                       XK_g,      setlayout,      {.v = &layouts[10]} },
	{ MODKEY|ShiftMask,             XK_g,      setlayout,      {.v = &layouts[9]} },
	{ MODKEY,                       XK_c,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_a,      setlayout,      {.v = &layouts[5]} },
	{ MODKEY|ShiftMask,             XK_a,      setlayout,      {.v = &layouts[6]} },
	{ MODKEY|ControlMask,           XK_a,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ShiftMask,             XK_c,      setlayout,      {.v = &layouts[11]} },
	{ MODKEY|ShiftMask,             XK_b,      togglebar,      {0} },

	{ MODKEY|ControlMask,           XK_k,      incnmaster,     {.i = +1 } },
	{ MODKEY|ControlMask,           XK_j,      incnmaster,     {.i = -1 } },

	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },

	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button2,        setlayout,      {.v = &layouts[0]} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[13]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = terminal } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
