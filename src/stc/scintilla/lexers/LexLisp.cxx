// Scintilla source code edit control
/** @file LexLisp.cxx
 ** Lexer for Lisp.
 ** Written by Alexey Yutkin.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

#if 1  // Bricsys change : adjusted for AutoLISP syntax

#include "wx/bricsys/LexLisp.hxx"
#include "Catalogue.h"

// AutoLISP : Colourise + Fold

static inline bool isLispOperator(const char ch)
{
    return ((ch == '(' || ch == ')' || ch == '\''));
}

static inline bool isLispWordstart(const char ch)
{
    return isascii(ch) && ch != ';'  && !isspacechar(ch) && !isLispOperator(ch) &&  ch != '\"';
}

static inline bool isLispWordEnd(const char ch)
{
    return (isspacechar(ch) || isLispOperator(ch) || ch == '\"' || ch == ';'); // 'isspacechar()' includes 0xD and oxA and TAB
}

static int parseKeyFunctionLisp(Accessor &styler, unsigned int& pos, unsigned int lengthDoc)
{
    static const wxString s_defun  (wxT("defun"));            static const size_t l_defun(s_defun.Length());
    static const wxString s_loadlsp(wxT("load"));             static const size_t l_loadlsp(s_loadlsp.Length());
    static const wxString s_loaddcl(wxT("load_dialog"));      static const size_t l_loaddcl(s_loaddcl.Length());
    static const wxString s_setvar (wxT("setvar"));           static const size_t l_setvar(s_setvar.Length());

    static const wxString s_cmdcall (wxT("command"));         static const size_t l_cmdcall(s_cmdcall.Length());
    static const wxString s_cmdcalls(wxT("command-s"));       static const size_t l_cmdcalls(s_cmdcalls.Length());
    static const wxString s_vlcmdf  (wxT("vl-cmdf"));         static const size_t l_vlcmdf(s_vlcmdf.Length());

    static const wxString s_vl_prefix(wxT("vl-"));
    static const wxString s_vlxdocexp(wxT("vl-doc-export"));  static const size_t l_vlxdocexp(s_vlxdocexp.Length());
    static const wxString s_vlxdocimp(wxT("vl-doc-import"));  static const size_t l_vlxdocimp(s_vlxdocimp.Length());
    static const wxString s_vlxdocset(wxT("vl-doc-set"));     static const size_t l_vlxdocset(s_vlxdocset.Length());
    static const wxString s_vlxdocref(wxT("vl-doc-ref"));     static const size_t l_vlxdocref(s_vlxdocref.Length());
    static const wxString s_vlxarximp(wxT("vl-arx-import"));  static const size_t l_vlxarximp(s_vlxarximp.Length());

    static const wxString s_vlbbset(wxT("vl-bb-set"));        static const size_t l_vlbbset(s_vlbbset.Length());
    static const wxString s_vlbbref(wxT("vl-bb-ref"));        static const size_t l_vlbbref(s_vlbbref.Length());

    static const size_t l_min(4);
    static const size_t l_max(13);

    unsigned int initPos = pos;
    // skip leading '('
    char ch = styler.SafeGetCharAt(pos); //[pos];
    if (ch == '(') // just skip leading '('
    {
        ch = styler.SafeGetCharAt(++pos); //[++pos];
    }
    // now skip white spaces
    for (; isspacechar(ch) && (pos < lengthDoc); ) // 'isspacechar()' includes 0xD and oxA and TAB
    {
        ch = styler.SafeGetCharAt(++pos); //[pos];
    }

    // parse till next whitespace or ( or ) or ' or \n or \r
    // extract the word - check against key functions
    wxString word; word.reserve(16);
    for (; !isLispWordEnd(ch) && (pos < lengthDoc); )
    {
        word += ch;
        ch = styler.SafeGetCharAt(++pos); //[pos];
    }

    const size_t l_word(word.Length());
    if ((l_word < l_min) || (l_word > l_max))
    {
        pos = initPos + 1;
        return stateNoState;
    }

    word.MakeLower();

    if ((l_word == l_defun)   && (word.compare(s_defun) == 0))
        return stateIdDefun;
    if ((l_word == l_loadlsp) && (word.compare(s_loadlsp) == 0))
        return stateLspLoad;
    if ((l_word == l_loaddcl) && (word.compare(s_loaddcl) == 0))
        return stateDclLoad;
    if ((l_word == l_setvar)  && (word.compare(s_setvar) == 0))
        return stateLspSetvar;

    if ((l_word == l_cmdcall)   && (word.compare(s_cmdcall) == 0))
        return stateLspCmdCall;
    if ((l_word == l_vlcmdf)    && (word.compare(s_vlcmdf) == 0))
        return stateLspCmdCall;
    if ((l_word == l_cmdcalls)  && (word.compare(s_cmdcalls) == 0))
        return stateLspCmdCall;

    if (word.Left(3).compare(s_vl_prefix) != 0)
        return stateNoState;

    // vl-doc-xxx
    if ((l_word == l_vlxdocexp) && (word.compare(s_vlxdocexp) == 0))
        return stateLspVlx;
    if ((l_word == l_vlxdocimp) && (word.compare(s_vlxdocimp) == 0))
        return stateLspVlx;
    if ((l_word == l_vlxdocset) && (word.compare(s_vlxdocset) == 0))
        return stateLspVlx;
    if ((l_word == l_vlxdocref) && (word.compare(s_vlxdocref) == 0))
        return stateLspVlx;
    if ((l_word == l_vlxarximp) && (word.compare(s_vlxarximp) == 0))
        return stateLspVlx;
    // vl-bb-set/ref
    if ((l_word == l_vlbbset) && (word.compare(s_vlbbset) == 0))
        return stateLspBB;
    if ((l_word == l_vlbbref) && (word.compare(s_vlbbref) == 0))
        return stateLspBB;

    pos = initPos + 1;
    return stateNoState;
}

static void classifyWordLisp(unsigned int start, unsigned int end,
                             const WordList &keywords, const WordList &keywords_kw,
                             const WordList &keywords_us,
                             Accessor &styler)
{
	assert(end >= start);
    static const size_t s_maxChars(8192);
	static char s[s_maxChars];

    int len = end - start + 1;
    if (len >= s_maxChars)
        len = s_maxChars - 1;
    else
    if (len < 0)
        len = 0;

    *s = '\0';

    bool digit_flag = true;
    int ch = 0, numSigns = 0, numExps = 0, numDots = 0;
    --len;
	for (int i = 0, pos = (int)start; i <= len; ++pos)
    {
		s[i] = ch = tolower(styler[pos]);
        if (digit_flag)
        {
            if ((ch == '+') || (ch == '-')) ++numSigns;
            else
            if (ch == 'e') ++numExps;
            else
            if (ch == '.') ++numDots;
            else
            if (!isdigit(ch)) digit_flag = false;
        }
		s[++i] = '\0';
	}

    if (digit_flag && (numExps == 1)) --numSigns; // allow 1 more '-/+' if 'e' is present

    if (digit_flag && (numExps  > 1)) digit_flag = false;
    else
    if (digit_flag && (numSigns > 1)) digit_flag = false;
    else
    if (digit_flag && (numDots  > 1)) digit_flag = false;
    else
    if (digit_flag && numDots && (s[0] == '.')) digit_flag = false; // no leading '.' allowed
    else
    if (digit_flag && (len <= 1) && ((numExps + numSigns + numDots) > 0)) digit_flag = false;

    char chAttr = SCE_LISP_IDENTIFIER;
	if (digit_flag)
    {
        chAttr = SCE_LISP_NUMBER;
    }
    else
	if (keywords.InList(s))
    {
		chAttr = SCE_LISP_KEYWORD;
	}
    else
    if (keywords_kw.InList(s))
    {
		chAttr = SCE_LISP_KEYWORD_KW;
	}
    else
    if (keywords_us.InList(s))
    {
		chAttr = SCE_LISP_SPECIAL;
	}

    styler.ColourTo(end, chAttr);
	return;
}

// start + end positions need to be outside block comment area ...
static void AdjustStartEndPositions(unsigned int& startPos, int& length, Accessor& styler)
{
    int sPos  = (int)startPos;
    int ePos  = sPos + length;
	int sLine = styler.GetLine(sPos);
	int eLine = styler.GetLine(ePos);

    // adjust start position
    int oldState = styler.GetLineState(sLine);
    while ((sLine > 0) && ((oldState & (stateInBlockComment|stateInComment)) != 0))
    {
        oldState = styler.GetLineState(--sLine);
    }
    if (sLine < 0) sLine = 0;

    // adjust end position
    const int maxLine = styler.GetLine(styler.Length());
    int line(eLine);
    oldState = styler.GetLineState(line);
    while ((line < maxLine) && ((oldState & (stateInBlockComment|stateInComment)) != 0))
    {
        oldState = styler.GetLineState(++line);
    }
    eLine = line;
    if (eLine > maxLine) eLine = maxLine;

    // clear block comment + function type flags
    for (line = sLine; line <= eLine; ++line)
    {
        oldState = styler.GetLineState(line);
        styler.SetLineState(line, oldState & (~stateInComment) & (~stateInBlockComment) & (~stateLineMask));
    }

	sPos = styler.LineStart(sLine);
    if (sPos < 0) sPos = 0;

    ePos = styler.LineStart(eLine); // if at last line, use last position
    if (eLine >= maxLine) ePos = styler.Length();
    if (ePos < ((int)startPos + length))
        ePos = ((int)startPos + length);

    startPos = sPos;
    length   = ePos - sPos;
}

static void ColouriseLispDoc(unsigned int startPos, int length, int initStyle,
                             WordList *keywordlists[], Accessor &styler)
{
	static const int stylingBitsMask = 0x1F; // from Document::Document() CTor

    const WordList& keywords    = *keywordlists[0];
    const WordList& keywords_kw = *keywordlists[1];
    const WordList& keywords_us = *keywordlists[2];

    bool atEOL = false, insideString = false;
    int radix = -1;
	char ch = 0;

    AdjustStartEndPositions(startPos, length, styler);

	int lineCurrent = styler.GetLine((int)startPos);
    int state = (startPos > 0) ? styler.StyleAt(startPos - 1) : 0;
    state &= stylingBitsMask;

    int oldLineState = styler.GetLineState(lineCurrent);
    oldLineState &= (~stateInComment) & (~stateInBlockComment) & (~stateLineMask);

	styler.StartAt(startPos);

    char chNext = styler[startPos];
	int lengthDoc = startPos + length;
	styler.StartSegment(startPos);
	for (int i = (int)startPos; i < lengthDoc; ++i)
    {
		ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);

		atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (styler.IsLeadByte(ch))
        {
			chNext = styler.SafeGetCharAt(i + 2);
			++i;
			continue;
		}

		if (state == SCE_LISP_DEFAULT)
        {
            if (isLispWordstart(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
			    state = SCE_LISP_IDENTIFIER;
			}
			else
            if (ch == ';')
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
//styler.ColourTo(i, SCE_LISP_COMMENT);
				state = SCE_LISP_COMMENT;
                if (chNext == '|')
                {
                    state = SCE_LISP_MULTI_COMMENT;
                    styler.ColourTo(i /*+ 1*/, SCE_LISP_COMMENT);
                }
			}
			else
            if (isLispOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
			}
			else
            if (ch == '\"')
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				state = SCE_LISP_STRING;
			}
		}
        else
        if (state == SCE_LISP_IDENTIFIER || state == SCE_LISP_SYMBOL)
        {
			if (!isLispWordstart(ch))
            {
				if (state == SCE_LISP_IDENTIFIER)
                {
                    if ((i - 1) >= 0)
                        classifyWordLisp(styler.GetStartSegment(), i - 1, keywords, keywords_kw, keywords_us, styler);
				}
                else
                {
                    if ((i - 1) >= 0)
                        styler.ColourTo(i - 1, state);
				}
				state = SCE_LISP_DEFAULT;
                if (ch == ';')
                {
                    if ((i - 1) >= 0)
                        styler.ColourTo(i - 1, state);
                    //styler.ColourTo(i, SCE_LISP_COMMENT);
    				state = SCE_LISP_COMMENT;
                    if (chNext == '|')
                    {
                        state = SCE_LISP_MULTI_COMMENT;
                        styler.ColourTo(i /*+ 1*/, SCE_LISP_COMMENT);
                    }
    			}
                else
                if (ch == '\"')
                {
                    state = SCE_LISP_STRING;
                }
            } /*else*/
			if (isLispOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
			}
		}
        else
        if (state == SCE_LISP_SPECIAL)
        {
			if (!isLispWordstart(ch) || (radix != -1 && !IsADigit(ch, radix)))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				state = SCE_LISP_DEFAULT;
			}
			if (isLispOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
			}
		}
        else
		if (state == SCE_LISP_COMMENT)
        {
			if (atEOL)
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInComment);
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, SCE_LISP_COMMENT);
			    state = SCE_LISP_DEFAULT;
			}
		}
        else
        if (state == SCE_LISP_MULTI_COMMENT)
        {
			if (ch == '|' && chNext == ';')
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInBlockComment);
                styler.ColourTo(++i, SCE_LISP_COMMENT);
                state = SCE_LISP_DEFAULT;
                chNext = styler.SafeGetCharAt(i + 1);
                continue;
			}
		}
        else
        if (state == SCE_LISP_STRING)
        {
			if (ch == '\\')
            {
				if (chNext == '\"' || chNext == '\'' || chNext == '\\')
                {
					++i;
					chNext = styler.SafeGetCharAt(i + 1);
				}
			}
            else
            if (ch == '\"')
            {
				styler.ColourTo(i, state);
				state = SCE_LISP_DEFAULT;
			}
		}

		if (atEOL)
        {
            if (state == SCE_LISP_MULTI_COMMENT)
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInBlockComment);
                styler.ColourTo(i - 1, SCE_LISP_COMMENT);
            }
            ++lineCurrent;
            oldLineState = styler.GetLineState(lineCurrent);
            oldLineState &= (~stateLineMask); // clear old function line states
        }
	}

    styler.ColourTo(lengthDoc - 1, state);
}

