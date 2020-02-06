#!/usr/bin/python
#
# NOTE: $.something won't be converted, and will result in parsing error =]
#

# else pode sim ser seguido de identifier em (...  if ... else IDENTIFIER)

import sys
import os
import collections
import io
import re
import tokenize
import cbor

def ASSERT_EQ(a, b):
    assert a == b, (a, b)

def is_blank(_):
    return all((_ in ' \n\r\t\v') for _ in _)

def log(fmt, *values):
    print(f'{LOG_COLOR}{fmt.replace("%s", f"{COLOR_CYAN}%s{LOG_COLOR}")}{COLOR_RESET}' % values)

def dbg(fmt, *values):
    print(f'{DBG_COLOR}{fmt}{COLOR_RESET}' % values)

def err(fmt, *values):
    print(f'{ERR_COLOR}{fmt}{COLOR_RESET}' % values)

if True:
    def dbg(*_):
        pass

TOKEN_ENCODING  = tokenize.ENCODING
TOKEN_ERROR     = tokenize.ERRORTOKEN
TOKEN_OPERATOR  = tokenize.OP
TOKEN_NAME      = tokenize.NAME
TOKEN_NUMBER    = tokenize.NUMBER
TOKEN_STRING    = tokenize.STRING
TOKEN_NEWLINE   = tokenize.NEWLINE
TOKEN_NL        = tokenize.NL
TOKEN_DEDENT    = tokenize.DEDENT
TOKEN_INDENT    = tokenize.INDENT
TOKEN_COMMENT   = tokenize.COMMENT
TOKEN_ENDMARKER = tokenize.ENDMARKER

tokensNames = {
    TOKEN_ENCODING : 'ENCODING',
    TOKEN_NAME     : 'NAME',
    TOKEN_NUMBER   : 'NUMBER',
    TOKEN_STRING   : 'STRING',
    TOKEN_OPERATOR : 'OP',
    TOKEN_NEWLINE  : 'NEWLINE',
    TOKEN_NL       : 'NL',
    TOKEN_DEDENT   : 'DEDENT',
    TOKEN_INDENT   : 'INDENT',
    TOKEN_ENDMARKER: 'ENDMARKER',
    TOKEN_COMMENT  : 'COMMENT',
    TOKEN_ERROR    : 'ERRORTOKEN',
    }

# palavras especiais que não podem aparecer como identifier
#'and', 'as', 'assert', 'async', 'await', 'break', 'class', 'continue', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'has', 'if', 'import', 'in', 'is', 'lambda', 'nonlocal', 'not', 'or', 'pass', 'raise', 'return', 'try', 'while', 'with', 'yield' '~', '+', '-', '/', '*', '%', '&', '|', '^', '~', '.', ',', '==', '**'
# 'bool', 'bytearray', 'bytes', 'dict', 'float', 'int', 'list', 'object', 'set', 'str', 'tuple'

# 'None',
# 'False',
# 'True',
# 'BaseException',
# 'Exception',
# 'TimeoutError',
# 'KeyError',

# '__import__',
# 'hasattr',
# 'abs',
# 'isclass',
# 'isinstance',
# 'issubclass',
# 'locals',
# 'all',
# 'any',
# 'ascii',
# 'bin',
# 'breakpoint',
# 'callable',
# 'chr',
# 'classmethod',
# 'compile',
# 'complex',
# 'delattr',
# 'dir',
# 'divmod',
# 'enumerate',
# 'eval',
# 'exec',
# 'filter',
# 'format',
# 'frozenset',
# 'getattr',
# 'globals',
# 'hasattr',
# 'hash',
# 'help',
# 'hex',
# 'id',
# 'input',
# 'isinstance',
# 'issubclass',
# 'iter',
# 'len',
# 'locals',
# 'map',
# 'max',
# 'memoryview',
# 'min',
# 'next',
# 'oct',
# 'open',
# 'ord',
# 'pow',
# 'print',
# 'property',
# 'range',
# 'repr',
# 'reversed',
# 'round',
# 'setattr',
# 'slice',
# 'sorted',
# 'staticmethod',
# 'sum',
# 'super',
# 'type',
# 'vars',
# 'zip',

