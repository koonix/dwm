/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "st"
#define TERMCLASS "st-256color"

/* appearance */
static const unsigned int stairpx   = 50;       /* depth of stairs layout */
static const unsigned int borderpx  = 6;        /* border pixel of windows */
static const unsigned int snap      = 20;       /* snap pixel */
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
static const unsigned int gappih    = 20;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 20;       /* vert inner gap between windows */
static const unsigned int gappoh    = 20;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 20;       /* vert outer gap between windows and screen edge */
static       int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const unsigned char xkblayout = 0;       /* the default keyboard layout number (starts from 0) */

/* the default input block time of new windows (in milliseconds).
 * see the rules below for explanation. */
static const unsigned int blockinputmsec = 500;

/* fonts */
static const char *fonts[] = {
	"peep:size=12",
	"Vazirmatn NL:size=10",
	"Symbols Nerd Font:size=10",
	"JoyPixels:size=16",
};

/* colors */
static const char col_null[]       = "#000000";
static const char col_bg[]         = "#000000";
static const char col_normfg[]     = "#555555";
static const char col_normborder[] = "#333333";
static const char col_selfg[]      = "#bfbfbf";
static const char col_selborder[]  = "#a8a8a8";
static const char col_titlefg[]    = "#9d9d9d";
static const char col_urgborder[]  = "#ff0000";
static const char col_status[]     = "#9d9d9d";

static const char *colors[][4]      = {
	/*               fg        bg   border    */
	[SchemeNorm]  = { col_normfg,  col_bg,  col_normborder },
	[SchemeSel]   = { col_selfg,   col_bg,  col_selborder  },
	[SchemeTitle] = { col_titlefg, col_bg,  col_null       },
	[SchemeUrg]   = { col_normfg,  col_bg,  col_urgborder  },
};

/* colors that can be used by the statusbar */
static const char *statuscolors[] = { col_normfg, col_status };

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* helper macros for rules */
#define SYMBOL(c, s)     { c, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, -1, s }
#define TSYMBOL(t, s) { TERMCLASS, NULL, t, 0, 0, 0, 0, 0, 1, 0, -1, s }

/* hint for rules
 *
 * blockinput:
 *   sometimes you're typing and all of a sudden a window pops out of nowhere,
 *   steals yours input focus and receives some bogus input. this rule is here to stop that.
 *   this rule blocks the input of newly opened windows for the specified amount of time in milliseconds.
 *   set to 0 to disable, set to a negative value to use the default value (blockinputmsec).
 *   swallowing windows won't be input-blocked because they don't steal the focus.
 *
 * sametagid, sametagchildof:
 *   with these rules you can have some windows open in the same tag and monitor
 *   as other windows. ex. if window A has a sametagid of 10, and window B has a
 *   sametagchildof of 10, window B will be opened next to window A.
 *   can be an integer between 0 and 127. a value of zero disables this feature.
 *
 * clientsymbol:
 *   the string that represents the window in clientsymbols.
 *
 *  xprop(1):
 *    WM_CLASS(STRING) = instance, class
 *    WM_NAME(STRING) = title
 */
static const Rule rules[] = {
	/* class, instance, title, tags mask, isfloating, blockinput, sametagid, sametagchildof, isterminal, noswallow, monitor, clientsymbol */
	{ "TelegramDesktop", "telegram-desktop", "Media viewer", 0,  1,  0,  0,  0,  0,  0,  -1, NULL }, /* don't tile telegram's media viewer */
	{ "Qalculate-gtk", NULL, NULL,                           0,  1, -1,  0,  0,  0,  0,  -1, ""  }, /* don't tile qalculate */
	{ "Safeeyes", "safeeyes", "safeeyes",                    0,  1,  0,  0,  0,  0,  0,  -1, NULL }, /* don't tile safeeyes */
	{ ".exe", NULL, NULL,                                    0,  0, -1,  1,  1,  0,  0,  -1, ""  }, /* spawn wine programs next to eachother */
	{ "firefox", NULL, NULL,                                 0,  0,  0,  0,  0,  0,  0,  -1, ""  }, /* don't block firefox's input */
	/* swallowing rules: */
	{ TERMCLASS, NULL, NULL,                                 0,  0,  0,  0,  0,  1,  0,  -1, "" },
	{ NULL, NULL, "Event Tester",                            0,  0,  0,  0,  0,  0,  1,  -1, NULL  },
	/* symbol rules: */
	TSYMBOL("vim", ""),             TSYMBOL("lf", ""),             TSYMBOL("bashmount", ""),
	TSYMBOL("newsboat", ""),        TSYMBOL("ncmpcpp", ""),        TSYMBOL("calculator", ""),
	TSYMBOL("pulsemixer", "蓼"),     TSYMBOL("aria2p", ""),         TSYMBOL("tremc", ""),
	TSYMBOL("man", "ﲉ"),
	SYMBOL("TelegramDesktop", ""),  SYMBOL("mpv", ""),             SYMBOL("Zathura", ""),
	SYMBOL("Foliate", ""),          SYMBOL("Sxiv", ""),
};

