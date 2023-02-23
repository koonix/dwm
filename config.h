/* See LICENSE file for copyright and license details. */

/* terminal */
#define TERM "st"
#define TERMCLASS "st-256color"

/* border */
static const int borderpx             = 7;  /* border pixel of windows */
static const int innerborderpx        = 3;  /* inner border pixel of windows */
static const int innerborderoffsetpx  = 2;  /* distance between inner border and window */

/* layout settings */
static const int pertag            = 1;    /* 1 means remember layout, mfact and nmaster per tag */
static const int resettag          = 1;    /* 1 means reset layout, mfact and nmaster when tag is emptied */
static const int gappx             = 15;   /* gaps between windows */
static const float mfact           = 0.5;  /* factor of master area size [0.05..0.95] */
static const int nmaster           = 1;    /* number of clients in master area */
static const int nmasterbias       = 1;    /* reduce nmaster if masters are removed when nmaster > nmasterbias; <0 to disable */
static const int stairpx           = 75;   /* depth of the stairs layout */
static const int stairsdirection   = 1;    /* alignment of the stairs layout; 0: left-aligned, 1: right-aligned */
static const int stairssamesize    = 0;    /* 1 means shrink all the staired windows to the same size */

/* bar and systray */
static const int showbar          = 1;     /* 0 means no bar */
static const int topbar           = 1;     /* 0 means bottom bar */
static const float barheightfact  = 1.30;  /* multiply bar height by this value */
static const int showsystray      = 1;     /* 0 means no systray */
static const int statusmonnum     = -1;    /* monitor number to show status and systray on; <0 means follow selected monitor */
static const float cindfact       = 0.1;   /* size of client indicators relative to font height */

/* other settings */
static const int snap                 = 32;  /* snap pixel */
static const int lockfullscreen       = 0;   /* 1 will force focus on the fullscreen window */
static const int swallowfloating      = 0;   /* 1 means swallow floating windows as well */
static const int resizehints          = 1;   /* 1 means respect size hints in tiled resizals */
static const int hintcenter           = 1;   /* 1 means center size-hinted windows in their tiled resizal */
static const unsigned char xkblayout  = 0;   /* the default keyboard layout number; 0 is the main layout */
static const int noautofocus          = 1;   /* the default noautofocus setting; see the noautofocus rule below */
static const int allowcolorfonts      = 1;   /* wether to use color fonts (eg. emoji fonts) in the bar */

/* fonts */
static const FontDef fontdefs[] = {
    /* font name                        favoring unicode block */
    { "Signika Negative:size=13",       UnicodeGeneric  },
    { ":lang=fa:size=13",               UnicodeFarsi    },
    { "Symbols Nerd Font:size=10",      UnicodeNerd     },
    { "JoyPixels:size=12",              UnicodeEmoji    },
};

/* colors */
static const char normfg[]    = "#666666";
static const char bgcolor[]   = "#181b1c";
static const char borderbg[]  = "#000000";
static const char textcolor[] = "#bfbfbf";
static const char *colors[SchemeLast][ColorLast] = {
    /*                    FG           BG        Border       BorderBG */
    [SchemeNorm]       = { normfg,      bgcolor,  "#333333",   borderbg }, /* colors of normal (unselected) items, tags, and window borders */
    [SchemeSel]        = { "#bfbfbf",   bgcolor,  "#ffffff",   borderbg }, /* colors of selected items, tags and and window borders */
    [SchemeUrg]        = { NULL,        NULL,     "#993333",   borderbg }, /* border color of urgent windows */
    [SchemeTitle]      = { textcolor,   bgcolor,  NULL,        NULL     }, /* fg and bg color of the window title area in the bar */
    [SchemeStatus]     = { "#999999",   bgcolor,  NULL,        NULL     }, /* fg and bg color of statusbar */
    [SchemeStatusSep]  = { "#333333",   bgcolor,  NULL,        NULL     }, /* fg and bg color of statusbar separator characters */
    [SchemeWinButton]  = { "#993333",   NULL,     "#000000",   NULL     }, /* fg and bg color of statusbar separator characters */
};

/* statusbar module separator characters */
static const char statusseparators[]  = { '|' };

/* tags */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* layout array. first entry is default. */
static const Layout layouts[] = {
    /* symbol     arrange function */
    { "[]=",      tile    },
    { "[M]",      monocle },
    { "[]/",      stairs  },
};