# 'os',
# 'sys',
# 'random',
# 'time',
# 'asyncio',
# 'aiohttp',
# 'websockets',
# 'collections',
# 'lxml',


# TODO: FIXME: erro se IDENTIFIER seguido de um nome, que não seja @ ou $, e o último não tenha sido $ ou @

# certos builtins podem começar a palavra, mas não continuar
COLOR_RED         = '\x1b[31m'
COLOR_GREEN       = '\x1b[32m'
COLOR_YELLOW      = '\x1b[33m'
COLOR_BLUE        = '\x1b[34m'
COLOR_PURPLE      = '\x1b[35m'
COLOR_CYAN        = '\x1b[36m'
COLOR_WHITE       = '\x1b[37m'
COLOR_BOLD        = '\x1b[1m'
COLOR_RESET       = '\x1b[0m'

COLOR_RED_BOLD    =     f'{COLOR_RED}{COLOR_BOLD}'
COLOR_GREEN_BOLD  =   f'{COLOR_GREEN}{COLOR_BOLD}'
COLOR_YELLOW_BOLD =  f'{COLOR_YELLOW}{COLOR_BOLD}'
COLOR_BLUE_BOLD   =    f'{COLOR_BLUE}{COLOR_BOLD}'
COLOR_PURPLE_BOLD =  f'{COLOR_PURPLE}{COLOR_BOLD}'
COLOR_CYAN_BOLD   =    f'{COLOR_CYAN}{COLOR_BOLD}'
COLOR_WHITE_BOLD  =   f'{COLOR_WHITE}{COLOR_BOLD}'

DBG_COLOR  = COLOR_GREEN_BOLD
LOG_COLOR  = COLOR_GREEN_BOLD
WARN_COLOR = COLOR_PURPLE_BOLD
ERR_COLOR  = COLOR_RED_BOLD

# SCRIPT OUTPUT SOURCE_0 SOURCE_1 ... SOURCE_N
assert len(sys.argv) >= 3

# Não pode ter duplicados
# Nenhum pode ser o script em si
# Output não pode ser um source
assert len(sys.argv) == len(set(sys.argv))

outputPath, sourcePaths = sys.argv[1], sys.argv[2:]

# OBS.: NÃO PODE TERMINAR EM $, pois obj$ significaria obj em si
# TODO: FIXME: tem que substituir as constantes :O

def _VALUE(x):
    print(repr(x))
    return x

# def fstringer(fstring):
    # last = None
    # return ''.join((last := (f(x) if last == '{' else x)) for x in re.split(r'([{]|[}])', fstring.replace(r'\{', '\x00'))).replace('\x00', r'\{')

# esse trata todos os nomes dentro de uma f-string
# if x.startswith(('f"', "f'", '"', "'"))

def fstringer(str_):
    # return '(' +
    if str_[0] in ('"', "'"):
        return str_
    return  ' '.join((repr(x).replace('\\\\', '\\') if not x.startswith('{') else 'f' + repr('{' + ''.join((faz(t) if t and t not in '([{' else t) for t in re.split(r'''([\s\[\]\(\)\{\}+%*/-])''', x[1:-1].strip()) ) + '}'))
            for x in re.split(r'([{][^}]*[}])', str_[2:-1]) if x
        )
    # + ')'

# tem que fazer melhor do qu eisso. após isolar o "{ F.efw@.x$[1] }", extrair só os identifiers contidos nele e convertê-los individualmente :S
# por enquant os uporta somente "{ MACRO_NAME }"
### SE FOR PARA SUBSTITUIR POR VALOR, tem qwue retirar o {}
# se for para substituir por nome, manter o {}?

