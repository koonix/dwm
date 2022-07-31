/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "st"
#define TERMCLASS "st-256color"

/* appearance */
static const unsigned int borderpx          = 9; /* border pixel of windows */
static const unsigned int innerborderpx     = 3; /* inner border pixel of windows */
static const unsigned int innerborderoffset = 3; /* distance between inner border and window */
static const unsigned int gappx      = 5;   /* gaps between windows */
static const unsigned int snap       = 32;  /* snap pixel */
static const float mfact             = 0.5; /* factor of master area size [0.05..0.95] */
static const int nmaster             = 1;   /* number of clients in master area */
static const int showbar             = 1;   /* 0 means no bar */
static const int topbar              = 1;   /* 0 means bottom bar */
static const int resizehints         = 1;   /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen      = 0;   /* 1 will force focus on the fullscreen window */
static const int swallowfloating     = 0;   /* 1 means swallow floating windows by default */
static const unsigned char xkblayout = 0;   /* the default keyboard layout number (starts from 0) */

/* the stairs layout */
static const unsigned int stairpx    = 75;  /* depth of the stairs layout */
static const int stairsdirection     = 1;   /* 0: left-aligned, 1: right-aligned */
static const int stairssamesize      = 0;   /* 1 means shrink all the staired windows to the same size */

/* systray */
static const int showsystray             = 1;   /* 0 means no systray */
static const int systrayoffset           = -5;  /* systray offset from the edge */
static const unsigned int systrayspacing = 4;   /* systray spacing */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, 0: display systray on the last monitor */
static const unsigned int systrayonleft  = 0;   /* 0: systray in the right corner, >0: systray on left of status text */

/* if nmaster is bigger than this number, killing a focused master
 * will decrease nmaster by 1. set to a negative value to disable. */
static const int nmasterbias = 1;

/* the default input block time of new windows (in milliseconds).
 * see the rules below for explanation. */
static const unsigned int blockinputmsec = 1000;

/* fonts */
static const char *fonts[] = {
	"peep:size=12",
	"Vazirmatn NL:size=10",
	"Symbols Nerd Font:size=10",
};

/* colors */
static const char normfg[]    = "#555555";
static const char bgcol[]     = "#000000";
static const char bordercol[] = "#000000";
static const char textcol[]   = "#9d9d9d";
static const char *colors[][4] = {
	/*                    fg           bg       innerborder,  outerborder */
	[SchemeNorm]      = { normfg,      bgcol,  "#333333",     bordercol }, /* colors of normal (unselected) items, tags, and window borders */
	[SchemeSel]       = { "#bfbfbf",   bgcol,  "#ffffff",     bordercol }, /* colors of selected items, tags and and window borders */
	[SchemeTitle]     = { textcol,     bgcol,  NULL,          NULL      }, /* fg and bg color of the window title area in the bar */
	[SchemeUrg]       = { NULL,        NULL,   "#ff0000",     bordercol }, /* border color of urgent windows */
	[SchemeBlockNorm] = { NULL,        NULL,   "#004d00",     bordercol }, /* border color of unselected input-blocked windows */
	[SchemeBlockSel]  = { NULL,        NULL,   "#00b301",     bordercol }, /* border color of selected input-blocked windows */
};

/* colors that can be used by the statusbar */
static const char *statuscolors[] = { "#333333", textcol };

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* layout array. first entry is default. */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "[M]",      monocle },
	{ "[S]",      stairs },
	{ "><>",      NULL },    /* no layout function means floating behavior */
};

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
 * fixjump:
 *   some clients jump around every time they're focues or unfocued.
 *   by setting fixjump to 1, the x and y of the configure event of the client
 *   will be ignored, thus preventing it from dictating it's own window position.
 *
 * xprop(1):
 *    WM_CLASS(STRING) = instance, class
 *    WM_NAME(STRING) = title
 *
 * 1 = tagmask,   2 = isfloating,     3 = blockinput,
 * 4 = sametagid, 5 = sametagchildof, 6 = isterminal,
 * 7 = noswallow, 8 = fixjump,        9 = monitor      */