/* hint for rules
 *
 * completefullscreen:
 *   1 means make the window cover the entire monitor (even the bar) when
 *   it's fullscreen.
 *
 * noautofocus:
 *   1 means don't focus the window initially,
 *   0 means default behavior (autofocus).
 *
 * nojitter:
 *   some clients jump around every time their focue changes because they send
 *   ConfigureRequest events with incorrect coordinates.
 *   by setting nojitter to 1, the x and y of the ConfigureRequest of the client
 *   will be ignored, preventing it from dictating it's own window position.
 *
 * xprop(1):
 *    WM_CLASS(STRING) = instance, class
 *    WM_NAME(STRING) = title
 *
 * flt : isfloating
 * cfs : completefullscreen
 * naf : noautofocus
 * nsl : noswallow
 * ist : isterminal
 * njt : nojitter
 * tag : tagmask
 * mon : monitor
 */
static const Rule rules[] = {
    /* class, instance, title,                 flt cfs naf nsl ist njt tag mon  */
    { "TelegramDesktop", NULL, NULL,            0,  0,  0,  0,  0,  0,  0, -1 },
    { "TelegramDesktop", NULL, "Media viewer",  1,  1,  0,  0,  0,  0,  0, -1 },
    { "Qalculate", NULL, NULL,                  1,  0,  0,  0,  0,  0,  0, -1 },
    { "Droidcam", NULL, NULL,                   1,  0,  0,  0,  0,  0,  0, -1 },
    { ".exe", NULL, NULL,                       0,  0,  1,  0,  0,  0,  0, -1 },
    { "Steam", NULL, NULL,                      0,  0,  1,  0,  0,  1,  0, -1 },
    { "firefox", NULL, NULL,                    0,  0,  0,  0,  0,  0,  0, -1 },
    { "firefox", NULL, "Picture-in-Picture",    0,  1,  0,  0,  0,  0,  0, -1 },
    { "chromium", NULL, NULL,                   0,  0,  0,  0,  0,  0,  0, -1 },
    { "tabbed", NULL, NULL,                     0,  0,  0,  0,  0,  0,  0, -1 },
    { "Sxiv", NULL, NULL,                       0,  1,  1,  0,  0,  0,  0, -1 },
    { "mpv", NULL, NULL,                        0,  1,  1,  0,  0,  0,  0, -1 },
    { "Pinentry", NULL, NULL,                   0,  0,  0,  0,  0,  0,  0, -1 },
    { TERMCLASS, NULL, NULL,                    0,  0,  0,  0,  1,  0,  0, -1 },
    { NULL, NULL, "Event Tester",               0,  0,  0,  1,  0,  0,  0, -1 },
};

/* hint for attachdirection
 *
 * attach:
 *   the default behavior; new clients become a master.
 *
 * attachabove:
 *   make new clients attach above the selected client.
 *   this behavior is known from xmonad.
 *
 * attachbelow:
 *   make new clients attach below the selected client.
 *
 * attachtop:
 *   new client attaches below the last master, on top of the stack.
 *   behavior feels very intuitive as it doesn't disrupt the existing masters.
 *
 * attachbottom:
 *   new clients attach at the bottom of the stack.
 */
static void (*attachdirection)(Client *) = attachbelow;

/* ==============
 * = Key Macros
 * ============== */

/* key definitions */
#define Mod           Mod4Mask
#define Shift         ShiftMask
#define Ctrl          ControlMask
#define Alt           Mod1Mask
#define ModShift      Mod|Shift
#define ModCtrl       Mod|Ctrl
#define ModAlt        Mod|Alt
#define ModCtrlShift  ModCtrl|Shift
#define ModAltShift   ModAlt|Shift
#define ModAltCtrl    ModAlt|Ctrl
#define ModCtrlAlt    ModAltCtrl
#define CtrlShift     Ctrl|Shift
#define AltCtrl       Alt|Ctrl
#define AltCtrlShift  AltCtrl|Shift

/* key pair definition */
#define K1(K1, K2) K1
#define K2(K1, K2) K2
#define PAIR(PAIR, MOD, FN, ARG1, ARG2) \
    { K1(PAIR), MOD, FN, ARG1 }, \
    { K2(PAIR), MOD, FN, ARG2 }

/* common key pairs */
#define PAIR_JK            XK_j, XK_k
#define PAIR_HL            XK_h, XK_l
#define PAIR_COMMAPERIOD   XK_comma, XK_period
#define PAIR_BRACKET       XK_bracketleft, XK_bracketright
#define PAIR_VOL           XF86XK_AudioLowerVolume,  XF86XK_AudioRaiseVolume
#define PAIR_BRIGHTNESS    XF86XK_MonBrightnessDown, XF86XK_MonBrightnessUp

