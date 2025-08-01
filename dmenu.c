/* See LICENSE file for copyright and license details. */

#include "dmenu.h"

#include "config.h"
#include "drw.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <X11/extensions/render.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

/* macros */
#define INTERSECT(x, y, w, h, r)                                            \
    (MAX(0, MIN((x) + (w), (r).x_org + (r).width) - MAX((x), (r).x_org)) && \
        MAX(0, MIN((y) + (h), (r).y_org + (r).height) - MAX((y), (r).y_org)))
#define LENGTH(X)        (sizeof X / sizeof X[0])
#define TEXTW(X)         (drw_fontset_getwidth(drw, (X)) + lrpad)
#define OPAQUE           0xffU
#define OPACITY          "_NET_WM_WINDOW_OPACITY"
#define NUMBERSMAXDIGITS 100
#define NUMBERSBUFSIZE   (NUMBERSMAXDIGITS * 2) + 1

struct item {
    char *text;
    struct item *left, *right;
    int out;
};

static const unsigned int baralpha = 0xFF;
static const unsigned int borderalpha = OPAQUE;
// clang-format off
static unsigned int alphas[SchemeLast][3] = {
  /*                          fg      bg        border     */
    [SchemeNorm]          = {OPAQUE, baralpha, borderalpha},
    [SchemeSel]           = {OPAQUE, baralpha, borderalpha},
    [SchemeNormHighlight] = {OPAQUE, baralpha, borderalpha},
    [SchemeSelHighlight]  = {OPAQUE, baralpha, borderalpha},
    [SchemeOut]           = {OPAQUE,   OPAQUE,      OPAQUE},
};
// clang-format on

static char numbers[NUMBERSBUFSIZE] = "";
static char text[BUFSIZ] = "";
static char *embed;
static int bh, mw, mh;
static int inputw = 0, promptw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static char **argv_items = NULL;
static struct item *items = NULL;
static struct item *matches, *matchend;
static struct item *prev, *curr, *next, *sel;
static int mon = -1, screen;

static Atom clip, utf8;
static Display *dpy;
static Window root, parentwin, win;
static XIC xic;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

static Drw *drw;
static Clr *scheme[SchemeLast];

static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;

static void appenditem(struct item *item, struct item **list, struct item **last) {
    if (*last)
        (*last)->right = item;
    else
        *list = item;

    item->left = *last;
    item->right = NULL;
    *last = item;
}

static void calcoffsets(void) {
    int i, n;

    if (lines > 0)
        n = lines * columns * bh;
    else
        n = mw - (promptw + inputw + TEXTW("<") + TEXTW(">"));
    /* calculate which items will begin the next page and previous page */
    for (i = 0, next = curr; next; next = next->right)
        if ((i += (lines > 0) ? bh : MIN(TEXTW(next->text), n)) > n)
            break;
    for (i = 0, prev = curr; prev && prev->left; prev = prev->left)
        if ((i += (lines > 0) ? bh : MIN(TEXTW(prev->left->text), n)) > n)
            break;
}

static int max_textw(void) {
    int len = 0;
    for (struct item *item = items; item && item->text; item++)
        len = MAX(TEXTW(item->text), len);
    return len;
}

static void cleanup(void) {
    size_t i;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < SchemeLast; i++)
        free(scheme[i]);
    drw_free(drw);
    XSync(dpy, False);
    XCloseDisplay(dpy);
}

static char *cistrstr(const char *s, const char *sub) {
    size_t len;

    for (len = strlen(sub); *s; s++)
        if (!strncasecmp(s, sub, len))
            return (char *)s;
    return NULL;
}