/* layout(s) */
static const float mfact        = 0.5;  /* factor of master area size [0.05..0.95] */
static const int nmaster        = 1;    /* number of clients in master area */
static const int resizehints    = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0;    /* 1 will force focus on the fullscreen window */

/* hint for attachdirection
 * attach:
 *   the default behavior; new clients go in the master area.
 * attachabove:
 *   make new clients attach above the selected client,
 *   instead of always becoming the new master. this behavior is known from xmonad.
 * attachaside:
 *   make new clients get attached and focused in the stacking area,
 *   instead of always becoming the new master. it's basically an attachabove modification.
 * attachbelow:
 *   make new clients attach below the selected client,
 *   instead of always becoming the new master. inspired heavily by attachabove.
 * attachbottom:
 *   new clients attach at the bottom of the stack instead of the top.
 * attachtop:
 *   new client attaches below the last master/on top of the stack.
 *   behavior feels very intuitive as it doesn't disrupt existing masters,
 *   no matter the amount of them, it only pushes the clients in stack down.
 *   in case of nmaster = 1 feels like attachaside. */
static void (*attachdirection)(Client *c) = attachbelow;

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
#define Mod           Mod4Mask
#define ModShift      Mod|ShiftMask
#define ModCtrl       Mod|ControlMask
#define ModAlt        Mod|Mod1Mask
#define ModCtrlShift  ModCtrl|ShiftMask
#define ModAltShift   ModAlt|ShiftMask
#define ModAltCtrl    ModAlt|ControlMask
#define ModCtrlAlt    ModAltCtrl
#define CtrlShift     ControlMask|ShiftMask
#define AltCtrl       Mod1Mask|ControlMask
#define AltCtrlShift  AltCtrl|ShiftMask

/* key pair definition */
#define K1(k1, k2) k1
#define K2(k1, k2) k2
#define KP(mod, kp, fn, arg1, arg2) \
	{ mod, K1(kp), fn, arg1 }, \
	{ mod, K2(kp), fn, arg2 }

/* common key pairs */
#define KP_JK            XK_j, XK_k
#define KP_HL            XK_h, XK_l
#define KP_COMMAPERIOD   XK_comma, XK_period
#define KP_BRACKET       XK_bracketleft, XK_bracketright
#define KP_VOL           XF86XK_AudioLowerVolume,  XF86XK_AudioRaiseVolume
#define KP_BRIGHTNESS    XF86XK_MonBrightnessDown, XF86XK_MonBrightnessUp

#define TAGKEYS(KEY,TAG) \
	{ Mod,              KEY,      view,           {.ui = 1 << TAG} }, \
	{ ModCtrl,          KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ ModShift,         KEY,      tag,            {.ui = 1 << TAG} }, \
	{ ModCtrlShift,     KEY,      toggletag,      {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define CMD(...)   { .v = (const char*[]){ __VA_ARGS__, NULL } }
#define TUI(...)   { .v = (const char*[]){ TERM, "-e", __VA_ARGS__, NULL } }
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }
#define SHTUI(cmd) { .v = (const char*[]){ TERM, "-e", "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-p", "Programs", NULL };

/* audio */
#define VOLINC(n) { .v = (const char*[]){ "pamixer", "--allow-boost", "-i", #n, NULL } }
#define VOLDEC(n) { .v = (const char*[]){ "pamixer", "--allow-boost", "-d", #n, NULL } }
#define MPCVOL(n) { .v = (const char*[]){ "mpc", "volume", #n, NULL } }
static const char *mute[] = { "pamixer", "-t", NULL };
static const char *cycle[] = { "pacycle", NULL };
/**/
#define TOGGLE_MIC_MUTE SHCMD("pacmd list-sources | grep -q 'muted: yes' && { " \
"pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} false && " \
"notify-send ' Mic Enabled.' -u low -h string:x-canonical-private-synchronous:togglemicmute ;:; } || { " \
"pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} true && " \
"notify-send ' Mic Muted.' -u low -h string:x-canonical-private-synchronous:togglemicmute ;:; }")
/**/