static void FoldLispDoc(unsigned int startPos, int length, int /* initStyle */, WordList *[],
                        Accessor &styler)
{
	unsigned int lengthDoc = startPos + length;
	int visibleChars = 0;
	int lineCurrent = styler.GetLine(startPos);
    int levelPrev = styler.LevelAt(lineCurrent) & SC_FOLDLEVELNUMBERMASK;
    int levelCurrent = levelPrev;
    int resState = stateNoState;

    int oldLineState = styler.GetLineState(lineCurrent);
    oldLineState &= (~stateLineMask); // clear old function line states

    char ch = 0;
	char chNext = styler[startPos];
	int style = 0;
	int styleNext = styler.StyleAt(startPos);
	bool atEOL = false;
	for (unsigned int pos = startPos; pos < lengthDoc; ++pos)
    {
		ch        = chNext;
        style     = styleNext;

		chNext    = styler.SafeGetCharAt(pos + 1);
        styleNext = styler.StyleAt(pos + 1);

        atEOL     = (ch == '\r' && chNext != '\n') || (ch == '\n');

        if (style == SCE_LISP_OPERATOR)
        {
			if (ch == '(')
            {
				++levelCurrent;
                resState = parseKeyFunctionLisp(styler, pos, lengthDoc);
                if (resState != stateNoState)
                {
                    styler.SetLineState(lineCurrent, oldLineState | resState);
                }
                // adjust pos, chNext, styleNext
                chNext    = styler.SafeGetCharAt(pos);
                styleNext = styler.StyleAt(pos--);
                ++visibleChars;
            }
            else
            if (ch == ')')
            {
				--levelCurrent;
			}
		}
        else
		if (atEOL)
        {
			int lev = levelPrev;
            int foldLevel = (levelCurrent & ~SC_FOLDLEVELNUMBERMASK);

            if ((foldLevel > SC_FOLDLEVELBASE) && (visibleChars == 0))
				lev |= SC_FOLDLEVELWHITEFLAG;

            if (levelCurrent > levelPrev)
				lev |= SC_FOLDLEVELHEADERFLAG;

            if (lev != styler.LevelAt(lineCurrent))
				styler.SetLevel(lineCurrent, lev);

			++lineCurrent;
			levelPrev = levelCurrent;
			visibleChars = 0;

            oldLineState = styler.GetLineState(lineCurrent);
            oldLineState &= (~stateLineMask); // clear old function line states
		}
        else
		if (!isspacechar(ch))
        {
			++visibleChars;
        }
	}

    // Fill in the real level of the next line, keeping the current flags as they will be filled in later
	int flagsNext = styler.LevelAt(lineCurrent) & ~SC_FOLDLEVELNUMBERMASK;
	styler.SetLevel(lineCurrent, levelPrev | flagsNext);
}