static void drawhighlights(struct item *item, int x, int y, int maxw) {
    char restorechar, tokens[sizeof text], *highlight, *token;
    int indentx, highlightlen;

    drw_setscheme(drw, scheme[item == sel ? SchemeSelHighlight : SchemeNormHighlight]);
    strcpy(tokens, text);
    for (token = strtok(tokens, " "); token; token = strtok(NULL, " ")) {
        highlight = fstrstr(item->text, token);
        while (highlight) {
            // Move item str end, calc width for highlight indent, & restore
            highlightlen = highlight - item->text;
            restorechar = *highlight;
            item->text[highlightlen] = '\0';
            indentx = TEXTW(item->text);
            item->text[highlightlen] = restorechar;

            // Move highlight str end, draw highlight, & restore
            restorechar = highlight[strlen(token)];
            highlight[strlen(token)] = '\0';
            if (indentx - (lrpad / 2) - 1 < maxw)
                drw_text(drw, x + indentx - (lrpad / 2) - 1, y, MIN(maxw - indentx, TEXTW(highlight) - lrpad), bh, 0, highlight, 0);
            highlight[strlen(token)] = restorechar;

            if (strlen(highlight) - strlen(token) < strlen(token))
                break;
            highlight = fstrstr(highlight + strlen(token), token);
        }
    }
}

static int drawitem(struct item *item, int x, int y, int w) {
    if (item == sel)
        drw_setscheme(drw, scheme[SchemeSel]);
    else if (item->out)
        drw_setscheme(drw, scheme[SchemeOut]);
    else
        drw_setscheme(drw, scheme[SchemeNorm]);

    int r = drw_text(drw, x, y, w, bh, lrpad / 2, item->text, 0);
    drawhighlights(item, x, y, w);
    return r;
}

static void recalculatenumbers(void) {
    unsigned int numer = 0, denom = 0;
    struct item *item;
    if (matchend) {
        numer++;
        for (item = matchend; item && item->left; item = item->left)
            numer++;
    }
    for (item = items; item && item->text; item++)
        denom++;
    snprintf(numbers, NUMBERSBUFSIZE, "%d/%d", numer, denom);
}

static void drawmenu(void) {
    unsigned int curpos;
    struct item *item;
    int x = 0, y = 0, fh = drw->fonts->h, w;

    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_rect(drw, 0, 0, mw, mh, 1, 1);

    if (prompt && *prompt) {
        drw_setscheme(drw, scheme[SchemeSel]);
        x = drw_text(drw, x, 0, promptw, bh, lrpad / 2, prompt, 0);
    }
    /* draw input field */
    w = (lines > 0 || !matches) ? mw - x : inputw;
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, text, 0);

    curpos = TEXTW(text) - TEXTW(&text[cursor]);
    if ((curpos += lrpad / 2 - 1) < w) {
        drw_setscheme(drw, scheme[SchemeNorm]);
        drw_rect(drw, x + curpos, 2 + (bh - fh) / 2, 2, fh - 4, 1, 0);
    }

    recalculatenumbers();
    if (lines > 0) {
        /* draw grid */
        int i = 0;
        for (item = curr; item != next; item = item->right, i++)
            drawitem(item, x + ((i / lines) * ((mw - x) / columns)), y + (((i % lines) + 1) * bh), (mw - x) / columns);
    } else if (matches) {
        /* draw horizontal list */
        x += inputw;
        w = TEXTW("<");
        if (curr->left) {
            drw_setscheme(drw, scheme[SchemeNorm]);
            drw_text(drw, x, 0, w, bh, lrpad / 2, "<", 0);
        }
        x += w;
        for (item = curr; item != next; item = item->right)
            x = drawitem(item, x, 0, MIN(TEXTW(item->text), mw - x - TEXTW(">") - TEXTW(numbers)));
        if (next) {
            w = TEXTW(">");
            drw_setscheme(drw, scheme[SchemeNorm]);
            drw_text(drw, mw - w - TEXTW(numbers), 0, w, bh, lrpad / 2, ">", 0);
        }
    }
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, mw - TEXTW(numbers), 0, TEXTW(numbers), bh, lrpad / 2, numbers, 0);
    drw_map(drw, win, 0, 0, mw, mh);
}