/* media */
#define MEDIA_PLAYPAUSE \
	SHCMD("mpc pause & f=${XDG_RUNTIME_DIR:?}/playpause p=$(playerctl -a status " \
	"-f '{{playerInstance}}	{{status}}' | grep -v '\\<mpd\\>' | grep Playing) && { " \
	"printf '%s\\n' \"$p\" >\"$f\"; playerctl -a pause; :;} || " \
	"cut -f1 \"$f\" | xargs -rL1 playerctl play -p")

#define MEDIACMD(a, b) \
	SHCMD("(mpc | grep -q '^\\[playing' && mpc " a ") & " \
	"playerctl -a status -f '{{playerInstance}}	{{status}}' | " \
	"grep Playing | cut -f1 | xargs -rL1 playerctl " b " -p")

#define MEDIA_NEXT MEDIACMD("next", "next")
#define MEDIA_PREV MEDIACMD("prev", "previous")
#define MEDIA_SEEK_FWD  MEDIACMD("seek +10", "position 10+")
#define MEDIA_SEEK_BACK MEDIACMD("seek -10", "position 10-")
/**/

/* backlight */
#define LIGHTINC(n) { .v = (const char*[]){ "light", "-A", #n, NULL } }
#define LIGHTDEC(n) { .v = (const char*[]){ "light", "-U", #n, NULL } }

/* other */
#define NOTIFYSONG SHCMD("notify-send -u low -h string:x-canonical-private-synchronous:notifysong Playing: \"$(mpc current)\"")
#define XMOUSELESS SHCMD("usv down unclutter; xmouseless; usv up unclutter")
#define KEY(a, ...) { .v = (const char*[]){ "xdotool", "keyup", a, "key", "--clearmodifiers", __VA_ARGS__, NULL } }
#define KEYREP(a, b) KEY(a, b,b,b,b,b,b,b,b,b,b)