static const Rule rules[] = {
	 /* class, instance, title,                 1   2   3   4   5   6   7   8   9 */
	{ "TelegramDesktop", NULL, "Media viewer",  0,  1,  0,  0,  0,  0,  0,  0, -1 }, /* don't tile telegram's media viewer */
	{ "Qalculate-gtk", NULL, NULL,              0,  1, -1,  0,  0,  0,  0,  0, -1 }, /* don't tile qalculate */
	{ "Safeeyes", "safeeyes", "safeeyes",       0,  1,  0,  0,  0,  0,  0,  0, -1 }, /* don't tile safeeyes */
	{ ".exe", NULL, NULL,                       0,  0, -1,  1,  1,  0,  0,  0, -1 }, /* spawn wine programs next to each other */
	{ "Steam", NULL, NULL,                      0,  0, -1,  2,  2,  0,  0,  1, -1 }, /* spawn steam windows next to each other and fixjump it */
	{ "firefox", NULL, NULL,                    0,  0,  0,  0,  0,  0,  0,  0, -1 }, /* don't block firefox's input */
	{ "tabbed", NULL, NULL,                     0,  0,  0,  0,  0,  0,  0,  0, -1 }, /* don't block tabbed's input */
	{ TERMCLASS, NULL, NULL,                    0,  0,  0,  0,  0,  1,  0,  0, -1 }, /* set terminal's isterminal rule */
	{ NULL, NULL, "Event Tester",               0,  0,  0,  0,  0,  0,  1,  0, -1 }, /* don't swallow xev's window */
};

/* hint for attachdirection
 *
 * attach:
 *   the default behavior; new clients go in the master area.
 *
 * attachabove:
 *   make new clients attach above the selected client,
 *   instead of always becoming the new master. this behavior is known from xmonad.
 *
 * attachaside:
 *   make new clients get attached and focused in the stacking area,
 *   instead of always becoming the new master. it's basically an attachabove modification.
 *
 * attachbelow:
 *   make new clients attach below the selected client,
 *   instead of always becoming the new master. inspired heavily by attachabove.
 *
 * attachbottom:
 *   new clients attach at the bottom of the stack instead of the top.
 *
 * attachtop:
 *   new client attaches below the last master/on top of the stack.
 *   behavior feels very intuitive as it doesn't disrupt existing masters,
 *   no matter the amount of them, it only pushes the clients in stack down.
 *   in case of nmaster = 1 feels like attachaside. */
static void (*attachdirection)(Client *) = attachbelow;

/* ======== */
/* = KEYS = */
/* ======== */

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
#define K1(K1, K2) K1
#define K2(K1, K2) K2
#define KP(MOD, KP, FN, ARG1, ARG2) \
	{ MOD, K1(KP), FN, ARG1 }, \
	{ MOD, K2(KP), FN, ARG2 }

/* common key pairs */
#define KP_JK            XK_j, XK_k
#define KP_HL            XK_h, XK_l
#define KP_COMMAPERIOD   XK_comma, XK_period
#define KP_BRACKET       XK_bracketleft, XK_bracketright
#define KP_VOL           XF86XK_AudioLowerVolume,  XF86XK_AudioRaiseVolume
#define KP_BRIGHTNESS    XF86XK_MonBrightnessDown, XF86XK_MonBrightnessUp