// DCL : Colourise + Fold

static inline bool isDclOperator(const char ch)
{
    return ((ch == '=' || ch == '{' || ch == '}' || ch == ':' || ch == ';'));
}

static inline bool isDclWordstart(const char ch)
{
	return isascii(ch) && ch != '/'  && !isspacechar(ch) && !isDclOperator(ch) &&  ch != '\"';
}

static bool parseDialog(Accessor &styler, unsigned int pos, unsigned int lengthDoc)
{
    static const wxString s_dialog(wxT("dialog"));

    char ch = 0;

    // skip white spaces
    for (; pos < lengthDoc; ++pos)
    {
        ch = styler.SafeGetCharAt(pos);
        if (wxIsgraph(ch))
            break;
    }
    wxString str(ch);
    for (unsigned int i=0; i < 5; ++i)
    {
        ch = styler.SafeGetCharAt(++pos);
        str += ch;
    }
    if (str.CmpNoCase(s_dialog) != 0)
        return false;

    // check next character after 'dialog'
    ch = styler.SafeGetCharAt(++pos);
    if (!wxIsspace(ch) && (ch != wxT('{')))
        return false;
    return true;
}

static void classifyWordDcl(unsigned int start, unsigned int end,
                            const WordList &keywords, const WordList &keywords_kw,
                            Accessor &styler)
{
	assert(end >= start);
	static char s[8192];

    int len = end - start + 1;
    if (len >= 8192) --len;

    bool digit_flag = true;
	for (int i = 0, pos = (int)start; i < len; ++pos)
    {
		s[i] = tolower(styler[pos]);
		if (!isdigit(s[i]) && (s[i] != '.')) digit_flag = false;
		s[++i] = '\0';
	}

    char chAttr = SCE_DCL_IDENTIFIER;
	if (digit_flag)
    {
        chAttr = SCE_DCL_NUMBER;
    }
    else
	if (keywords.InList(s))
    {
		chAttr = SCE_DCL_KEYWORD;
	}
    else
    if (keywords_kw.InList(s))
    {
		chAttr = SCE_DCL_SPECIAL;
	}

    styler.ColourTo(end, chAttr);
	return;
}

