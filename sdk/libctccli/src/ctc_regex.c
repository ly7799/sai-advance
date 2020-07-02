
#include "ctc_regex.h"
#include "ctc_cli.h"

#define MAX_NFA  1024
#define MAX_TAG  10
#define OKP     1
#define META_ANYSKIP 2
#define META_CHRSKIP 3
#define META_CCLSKIP 18
#define MAXMETA_CHR 128
#define META_CHRBIT 8

enum META {
    META_NOP = 0,
    META_CHR,
    META_ANY,
    META_CCL,
    META_BOL,
    META_EOL,
    META_BOT,
    META_EOT,
    META_BOW,
    META_EOW,
    META_REF,
    META_CLO,
    META_END = 0
};

#define BIT_BLOCK MAXMETA_CHR/META_CHRBIT
#define ERR_PAT(x) (*g_ctc_nfa = META_END, x)
#define MARK(x)  *mp++ = x
#define IS_WORD(x)  g_ctc_char_type[(0177&(x))]
#define IS_IN_SET(x,y) ((x)[((y)&0170)>>3] & g_ctc_bit_arr[(y)&07])


static char g_ctc_char_type[MAXMETA_CHR] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 0, 0, 0, 0, 1, 0, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 0, 0, 0, 0, 0
};

static int32 g_ctc_tag_stack[MAX_TAG];
static char  g_ctc_nfa[MAX_NFA];
static int32 g_ctc_sta = META_NOP;
static char g_ctc_bit_tab[BIT_BLOCK];
static char g_ctc_bit_arr[] = {1,2,4,8,16,32,64,128};
static char *g_ctc_bol;
char *g_ctc_bop[MAX_TAG];
char *g_ctc_eop[MAX_TAG];

STATIC void
ctc_char_set(char c)
{
    g_ctc_bit_tab[(char) ((c) & 0170) >> 3] |= g_ctc_bit_arr[(c) & 07];
}


STATIC char *
ctc_regex_match(char *p1, char *p2)
{
    int32 op = 0;
    int32 c = 0;
    int32 n = 0;
    uint8 c2 = 0;
    char *e = NULL;
    char *bp= NULL;
    char *ep= NULL;
    char *are= NULL;

    while ((op = *p2++) != META_END)
        switch(op)
    {

    case META_CHR:
        if (*p1++ != *p2++)
        {
            return 0;
        }
        break;

    case META_ANY:
        if (!*p1++)
        {
            return 0;
        }
        break;

    case META_CCL:
        c = *p1++;

        if (!IS_IN_SET(p2, c))
        {
            return 0;
        }

        p2 += BIT_BLOCK;
        break;

    case META_BOL:
        if (p1 != g_ctc_bol)
        {
            return 0;
        }
        break;

    case META_EOL:
        if (*p1)
        {
            return 0;
        }
        break;

    case META_BOT:
        c2 = *p2++;
        g_ctc_bop[c2] = p1;
        break;

    case META_EOT:
        c2 = *p2++;
        g_ctc_eop[c2] = p1;
        break;

    case META_BOW:
        if (((p1 != g_ctc_bol) && (IS_WORD(p1[-1])) ) || (!IS_WORD(*p1)))
        {
            return 0;
        }
        break;

    case META_EOW:
        if (p1 == g_ctc_bol || !IS_WORD(p1[-1]) || IS_WORD(*p1))
        {
            return 0;
        }
        break;

    case META_REF:
        n = *p2++;
        bp = g_ctc_bop[n];
        ep = g_ctc_eop[n];

        while (bp < ep)
        {
            if (*bp++ != *p1++)
            {
                return 0;
            }
        }
        break;

    case META_CLO:
        are = p1;
        switch(*p2)
        {

        case META_CHR:
            c = *(p2 + 1);
            while (*p1 && c == *p1)
            {
                p1++;
            }
            n = META_CHRSKIP;
            break;


        case META_ANY:
            while (*p1)
            {
                p1++;
            }
            n = META_ANYSKIP;
            break;

        case META_CCL:
            while ((c = *p1) && IS_IN_SET(p2 + 1, c))
            {
                p1++;
            }
            n = META_CCLSKIP;
            break;

        default:
            ctc_cli_out("error match %o\n", *p2);
            return 0;
        }

        p2 += n;

        while (p1 >= are)
        {
            e = ctc_regex_match(p1, p2);
            if (e)
            {
                return e;
            }
            --p1;
        }
        return 0;
    default:
        ctc_cli_out("error match %o\n", op);
        return 0;
    }
    return p1;
}