static void grabfocus(void) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    Window focuswin;
    int i, revertwin;

    for (i = 0; i < 100; ++i) {
        XGetInputFocus(dpy, &focuswin, &revertwin);
        if (focuswin == win)
            return;
        XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
        nanosleep(&ts, NULL);
    }
    die("cannot grab focus");
}

static void grabkeyboard(void) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
    int i;

    if (embed)
        return;
    /* try to grab keyboard, we may have to wait for another process to ungrab */
    for (i = 0; i < 1000; i++) {
        if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
            return;
        nanosleep(&ts, NULL);
    }
    die("cannot grab keyboard");
}

static void match(void) {
    static char **tokv = NULL;
    static int tokn = 0;

    char buf[sizeof text], *s;
    int i, tokc = 0;
    size_t len, textsize;
    struct item *item, *lprefix, *lsubstr, *prefixend, *substrend;

    strcpy(buf, text);
    /* separate input text into tokens to be matched individually */
    for (s = strtok(buf, " "); s; tokv[tokc - 1] = s, s = strtok(NULL, " "))
        if (++tokc > tokn && !(tokv = realloc(tokv, ++tokn * sizeof *tokv)))
            die("cannot realloc %u bytes:", tokn * sizeof *tokv);
    len = tokc ? strlen(tokv[0]) : 0;

    if (use_prefix) {
        matches = lprefix = matchend = prefixend = NULL;
        textsize = strlen(text);
    } else {
        matches = lprefix = lsubstr = matchend = prefixend = substrend = NULL;
        textsize = strlen(text) + 1;
    }
    for (item = items; item && item->text; item++) {
        for (i = 0; i < tokc; i++)
            if (!fstrstr(item->text, tokv[i]))
                break;
        if (i != tokc) /* not all tokens match */
            continue;
        /* exact matches go first, then prefixes, then substrings */
        if (!tokc || !fstrncmp(text, item->text, textsize))
            appenditem(item, &matches, &matchend);
        else if (!fstrncmp(tokv[0], item->text, len))
            appenditem(item, &lprefix, &prefixend);
        else if (!use_prefix)
            appenditem(item, &lsubstr, &substrend);
    }
    if (lprefix) {
        if (matches) {
            matchend->right = lprefix;
            lprefix->left = matchend;
        } else
            matches = lprefix;
        matchend = prefixend;
    }
    if (!use_prefix && lsubstr) {
        if (matches) {
            matchend->right = lsubstr;
            lsubstr->left = matchend;
        } else
            matches = lsubstr;
        matchend = substrend;
    }

    curr = sel = matches;
    calcoffsets();
}

static void insert(const char *str, ssize_t n) {
    if (strlen(text) + n > sizeof text - 1)
        return;
    /* move existing text out of the way, insert new text, and update cursor */
    memmove(&text[cursor + n], &text[cursor], sizeof text - cursor - MAX(n, 0));
    if (n > 0)
        memcpy(&text[cursor], str, n);
    cursor += n;
    match();
}

static size_t nextrune(int inc) {
    ssize_t n;

    /* return location of next utf8 rune in the given direction (+1 or -1) */
    for (n = cursor + inc; n + inc >= 0 && (text[n] & 0xc0) == 0x80; n += inc)
        ;
    return n;
}

static void movewordedge(int dir) {
    if (dir < 0) { /* move cursor to the start of the word*/
        while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
            cursor = nextrune(-1);
        while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
            cursor = nextrune(-1);
    } else { /* move cursor to the end of the word */
        while (text[cursor] && strchr(worddelimiters, text[cursor]))
            cursor = nextrune(+1);
        while (text[cursor] && !strchr(worddelimiters, text[cursor]))
            cursor = nextrune(+1);
    }
}