static void ColouriseDclDoc(unsigned int startPos, int length, int initStyle,
                            WordList *keywordlists[], Accessor &styler)
{
	const WordList &keywords    = *keywordlists[0];
	const WordList &keywords_kw = *keywordlists[1];

	styler.StartAt(startPos);

	int lineCurrent = styler.GetLine(startPos);
    bool atEOL = false, insideString = false;
	char ch = 0;

    int state = initStyle, radix = -1;
    if ((styler.GetLineState(lineCurrent) & stateInBlockComment) != 0)
        state = SCE_DCL_COMMENT;

    int oldLineState = styler.GetLineState(lineCurrent);
    oldLineState &= (~stateLineMask); // clear old function line states

    char chNext = styler[startPos];
	int lengthDoc = startPos + length;
	styler.StartSegment(startPos);
	for (int i = (int)startPos; i < lengthDoc; ++i)
    {
		ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);

		atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (styler.IsLeadByte(ch))
        {
			chNext = styler.SafeGetCharAt(i + 2);
			++i;
			continue;
		}

		if (state == SCE_DCL_DEFAULT)
        {
            if (isDclWordstart(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				state = SCE_DCL_IDENTIFIER;
			}
			else
            if (ch == '/') // comment
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);

                if ((chNext == '/') || (chNext == '*'))
                {
                    state = SCE_DCL_COMMENT;
                    if (chNext == '*') state = SCE_DCL_MULTI_COMMENT;
                }
			}
			else
            if (isDclOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_DCL_OPERATOR);
				if (ch == '\'' && isDclWordstart(chNext))
                {
					state = SCE_DCL_SYMBOL;
				}
			}
			else
            if (ch == '\"')
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				state = SCE_DCL_STRING;
			}
		}
        else
        if (state == SCE_DCL_IDENTIFIER || state == SCE_DCL_SYMBOL)
        {
			if (!isDclWordstart(ch))
            {
				if (state == SCE_DCL_IDENTIFIER)
                {
                    if ((i - 1) >= 0)
                        classifyWordDcl(styler.GetStartSegment(), i - 1, keywords, keywords_kw, styler);
				}
                else
                {
                    if ((i - 1) >= 0)
                        styler.ColourTo(i - 1, state);
				}
				state = SCE_DCL_DEFAULT;
                if (ch == '\"')
                {
                    state = SCE_DCL_STRING;
                }
			} /*else*/
			if (isDclOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_DCL_OPERATOR);
				if (ch == '\'' && isDclWordstart(chNext)) {
					state = SCE_DCL_SYMBOL;
				}
			}
		}
        else
        if (state == SCE_DCL_SPECIAL)
        {
			if (!isDclWordstart(ch) || (radix != -1 && !IsADigit(ch, radix)))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				state = SCE_DCL_DEFAULT;
			}
			if (isDclOperator(ch))
            {
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_DCL_OPERATOR);
				if (ch == '\'' && isDclWordstart(chNext))
                {
					state = SCE_DCL_SYMBOL;
				}
			}
		}
        else
		if (state == SCE_DCL_COMMENT)
        {
			if (atEOL)
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInComment);
                if ((i - 1) >= 0)
                    styler.ColourTo(i - 1, SCE_DCL_COMMENT);
			    state = SCE_DCL_DEFAULT;
			}
		}
        else
        if (state == SCE_DCL_MULTI_COMMENT)
        {
			if (ch == '*' && chNext == '/')
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInBlockComment);
                styler.ColourTo(++i, SCE_DCL_COMMENT);
                state = SCE_DCL_DEFAULT;
                chNext = styler.SafeGetCharAt(++i);
                continue;
			}
		}
        else
        if (state == SCE_DCL_STRING)
        {
			if (ch == '\\')
            {
				if (chNext == '\"' || chNext == '\'' || chNext == '\\')
                {
					++i;
					chNext = styler.SafeGetCharAt(i + 1);
				}
			}
            else
            if (ch == '\"')
            {
				styler.ColourTo(i, state);
				state = SCE_DCL_DEFAULT;
			}
		}

		if (atEOL)
        {
            if (state == SCE_DCL_MULTI_COMMENT)
            {
                styler.SetLineState(lineCurrent, oldLineState | stateInBlockComment);
            }
            ++lineCurrent;
            oldLineState = styler.GetLineState(lineCurrent);
            oldLineState &= (~stateLineMask); // clear old function line states
        }
    }

    styler.ColourTo(lengthDoc - 1, state);
}