# Ele pode começar com ']', pois pode ter vindo logo após um 'object[0].attr'
# TODO: FIXME: suportamos apenas coisas como f'THIS IS MY {SUPER["man"]} FSTRING'
#   coisas como f'THIS IS MY {SUPER["man"]} FSTRING' vai dar erro
def faz(old):
    # print(repr(old))

    assert '(' not in old
    # assert old

    try:
        last, *old = (_ for _ in re.split('''(['][^']*[']|["][^']*["]|[\s@$.\)\]\}])''', old) if _ and _ not in ' \t\v\r\n')
    except:
        return ' '

    new = ('self' if last == '$' else ('self.__class__' if last == '@' else last))

    # assert new != '.'

    for old in old:
        # print(repr(old), '!!')
        if old == '$':
            assert last != '@'
            if last == '$':
                new += '.parent'
        elif old == '@':
            new += ('.parent' if last == '@' else '.__class__')
        elif old == '.':
            assert last != '.'
            new += old
        elif last in '$@)]}':
            new += '.'
            new += old
        else:
            new += old
        last = old

    return new

# se começa com um . está errado

ASSERT_EQ( faz('$') , 'self' )
ASSERT_EQ( faz('$$') , 'self.parent' )
ASSERT_EQ( faz('$$$') , 'self.parent.parent' )
ASSERT_EQ( faz('$$$$') , 'self.parent.parent.parent' )
ASSERT_EQ( faz('@') , 'self.__class__' )
ASSERT_EQ( faz('@@') , 'self.__class__.parent' )
ASSERT_EQ( faz('@@@') , 'self.__class__.parent.parent' )
ASSERT_EQ( faz('@@@@') , 'self.__class__.parent.parent.parent' )
ASSERT_EQ( faz('object$'), 'object' )
ASSERT_EQ( faz('object0$object1@object3'), 'object0.object1.__class__.object3' )
ASSERT_EQ( faz('$object'), 'self.object' )
ASSERT_EQ( faz('$object@attr[1]@@'), 'self.object.__class__.attr[1].__class__.parent' )
ASSERT_EQ( faz('A$$@B.C.D@E@'), 'A.parent.__class__.B.C.D.__class__.E.__class__' )

ASSERT_EQ( faz('self$'), 'self' )

ASSERT_EQ( fstringer("""f'https://www.stock-world.de/detail/{symbolID}-{(symbolURI if symbolURI else "Vale")}.html'"""), '\'https://www.stock-world.de/detail/\' f\'{symbolID}\' \'-\' f\'{(symbolURI if symbolURI else "Vale")}\' \'.html\'' )
# ASSERT_EQ( fstringer(r'f"io={sid}"'), "('io=' f'{sid}')" )
# ASSERT_EQ( fstringer(r'f"io={$sid}"'), "('io=' f'{self.sid}')")
# ASSERT_EQ( fstringer(r'f"XXXXXX{LOG_COLOR}YYYY"'), "('XXXXXX' '\\x1b[32m\\x1b[1m' 'YYYY')" )
# ASSERT_EQ( fstringer(r"f'ERROR: ARGUMENT #{argN}: UNKNOWN OPTION {name}: {arg}.'"), "('ERROR: ARGUMENT #' f'{argN}' ': UNKNOWN OPTION ' f'{name}' ': ' f'{arg}' '.')" )
# ASSERT_EQ( fstringer('''f"a.b.c {a.b.c} {$} {$$} {$@} {@@} {PARSERTEST0} {$A[B.C['$.@.$$$@@@']@ + D.E + F]@}: {G['H']}"'''), '(\'a.b.c \' f\'{a.b.c}\' \' \' f\'{self}\' \' \' f\'{self.parent}\' \' \' f\'{self.__class__}\' \' \' f\'{self.__class__.parent}\' \' \' \'@@$$${x1+x2+3+$a$b}\' \' \' f"{self.A[B.C[\'$.@.$$$@@@\'].__class__+D.E+F].__class__}" \': \' f"{G[\'H\']}")' )
# ASSERT_EQ( fstringer(r"""f'https://www.bvl.com.pe/includes/{base64.b64encode(nemonico.encode()).rstrip(b"=").decode()}.csv'"""), '(\'https://www.bvl.com.pe/includes/\' f\'{base64.b64encode(nemonico.encode()).rstrip(b"=").decode()}\' \'.csv\')' )
# ASSERT_EQ( fstringer("""f'https://www.euronext.com/sites/www.euronext.com/modules/common/common_listings/custom/nyx_eu_listings/nyx_eu_listings_price_chart/pricechart/pricechart.php?q=intraday_data&from={int(CALENDARTIME() - 24*60*60)*1000}&isin={isin}&mic={mic}&dateFormat=Ymd&locale=UTC'"""), "('https://www.euronext.com/sites/www.euronext.com/modules/common/common_listings/custom/nyx_eu_listings/nyx_eu_listings_price_chart/pricechart/pricechart.php?q=intraday_data&from=' f'{int(CALENDARTIME()-24*60*60)*1000}' '&isin=' f'{isin}' '&mic=' f'{mic}' '&dateFormat=Ymd&locale=UTC')" )
ASSERT_EQ( fstringer("f'https://www.nzx.com/statistics/{symbol}/intraday.json?market_id={self.site.table[symbol].market}'"), "'https://www.nzx.com/statistics/' f'{symbol}' '/intraday.json?market_id=' f'{self.site.table[symbol].market}'" )
ASSERT_EQ( fstringer("f'hostpage={next(@@hostpages)}'"), "'hostpage=' f'{next(self.__class__.parent.hostpages)}'" )

