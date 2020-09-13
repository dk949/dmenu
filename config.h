/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;                      /* -b  option; if 0, dmenu appears at bottom     */
static double opacity = 1.0;                /* -o  option; defines alpha translucency        */

static int centered = 0;                    /* -c option; centers dmenu on screen */
static int min_width = 1500;                    /* minimum width when centered */

/* -fn option overrides fonts[0]; default X11 font or font set */
static const char *fonts[] = {
	"monospace:size=10"
};
static const char *prompt      = NULL;      /* -p  option; prompt to the left of input field */

static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const unsigned int alphas[][3]      = {
	/*               fg      bg        border     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha },
};


static const char * const foreground  = "#F8F8F2";
static const char * const dull_white   = "#E6E6D1";
static const char * const background   = "#101421";
static const char * const dull_magenta = "#FF46B0";
static const char * const dark_magenta = "#FF79C6";
static const char * const black        = "#000000";


static const char *colors[SchemeLast][2] = {
	/*                              fg              bg       */
	[SchemeNorm]             = { dull_white,   background  }, // dull white    / dark purple
    [SchemeNormHighlight]    = { dark_magenta, background  }, // dark magenta  / dark purple

	[SchemeSel]              = { background,   foreground  }, // dark purple   / bright white
    [SchemeSelHighlight]     = { dull_magenta, foreground  }, // dull magenta  / bright white


	[SchemeOut]              = { black,        background  },
};
/* -l option; if nonzero, dmenu uses vertical list with given number of lines */
static unsigned int lines      = 0;
static unsigned int lineheight = 0;         /* -h option; minimum height of a menu line     */

/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " ";

/* Size of the window border */
static unsigned int border_width = 0;

/*
 * Use prefix matching by default; can be inverted with the -x flag.
 */
static int use_prefix = 1;