static void FoldDclDoc(unsigned int startPos, int length, int /* initStyle */, WordList *[],
                       Accessor &styler)
{
	unsigned int lengthDoc = startPos + length;
	int visibleChars = 0;
	int lineCurrent = styler.GetLine(startPos);
    int levelPrev = styler.LevelAt(lineCurrent) & SC_FOLDLEVELNUMBERMASK;
    int levelCurrent = levelPrev;
	char ch = 0;
	char chNext = styler[startPos];
	int style = 0;
	int styleNext = styler.StyleAt(startPos);

    int oldLineState = styler.GetLineState(lineCurrent);
    oldLineState &= (~stateLineMask); // clear old function line states
    
    bool atEOL = false;
    for (unsigned int i = startPos; i < lengthDoc; ++i)
    {
		ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (style == SCE_DCL_OPERATOR)
        {
            if (ch == ':')
            {
                // check for next word is 'dialog'
                if (parseDialog(styler, i + 1, lengthDoc))
                    styler.SetLineState(lineCurrent, oldLineState | stateIdDialog);
            }
            else
			if (ch == '{')
            {
				++levelCurrent;
			}
            else
            if (ch == '}')
            {
				--levelCurrent;
			}
		}
        else
		if (atEOL)
        {
			int lev = levelPrev;
            int foldLevel = (levelCurrent & ~SC_FOLDLEVELNUMBERMASK);

            if ((foldLevel > SC_FOLDLEVELBASE) && (visibleChars == 0))
				lev |= SC_FOLDLEVELWHITEFLAG;

            if (levelCurrent > levelPrev)
				lev |= SC_FOLDLEVELHEADERFLAG;

            if (lev != styler.LevelAt(lineCurrent))
				styler.SetLevel(lineCurrent, lev);

			++lineCurrent;
			levelPrev = levelCurrent;
			visibleChars = 0;

            oldLineState = styler.GetLineState(lineCurrent);
            oldLineState &= (~stateLineMask); // clear old function line states
        }
        else
		if (!isspacechar(ch))
        {
			++visibleChars;
        }
	}

    // Fill in the real level of the next line, keeping the current flags as they will be filled in later
	int flagsNext = styler.LevelAt(lineCurrent) & ~SC_FOLDLEVELNUMBERMASK;
	styler.SetLevel(lineCurrent, levelPrev | flagsNext);
}

