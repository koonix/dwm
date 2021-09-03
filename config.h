/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "st"
#define TERMCLASS "St"

/* appearance */
static const unsigned int stairpx   = 50;       /* depth of stairs layout */
static const unsigned int borderpx  = 2;        /* border pixel of windows */
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
static char null[]           = "#000000";
static char bg[]             = "#000000";
static char normfg[]         = "#555555";
static char normborder[]     = "#444444";
static char selfg[]          = "#9d9d9d";
static char selborder[]      = "#9d9d9d";
static char titlefg[]        = "#9d9d9d";

static char *colors[][4]      = {
    /*               fg        bg   border    */
    [SchemeNorm]  = { normfg,  bg,  normborder },
    [SchemeSel]   = { selfg,   bg,  selborder  },
    [SchemeTitle] = { titlefg, bg,  null       },
};

/* colors that can be used by the statusbar */
static char *statuscolors[] = { normfg, selfg };

/* specify colors to read from xrdb */
XCOLORS
    XLOAD( bg,         "*.background"  );
    XLOAD( normfg,     "*.color2"      );
    XLOAD( normborder, "*.background"  );
    XLOAD( selfg,      "*.color6"      );
    XLOAD( selborder,  "*.color6"      );
    XLOAD( titlefg,    "*.color2"      );
XCOLORS_END

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
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

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
	{ "H==",      stairs },
	{ "[M]",      monocle },
	{ ":::",      gaplessgrid },
	{ "|M|",      centeredmaster },
	{ ">M>",      centeredfloatingmaster },
	{ "[@]",      spiral },
	{ "[\\]",     dwindle },
	{ "H[]",      deck },
	{ "TTT",      bstack },
	{ "===",      bstackhoriz },
	{ "###",      nrowgrid },
	{ "---",      horizgrid },
	{ "HHH",      grid },
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
#define TUI(cmd) { .v = (const char*[]){ TERM, "-e", "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, NULL };
static const char *terminal[] = { TERM, NULL };

/* tui */
#define BASHMOUNT TUI("exec bashmount")
#define NCMPCPP TUI("exec ncmpcpp")
#define ARIA2P TUI("exec aria2p")
#define TREMC TUI("exec tremc")
#define VIM TUI("exec nvim")

/* dmenu */
static const char *sysact[] = { "sysact", NULL };
static const char *lock[] = { "loginctl", "lock-session", NULL };
static const char *suspend[] = { "sysact", "sleep", NULL };
static const char *dmount[] = { "dmount", NULL };
static const char *dshot[] = { "dshot", NULL };
static const char *bookmarks[] = { "bm", "-m", NULL };
#define PASSMENU SHCMD("passmenu -p 'Wat passwd u want?' && notify-send 'Copied to clipboard.'")

/* audio */
static const char *volinc[] = { "pamixer", "--allow-boost", "-i", "5", NULL };
static const char *voldec[] = { "pamixer", "--allow-boost", "-d", "5", NULL };
static const char *mute[] = { "pamixer", "-t", NULL };
static const char *cycle[] = { "pacycle", NULL };
static const char *toggleall[] = { "toggleall", NULL };
#define MIC_MUTE SHCMD("pactl list short sources | cut -f1 | xargs -I{} pactl set-source-mute {} toggle; dwmbarref audio")

/* music */
static const char *music[] = { "mpc", "toggle", NULL };
static const char *nextsong[] = { "mpc", "next", NULL };
static const char *prevsong[] = { "mpc", "prev", NULL };
static const char *frwd[] = { "mpc", "seek", "+10", NULL };
static const char *back[] = { "mpc", "seek", "-10", NULL };

/* backlight */
static const char *lightinc[] = { "light", "-A", "10", NULL };
static const char *lightdec[] = { "light", "-U", "10", NULL };
static const char *lightincsmall[] = { "light", "-A", "1", NULL };
static const char *lightdecsmall[] = { "light", "-U", "1", NULL };

/* other */
#define BROWSER SHCMD("exec $BROWSER")
static const char *tray[] = { "tray", NULL };
static const char *gimme[] = { "gimme", "-m", NULL };
static const char *fffixfocus[] = { "fffixfocus", NULL };
#define CALCULATOR TUI("echo Calculator; printf '\\033[6 q'; if command -v qalc >/dev/null; then exec qalc; else exec bc -qi; fi")
#define NOTIFY_SONG SHCMD("dunstify -r 45 -u low Playing: \"$(mpc current)\" || notify-send -u low Playing: \"$(mpc current)\"")
#define PIPEURL SHCMD("clipnotify && pipeurl \"$(xclip -o)\"")