#define TAGKEYS(KEY,TAG) \
    { KEY,  Mod,           view,        {.ui = 1 << TAG} }, \
    { KEY,  ModCtrl,       toggleview,  {.ui = 1 << TAG} }, \
    { KEY,  ModShift,      tag,         {.ui = 1 << TAG} }, \
    { KEY,  ModCtrlShift,  toggletag,   {.ui = 1 << TAG} }

/* ============
 * = Commands
 * ============ */

#define CMD(...)   { .v = (const char*[]){ __VA_ARGS__, NULL } }
#define TUI(...)   { .v = (const char*[]){ TERM, "-e", __VA_ARGS__, NULL } }
#define SHCMD(CMD) { .v = (const char*[]){ "/bin/sh", "-c", CMD, NULL } }
#define SHTUI(CMD) { .v = (const char*[]){ TERM, "-e", "/bin/sh", "-c", CMD, NULL } }

#define VOL(dB) CMD("pactl", "set-sink-volume", "@DEFAULT_SINK@", #dB "dB")
#define MPCVOL(PERCENT) CMD("mpc", "volume", #PERCENT)
#define MUTE CMD("pamixer", "-t")
#define PACYCLE CMD("pacycle")
#define MPC_TOGGLE CMD("mpc", "toggle")
#define MEDIA_PLAYPAUSE SHCMD("mpc pause & playerctl play-pause")

/* run media actions using both mpc and playerctl */
#define MEDIACMD(MPC_CMD, PLAYERCTL_CMD) \
    SHCMD("(mpc | grep -q '^\\[playing' && mpc " MPC_CMD ") & playerctl " PLAYERCTL_CMD)

#define MEDIA_NEXT MEDIACMD("next", "next")
#define MEDIA_PREV MEDIACMD("prev", "previous")
#define MEDIA_SEEK_FWD  MEDIACMD("seek +10", "position 10+")
#define MEDIA_SEEK_BACK MEDIACMD("seek -10", "position 10-")

#define TOGGLE_MIC_MUTE \
    SHCMD("pacmd list-sources | grep -q 'muted: yes' && { " \
    "pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} false && " \
    "notify-send ' Mic Enabled.' -u low -h string:x-canonical-private-synchronous:togglemicmute; : ;} || { " \
    "pactl list short sources | cut -f1 | xargs -I{} pacmd set-source-mute {} true && " \
    "notify-send ' Mic Muted.' -u low -h string:x-canonical-private-synchronous:togglemicmute; : ;}")

/* change the brightness of internal and external monitors */
#define LIGHTINC(N) SHCMD("light -A " #N "; monbrightness raise " #N)
#define LIGHTDEC(N) SHCMD("light -U " #N "; monbrightness lower " #N)

/* other */
#define XMOUSELESS SHCMD("usv down unclutter; xmouseless; usv up unclutter")
#define SENDKEY(KEYUP, ...) CMD("xdotool", "keyup", KEYUP, "key", "--clearmodifiers", __VA_ARGS__)
#define TERMCWD SHCMD("cd \"$(xcwd)\" && " TERM)
#define LASTDL CMD("zsh", "-c", "termopen ~/Downloads/*(om[1])") /* open the last downloaded file */

/* copy the clipboard contents to all running Xephyr instances */
#define COPYTOXEPHYR \
    SHCMD("confirm=$(printf 'No\\nYes\\n' | dmenu -p 'Copy Clipboard to all Xephyr instances?' " \
    "-nb '#222222' -nf '#aaaaaa' -sb '#52161e'); [ \"$confirm\" = Yes ] || exit; " \
    "xclip -o -selection clipboard -t TARGETS | grep -q image/png && target=image/png || unset target; " \
    "for dpy in $(pgrep -ax Xephyr | grep -o ' :[0-9]\\+'); do " \
    "xclip -o -r -selection clipboard ${target:+-t $target} | " \
    "DISPLAY=$dpy xclip -selection clipboard ${target:+-t $target}; done")

/* ============
 * = Bindings
 * ============ */

/* binding logic:
 * - audio and music related bindings start with super+alt (ModAlt)
 * - most bindings that have a similar function only differ by a shift modifier */