static const char * const dclWordListDesc[] = {
	"Functions and special operators",
	"Keywords",
	0
};

static bool registerDclLexer()
{
    static LexerModule lmDCL(SCLEX_DCL, ColouriseDclDoc, "DCL", FoldDclDoc, dclWordListDesc);
    Catalogue::AddLexerModule(&lmDCL);
    return true;
}
static const bool dummy = registerDclLexer();


#else

#define SCE_LISP_CHARACTER 29
#define SCE_LISP_MACRO 30
#define SCE_LISP_MACRO_DISPATCH 31

static inline bool isLispoperator(char ch) {
	if (IsASCII(ch) && isalnum(ch))
		return false;
	if (ch == '\'' || ch == '`' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}')
		return true;
	return false;
}

static inline bool isLispwordstart(char ch) {
	return IsASCII(ch) && ch != ';'  && !isspacechar(ch) && !isLispoperator(ch) &&
		ch != '\n' && ch != '\r' &&  ch != '\"';
}


static void classifyWordLisp(Sci_PositionU start, Sci_PositionU end, WordList &keywords, WordList &keywords_kw, Accessor &styler) {
	assert(end >= start);
	char s[100];
	Sci_PositionU i;
	bool digit_flag = true;
	for (i = 0; (i < end - start + 1) && (i < 99); i++) {
		s[i] = styler[start + i];
		s[i + 1] = '\0';
		if (!isdigit(s[i]) && (s[i] != '.')) digit_flag = false;
	}
	char chAttr = SCE_LISP_IDENTIFIER;

	if(digit_flag) chAttr = SCE_LISP_NUMBER;
	else {
		if (keywords.InList(s)) {
			chAttr = SCE_LISP_KEYWORD;
		} else if (keywords_kw.InList(s)) {
			chAttr = SCE_LISP_KEYWORD_KW;
		} else if ((s[0] == '*' && s[i-1] == '*') ||
			   (s[0] == '+' && s[i-1] == '+')) {
			chAttr = SCE_LISP_SPECIAL;
		}
	}
	styler.ColourTo(end, chAttr);
	return;
}


