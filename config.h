/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "st"
#define TERMCLASS "st-256color"

/* appearance */
static const unsigned int stairpx   = 50;       /* depth of stairs layout */
static const unsigned int borderpx  = 4;        /* border pixel of windows */
static const unsigned int snap      = 20;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const unsigned int gappih    = 20;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 20;       /* vert inner gap between windows */
static const unsigned int gappoh    = 20;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 20;       /* vert outer gap between windows and screen edge */
static       int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const unsigned char defxkblayout = 0;    /* the default keyboard layout number (starts from 0) */

/* fonts */
static const char *fonts[] = {
	"peep:size=12",
	"Symbols Nerd Font:size=10",
	"JoyPixels:size=16"
};

/* colors */
static char null[]       = "#000000";
static char bg[]         = "#000000";
static char normfg[]     = "#555555";
static char normborder[] = "#333333";
static char selfg[]      = "#9d9d9d";
static char selborder[]  = "#a8a8a8";
static char titlefg[]    = "#9d9d9d";
static char urgborder[]  = "#ff0000";

static char *colors[][4]      = {
	/*               fg        bg   border    */
	[SchemeNorm]  = { normfg,  bg,  normborder },
	[SchemeSel]   = { selfg,   bg,  selborder  },
	[SchemeTitle] = { titlefg, bg,  null       },
	[SchemeUrg]   = { normfg,  bg,  urgborder  },
};

/* colors that can be used by the statusbar */
static char *statuscolors[] = { normfg, selfg };

/* specify colors to read from xrdb */
XCOLORS
	XLOAD( bg,         "*.background"  );
	XLOAD( normfg,     "*.color2"      );
	XLOAD( selfg,      "*.color6"      );
	XLOAD( titlefg,    "*.color2"      );
XCOLORS_END

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 * WM_CLASS(STRING) = instance, class
	 * WM_NAME(STRING) = title */
	/* class, instance, title, tags mask, isfloating, isterminal, noswallow, monitor */
	{ "TelegramDesktop", "telegram-desktop", "Media viewer", 0, 1, 0, 0, -1 }, /* don't tile telegram's media viewer */
	{ "Qalculate-gtk", NULL, NULL, 0, 1, 0, 0, -1 }, /* keep qalculate floating */
	{ "Safeeyes", "safeeyes", "safeeyes", 0, 1, 0, 0, -1 }, /* don't tile safeeyes */
	/* swallowing rules: */
	{ TERMCLASS, NULL, NULL, 0, 0, 1, 0, -1 },
	{ NULL, NULL, "Event Tester", 0, 0, 0, 1, -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

/* hint for attachdirection
  0 - default
  1 - attach above:
    Make new clients attach above the selected client,
    instead of always becoming the new master. This behaviour is known from xmonad.
  2 - attach aside:
    Make new clients get attached and focused in the stacking area,
    instead of always becoming the new master. It's basically an attachabove modification.
  3 - attach below:
    Make new clients attach below the selected client,
    instead of always becoming the new master. Inspired heavily by attachabove.
  4 - attach bottom:
    New clients attach at the bottom of the stack instead of the top.
  5 - attach top:
    New client attaches below the last master/on top of the stack.
    Behavior feels very intuitive as it doesn't disrupt existing masters,
    no matter the amount of them, it only pushes the clients in stack down.
    In case of nmaster = 1 feels like attachaside */
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
	{ "TTT",      bstack },
	{ "[\\]",     dwindle },
	{ "[@]",      spiral },
	{ "H[]",      deck },
	{ "HHH",      grid },
	{ "###",      nrowgrid },
	{ "---",      horizgrid },
	{ "===",      bstackhoriz },
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
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-p", "Programs", NULL };
static const char *terminal[] = { TERM, NULL };

/* tui */
#define EDITOR TUI("exec \"${EDITOR:-nvim}\"")
#define DICTFZF TUI("exec dictfzf")
#define NCMPCPP TUI("exec ncmpcpp")
#define PULSEMIXER TUI("exec pulsemixer")
#define BASHMOUNT TUI("exec bashmount")
#define NEWSBOAT TUI("exec nboat")
#define NEOMUTT TUI("exec neomutt")
#define ARIA2P TUI("exec aria2p")
#define TREMC TUI("exec tremc")

/* dmenu */
static const char *sysact[] = { "sysact", NULL };
static const char *lock[] = { "loginctl", "lock-session", NULL };
static const char *suspend[] = { "sysact", "sleep", NULL };
static const char *dshot[] = { "dshot", NULL };
static const char *bookmarks[] = { "bm", "-m", NULL };
static const char *dpass[] = { "dpass", NULL };
static const char *unread[] = { "unread", NULL }; /* notifies unread emails */
static const char *daria2[] = { "daria2", NULL };

/* audio */
#define VOLINC(n) { .v = (const char*[]){ "pamixer", "--allow-boost", "-i", #n, NULL } }
#define VOLDEC(n) { .v = (const char*[]){ "pamixer", "--allow-boost", "-d", #n, NULL } }
static const char *mute[] = { "pamixer", "-t", NULL };
static const char *cycle[] = { "pacycle", NULL };
/**/
#define TOGGLE_MIC_MUTE SHCMD("pacmd list-sources | grep -q 'muted: yes' && { \
pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} false && \
notify-send ' Mic Enabled.' -u low -h string:x-canonical-private-synchronous:togglemicmute ;:; } || { \
pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} true && \
notify-send ' Mic Muted.' -u low -h string:x-canonical-private-synchronous:togglemicmute ;:; }")
/**/