/* library for XF86XK_Audio keys */
#include <X11/XF86keysym.h>

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = gimme } },
	{ MODKEY|ControlMask|Mod1Mask,  XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY,                       XK_t,      spawn,          {.v = terminal } },
	{ MODKEY,                       XK_b,      spawn,          BROWSER },
	{ MODKEY,                       XK_c,      spawn,          CALCULATOR },

	{ MODKEY,                       XK_m,      spawn,          BASHMOUNT },
	{ MODKEY|ShiftMask,             XK_m,      spawn,          NCMPCPP },
	{ MODKEY,                       XK_d,      spawn,          ARIA2P },
	{ MODKEY|ControlMask,           XK_t,      spawn,          TREMC },
	{ MODKEY,                       XK_v,      spawn,          VIM },

	{ MODKEY,                       XK_q,      spawn,          {.v = sysact } },
	{ MODKEY,                       XK_e,      spawn,          {.v = lock } },
	{ MODKEY|ShiftMask,             XK_e,      spawn,          {.v = suspend } },
	{ MODKEY,                       XK_m,      spawn,          {.v = dmount } },
	{ MODKEY|ShiftMask,             XK_s,      spawn,          {.v = dshot } },
	{ MODKEY|ShiftMask,             XK_b,      spawn,          {.v = bookmarks } },
	{ MODKEY|ShiftMask,             XK_p,      spawn,          PASSMENU },
	{ MODKEY|Mod1Mask,              XK_z,      quit,           {0} },
	{ MODKEY,                       XK_F12,    xrdb,           {0} },

	{ MODKEY|ControlMask,           XK_s,      spawn,          {.v = cycle } },
	{ MODKEY|ControlMask,           XK_p,      spawn,          {.v = toggleall } },
	{ 0,XF86XK_AudioPlay,                      spawn,          {.v = toggleall } },
	{ MODKEY|Mod1Mask,              XK_k,      spawn,          {.v = volinc } },
	{ 0,XF86XK_AudioRaiseVolume,               spawn,          {.v = volinc } },
	{ MODKEY|Mod1Mask,              XK_j,      spawn,          {.v = voldec } },
	{ 0,XF86XK_AudioLowerVolume,               spawn,          {.v = voldec } },
	{ MODKEY|Mod1Mask,              XK_m,      spawn,          {.v = mute } },
	{ 0,XF86XK_AudioMute,                      spawn,          {.v = mute } },
	{ MODKEY|ControlMask,           XK_m,      spawn,          MIC_MUTE },
	{ 0,XF86XK_AudioMicMute,                   spawn,          MIC_MUTE },

	{ MODKEY|Mod1Mask,              XK_p,      spawn,          {.v = music } },
	{ MODKEY|Mod1Mask,              XK_h,      spawn,          {.v = prevsong } },
	{ 0,XF86XK_AudioPrev,                      spawn,          {.v = prevsong } },
	{ MODKEY|Mod1Mask,              XK_l,      spawn,          {.v = nextsong } },
	{ 0,XF86XK_AudioNext,                      spawn,          {.v = nextsong } },
	{ MODKEY|ControlMask,           XK_h,      spawn,          {.v = back } },
	{ MODKEY|ControlMask,           XK_l,      spawn,          {.v = frwd } },
	{ MODKEY|Mod1Mask,              XK_n,      spawn,          NOTIFY_SONG },

	{ MODKEY,            XK_bracketright,      spawn,          {.v = lightinc } },
	{ MODKEY|ShiftMask,  XK_bracketright,      spawn,          {.v = lightincsmall } },
	{ 0,XF86XK_MonBrightnessUp,                spawn,          {.v = lightinc } },

	{ MODKEY,             XK_bracketleft,      spawn,          {.v = lightdec } },
	{ MODKEY|ShiftMask,   XK_bracketleft,      spawn,          {.v = lightdecsmall } },
	{ 0,XF86XK_MonBrightnessDown,              spawn,          {.v = lightdec } },

	{ MODKEY,                       XK_x,      spawn,          {.v = tray } },
	{ ControlMask|ShiftMask,        XK_b,      spawn,          {.v = fffixfocus } },
	{ MODKEY,                       XK_a,      spawn,          PIPEURL },

	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_j,      pushdown,       {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_k,      pushup,         {.i = -1 } },
	{ MODKEY,                       XK_s,      switchcol,      {0} },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_h,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,             XK_l,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,             XK_o,      setcfact,       {.f =  0.00} },
	{ MODKEY|ShiftMask,             XK_Return, transfer,       {0} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY,                       XK_w,      killclient,     {0} },
	{ MODKEY|ControlMask,           XK_b,      togglebar,      {0} },
	{ MODKEY|ShiftMask,             XK_t,      setlayout,      {.v = &layouts[0]} }, /* tile   */
	{ MODKEY|ShiftMask,             XK_d,      setlayout,      {.v = &layouts[1]} }, /* stairs */
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[2]} }, /* monocle */
	{ MODKEY,                       XK_g,      setlayout,      {.v = &layouts[3]} }, /* grid */
	{ MODKEY|ShiftMask,             XK_c,      setlayout,      {.v = &layouts[4]} }, /* centeredmaster */
	{ MODKEY|ControlMask,           XK_c,      setlayout,      {.v = &layouts[5]} }, /* centeredfloatingmaster*/

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