static void ColouriseLispDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, WordList *keywordlists[],
                            Accessor &styler) {

	WordList &keywords = *keywordlists[0];
	WordList &keywords_kw = *keywordlists[1];

	styler.StartAt(startPos);

	int state = initStyle, radix = -1;
	char chNext = styler[startPos];
	Sci_PositionU lengthDoc = startPos + length;
	styler.StartSegment(startPos);
	for (Sci_PositionU int i = startPos; i < lengthDoc; ++i)
    {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);

		bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (styler.IsLeadByte(ch))
        {
			chNext = styler.SafeGetCharAt(i + 2);
			i += 1;
			continue;
		}

		if (state == SCE_LISP_DEFAULT) {
			if (ch == '#') {
				styler.ColourTo(i - 1, state);
				radix = -1;
				state = SCE_LISP_MACRO_DISPATCH;
			}
            else
            if (ch == ':' && isLispwordstart(chNext)) {
				styler.ColourTo(i - 1, state);
				state = SCE_LISP_SYMBOL;
			}
            else
            if (isLispwordstart(ch)) {
				styler.ColourTo(i - 1, state);
				state = SCE_LISP_IDENTIFIER;
			}
			else
            if (ch == ';') {
				styler.ColourTo(i - 1, state);
				state = SCE_LISP_COMMENT;
			}
			else
            if (isLispoperator(ch)) // || ch=='\'') {
				styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
if (ch=='\'' && isLispwordstart(chNext))
					state = SCE_LISP_SYMBOL;
			}
			else
            if (ch == '\"') {
				styler.ColourTo(i - 1, state);
				state = SCE_LISP_STRING;
			}
		}
        else
        if (state == SCE_LISP_IDENTIFIER || state == SCE_LISP_SYMBOL)
        {
			if (!isLispwordstart(ch))
            {
				if (state == SCE_LISP_IDENTIFIER)
                {
					classifyWordLisp(styler.GetStartSegment(), i - 1, keywords, keywords_kw, styler);
				}
                else
                {
					styler.ColourTo(i - 1, state);
				}
				state = SCE_LISP_DEFAULT;
			} /*else*/
			if (isLispoperator(ch)) // || ch=='\'')
            {
				styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
if (ch=='\'' && isLispwordstart(chNext))
					state = SCE_LISP_SYMBOL;
			}
		}
        else
        if (state == SCE_LISP_MACRO_DISPATCH)
        {
			if (!(IsASCII(ch) && isdigit(ch)))
            {
				if (ch != 'r' && ch != 'R' && (i - styler.GetStartSegment()) > 1) {
					state = SCE_LISP_DEFAULT;
				} else {
					switch (ch) {
						case '|': state = SCE_LISP_MULTI_COMMENT; break;
						case 'o':
						case 'O': radix = 8; state = SCE_LISP_MACRO; break;
						case 'x':
						case 'X': radix = 16; state = SCE_LISP_MACRO; break;
						case 'b':
						case 'B': radix = 2; state = SCE_LISP_MACRO; break;
						case '\\': state = SCE_LISP_CHARACTER; break;
						case ':':
						case '-':
						case '+': state = SCE_LISP_MACRO; break;
						case '\'': if (isLispwordstart(chNext)) {
								   state = SCE_LISP_SPECIAL;
							   } else {
								   styler.ColourTo(i - 1, SCE_LISP_DEFAULT);
								   styler.ColourTo(i, SCE_LISP_OPERATOR);
								   state = SCE_LISP_DEFAULT;
							   }
							   break;
						default: if (isLispoperator(ch)) {
								 styler.ColourTo(i - 1, SCE_LISP_DEFAULT);
								 styler.ColourTo(i, SCE_LISP_OPERATOR);
							 }
							 state = SCE_LISP_DEFAULT;
							 break;
					}
				}
			}
		}
        else
        if (state == SCE_LISP_MACRO)
        {
			if (isLispwordstart(ch) && (radix == -1 || IsADigit(ch, radix))) {
				state = SCE_LISP_SPECIAL;
			} else {
				state = SCE_LISP_DEFAULT;
			}
		}
        else
        if (state == SCE_LISP_CHARACTER)
        {
			if (isLispoperator(ch)) {
				styler.ColourTo(i, SCE_LISP_SPECIAL);
				state = SCE_LISP_DEFAULT;
			} else if (isLispwordstart(ch)) {
				styler.ColourTo(i, SCE_LISP_SPECIAL);
				state = SCE_LISP_SPECIAL;
			} else {
				state = SCE_LISP_DEFAULT;
			}
		}
        else
        if (state == SCE_LISP_SPECIAL)
        {
			if (!isLispwordstart(ch) || (radix != -1 && !IsADigit(ch, radix))) {
				styler.ColourTo(i - 1, state);
				state = SCE_LISP_DEFAULT;
			}
			if (isLispoperator(ch)) // || ch=='\'') {
				styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_LISP_OPERATOR);