/* media */
static const char *music[] = { "mpc", "toggle", NULL };
#define MEDIA_NEXT SHCMD("(mpc | grep -q '^\\[playing' && mpc next) & playerctl -a status -f '{{playerInstance}}	{{status}}' | grep Playing | cut -f1 | xargs -rL1 playerctl next -p")
#define MEDIA_PREV SHCMD("(mpc | grep -q '^\\[playing' && mpc prev) & playerctl -a status -f '{{playerInstance}}	{{status}}' | grep Playing | cut -f1 | xargs -rL1 playerctl previous -p")
#define MEDIA_SEEK_FWD SHCMD("(mpc | grep -q '^\\[playing' && mpc seek +10) & playerctl -a status -f '{{playerInstance}}	{{status}}' | grep Playing | cut -f1 | xargs -rL1 playerctl position 10+ -p")
#define MEDIA_SEEK_BACK SHCMD("(mpc | grep -q '^\\[playing' && mpc seek -10) & playerctl -a status -f '{{playerInstance}}	{{status}}' | grep Playing | cut -f1 | xargs -rL1 playerctl position 10- -p")
/**/
#define MEDIA_PLAYPAUSE SHCMD("mpc pause & f=${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}/playpause; \
p=$(playerctl -a status -f '{{playerInstance}}	{{status}}' | grep -v '\\<mpd\\>' | grep Playing) && { \
printf '%s\\n' \"$p\" >\"$f\"; playerctl -a pause; :;} || \
cut -f1 \"$f\" | xargs -rL1 playerctl play -p")
/**/

/* backlight */
#define LIGHTINC(n) { .v = (const char*[]){ "light", "-A", #n, NULL } }
#define LIGHTDEC(n) { .v = (const char*[]){ "light", "-U", #n, NULL } }

/* dunst */
static const char *dunst_close[]   = { "dunstctl", "close", NULL };
static const char *dunst_history[] = { "dunstctl", "history-pop", NULL };
static const char *dunst_context[] = { "dunstctl", "context", NULL };

/* other */
#define BROWSER SHCMD("exec $BROWSER")
static const char *gimme[] = { "gimme", "-m", NULL };
static const char *fffixfocus[] = { "fffixfocus", NULL };
static const char *ffmerge[] = { "ffmerge", NULL };
static const char *ytfzf[] = { "yt", NULL };
static const char *nmdmenu[] = { "networkmanager_dmenu", NULL };
#define CALCULATOR TUI("echo Calculator; printf '\\033[6 q'; if command -v qalc >/dev/null; then trap exit HUP; qalc; else exec bc -qi; fi")
#define NOTIFY_SONG SHCMD("notify-send -u low -h string:x-canonical-private-synchronous:notifysong Playing: \"$(mpc current)\"")
/**/
#define CLIPLISTEN SHCMD("flock -eno /tmp/cliplisten timeout 30 \
sh -c 'notify-send -u low \"Listening to Clipboard...\"; \
while :; do echo PipeURL | xclip -selection clipboard; clipnotify || exit 1; \
clip=$(xclip -o -selection clipboard); [ \"$clip\" = PipeURL ] && continue; \
notify-send -u low \"Got it.\"; pipeurl \"${clip:?}\" >/dev/null 2>&1 & break; done'")
/**/