static const Key keys[] = {
  /*  modifier          key               function          argument */
    { XK_q,             Mod,              spawn,            CMD("sysact") },
    { XK_m,             Mod,              spawn,            CMD("manuals") },
    { XK_p,             Mod,              spawn,            CMD("stuff", "-m") },
    { XK_p,             ModShift,         spawn,            CMD("dmenu_run", "-p", "Programs") },
    { XK_t,             Mod,              spawn,            CMD(TERM) },
    { XK_t,             ModShift,         spawn,            TERMCWD },
    { XK_b,             Mod,              spawn,            SHCMD("exec $BROWSER") },
    { XK_g,             Mod,              spawn,            XMOUSELESS },
    { XK_n,             Mod,              spawn,            CMD("dunstctl", "close") },
    { XK_n,             ModShift,         spawn,            CMD("dunstctl", "action") },
    { XK_n,             ModCtrl,          spawn,            CMD("dunstctl", "history-pop") },
    { XK_v,             Mod,              spawn,            SHTUI("exec ${EDITOR:-nvim}") },
    { XK_e,             Mod,              spawn,            CMD("loginctl", "lock-session") },
    { XK_d,             Mod,              spawn,            TUI("dictfzf") },
    { XK_x,             Mod,              spawn,            COPYTOXEPHYR },
    { XK_q,             ModShift,         restart,          {0} },

PAIR( PAIR_VOL,         0,                spawn,            VOL(-3), VOL(+3) ),
PAIR( PAIR_JK,          ModAlt,           spawn,            VOL(-3), VOL(+3) ),
PAIR( PAIR_JK,          ModAltShift,      spawn,            MPCVOL(-10), MPCVOL(+10) ),
    { XK_m,             ModAlt,           spawn,            MUTE },
    { XK_m,             ModAltShift,      spawn,            TOGGLE_MIC_MUTE },
    { XK_s,             ModCtrl,          spawn,            PACYCLE },
    { XF86XK_AudioMute,     0,            spawn,            MUTE },
    { XF86XK_AudioMicMute,  0,            spawn,            TOGGLE_MIC_MUTE },

    { XK_p,             ModAltShift,      spawn,            MPC_TOGGLE },
    { XK_p,             ModAlt,           spawn,            MEDIA_PLAYPAUSE },
PAIR( PAIR_HL,          ModAlt,           spawn,            MEDIA_SEEK_BACK, MEDIA_SEEK_FWD ),
PAIR( PAIR_HL,          ModAltShift,      spawn,            MEDIA_PREV,      MEDIA_NEXT     ),
    { XF86XK_AudioPlay, 0,                spawn,            MEDIA_PLAYPAUSE },
    { XF86XK_AudioPrev, 0,                spawn,            MEDIA_PREV },
    { XF86XK_AudioNext, 0,                spawn,            MEDIA_NEXT },

PAIR( PAIR_BRIGHTNESS,  0,                spawn,            LIGHTDEC(10), LIGHTINC(10) ),
PAIR( PAIR_BRIGHTNESS,  Shift,            spawn,            LIGHTDEC(1),  LIGHTINC(1)  ),
PAIR( PAIR_BRACKET,     Mod,              spawn,            LIGHTDEC(10), LIGHTINC(10) ),
PAIR( PAIR_BRACKET,     ModShift,         spawn,            LIGHTDEC(1),  LIGHTINC(1)  ),

    { XK_r,             Mod,              spawn,            CMD("pipeurl", "--clipboard", "ask") },
    { XK_r,             ModShift,         spawn,            CMD("pipeurl", "history") },
    { XK_y,             Mod,              spawn,            CMD("qrsend") },

PAIR( PAIR_JK,          Mod,              focusstack,       {.i = +1 },    {.i = -1 } ),
PAIR( PAIR_JK,          ModShift,         push,             {.i = +1 },    {.i = -1 } ),
PAIR( PAIR_HL,          Mod,              setmfact,         {.f = -0.05 }, {.f = +0.05 } ),
    { XK_s,             Mod,              switchcol,        {0} },
    { XK_space,         Mod,              zoom,             {0} },
    { XK_space,         ModShift,         transfer,         {0} },
    { XK_Tab,           Mod,              view,             {0} },
    { XK_w,             ModShift,         killclient,       {0} },
    { XK_b,             ModCtrl,          togglebar,        {0} },
    { XK_f,             Mod,              togglefullscreen, {0} },
    { XK_semicolon,     Mod,              setlayout,        {.lt = tile } },
    { XK_semicolon,     ModShift,         setlayout,        {.lt = stairs } },
    { XK_semicolon,     ModCtrl,          setlayout,        {.lt = monocle } },
PAIR( PAIR_JK,          ModCtrl,          incnmaster,       {.i = -1 }, {.i = +1 } ),
    { XK_f,             ModShift,         togglefloating,   {0} },
    { XK_0,             Mod,              view,             {.ui = ~0U } },
    { XK_0,             ModShift,         tag,              {.ui = ~0U } },

PAIR( PAIR_COMMAPERIOD, Mod,              viewmon,          {.i = +1 }, {.i = -1 } ),
PAIR( PAIR_COMMAPERIOD, ModShift,         tagmon,           {.i = +1 }, {.i = -1 } ),
    { XK_comma,         Mod,              viewmon,          {.i = -1 } },
    { XK_period,        Mod,              viewmon,          {.i = +1 } },
    { XK_comma,         ModShift,         tagmon,           {.i = -1 } },
    { XK_period,        ModShift,         tagmon,           {.i = +1 } },

    TAGKEYS(XK_1, 0), TAGKEYS(XK_2, 1), TAGKEYS(XK_3, 2),
    TAGKEYS(XK_4, 3), TAGKEYS(XK_5, 4), TAGKEYS(XK_6, 5),
    TAGKEYS(XK_7, 6), TAGKEYS(XK_8, 7), TAGKEYS(XK_9, 8),
};