if (ch=='\'' && isLispwordstart(chNext))
					state = SCE_LISP_SYMBOL;
			}
		}
        else
        {
			if (state == SCE_LISP_COMMENT)
            {
				if (atEOL) {
					styler.ColourTo(i - 1, state);
					state = SCE_LISP_DEFAULT;
				}
			}
            else
            if (state == SCE_LISP_MULTI_COMMENT)
            {
				if (ch == '|' && chNext == '#') {
					++i;
					chNext = styler.SafeGetCharAt(i + 1);
					styler.ColourTo(i, state);
					state = SCE_LISP_DEFAULT;
				}
			}
            else if (state == SCE_LISP_STRING)
            {
				if (ch == '\\') {
					if (chNext == '\"' || chNext == '\'' || chNext == '\\') {
						i++;
						chNext = styler.SafeGetCharAt(i + 1);
					}
				} else if (ch == '\"') {
					styler.ColourTo(i, state);
					state = SCE_LISP_DEFAULT;
				}
			}
		}

	}
	styler.ColourTo(lengthDoc - 1, state);
}

static void FoldLispDoc(Sci_PositionU startPos, Sci_Position length, int /* initStyle */, WordList *[],
                            Accessor &styler) {
	Sci_PositionU lengthDoc = startPos + length;
	int visibleChars = 0;
	Sci_Position lineCurrent = styler.GetLine(startPos);
	int levelPrev = styler.LevelAt(lineCurrent) & SC_FOLDLEVELNUMBERMASK;
	int levelCurrent = levelPrev;
	char chNext = styler[startPos];
	int styleNext = styler.StyleAt(startPos);
	for (Sci_PositionU i = startPos; i < lengthDoc; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		int style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (style == SCE_LISP_OPERATOR) {
			if (ch == '(' || ch == '[' || ch == '{') {
				levelCurrent++;
			} else if (ch == ')' || ch == ']' || ch == '}') {
				levelCurrent--;
			}
		}
		if (atEOL) {
			int lev = levelPrev;
			if (visibleChars == 0)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if ((levelCurrent > levelPrev) && (visibleChars > 0))
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelPrev = levelCurrent;
			visibleChars = 0;
		}
		if (!isspacechar(ch))
			visibleChars++;
	}
	// Fill in the real level of the next line, keeping the current flags as they will be filled in later
	int flagsNext = styler.LevelAt(lineCurrent) & ~SC_FOLDLEVELNUMBERMASK;
	styler.SetLevel(lineCurrent, levelPrev | flagsNext);
}

#endif

static const char * const lispWordListDesc[] = {
	"Functions and special operators",
	"Keywords",
	"User Functions",
	0
};

LexerModule lmLISP(SCLEX_LISP, ColouriseLispDoc, "lisp", FoldLispDoc, lispWordListDesc);