ASSERT_EQ( fstringer(r'"HTTP/1.1\r\n\r\n"'), r'"HTTP/1.1\r\n\r\n"' )
ASSERT_EQ( fstringer(r'f"HTTP/1.1\r\n\r\n"'), r"'HTTP/1.1\r\n\r\n'" )

# TODO: FIXME: fstrings rf'' fr''

def PRINTVERBOSE(*_):
    pass

hash_ = '|'.join((str((_ := os.stat(sys.argv[0])).st_size), str(int(_.st_mtime)), *sourcePaths))

_ = logFmts = sources = None

try:
    _, logFmts, sources = cbor.loads(open('parser-cache', 'rb').read())
except AssertionError:
    err('FAILED TO LOAD CACHE - HASH MISMATCHED')
except FileNotFoundError:
    err('FAILED TO LOAD CACHE - FILE NOT FOUND')
except:
    err('FAILED TO LOAD CACHE')

if _ != hash_:
    err('BAD/OUTDATED CACHE')
    logFmts, sources = [], [['HASH', 'LOGS', 'TOKENS'] for sourcePath in sourcePaths]

LOG_FMT_MAP = {fmt:id_ for id_, fmt in enumerate(logFmts)}

# Carrega primeiro os metadados dos arquivos do diretório
hashes = tuple((f'{sourcePath}|{(sourceSize := ((_ := os.stat(sourcePath)).st_size))}|{int(_.st_mtime)}', sourceSize) for sourcePath in sourcePaths)

# TODO: FIXME: pode acabar ficando com coisas já não usadas mais :/
# LOG_FMT_MAP  { FMT_STRING: id }
# logFmts  [ FMT_STRING, ]

assert len(sources) == len(sourcePaths)