static void keypress(XKeyEvent *ev) {
    char buf[32];
    int len;
    struct item *item;
    KeySym ksym;
    Status status;
    int i;
    struct item *tmpsel;
    bool offscreen = false;

    len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
    switch (status) {
        default: /* XLookupNone, XBufferOverflow */
            return;
        case XLookupChars:
            goto insert;
        case XLookupKeySym:
        case XLookupBoth:
            break;
    }

    if (ev->state & ControlMask) {
        switch (ksym) {
            case XK_a:
                ksym = XK_Home;
                break;
            case XK_b:
                ksym = XK_Left;
                break;
            case XK_c:
                ksym = XK_Escape;
                break;
            case XK_d:
                ksym = XK_Delete;
                break;
            case XK_e:
                ksym = XK_End;
                break;
            case XK_f:
                ksym = XK_Right;
                break;
            case XK_g:
                ksym = XK_Escape;
                break;
            case XK_h:
                ksym = XK_BackSpace;
                break;
            case XK_i:
                ksym = XK_Tab;
                break;
            case XK_j: /* fallthrough */
            case XK_J: /* fallthrough */
            case XK_m: /* fallthrough */
            case XK_M:
                ksym = XK_Return;
                ev->state &= ~ControlMask;
                break;
            case XK_n:
                ksym = XK_Down;
                break;
            case XK_p:
                ksym = XK_Up;
                break;

            case XK_k: /* delete right */
                text[cursor] = '\0';
                match();
                break;
            case XK_u: /* delete left */
                insert(NULL, 0 - cursor);
                break;
            case XK_w: /* delete word */
                while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
                    insert(NULL, nextrune(-1) - cursor);
                while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
                    insert(NULL, nextrune(-1) - cursor);
                break;
            case XK_y: /* paste selection */
            case XK_Y:
                XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY, utf8, utf8, win, CurrentTime);
                return;
            case XK_Left:
                movewordedge(-1);
                goto draw;
            case XK_Right:
                movewordedge(+1);
                goto draw;
            case XK_Return:
            case XK_KP_Enter:
                break;
            case XK_bracketleft:
                cleanup();
                exit(1);
            default:
                return;
        }
    } else if (ev->state & Mod1Mask) {
        switch (ksym) {
            case XK_b:
                movewordedge(-1);
                goto draw;
            case XK_f:
                movewordedge(+1);
                goto draw;
            case XK_g:
                ksym = XK_Home;
                break;
            case XK_G:
                ksym = XK_End;
                break;
            case XK_h:
                ksym = XK_Up;
                break;
            case XK_j:
                ksym = XK_Next;
                break;
            case XK_k:
                ksym = XK_Prior;
                break;
            case XK_l:
                ksym = XK_Down;
                break;
            default:
                return;
        }
    }

    switch (ksym) {
        default:
        insert:
            if (!iscntrl(*buf))
                insert(buf, len);
            break;
        case XK_Delete:
            if (text[cursor] == '\0')
                return;
            cursor = nextrune(+1);
            /* fallthrough */
        case XK_BackSpace:
            if (cursor == 0)
                return;
            insert(NULL, nextrune(-1) - cursor);
            break;
        case XK_End:
            if (text[cursor] != '\0') {
                cursor = strlen(text);
                break;
            }
            if (next) {
                /* jump to end of list and position items in reverse */
                curr = matchend;
                calcoffsets();
                curr = prev;
                calcoffsets();
                while (next && (curr = curr->right))
                    calcoffsets();
            }
            sel = matchend;
            break;
        case XK_Escape:
            cleanup();
            exit(1);
        case XK_Home:
            if (sel == matches) {
                cursor = 0;
                break;
            }
            sel = curr = matches;
            calcoffsets();
            break;
        case XK_Left:
            if (columns > 1) {
                if (!sel)
                    return;
                tmpsel = sel;
                for (i = 0; i < lines; i++) {
                    if (!tmpsel->left || tmpsel->left->right != tmpsel)
                        return;
                    if (tmpsel == curr)
                        offscreen = true;
                    tmpsel = tmpsel->left;
                }
                sel = tmpsel;
                if (offscreen) {
                    curr = prev;
                    calcoffsets();
                }
                break;
            }
            if (cursor > 0 && (!sel || !sel->left || lines > 0)) {
                cursor = nextrune(-1);
                break;
            }
            if (lines > 0)
                return;
            /* fallthrough */
        case XK_Up:
            if (sel && sel->left && (sel = sel->left)->right == curr) {
                curr = prev;
                calcoffsets();
            }
            break;
        case XK_Next:
            if (!next)
                return;
            sel = curr = next;
            calcoffsets();
            break;
        case XK_Prior:
            if (!prev)
                return;
            sel = curr = prev;
            calcoffsets();
            break;
        case XK_Return:
        case XK_KP_Enter:
            puts((sel && !(ev->state & ShiftMask)) ? sel->text : text);
            if (!(ev->state & ControlMask)) {
                cleanup();
                exit(0);
            }
            if (sel)
                sel->out = 1;
            break;
        case XK_Right:
            if (columns > 1) {
                if (!sel)
                    return;
                tmpsel = sel;
                for (i = 0; i < lines; i++) {
                    if (!tmpsel->right || tmpsel->right->left != tmpsel)
                        return;
                    tmpsel = tmpsel->right;
                    if (tmpsel == next)
                        offscreen = true;
                }
                sel = tmpsel;
                if (offscreen) {
                    curr = next;
                    calcoffsets();
                }
                break;
            }
            if (text[cursor] != '\0') {
                cursor = nextrune(+1);
                break;
            }
            if (lines > 0)
                return;
            /* fallthrough */
        case XK_Down:
            if (sel && sel->right && (sel = sel->right) == next) {
                curr = next;
                calcoffsets();
            }
            break;
        case XK_Tab:
            if (!matches)
                break; /* cannot complete no matches */
            strncpy(text, matches->text, sizeof text - 1);
            text[sizeof text - 1] = '\0';
            len = cursor = strlen(text); /* length of longest common prefix */
            for (item = matches; item && item->text; item = item->right) {
                cursor = 0;
                while (cursor < len && text[cursor] == item->text[cursor])
                    cursor++;
                len = cursor;
            }
            memset(text + len, '\0', strlen(text) - len);
            break;
    }