/* binding logic:
 * - audio and music related bindings start with super+alt
 * - layout bindigns start with super+control
 * - most bindings that have a similar function only differ in the shift key */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static Key keys[] = {
    /* modifier                     key        function        argument */
    { Mod,              XK_p,           spawn,          CMD("gimme", "-m") },
    { ModShift,         XK_p,           spawn,          {.v = dmenucmd } },
    { Mod,              XK_t,           spawn,          CMD(TERM) },
    { ModShift,         XK_t,           spawn,          CMD("cwdrun", TERM) },
    { Mod,              XK_b,           spawn,          SHCMD("exec $BROWSER") },
    { ModShift,         XK_b,           spawn,          CMD("ffdo") },
    { Mod,              XK_g,           spawn,          XMOUSELESS },
    { ControlMask,      XK_space,       spawn,          CMD("dunstctl", "close") },
    { ControlMask,      XK_grave,       spawn,          CMD("dunstctl", "history-pop") },
    { CtrlShift,        XK_period,      spawn,          CMD("dunstctl", "context") },
    { Mod,              XK_c,           spawn,          TUI("calc") },
    { Mod,              XK_v,           spawn,          SHTUI("exec ${EDITOR:-nvim}") },
    { Mod,              XK_q,           spawn,          CMD("sysact") },
    { Mod,              XK_e,           spawn,          CMD("loginctl", "lock-session") },
    { ModShift,         XK_e,           spawn,          CMD("sysact", "sleep") },
    { Mod,              XK_i,           spawn,          CMD("bm", "-m") },
    { ModShift,         XK_i,           spawn,          CMD("cwds") },
    { ModCtrl,          XK_p,           spawn,          CMD("dpass") },
    { ModAlt,           XK_F4,          quit,           {0} },

  KP( 0,                KP_VOL,         spawn,          VOLDEC(5),   VOLINC(5)   ),
  KP( ShiftMask,        KP_VOL,         spawn,          VOLDEC(20),  VOLINC(20)  ),
  KP( ModAlt,           KP_JK,          spawn,          VOLDEC(5),   VOLINC(5)   ),
  KP( ModAltShift,      KP_JK,          spawn,          VOLDEC(20),  VOLINC(20)  ),
  KP( ModAltCtrl,       KP_JK,          spawn,          MPCVOL(-10), MPCVOL(+10) ),
    { ModAlt,           XK_m,           spawn,          {.v = mute } },
    { ModAltShift,      XK_m,           spawn,          TOGGLE_MIC_MUTE },
    { ModCtrl,          XK_s,           spawn,          {.v = cycle } },
    { 0,        XF86XK_AudioMute,       spawn,          {.v = mute } },
    { 0,        XF86XK_AudioMicMute,    spawn,          TOGGLE_MIC_MUTE },

    { ModAltShift,      XK_p,           spawn,          CMD("mpc", "toggle") },
    { ModAlt,           XK_p,           spawn,          MEDIA_PLAYPAUSE },
  KP( ModAlt,           KP_HL,          spawn,          MEDIA_SEEK_BACK, MEDIA_SEEK_FWD ),
  KP( ModAltShift,      KP_HL,          spawn,          MEDIA_PREV,      MEDIA_NEXT     ),
    { ModAlt,           XK_n,           spawn,          NOTIFYSONG },
    { 0,        XF86XK_AudioPlay,       spawn,          MEDIA_PLAYPAUSE },
    { 0,        XF86XK_AudioPrev,       spawn,          MEDIA_PREV },
    { 0,        XF86XK_AudioNext,       spawn,          MEDIA_NEXT },

  KP( 0,                KP_BRIGHTNESS,  spawn,          LIGHTINC(10), LIGHTDEC(10) ),
  KP( ShiftMask,        KP_BRIGHTNESS,  spawn,          LIGHTINC(1),  LIGHTDEC(1)  ),
  KP( Mod,              KP_BRACKET,     spawn,          LIGHTINC(10), LIGHTDEC(10) ),
  KP( ModShift,         KP_BRACKET,     spawn,          LIGHTINC(1),  LIGHTDEC(1)  ),

    { Mod,              XK_r,           spawn,          CMD("pipeurl", "-c", "ask") },
    { ModShift,         XK_r,           spawn,          CMD("pipeurl", "history") },

  KP( Mod,              KP_JK,          focusstack,     {.i = +1 },    {.i = -1} ),
  KP( ModShift,         KP_JK,          push,           {.i = +1 },    {.i = -1} ),
  KP( Mod,              KP_HL,          setmfact,       {.f = -0.05 }, {.f = +0.05 } ),
  KP( Mod,              KP_HL,          setcfact,       {.f = -0.25 }, {.f = +0.25 } ),
    { ModShift,         XK_o,           setcfact,       {.f =  0.00 } },
    { Mod,              XK_s,           switchcol,      {0} },
    { Mod,              XK_space,       zoom,           {0} },
    { ModShift,         XK_space,       transfer,       {0} },
    { Mod,              XK_u,           view,           {0} },
    { Mod,              XK_u,           gotourgent,     {0} },
    { Mod,              XK_w,           killclient,     {0} },
    { ModCtrl,          XK_b,           togglebar,      {0} },
    { Mod,              XK_f,           togglefullscr,  {0} },
    { Mod,              XK_o,           gototag,        {0} },
    { ModCtrl,          XK_t,           setlayout,      {.v = tile } },
    { ModCtrl,          XK_d,           setlayout,      {.v = stairs } },
    { ModCtrl,          XK_f,           setlayout,      {.v = monocle } },
    { ModCtrl,          XK_g,           setlayout,      {.v = gaplessgrid } },
    { ModCtrl,          XK_c,           setlayout,      {.v = centeredmaster } },
    { ModCtrlShift,     XK_c,           setlayout,      {.v = centeredfloatingmaster } },
    { ModCtrl,          XK_a,           setlayout,      {.v = bstack } },
    { ModCtrl,          XK_x,           setlayout,      {.v = dwindle } },
  /* KP( ModCtrl,                      KP_JK,     incnmaster,  {.i = +1 }, {.i = -1} ), */
  KP( AltCtrl,          KP_JK,          spawn,          KEY("j", "Down"),    KEY("k", "Up") ),
  KP( AltCtrl,          KP_HL,          spawn,          KEY("h", "Left"),    KEY("l", "Right") ),
  KP( AltCtrlShift,     KP_JK,          spawn,          KEYREP("j", "Down"), KEYREP("k", "Up") ),
  KP( AltCtrlShift,     KP_HL,          spawn,          KEYREP("h", "Left"), KEYREP("l", "Right") ),
    { AltCtrl,          XK_g,           spawn,          KEY("g", "Home") },
    { AltCtrlShift,     XK_g,           spawn,          KEY("g", "End") },
    { AltCtrl,          XK_b,           spawn,          KEY("b", "Page_Up") },
    { AltCtrl,          XK_f,           spawn,          KEY("f", "Page_Down") },

    { ModShift,         XK_f,           togglefloating, {0} },
    { Mod,              XK_0,           view,           {.ui = ~0 } },
    { Mod,              XK_0,           setlayout,      {.v = gaplessgrid } },
    { ModShift,         XK_0,           tag,            {.ui = ~0 } },

  KP( Mod,              KP_COMMAPERIOD, focusmon,       {.i = +1}, {.i = -1}),
  KP( ModShift,         KP_COMMAPERIOD, tagmon,         {.i = +1}, {.i = -1}),

    { Mod,              XK_comma,       focusmon,       {.i = -1 } },
    { Mod,              XK_period,      focusmon,       {.i = +1 } },
    { ModShift,         XK_comma,       tagmon,         {.i = -1 } },
    { ModShift,         XK_period,      tagmon,         {.i = +1 } },

    TAGKEYS(XK_1, 0), TAGKEYS(XK_2, 1), TAGKEYS(XK_3, 2),
    TAGKEYS(XK_4, 3), TAGKEYS(XK_5, 4), TAGKEYS(XK_6, 5),
    TAGKEYS(XK_7, 6), TAGKEYS(XK_8, 7), TAGKEYS(XK_9, 8),
};