for sourcePath, (sourceHash, sourceSize), cacheEntry in zip(sourcePaths, hashes, sources):

    sourceHashOld, _, _ = cacheEntry

    if sourceHashOld == sourceHash:
        log('SOURCE FILE %s IS UNCHANGED', sourcePath)
        continue

    log('RELOADING SOURCE FILE %s', sourcePath)

    try:
        tokensOrig = open(sourcePath, 'rb').read()
    except FileNotFoundError:
        err('FAILED TO OPEN FILE - FILE NOT FOUND')
        exit(1)
    except:
        err('FAILED TO OPEN/READ FILE')
        raise

    if len(tokensOrig) != sourceSize:
        err('FILE SIZE MISMATCH')
        exit(1)

    tokensOrig = [(line, code, str_) for code, str_, (line, _), *_ in tokenize.tokenize(io.BytesIO(tokensOrig).readline)]

    logs, tokens = [], []

    # É sintaxamente possível haver um identifier?
    identifierIsAllowed = True

    identifier = identifierLast = logLine = logLevel = logOffset = logFMT = logBrackets = MEXERICO = None

    for sourceLine, code, token in tokensOrig:

        try:
            tokenThisName = tokensNames[code]
        except KeyError:
            err('UNKNOWN TOKEN CODE')
            print((code, token, sourceLine))
            exit(1)

        # se o último for um ) então podemos ter um PONTo
        # mas não um identifier direto sem ser $@

        # Para estar começando deveria ter tido algo antes
        assert not (is_blank(token) and code not in (TOKEN_NL, TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_ENDMARKER, TOKEN_ERROR))

        assert code != TOKEN_OPERATOR or token in ('%', '&', '(', ')', '*', '**', '+', ',', '-', '.', '/', ':', ':=', ';', '<', '<=', '=', '==', '!=', '>', '>=', '[', ']', '^', '{', '|', '}', '~', '^=', '|=', '&=', '+=', '-=', '//', '//=', '/=', '*=', '%=', '<<', '>>', '@'), token

        # se é operador tem que ter sido lido como tal
        # assert  code == TOKEN_OPERATOR
        # print(tokenThisName, repr(token), '        ', f'{sourcePath}:{sourceLine}')
        PRINTVERBOSE(f'{sourcePath}:{sourceLine} {tokenThisName} {repr(token)}')

        assert code == TOKEN_OPERATOR or token != '@'
        assert code == TOKEN_ERROR or token != '$'

        assert code == TOKEN_OPERATOR or token not in ('@', '.', '!=', '%', '%=', '&', '&=', '(', '*', '**', '*=', '+', '+=', ',', '-', '-=', '/', '//', '//=', '/=', ':', ':=', ';', '<', '<<', '<=', '=', '==', '>', '>=', '>>', '[', ']', '^', '^=', '{', '|', '|=', '}', '~')
        assert code != TOKEN_OPERATOR or token in ('@', '.', '!=', '%', '%=', '&', '&=', '(', '*', '**', '*=', '+', '+=', ',', '-', '-=', '/', '//', '//=', '/=', ':', ':=', ';', '<', '<<', '<=', '=', '==', '>', '>=', '>>', '[', ']', '^', '^=', '{', '|', '|=', '}', '~', ')')

        if code == TOKEN_COMMENT:
            dbg('IGNORING COMMENT')
            PRINTVERBOSE(f'   -> SKIPPED')
            continue

        if code == TOKEN_NL:
            dbg('IGNORING NEW LINE')
            PRINTVERBOSE(f'   -> SKIPPED')
            continue

        if code == TOKEN_ENCODING:
            dbg('IGNORING ENCODING')
            PRINTVERBOSE(f'   -> SKIPPED')
            continue

        if code == TOKEN_ERROR and token == ' ':
            dbg('IGNORING')
            PRINTVERBOSE(f'   -> SKIPPED')
            continue

        if code == TOKEN_ENDMARKER: # TODO: FIXME: substituir por newline? :/
            dbg('IGNORING')
            PRINTVERBOSE(f'   -> SKIPPED')
            continue

        if code == TOKEN_STRING:
            if token.startswith('f'):
                dbg('F-STRING')
                token = fstringer(token)
            else:
                dbg('STRING')

        # TODO: FIXME: vai ter que lembrar da quantidade de espaços quebrados entre os elementos do identifier para questões de número de linha? :/
        if identifier is not None: #  or (code == TOKEN_NL and not identifierInners)
            if token in ('assert', 'await', 'class', 'def', 'raise', 'return', 'try', 'while', 'with', 'yield', 'except', 'finally', 'lambda'):
                err('UNEXPECTED TOKEN')
                exit(1)
            if code == TOKEN_NEWLINE or token in ('%', '&', '/=', '//=', '(', ')', '*', '**', '+', ',', '-', '/', ':', ':=', ';', '<', '<=', '=', '==', '!=', '>', '>=', '[', ']', '^', '{', '|', '}', '~', '^=', '|=', '&=', '+=', '-=', '//', '//=', '/=', '*=', '%=', 'and', 'as',  'del', 'elif', 'else', 'for', 'from', 'global', 'if', 'import', 'in', 'is', 'not', 'or', '<<', '>>'):
                dbg('IDENTIFIER ENDED %s', repr(identifier))
                assert identifier

                identifier = faz(identifier)

                if identifier == 'self.__class__.classmethod':
                    identifier = '@classmethod'

                dbg('IDENTIFIER DECODED %s', repr(identifier))

                try:
                    obj, method = identifier.rsplit('.', 1)
                except: # NOTE: quando implementar em C, pode passar None como objeto ao invés de task =]
                    obj, method = 'TASK', identifier

                # TODO: FIXME: _ASSERT() é só em modo debug
                # _ASSERT() é sempre fatal, mas só existe em modo debug
                # ASSERT() é sempre fatal

                if method in ('dbg', 'log', 'warn_dbg', 'warn', 'err_dbg', 'err'):
                    dbg('IDENTIFIER - LOG')
                    assert logLine is logOffset is logLevel is logFMT is logBrackets is None
                    logLine = sourceLine
                    logLevel = ('dbg', 'log', 'warn_dbg', 'warn', 'err_dbg', 'err').index(method)
                # ERROR_ -> def ERROR_(): raise Err  MESMA COISA QU E O LOG MAS TERMINA EM ERROR
                elif identifier == 'isclass': # TODO: FIXME: só se for call isclass()
                    dbg('IDENTIFIER - ISCLASS')
                    MEXERICO=True
                    tokens.extend((
                        (TOKEN_OPERATOR, '('),
                        (TOKEN_OPERATOR, '('),
                        ))
                else: # Insere ele antes deste
                    dbg('IDENTIFIER - NORMAL')
                    tokens.append((TOKEN_NAME, identifier))
                identifierLast = identifier
                identifier = None
            elif token in '.$@':
                dbg('IDENTIFIER CONTINUATION')
                assert token
                identifier += token
            else:
                dbg('IDENTIFIER CONTINUATION - NORMAL WORD')
                assert code == TOKEN_NAME
                assert token.strip()
                assert identifierIsAllowed is True
                identifier += token

        if identifier is None:
            if code == TOKEN_NEWLINE: # TODO: FIXME: ele nao destá deixando ter comentários entre os parenteses de um log() por exemplo
                dbg('NOT STARTING IDENTIFIER - LINE ENDED')
            elif code in (TOKEN_INDENT, TOKEN_DEDENT):
                dbg('NOT STARTING IDENTIFIER - IDENTATION')
            elif code == TOKEN_STRING:
                dbg('NOT STARTING IDENTIFIER - STRING')
            elif code == TOKEN_NUMBER:
                dbg('NOT STARTING IDENTIFIER - NUMBER')
            elif token == '.':
                dbg('NOT STARTING IDENTIFIER - DOT')
                # if # TODO: FIXME: pode estar continuando uma strng, bytes, etc :/
                    # err('?')
                    # exit(1)
            elif token in ('!=', '%', '%=', '&', '&=', '(', '*', '**', '*=', '+', '+=', ',', '-', '-=', '.', '/=', '/', '//', '//=', ':', ':=', ';', '<', '<<', '<<=', '<=', '=', '==', '>', '>=', '>>', '>>=', '[', '^', '^=', 'and', 'as', 'assert', 'async', 'await', 'break', 'class', 'continue', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'has', 'if', 'import', 'in', 'is', 'lambda', 'nonlocal', 'not', 'or', 'pass', 'raise', 'return', 'try', 'while', 'with', 'yield', '{', '|', '|=', '~'):
                dbg('NOT STARTING IDENTIFIER - KEYWORD/OPERATOR')
            else:
                dbg('STARTING IDENTIFIER')
                assert code == TOKEN_NAME or token in ')]}$@'
                assert token
                identifier = token

        if code == TOKEN_STRING:
            dbg('PASS2 - STRING')
            identifierIsAllowed = None
            identifierIsAllowedShortcut = None
        elif code == TOKEN_NEWLINE:
            dbg('PASS2 - NEW LINE')
            assert logLine is logOffset is logLevel is logFMT is logBrackets is None
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
        elif code in (TOKEN_INDENT, TOKEN_DEDENT):
            dbg('PASS2 - IDENTATION')
        elif token == '.':
            dbg('PASS2 - DOT')
            identifierIsAllowed = True
            identifierIsAllowedShortcut = None
        elif token in ')]}':
            dbg('PASS2 - BRACKETS CLOSE')
            assert token
            identifierIsAllowed = None
            identifierIsAllowedShortcut = True
        elif token in ('!=', '%', '%=', '&', '&=', '(', '*', '**', '*=', '+', '+=', ',', '-', '-=', '/', '//', '//=', '/=', ':', ':=', ';', '<', '<<', '<=', '=', '==', '>', '>=', '>>', '[', '^', '^=', 'and', 'as', 'assert', 'await', 'class', 'def', 'del', 'elif', 'for', 'from', 'global', 'has', 'if', 'import', 'in', 'is', 'nonlocal', 'not', 'or', 'raise', 'return', 'while', '{', '|', '|=', '~', 'except', 'else', 'yield', 'with', 'lambda'):
            dbg('PASS2 - PRE IDENTIFIER')
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
        elif code == TOKEN_NAME and token in ('async', 'break', 'continue', 'finally', 'try', 'pass'):
            dbg('PASS2 - PRE NO-IDENTIFIER')
            identifierIsAllowed = None
            identifierIsAllowedShortcut = None
        elif code == TOKEN_NUMBER:
            dbg('PASS2 - NUMBER')
            identifierIsAllowed = None
            identifierIsAllowedShortcut = None
        elif code == TOKEN_NAME:
            dbg('PASS2 - NAME')
            assert identifierIsAllowed is True
            identifierIsAllowed = None
            identifierIsAllowedShortcut = True
        elif token in '$@':
            dbg('PASS2 - SHORTCUT')
            assert token
            assert identifierIsAllowedShortcut is True
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
        else:
            err('PASS2 - UNKNOWN')
            exit(1)

        if MEXERICO is True:
            dbg('ISCLASS')
            if token == ')':
                dbg('IS CLASS ENDED')
                MEXERICO = None
                tokens.extend((
                    (TOKEN_OPERATOR, ')'),
                    (TOKEN_OPERATOR, ')'),
                    (TOKEN_OPERATOR, '.'),
                    (TOKEN_NAME, '__class__'),
                    (TOKEN_NAME, 'is'),
                    (TOKEN_NAME, 'type'),
                    ))

        if logLine is not None:
            if logBrackets is None:
                dbg('LOG CALL STARTED')
                assert logLine >= 1
                assert logLevel >= 0
                assert logFMT is None
                assert logOffset is None
                assert token == '('
                logBrackets = ['(']
                logOffset = len(tokens) + 2
                tokens.extend(((TOKEN_NAME, 'LOG_'), (TOKEN_OPERATOR, '('), None, (TOKEN_OPERATOR, ','), (TOKEN_NAME, obj)))
                token = None
            elif logFMT is None:
                dbg('LOG FMT')
                assert logLine >= 1
                assert logLevel >= 0
                assert logFMT is None
                assert code == TOKEN_STRING
                assert token.startswith(('"', "'")) # Cannot be an f-string or bytes, or anything else
                token = eval(token)
                try:
                    logFMT = LOG_FMT_MAP[token]
                except:
                    logFMT = LOG_FMT_MAP[token] = len(LOG_FMT_MAP)
                    logFmts.append(token)
                token = None
            elif token in '([{':
                dbg('LOG OPEN BRACKET')
                assert logLine >= 1
                assert logLevel >= 0
                assert logFMT is not None
                logBrackets.append(token)
            elif token in ')]}':
                dbg('LOG CLOSE BRACKET')
                assert logLine >= 1
                assert logFMT is not None
                assert logOffset >= 1
                assert logBrackets.pop() == '([{'[')]}'.index(token)]
                if not logBrackets:
                    dbg('LOG CALL ENDED')
                    logs.append((logOffset, logLine, logLevel, logFMT))
                    logLine = logLevel = logOffset = logFMT = logBrackets = None
            else:
                dbg('LOG INSIDER')

        # raise (Err if LOG_() else Err)
        if identifier is None:
            if code is not None and token is not None:
                PRINTVERBOSE(f'   -> TOKENS')
                tokens.append((code, token))
            else:
                PRINTVERBOSE(f'   -> DROPPED')
        else:
            PRINTVERBOSE(f'   -> IDENTIFIER')

    cacheEntry[0] = sourceHash
    cacheEntry[1] = logs # [(offset,line,level,fmtID)]
    cacheEntry[2] = tokens