/* library for XF86XK_Audio keys */
#include <X11/XF86keysym.h>

/* the logic behind the bindings:
	- all of the audio and music related stuff start with super+alt
	- all of the layouts start with super+control
	- most bindings that have a similar function only differ in shift */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = gimme } },
	{ MODKEY|ShiftMask,             XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY,                       XK_t,      spawn,          {.v = terminal } },
	{ MODKEY,                       XK_b,      spawn,          BROWSER },
	{ MODKEY,                       XK_c,      spawn,          CALCULATOR },

	{ ControlMask,                  XK_space,  spawn,          {.v = dunst_close } },
	{ ControlMask,                  XK_grave,  spawn,          {.v = dunst_history } },
	{ ControlMask|ShiftMask,        XK_period, spawn,          {.v = dunst_context } },

	{ MODKEY,                       XK_u,      spawn,          BASHMOUNT },
	{ MODKEY,                       XK_n,      spawn,          NEWSBOAT },
	{ MODKEY|ShiftMask,             XK_n,      spawn,          NEOMUTT },
	{ MODKEY|ControlMask,           XK_n,      spawn,          {.v = unread } },
	{ MODKEY,                       XK_m,      spawn,          NCMPCPP },
	{ MODKEY|ShiftMask,             XK_m,      spawn,          PULSEMIXER },
	{ MODKEY,                       XK_d,      spawn,          ARIA2P },
	{ MODKEY|ShiftMask,             XK_t,      spawn,          TREMC },
	{ MODKEY,                       XK_v,      spawn,          EDITOR },
	{ MODKEY|ControlMask|ShiftMask, XK_d,      spawn,          DICTFZF },
	{ MODKEY,                       XK_y,      spawn,          {.v = ytfzf } },

	{ MODKEY,                       XK_q,      spawn,          {.v = sysact } },
	{ MODKEY,                       XK_e,      spawn,          {.v = lock } },
	{ MODKEY|ShiftMask,             XK_e,      spawn,          {.v = suspend } },
	{ MODKEY|Mod1Mask,              XK_s,      spawn,          {.v = dshot } },
	{ MODKEY|ShiftMask,             XK_b,      spawn,          {.v = bookmarks } },
	{ MODKEY|ControlMask,           XK_p,      spawn,          {.v = dpass } },
	{ MODKEY|ShiftMask,             XK_d,      spawn,          {.v = daria2 } },
	{ MODKEY|ShiftMask,             XK_r,      spawn,          {.v = nmdmenu } },
	{ MODKEY|Mod1Mask,              XK_F4,     quit,           {0} },
	{ MODKEY,                       XK_F12,    xrdb,           {0} },

	{ 0,XF86XK_AudioRaiseVolume,               spawn,          VOLINC(5) },
	{ 0,XF86XK_AudioLowerVolume,               spawn,          VOLDEC(5) },
	{ ShiftMask,XF86XK_AudioRaiseVolume,       spawn,          VOLINC(20) },
	{ ShiftMask,XF86XK_AudioLowerVolume,       spawn,          VOLDEC(20) },
	{ MODKEY|Mod1Mask,              XK_k,      spawn,          VOLINC(5) },
	{ MODKEY|Mod1Mask,              XK_j,      spawn,          VOLDEC(5) },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_k,      spawn,          VOLINC(20) },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_j,      spawn,          VOLDEC(20) },
	{ MODKEY|Mod1Mask,              XK_m,      spawn,          {.v = mute } },
	{ 0,XF86XK_AudioMute,                      spawn,          {.v = mute } },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_m,      spawn,          TOGGLE_MIC_MUTE },
	{ 0,XF86XK_AudioMicMute,                   spawn,          TOGGLE_MIC_MUTE },
	{ MODKEY|ControlMask,           XK_s,      spawn,          {.v = cycle } },

	{ MODKEY|Mod1Mask|ShiftMask,    XK_p,      spawn,          {.v = music } },
	{ MODKEY|Mod1Mask,              XK_p,      spawn,          MEDIA_PLAYPAUSE },
	{ 0,XF86XK_AudioPlay,                      spawn,          MEDIA_PLAYPAUSE },
	{ MODKEY|Mod1Mask,              XK_h,      spawn,          MEDIA_SEEK_BACK },
	{ MODKEY|Mod1Mask,              XK_l,      spawn,          MEDIA_SEEK_FWD },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_h,      spawn,          MEDIA_PREV },
	{ 0,XF86XK_AudioPrev,                      spawn,          MEDIA_PREV },
	{ MODKEY|Mod1Mask|ShiftMask,    XK_l,      spawn,          MEDIA_NEXT },
	{ 0,XF86XK_AudioNext,                      spawn,          MEDIA_NEXT },
	{ MODKEY|Mod1Mask,              XK_n,      spawn,          NOTIFY_SONG },

	{ 0,XF86XK_MonBrightnessUp,                spawn,          LIGHTINC(10) },
	{ 0,XF86XK_MonBrightnessDown,              spawn,          LIGHTDEC(10) },
	{ ShiftMask,XF86XK_MonBrightnessUp,        spawn,          LIGHTINC(1)  },
	{ ShiftMask,XF86XK_MonBrightnessDown,      spawn,          LIGHTDEC(1)  },
	{ MODKEY,            XK_bracketright,      spawn,          LIGHTINC(10) },
	{ MODKEY,            XK_bracketleft,       spawn,          LIGHTDEC(10) },
	{ MODKEY|ShiftMask,  XK_bracketright,      spawn,          LIGHTINC(1)  },
	{ MODKEY|ShiftMask,  XK_bracketleft,       spawn,          LIGHTDEC(1)  },

	{ ControlMask|ShiftMask,        XK_m,      spawn,          {.v = ffmerge } },
	{ ControlMask|ShiftMask,        XK_b,      spawn,          {.v = fffixfocus } },
	{ MODKEY,                       XK_r,      spawn,          CLIPLISTEN },

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
	{ MODKEY,                       XK_f,      togglestairs,   {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY,                       XK_o,      gototag,        {0} },
	{ MODKEY|ControlMask,           XK_t,      setlayout,      {.v = tile } },
	{ MODKEY|ControlMask,           XK_d,      setlayout,      {.v = stairs } },
	{ MODKEY|ControlMask,           XK_f,      setlayout,      {.v = monocle } },
	{ MODKEY|ControlMask,           XK_g,      setlayout,      {.v = gaplessgrid } },
	{ MODKEY|ControlMask,           XK_c,      setlayout,      {.v = centeredmaster } },
	{ MODKEY|ControlMask|ShiftMask, XK_c,      setlayout,      {.v = centeredfloatingmaster } },
	{ MODKEY|ControlMask,           XK_a,      setlayout,      {.v = bstack } },
	{ MODKEY|ControlMask,           XK_x,      setlayout,      {.v = dwindle } },

	{ MODKEY|ControlMask,           XK_k,      incnmaster,     {.i = +1 } },
	{ MODKEY|ControlMask,           XK_j,      incnmaster,     {.i = -1 } },

	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY,                       XK_0,      setlayout,      {.v = gaplessgrid } },
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
#pragma GCC diagnostic pop

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button2,        setlayout,      {.v = &layouts[0]} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[1]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = terminal } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkRootWin,           MODKEY,         Button4,        focusstack,     {.i = -1 } },
	{ ClkRootWin,           MODKEY,         Button5,        focusstack,     {.i = +1 } },
	{ ClkClientWin,         MODKEY,         Button4,        focusstack,     {.i = -1 } },
	{ ClkClientWin,         MODKEY,         Button5,        focusstack,     {.i = +1 } },
	{ ClkStatusText,        MODKEY,         Button1,        spawn,          {.v = mute } },
	{ ClkStatusText,        MODKEY,         Button3,        spawn,          {.v = cycle } },
	{ ClkStatusText,        MODKEY,         Button4,        spawn,          VOLINC(5) },
	{ ClkStatusText,        MODKEY,         Button5,        spawn,          VOLDEC(5) },
};

// vim:noexpandtab