/* macro for defining scroll actions for when the cursor is on any window or the root window */
#define ROOTSCROLL(mod, fn, arg_up, arg_down) \
	{ ClkRootWin,   mod, Button4, fn, arg_up }, \
	{ ClkRootWin,   mod, Button5, fn, arg_down }, \
	{ ClkClientWin, mod, Button4, fn, arg_up }, \
	{ ClkClientWin, mod, Button5, fn, arg_down }

/* same as above, but for clicking instead of scrolling */
#define ROOTCLICK(mod, fn, arg) \
	{ ClkRootWin,   mod, Button1, fn, arg }, \
	{ ClkClientWin, mod, Button1, fn, arg }

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,      0,      Button1,    setlayout,      {0} },
	{ ClkLtSymbol,      0,      Button2,    setlayout,      {.v = tile } },
	{ ClkLtSymbol,      0,      Button3,    setlayout,      {.v = stairs } },
	{ ClkWinTitle,      0,      Button2,    zoom,           {0} },
	{ ClkStatusText,    0,      Button2,    spawn,          CMD(TERM) },
	{ ClkClientWin,     Mod,    Button1,    movemouse,      {0} },
	{ ClkClientWin,     Mod,    Button2,    togglefloating, {0} },
	{ ClkClientWin,     Mod,    Button3,    resizemouse,    {0} },
	{ ClkTagBar,        0,      Button1,    view,           {0} },
	{ ClkTagBar,        0,      Button3,    toggleview,     {0} },
	{ ClkTagBar,        Mod,    Button1,    tag,            {0} },
	{ ClkTagBar,        Mod,    Button3,    toggletag,      {0} },
	{ ClkStatusText,    0,      Button1,    spawn,          {.v = mute } },
	{ ClkStatusText,    0,      Button3,    spawn,          {.v = cycle } },
	{ ClkStatusText,    0,      Button4,    spawn,          VOLINC(5) },
	{ ClkStatusText,    0,      Button5,    spawn,          VOLDEC(5) },
	{ ClkClientSymbol,  0,      Button4,    focusstack,     {.i = -1} },
	{ ClkClientSymbol,  0,      Button5,    focusstack,     {.i = +1} },

	ROOTSCROLL( Mod,        focusstacktiled,  {.i = -1},     {.i = +1}    ), /* super+scroll:          change focus */
	ROOTSCROLL( ModShift,   push,             {.i = -1},     {.i = +1}    ), /* super+shift+scroll:    push the focused window */
	ROOTSCROLL( ModCtrl,    setmfact,         {.f = +0.05},  {.f = -0.05} ), /* super+control+scroll:  change mfact */
	ROOTSCROLL( ModAlt,     setcfact,         {.f = +0.25},  {.f = -0.25} ), /* super+alt+scroll:      change cfact */
	ROOTCLICK(  ModAlt,     setcfact,         {.f = 0.00} ),                 /* super+alt+click:       reset cfact */
};
#pragma GCC diagnostic pop

// vim:noexpandtab