char *
ctc_regex_complete(char *pattern)
{
    char *p = NULL;
    char *cp = NULL;
    char *mp = g_ctc_nfa;
    char *sp = g_ctc_nfa;
    char mask = 0;
    int32 tag_i = 0;
    int32 tag_c = 1;
    int32 n = 0;
    int c1 = 0;
    int c2 = 0;

    if (!pattern || !*pattern)
    {
        if (g_ctc_sta)
        {
            return 0;
        }
        else
        {
            return ERR_PAT("error patten");
        }
    }

    g_ctc_sta = META_NOP;

    for (p = pattern; *p; p++)
    {
        cp = mp;

        switch(*p)
        {
        case '.':
            MARK(META_ANY);
            break;

        case '*':
        case '+':
            if (p == pattern)
            {
                return ERR_PAT("error patten");
            }

            cp = sp;

            if (*cp == META_CLO)
            {
                break;
            }

            switch(*cp)
            {
            case META_BOW:
            case META_EOW:

            case META_BOT:
            case META_EOT:

            case META_BOL:
            case META_REF:
                return ERR_PAT("error patten");
            default:
                break;
            }

            if (*p == '+')
            {
                for (sp = mp; cp < sp; cp++)
                {
                    MARK(*cp);
                }
            }

            MARK(META_END);
            MARK(META_END);
            sp = mp;

            while (--mp > cp)
            {
                *mp = mp[-1];
            }

            MARK(META_CLO);
            mp = sp;
            break;

        case '^':
            if (p == pattern)
            {
                MARK(META_BOL);
            }
            else
            {
                MARK(META_CHR);
                MARK(*p);
            }
            break;

        case '$':
            if (!*(p + 1))
                {
                MARK(META_EOL);
                }
            else
            {
                MARK(META_CHR);
                MARK(*p);
            }
            break;

        case '[':

            MARK(META_CCL);

            if (*++p == '^')
            {
                mask = 0377;
                p++;
            }
            else
            {
                mask = 0;
            }

            if (*p == '-')
            {
                ctc_char_set(*p++);
            }
            else if (*p == ']')
            {
                ctc_char_set(*p++);
            }

            while (*p && *p != ']')
            {
                if (*p == '-' && *(p + 1) && *(p + 1) != ']')
                {
                    p++;
                    c1 = *(p - 2) + 1;
                    c2 = *p++;

                    while (c1 <= c2)
                    {
                        ctc_char_set((char)c1++);
                    }
                }
                else
                {
                    ctc_char_set(*p++);
                }
            }

            if (!*p)
            {
                return ERR_PAT("error patten");
            }

            for (n = 0; n < BIT_BLOCK; g_ctc_bit_tab[n++] = (char) 0)
            {
                MARK(mask ^ g_ctc_bit_tab[n]);
            }

            break;

        case '\\':
            switch(*++p)
            {
            case '<':
                MARK(META_BOW);
                break;

            case '>':
                if (*sp == META_BOW)
                {
                    return ERR_PAT("error patten");
                }
                MARK(META_EOW);
                break;


            case '(':
                if (tag_c < MAX_TAG)
                {
                    g_ctc_tag_stack[++tag_i] = tag_c;
                    MARK(META_BOT);
                    MARK(tag_c++);
                }
                else
                {
                    return ERR_PAT("error patten");
                }
                break;

            case ')':
                if (*sp == META_BOT)
                {
                    return ERR_PAT("error patten");
                }

                if (tag_i > 0)
                {
                    MARK(META_EOT);
                    MARK(g_ctc_tag_stack[tag_i--]);
                }
                else
                {
                    return ERR_PAT("error patten");
                }

                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                n = *p - '0';

                if (tag_i > 0 && g_ctc_tag_stack[tag_i] == n)
                {
                    return ERR_PAT("error patten");
                }

                if (tag_c > n)
                {
                    MARK(META_REF);
                    MARK(n);
                }
                else
                {
                    return ERR_PAT("error patten");
                }
                break;

            default:
                MARK(META_CHR);
                MARK(*p);
            }
            break;

        default :               /* an ordinary char  */
            MARK(META_CHR);
            MARK(*p);
            break;
        }
        sp = cp;
    }

    if (tag_i > 0)
    {
        return ERR_PAT("error patten");
    }

    MARK(META_END);

    g_ctc_sta = OKP;
    return 0;
}


int32
ctc_regex_exec(char *patten)
{
    char c = 0;
    char *ep = 0;
    char *ap = g_ctc_nfa;

    g_ctc_bol = patten;
    sal_memset(g_ctc_bop, 0, sizeof(g_ctc_bop));

    switch(*ap)
    {

    case META_CHR:
        c = *(ap + 1);
        while (*patten && *patten != c)
        {
            patten++;
        }

        if (!*patten)
        {
            return 0;
        }

    case META_BOL:
        ep = ctc_regex_match(patten, ap);
        break;

    default:
        do
        {
            if ((ep = ctc_regex_match(patten, ap)))
            {
                break;
            }
            patten++;
        }
        while (*patten);

        break;

    case META_END:
        return 0;
    }

    if (!ep)
    {
        return 0;
    }

    g_ctc_bop[0] = patten;
    g_ctc_eop[0] = ep;
    return 1;
}