#define TAGKEYS(KEY,TAG) \
	{ Mod,              KEY,    view,           {.ui = 1 << TAG} }, \
	{ ModCtrl,          KEY,    toggleview,     {.ui = 1 << TAG} }, \
	{ ModShift,         KEY,    tag,            {.ui = 1 << TAG} }, \
	{ ModCtrlShift,     KEY,    toggletag,      {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define CMD(...)   { .v = (const char*[]){ __VA_ARGS__, NULL } }
#define TUI(...)   { .v = (const char*[]){ TERM, "-e", __VA_ARGS__, NULL } }
#define SHCMD(CMD) { .v = (const char*[]){ "/bin/sh", "-c", CMD, NULL } }
#define SHTUI(CMD) { .v = (const char*[]){ TERM, "-e", "/bin/sh", "-c", CMD, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-p", "Programs", NULL };

/* audio and media */
#define VOL(dB) CMD("pactl", "set-sink-volume", "@DEFAULT_SINK@", #dB "dB")
#define MPCVOL(PERCENT) CMD("mpc", "volume", #PERCENT)
#define MUTE CMD("pamixer", "-t")
#define PACYCLE CMD("pacycle")

#define TOGGLE_MIC_MUTE \
	SHCMD("pacmd list-sources | grep -q 'muted: yes' && { " \
	"pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} false && " \
	"notify-send ' Mic Enabled.' -u low -h string:x-canonical-private-synchronous:togglemicmute; : ;} || { " \
	"pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} true && " \
	"notify-send ' Mic Muted.' -u low -h string:x-canonical-private-synchronous:togglemicmute; : ;}")

#define MEDIA_PLAYPAUSE \
	SHCMD("mpc pause & f=${XDG_RUNTIME_DIR:?}/playpause p=$(playerctl -a status " \
	"-f '{{playerInstance}}	{{status}}' | grep -v '\\<mpd\\>' | grep Playing) && { " \
	"printf '%s\\n' \"$p\" >\"$f\"; playerctl -a pause; : ;} || " \
	"cut -f1 \"$f\" | xargs -rL1 playerctl play -p")

#define MEDIACMD(MPC_CMD, PLAYERCTL_CMD) \
	SHCMD("(mpc | grep -q '^\\[playing' && mpc " MPC_CMD ") & " \
	"playerctl -a status -f '{{playerInstance}}	{{status}}' | " \
	"grep Playing | cut -f1 | xargs -rL1 playerctl " PLAYERCTL_CMD " -p")

#define MEDIA_NEXT MEDIACMD("next", "next")
#define MEDIA_PREV MEDIACMD("prev", "previous")
#define MEDIA_SEEK_FWD  MEDIACMD("seek +10", "position 10+")
#define MEDIA_SEEK_BACK MEDIACMD("seek -10", "position 10-")
/**/

/* backlight */
#define LIGHTINC(N) CMD("light", "-A", #N)
#define LIGHTDEC(N) CMD("light", "-U", #N)

/* other */
#define PIPEURL CMD("pipeurl", "--clipboard", "ask")
#define NOTIFYSONG SHCMD("notify-send -u low -h string:x-canonical-private-synchronous:notifysong Playing: \"$(mpc current)\"")
#define XMOUSELESS SHCMD("usv down unclutter; xmouseless; usv up unclutter")
#define SENDKEY(KEYUP, ...) CMD("xdotool", "keyup", KEYUP, "key", "--clearmodifiers", __VA_ARGS__)
#define TERMCWD SHCMD("cd \"$(xcwd)\" && " TERM)
#define LASTDL CMD("zsh", "-c", "termopen ~/Downloads/*(om[1])") /* open the last downloaded file */

/* binding logic:
 * - audio and music related bindings start with super+alt
 * - layout bindigns start with super+control
 * - most bindings that have a similar function only differ in the shift key */
static Key keys[] = {
  /*  modifier          key             function        argument */
    { Mod,              XK_p,           spawn,          CMD("stuff", "-m") },
    { ModShift,         XK_p,           spawn,          {.v = dmenucmd } },
    { Mod,              XK_t,           spawn,          CMD(TERM) },
    { ModShift,         XK_t,           spawn,          TERMCWD },
    { Mod,              XK_b,           spawn,          SHCMD("exec $BROWSER") },
    { Mod,              XK_g,           spawn,          XMOUSELESS },
    { ControlMask,      XK_space,       spawn,          CMD("dunstctl", "close") },
    { ControlMask,      XK_grave,       spawn,          CMD("dunstctl", "history-pop") },
    { CtrlShift,        XK_period,      spawn,          CMD("dunstctl", "context") },
    { Mod,              XK_v,           spawn,          SHTUI("exec ${EDITOR:-nvim}") },
    { Mod,              XK_q,           spawn,          CMD("sysact") },
    { Mod,              XK_e,           spawn,          CMD("loginctl", "lock-session") },
    { Mod,              XK_o,           spawn,          CMD("freq", "-m") },
    { ModCtrl,          XK_p,           spawn,          CMD("dpass") },
    { Mod,              XK_d,           spawn,          LASTDL },
    { ModAlt,           XK_F3,          restart,        {0} },
    { ModAlt,           XK_F4,          quit,           {0} },

  KP( 0,                KP_VOL,         spawn,          VOL(-3),   VOL(+3)   ),
  KP( ModAlt,           KP_JK,          spawn,          VOL(-3),   VOL(+3)   ),
  KP( ModAltCtrl,       KP_JK,          spawn,          MPCVOL(-10), MPCVOL(+10) ),
    { ModAlt,           XK_m,           spawn,          MUTE },
    { ModAltShift,      XK_m,           spawn,          TOGGLE_MIC_MUTE },
    { ModCtrl,          XK_s,           spawn,          PACYCLE },
    { 0,        XF86XK_AudioMute,       spawn,          MUTE },
    { 0,        XF86XK_AudioMicMute,    spawn,          TOGGLE_MIC_MUTE },

    { ModAltShift,      XK_p,           spawn,          CMD("mpc", "toggle") },
    { ModAlt,           XK_p,           spawn,          MEDIA_PLAYPAUSE },
  KP( ModAlt,           KP_HL,          spawn,          MEDIA_SEEK_BACK, MEDIA_SEEK_FWD ),
  KP( ModAltShift,      KP_HL,          spawn,          MEDIA_PREV,      MEDIA_NEXT     ),
    { ModAlt,           XK_n,           spawn,          NOTIFYSONG },
    { 0,        XF86XK_AudioPlay,       spawn,          MEDIA_PLAYPAUSE },
    { 0,        XF86XK_AudioPrev,       spawn,          MEDIA_PREV },
    { 0,        XF86XK_AudioNext,       spawn,          MEDIA_NEXT },

  KP( 0,                KP_BRIGHTNESS,  spawn,          LIGHTDEC(10), LIGHTINC(10) ),
  KP( ShiftMask,        KP_BRIGHTNESS,  spawn,          LIGHTDEC(1),  LIGHTINC(1)  ),
  KP( Mod,              KP_BRACKET,     spawn,          LIGHTDEC(10), LIGHTINC(10) ),
  KP( ModShift,         KP_BRACKET,     spawn,          LIGHTDEC(1),  LIGHTINC(1)  ),

    { Mod,              XK_r,           spawn,          PIPEURL },
    { ModShift,         XK_r,           spawn,          CMD("pipeurl", "history") },
    { Mod,              XK_y,           spawn,          CMD("qrsend") },

  KP( Mod,              KP_JK,          focusstack,     {.i = +1 },    {.i = -1} ),
  KP( ModShift,         KP_JK,          push,           {.i = +1 },    {.i = -1} ),
  KP( Mod,              KP_HL,          setmfact,       {.f = -0.05 }, {.f = +0.05 } ),
    { Mod,              XK_s,           switchcol,      {0} },
    { Mod,              XK_space,       zoom,           {0} },
    { ModShift,         XK_space,       transfer,       {0} },
    { Mod,              XK_Tab,         view,           {0} },
    { Mod,              XK_u,           gotourgent,     {0} },
    { Mod,              XK_w,           killclient,     {0} },
    { ModCtrl,          XK_b,           togglebar,      {0} },
    { Mod,              XK_f,           togglefullscr,  {0} },
    { Mod,              XK_m,           setlayout,      {.lt = tile } },
    { ModShift,         XK_m,           setlayout,      {.lt = monocle } },
    { Mod,              XK_semicolon,   setlayout,      {.lt = stairs } },
  KP( ModCtrl,          KP_JK,          incnmaster,     {.i = -1 }, {.i = +1} ),
    { ModShift,         XK_f,           togglefloating, {0} },
    { Mod,              XK_0,           view,           {.ui = ~0 } },
    { ModShift,         XK_0,           tag,            {.ui = ~0 } },

    { Mod,              XK_n,           adjacent,       {.adj = view } },
    { ModCtrl,          XK_n,           adjacent,       {.adj = toggleview } },
    { ModShift,         XK_n,           adjacent,       {.adj = tag } },
    { ModCtrlShift,     XK_n,           adjacent,       {.adj = toggletag } },

  KP( Mod,              KP_COMMAPERIOD, focusmon,       {.i = +1}, {.i = -1} ),
  KP( ModShift,         KP_COMMAPERIOD, tagmon,         {.i = +1}, {.i = -1} ),
    { Mod,              XK_comma,       focusmon,       {.i = -1 } },
    { Mod,              XK_period,      focusmon,       {.i = +1 } },
    { ModShift,         XK_comma,       tagmon,         {.i = -1 } },
    { ModShift,         XK_period,      tagmon,         {.i = +1 } },

    TAGKEYS(XK_1, 0), TAGKEYS(XK_2, 1), TAGKEYS(XK_3, 2),
    TAGKEYS(XK_4, 3), TAGKEYS(XK_5, 4), TAGKEYS(XK_6, 5),
    TAGKEYS(XK_7, 6), TAGKEYS(XK_8, 7), TAGKEYS(XK_9, 8),
};

/* macro for defining scroll actions for when the cursor is on any window or the root window */
#define GLOBALSCROLL(MOD, FN, ARG_UP, ARG_DOWN) \
	{ ClkRootWin,   MOD, Button4, FN, ARG_UP }, \
	{ ClkRootWin,   MOD, Button5, FN, ARG_DOWN }, \
	{ ClkClientWin, MOD, Button4, FN, ARG_UP }, \
	{ ClkClientWin, MOD, Button5, FN, ARG_DOWN }

/* same as above, but for clicking instead of scrolling */
#define GLOBALCLICK(MOD, FN, ARG) \
	{ ClkRootWin,   MOD, Button1, FN, ARG }, \
	{ ClkClientWin, MOD, Button1, FN, ARG }

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,      0,        Button1,    setlayout,      {0} },
	{ ClkLtSymbol,      0,        Button2,    setlayout,      {.lt = tile } },
	{ ClkLtSymbol,      0,        Button3,    setlayout,      {.lt = stairs } },
	{ ClkWinTitle,      0,        Button1,    spawn,          PIPEURL },
	{ ClkStatusText,    0,        Button2,    spawn,          CMD(TERM) },
	{ ClkClientWin,     Mod,      Button1,    movemouse,      {0} },
	{ ClkClientWin,     Mod,      Button2,    togglefloating, {0} },
	{ ClkClientWin,     Mod,      Button3,    resizemouse,    {0} },
	{ ClkTagBar,        0,        Button1,    view,           {0} },
	{ ClkTagBar,        0,        Button3,    toggleview,     {0} },
	{ ClkTagBar,        Mod,      Button1,    tag,            {0} },
	{ ClkTagBar,        Mod,      Button3,    toggletag,      {0} },
	{ ClkStatusText,    0,        Button1,    spawn,          MUTE },
	{ ClkStatusText,    0,        Button3,    spawn,          PACYCLE },
	{ ClkStatusText,    0,        Button4,    spawn,          VOL(+3) },
	{ ClkStatusText,    0,        Button5,    spawn,          VOL(-3) },

	GLOBALSCROLL( Mod,        focusstacktiled,  {.i = -1},     {.i = +1}    ), /* super+scroll:          change focus */
	GLOBALSCROLL( ModShift,   push,             {.i = -1},     {.i = +1}    ), /* super+shift+scroll:    push the focused window */
	GLOBALSCROLL( ModCtrl,    setmfact,         {.f = +0.05},  {.f = -0.05} ), /* super+control+scroll:  change mfact */
};

// vim:noexpandtab