draw:
    drawmenu();
}

static void paste(void) {
    char *p, *q;
    int di;
    unsigned long dl;
    Atom da;

    /* we have been given the current selection, now insert it into input */
    if (XGetWindowProperty(dpy, win, utf8, 0, (sizeof text / 4) + 1, False, utf8, &da, &di, &dl, &dl, (unsigned char **)&p) == Success &&
        p) {
        insert(p, (q = strchr(p, '\n')) ? q - p : (ssize_t)strlen(p));
        XFree(p);
    }
    drawmenu();
}

static void xinitvisual(void) {
    XVisualInfo *infos;
    XRenderPictFormat *fmt;
    int nitems;
    int i;

    XVisualInfo tpl = {.screen = screen, .depth = 32, .class = TrueColor};
    long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

    infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
    visual = NULL;
    for (i = 0; i < nitems; i++) {
        fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
        if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            visual = infos[i].visual;
            depth = infos[i].depth;
            cmap = XCreateColormap(dpy, root, visual, AllocNone);
            useargb = 1;
            break;
        }
    }

    XFree(infos);

    if (!visual) {
        visual = DefaultVisual(dpy, screen);
        depth = DefaultDepth(dpy, screen);
        cmap = DefaultColormap(dpy, screen);
    }
}

static void readargv(void) {
    size_t len = 0;
    for (char **it = argv_items; *it; ++it, ++len) { }
    items = calloc(len + 1, sizeof(struct item));
    items[len].text = NULL;
    char const *max_text = NULL;
    unsigned int max_w = 0;
    for (size_t i = 0; i < len; ++i) {
        items[i].text = argv_items[i];
        unsigned int tmp;
        drw_font_getexts(drw->fonts, items[i].text, strlen(items[i].text), &tmp, NULL);
        if (tmp > max_w) {
            max_w = tmp;
            max_text = items[i].text;
        }
    }
    inputw = max_text ? TEXTW(max_text) : 0;
    lines = MIN(lines, len);
}

