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


static const char *colors[SchemeLast][2] = {
	/*                              fg         bg       */
	[SchemeNorm]             = { "#E6E6D1", "#101421" }, // dull white    / dark purple
	[SchemeSel]              = { "#101421", "#F8F8F2" }, // dark purple   / bright white
    [SchemeSelHighlight]     = { "#FF46B0", "#F8F8F2" }, // dull magenta  / bright white
    [SchemeNormHighlight]    = { "#FF79C6", "#101421" }, // dark magenta  / dark purple
	[SchemeOut]              = { "#000000", "#101421" },
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