open('parser-cache.tmp', 'wb').write(cbor.dumps((hash_, logFmts, sources)))

os.rename('parser-cache.tmp', 'parser-cache')

logX, tokens = [], [
    (TOKEN_ENCODING, 'utf-8'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    (TOKEN_NEWLINE, '\n'),
    ]

for fileID, (_, logs, tokens_) in enumerate(sources):
    for offset, line, level, fmtID in logs:
        tokens_[offset] = (TOKEN_NUMBER, str(len(logX)))
        logX.append((fileID, line, level, fmtID))
    tokens.extend(tokens_)

log(f'TOKENS: {COLOR_CYAN}%d' % len(tokens))

# TODO: FIXME: ficará ainda menor se colocar uma lisa de inteiros direto :/ ao  invés d elist ad etuples d einteros

tokens[1] = TOKEN_NAME, f'logFiles = {tuple(sourcePaths)}'
tokens[3] = TOKEN_NAME, f'logFmts = {tuple(logFmts)}'
tokens[5] = TOKEN_NAME, f'logX = {tuple(logX)}'

tokens = tokenize.untokenize(tokens)

log(f'SIZE: {COLOR_CYAN}%d' % len(tokens))

log(f'OUTPUTING TO FILE {COLOR_CYAN}%s' % outputPath)

# TODO: FIXME: autoexecuta o CPP
fd = os.open(outputPath, os.O_WRONLY | os.O_CREAT | os.O_CLOEXEC | os.O_EXCL | os.O_NOATIME | os.O_NOCTTY | os.O_NOFOLLOW, 0o0644)
assert fd >= 0
assert os.write(fd, tokens) == len(tokens)
os.close(fd)

# TODO: FIXME: usa o grep para procurar possíveis erros tb
# TODO: FIXME: converteos replacements e macros
# TODO: FIXME: gera o info

# TODO: FIXME: teoricamente seria possível u sar uma linked list dos tokens e ir executando regras :/

# ou pelo menos carregar todos os arquivos em um único deque, inserindo  amarca fFILEEND  ou deixa ro filestart
# começar o deque com um deque((ENCODING,))
# e simplesmente retirar eles

# TODO: FIXME: issubclass() -> checar isclass primeiro


# TODO: FIXME: um checksum no parser em si, e salvar o cache com este checksum
# TODO: FIXME: pré-carregar tudo; coisas como errro, deixar uma marca TOKEN_ERROR, TOKEN_LOG, etc apenas para ocupar o espaco do código, linha, etc =]
# TODO: FIXME: não carregar comentários

# TODO: FIXME: binary f-strings? :/

# TODO: FIXME: $(SOMETHING) -> sef.(SOMETHING)  NAÕ EXISTE, ntão dar erro =]

# TODO: FIXME: um LOG_() que aceita valores e outro que não


# TODO: FIXME: salvar onde começa cada arquivo para depois processar o linter