static void readstdin(void) {
    char buf[sizeof text], *p;
    size_t i, imax = 0, size = 0;
    unsigned int tmpmax = 0;

    /* read each line from stdin and add it to the item list */
    for (i = 0; fgets(buf, sizeof buf, stdin); i++) {
        if (i + 1 >= size / sizeof *items)
            if (!(items = realloc(items, (size += BUFSIZ))))
                die("cannot realloc %u bytes:", size);
        if ((p = strchr(buf, '\n')))
            *p = '\0';
        if (!(items[i].text = strdup(buf)))
            die("cannot strdup %u bytes:", strlen(buf) + 1);
        items[i].out = 0;
        drw_font_getexts(drw->fonts, buf, strlen(buf), &tmpmax, NULL);
        if (tmpmax > inputw) {
            inputw = tmpmax;
            imax = i;
        }
    }
    if (items)
        items[i].text = NULL;
    inputw = items ? TEXTW(items[imax].text) : 0;
    lines = MIN(lines, i);
}

static void readinput(void) {
    if (argv_items)
        readargv();
    else
        readstdin();
}


static void run(void) {
    XEvent ev;

    while (!XNextEvent(dpy, &ev)) {
        if (XFilterEvent(&ev, win))
            continue;
        switch (ev.type) {
            case DestroyNotify:
                if (ev.xdestroywindow.window != win)
                    break;
                cleanup();
                exit(1);
            case Expose:
                if (ev.xexpose.count == 0)
                    drw_map(drw, win, 0, 0, mw, mh);
                break;
            case FocusIn:
                /* regrab focus from parent window */
                if (ev.xfocus.window != win)
                    grabfocus();
                break;
            case KeyPress:
                keypress(&ev.xkey);
                break;
            case SelectionNotify:
                if (ev.xselection.property == utf8)
                    paste();
                break;
            case VisibilityNotify:
                if (ev.xvisibility.state != VisibilityUnobscured)
                    XRaiseWindow(dpy, win);
                break;
        }
    }
}

static void setup(void) {
    int x, y, i, j;
    unsigned int du;
    XSetWindowAttributes swa;
    XIM xim;
    Window w, dw, *dws;
    XWindowAttributes wa;
    XClassHint ch = {"dmenu", "dmenu"};
#ifdef XINERAMA
    XineramaScreenInfo *info;
    Window pw;
    int a, di, n, area = 0;
#endif
    /* init appearance */
    for (j = 0; j < SchemeLast; j++)
        scheme[j] = drw_scm_create(drw, colors[j], alphas[j], 2);

    clip = XInternAtom(dpy, "CLIPBOARD", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);

    /* calculate menu geometry */
    bh = drw->fonts->h + 2;
    bh = MAX(bh, lineheight); /* make a menu line AT LEAST 'lineheight' tall */
    lines = MAX(lines, 0);
    mh = (lines + 1) * bh;
    promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
#ifdef XINERAMA
    i = 0;
    if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
        XGetInputFocus(dpy, &w, &di);
        if (mon >= 0 && mon < n)
            i = mon;
        else if (w != root && w != PointerRoot && w != None) {
            /* find top-level window containing current input focus */
            do {
                if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
                    XFree(dws);
            } while (w != root && w != pw);
            /* find xinerama screen with which the window intersects most */
            if (XGetWindowAttributes(dpy, pw, &wa))
                for (j = 0; j < n; j++)
                    if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
                        area = a;
                        i = j;
                    }
        }
        /* no focused window is on screen, so use pointer location instead */
        if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
            for (i = 0; i < n; i++)
                if (INTERSECT(x, y, 1, 1, info[i]))
                    break;

        if (centered) {
            mw = MIN(MAX(max_textw() + promptw, min_width), info[i].width);
            x = info[i].x_org + ((info[i].width - mw) / 2);
            y = info[i].y_org + ((info[i].height - mh) / 2);
        } else {
            x = info[i].x_org;
            y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
            mw = info[i].width;
        }

        XFree(info);
    } else