/* ========================
 * = Button Click Actions
 * ======================== */

/* button definitions */
static const Button buttons[] = {
    /* click area         button     modifier    function          argument */
    { ClickLtSymbol,      Button1,   0,          setlayout,        {.i = +2 } },
    { ClickLtSymbol,      Button3,   0,          setlayout,        {.i = -2 } },

    { ClickClientWin,     Button1,   Mod,        movemouse,        {0} },
    { ClickClientWin,     Button2,   Mod,        togglefloating,   {0} },
    { ClickClientWin,     Button3,   Mod,        resizemouse,      {0} },

    { ClickTagBar,        Button1,   0,          view,             {0} },
    { ClickTagBar,        Button3,   0,          toggleview,       {0} },
    { ClickTagBar,        Button1,   Shift,      tag,              {0} },
    { ClickTagBar,        Button3,   Shift,      toggletag,        {0} },
    { ClickTagBar,        Button4,   0,          cycleview,        {.i = +1 } },
    { ClickTagBar,        Button5,   0,          cycleview,        {.i = -1 } },

    { ClickWinArea,       Button4,   Mod,        focusstacktile,   {.i = -1 } },
    { ClickWinArea,       Button5,   Mod,        focusstacktile,   {.i = +1 } },
    { ClickWinArea,       Button4,   ModShift,   push,             {.i = -1 } },
    { ClickWinArea,       Button5,   ModShift,   push,             {.i = +1 } },
    { ClickWinArea,       Button4,   ModCtrl,    setmfact,         {.f = +0.05 } },
    { ClickWinArea,       Button5,   ModCtrl,    setmfact,         {.f = -0.05 } },

    { ClickWinButtonDouble,  Button1,   0,          killclient,       {0} },
    { ClickWinButton,        Button2,   0,          zoom,             {0} },
    { ClickWinButton,        Button3,   0,          togglefullscreen, {0} },
    { ClickWinButton,        Button4,   0,          push,             {.i = -1 } },
    { ClickWinButton,        Button5,   0,          push,             {.i = +1 } },
};

/* statusbar module click actions */
static const StatusClick statusclick[] = {
    /* module       button     modifier    function          argument */
    { "date",       Button1,   0,          spawn,            SHCMD("notify-send \"$(pcal -t)\"") },

    { "audio",      Button1,   0,          spawn,            MUTE },
    { "audio",      Button2,   0,          spawn,            TUI("pulsemixer") },
    { "audio",      Button3,   0,          spawn,            PACYCLE },
    { "audio",      Button4,   0,          spawn,            VOL(+3) },
    { "audio",      Button5,   0,          spawn,            VOL(-3) },

    { "music",      Button1,   0,          spawn,            MPC_TOGGLE },
    { "music",      Button3,   0,          spawn,            TUI("ncmpcpp") },
    { "music",      Button4,   0,          spawn,            MPCVOL(+10) },
    { "music",      Button5,   0,          spawn,            MPCVOL(-10) },

    { "network",    Button1,   0,          spawn,            CMD("networkmanager_dmenu") },
};

// vim:expandtab
