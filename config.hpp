/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar       = 1;    /* -b  option; if 0, dmenu appears at bottom    */
static int centered     = 0;    /* -c option; centers dmenu on screen           */
static int min_width    = 1500; /* minimum width when centered                  */

static const char *fonts[]  = {"monospace:size=10"}; /* -fn option overrides fonts[0] */
static const char *prompt   = NULL; /* -p  option */

static const char *const foreground     = "#F8F8F2";
static const char *const dull_white     = "#E6E6D1";
static const char *const background     = "#101421";
static const char *const dull_magenta   = "#FF46B0";
static const char *const dark_magenta   = "#FF79C6";
static const char *const black          = "#000000";


static const char *colors[SchemeLast][2] = {
    [SchemeNorm]            = {dull_white,      background},
    [SchemeNormHighlight]   = {dark_magenta,    background},
    [SchemeSel]             = {background,      foreground},
    [SchemeSelHighlight]    = {dull_magenta,    foreground},
    [SchemeOut]             = {black,           background},
};



static unsigned int lines           = 0; /* -l option */
static unsigned int columns         = 0; /* -g option */
static unsigned int lineheight      = 0; /* -h option */
static const char worddelimiters[]  = " ";
static unsigned int border_width    = 0; /* -bw option */
static int use_prefix               = 0; /* -x option */