#endif
    {
        if (!XGetWindowAttributes(dpy, parentwin, &wa))
            die("could not get embedding window attributes: 0x%lx", parentwin);

        if (centered) {
            mw = MIN(MAX(max_textw() + promptw, min_width), wa.width);
            x = (wa.width - mw) / 2;
            y = (wa.height - mh) / 2;
        } else {
            x = 0;
            y = topbar ? 0 : wa.height - mh;
            mw = wa.width;
        }
    }
    inputw = MIN(inputw, mw / 3);
    match();

    /* create menu window */
    swa.override_redirect = True;
    swa.background_pixel = 0;
    swa.colormap = cmap;

    swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
    win = XCreateWindow(dpy,
        parentwin,
        x,
        y,
        mw,
        mh,
        border_width,
        depth,
        InputOutput,
        visual,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &swa);



    if (border_width)
        XSetWindowBorder(dpy, win, scheme[SchemeSel][ColBg].pixel);
    XSetClassHint(dpy, win, &ch);

    /* input methods */
    if ((xim = XOpenIM(dpy, NULL, NULL, NULL)) == NULL)
        die("XOpenIM failed: could not open input device");

    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, XNFocusWindow, win, NULL);

    XMapRaised(dpy, win);
    if (embed) {
        XSelectInput(dpy, parentwin, FocusChangeMask | SubstructureNotifyMask);
        if (XQueryTree(dpy, parentwin, &dw, &w, &dws, &du) && dws) {
            for (i = 0; i < du && dws[i] != win; ++i)
                XSelectInput(dpy, dws[i], FocusChangeMask);
            XFree(dws);
        }
        grabfocus();
    }
    drw_resize(drw, mw, mh);
    drawmenu();
}

static void usage(void) {
    fputs("usage: dmenu [-bfcivx] [-p prompt] [-fn font] [-h height]\n"
          "             [-l lines] [-g columns]\n"
          "             [-nb color] [-nf color] [-sb color] [-sf color]\n"
          "             [-w windowid] [-m monitor]\n"
          "             [-o opacity]\n"
          "\n"
          "man dmenu for more details\n",
        stderr);

    exit(1);
}

int getinteger(char const *flag, char const *value) {
    char *endptr = NULL;
    errno = 0;
    long out = strtol(value, &endptr, 10);
    if (errno)
        die("Could not parse %s value (%s) as an integer:", flag, value);
    if (out > INT_MAX || out < INT_MIN) {
        errno = ERANGE;
        die("Could not parse %s value (%s) as an integer:", flag, value);
    }
    return (int)out;
}

unsigned int getpositiveint(char const *flag, char const *value) {
    int out = getinteger(flag, value);

    if (out < 0)
        die("Could not parse %s value (%s) as a positive integer.", flag, value);
    return (unsigned)out;
}

int main(int argc, char *argv[]) {
    XWindowAttributes wa;
    int i, fast = 0;

    for (i = 1; i < argc; i++)
        /* these options take no arguments */
        if (!strcmp(argv[i], "-v")) { /* prints version information */
            puts("dmenu-" VERSION);
            exit(0);
        } else if (!strcmp(argv[i], "-b")) /* appears at the bottom of the screen */
            topbar = 0;
        else if (!strcmp(argv[i], "-f")) /* grabs keyboard before reading stdin */
            fast = 1;
        else if (!strcmp(argv[i], "-c")) /* centers dmenu on screen */
            centered = 1;
        else if (!strcmp(argv[i], "-i")) { /* case-insensitive item matching */
            fstrncmp = strncasecmp;
            fstrstr = cistrstr;
        } else if (!strcmp(argv[i], "-x")) /* invert use_prefix */
            use_prefix = !use_prefix;
        else if (i + 1 == argc)
            usage();
        /* these options take one argument */
        else if (!strcmp(argv[i], "-g")) { /* number of columns in grid */
            char const *flag = argv[i++];
            char const *value = argv[i];
            columns = getpositiveint(flag, value);
            if (lines == 0)
                lines = 1;
        } else if (!strcmp(argv[i], "-l")) { /* number of lines in grid */
            char const *flag = argv[i++];
            char const *value = argv[i];
            lines = getpositiveint(flag, value);
            if (columns == 0)
                columns = 1;
        } else if (!strcmp(argv[i], "-m")) {
            char const *flag = argv[i++];
            char const *value = argv[i];
            mon = getinteger(flag, value);
        } else if (!strcmp(argv[i], "-o")) { /* opacity */
            alphas[SchemeNorm][1] = 255 * atof(argv[++i]);
            alphas[SchemeSel][1] = alphas[SchemeNorm][1];
            alphas[SchemeNormHighlight][1] = alphas[SchemeNorm][1];
            alphas[SchemeSelHighlight][1] = alphas[SchemeNorm][1];
        } else if (!strcmp(argv[i], "-p")) /* adds prompt to left of input field */
            prompt = argv[++i];
        else if (!strcmp(argv[i], "-fn")) /* font or font set */
            fonts[0] = argv[++i];
        else if (!strcmp(argv[i], "-h")) { /* minimum height of one menu line */
            lineheight = atoi(argv[++i]);
            lineheight = MAX(lineheight, 8); /* reasonable default in case of value too small/negative */
        } else if (!strcmp(argv[i], "-nb"))  /* normal background color */
            colors[SchemeNorm][ColBg] = argv[++i];
        else if (!strcmp(argv[i], "-nf")) /* normal foreground color */
            colors[SchemeNorm][ColFg] = argv[++i];
        else if (!strcmp(argv[i], "-sb")) /* selected background color */
            colors[SchemeSel][ColBg] = argv[++i];
        else if (!strcmp(argv[i], "-sf")) /* selected foreground color */
            colors[SchemeSel][ColFg] = argv[++i];
        else if (!strcmp(argv[i], "-w")) /* embedding window id */
            embed = argv[++i];
        else if (!strcmp(argv[i], "-bw")) { /* border width */
            char const *flag = argv[i++];
            char const *value = argv[i];
            border_width = getpositiveint(flag, value);
        } else if (!strcmp(argv[i], "-it")) { /* items */
            argv_items = &argv[++i];
            break;
        } else
            usage();

    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
        die("cannot open display");
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    if (!embed || !(parentwin = strtol(embed, NULL, 0)))
        parentwin = root;
    if (!XGetWindowAttributes(dpy, parentwin, &wa))
        die("could not get embedding window attributes: 0x%lx", parentwin);
    xinitvisual();
    drw = drw_create(dpy, screen, root, wa.width, wa.height, visual, depth, cmap);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
        die("no fonts could be loaded.");
    lrpad = drw->fonts->h;

#ifdef __OpenBSD__
    if (pledge("stdio rpath", NULL) == -1)
        die("pledge");
#endif

    if (fast && !isatty(0)) {
        grabkeyboard();
        readinput();
    } else {
        readinput();
        grabkeyboard();
    }
    setup();
    run();

    return 1; /* unreachable */
}
