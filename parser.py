#!/usr/bin/python
#
# NOTE: $.something won't be converted, and will result in parsing error =]
#

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
    print(f'{LOG_COLOR}{fmt}{COLOR_RESET}' % values)

def err(fmt, *values):
    print(f'{ERR_COLOR}{fmt}{COLOR_RESET}' % values)

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
BUILTIN_TYPES = 'bool', 'bytearray', 'bytes', 'dict', 'float', 'int', 'list', 'object', 'set', 'str', 'tuple'

BUILTIN_CONSTANTS = (
    'None',
    'False',
    'True',
    'BaseException',
    'Exception',
    'TimeoutError',
    'KeyError',
    )

BUILTIN_FUNCTIONS = (
    '__import__',
    'hasattr',
    'abs',
    'isclass',
    'isinstance',
    'issubclass',
    'locals',
    'all',
    'any',
    'ascii',
    'bin',
    'breakpoint',
    'callable',
    'chr',
    'classmethod',
    'compile',
    'complex',
    'delattr',
    'dir',
    'divmod',
    'enumerate',
    'eval',
    'exec',
    'filter',
    'format',
    'frozenset',
    'getattr',
    'globals',
    'hasattr',
    'hash',
    'help',
    'hex',
    'id',
    'input',
    'isinstance',
    'issubclass',
    'iter',
    'len',
    'locals',
    'map',
    'max',
    'memoryview',
    'min',
    'next',
    'oct',
    'open',
    'ord',
    'pow',
    'print',
    'property',
    'range',
    'repr',
    'reversed',
    'round',
    'setattr',
    'slice',
    'sorted',
    'staticmethod',
    'sum',
    'super',
    'type',
    'vars',
    'zip',
    )

MODULES = (
    'os',
    'sys',
    'random',
    'time',
    'asyncio',
    'aiohttp',
    'websockets',
    'collections',
    'lxml',
    )

# TODO: FIXME: erro se IDENTIFIER seguido de um nome, que não seja @ ou $, e o último não tenha sido $ ou @

# não podem começar um identifier
# então não podem vir antes do . e nem serem considerados palavras
# '0123456789'

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

# substitui um nome por alguma outra coisa, da forma como está esse valor
# substitui um nome por um valor constante -> repr(value)

# Tem que resolver de baixo para cima
# UM -> 1
# DOIS -> 2
# TRES -> UM + DOIS
# print(TRES) -> 3

#constants = {k:repr(v) for k, v in constants.items()}

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
    print(f'{sourcePath}:{sourceLine}', *_)

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

    log(f'CHECKING SOURCE {COLOR_CYAN}%s' % sourcePath)

    if sourceHashOld == sourceHash:
        log('UNCHANGED')
        continue

    log('LOADING FILE')

    try:
        tokensOrig = open(sourcePath, 'rb').read()
        assert len(tokensOrig) == sourceSize
    except FileNotFoundError:
        err('FAILED TO LOAD FILE - FILE NOT FOUND')
        exit(1)
    except:
        err('FAILED TO LOAD FILE')
        raise

    log('TOKENIZING')

    tokensOrig = [(line, code, str_) for code, str_, (line, _), *_ in tokenize.tokenize(io.BytesIO(tokensOrig).readline)]

    logs, tokens = [], []

    lastLine = lastCode = lastStr = identifier = None

    # É sintaxamente possível haver um identifier?
    identifierIsAllowed = True

    puttingLog = None
    puttingLogGotFMT = None

    MEXERICO = None

    errorBrackets= errorStatus = None

    for sourceLine, code, str_ in tokensOrig:

        try:
            tokenThisName = tokensNames[code]
        except KeyError:
            err('UNKNOWN TOKEN CODE')
            print((code, str_, sourceLine))
            exit(1)

        #  O que acrescentar agora
        putCode, putStr = code, str_

        assert (lastStr, str_) not in (
            ('.', '.'), ('.', ','), (',', '.'),
            ('.', '('), ('.', '{'), ('.', '['),
            ('.', '"'), ('.', "'"),
            ('[', '}'), ('[', ')'),
            ('{', ']'), ('{', ')'),
            ('(', ']'), ('(', '}'),
            )

        # se o último for um ) então podemos ter um PONTo
        # mas não um identifier direto sem ser $@
        #assert not (lastCode != TOKEN_NAME and str_ == '.')

        # Para estar começando deveria ter tido algo antes
        assert not (lastStr == '.' and str_ in ('$', '@', 'class', 'def', 'assert', 'try', 'continue', 'except'))

        assert not (is_blank(str_) and code not in (TOKEN_NL, TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_ENDMARKER, TOKEN_ERROR))

        assert code != TOKEN_OPERATOR or str_ in ('%', '&', '(', ')', '*', '**', '+', ',', '-', '.', '/', ':', ':=', ';', '<', '<=', '=', '==', '!=', '>', '>=', '[', ']', '^', '{', '|', '}', '~', '^=', '|=', '&=', '+=', '-=', '//', '//=', '/=', '*=', '%=', '<<', '>>', '@'), str_

        # TODO: FIXME: teria que ignorar espaços entre uma "coisa. e.outra" -> acumular todos os espaços até en contrar um (, etc??
        # TODO: FIXME: se está entre parenteses, entao newlines não quebram nomes :/
        #   vou deixar do jeito que está... e esperar que coisas como algo.seila\noutra_coisa deem erro aqui com os asserts

        # Depois de operadores e dessas keywords, vai poder começar um identifier

        # se é operador tem que ter sido lido como tal
        # assert  code == TOKEN_OPERATOR
        PRINTVERBOSE(tokenThisName, '-->', repr(str_))

        if code == TOKEN_COMMENT:
            PRINTVERBOSE('COMMENT', repr(str_))
            continue

        # TODO: FIXME: vai ter que lembrar da quantidade de espaços quebrados entre os elementos do identifier para questões de número de linha? :/
        if identifier is not None: #  or (code == TOKEN_NL and not identifierInners)
            if code == TOKEN_NEWLINE or str_ in ('%', '&', '(', ')', '*', '**', '+', ',', '-', '/', ':', ':=', ';', '<', '<=', '=', '==', '!=', '>', '>=', '[', ']', '^', '{', '|', '}', '~', '^=', '|=', '&=', '+=', '-=', '//', '//=', '/=', '*=', '%=', 'and', 'as', 'assert', 'await', 'class', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'if', 'import', 'in', 'is', 'not', 'or', 'raise', 'return', 'try', 'while', 'with', 'yield', '<<', '>>', 'lambda'):
                PRINTVERBOSE('IDENTIFIER ALREADY ENDED', repr(identifier))
                assert identifier

                identifier = faz(identifier)

                if identifier == 'self.__class__.classmethod':
                    identifier = '@classmethod'

                PRINTVERBOSE('JOINED', repr(identifier))

                *obj, method = identifier.rsplit('.', 1)
                # NOTE: quando implementar em C, pode passar None como objeto ao invés de task =]
                obj, = (obj if obj else ('TASK',))

                # TODO: FIXME: _ASSERT() é só em modo debug
                # _ASSERT() é sempre fatal, mas só existe em modo debug
                # ASSERT() é sempre fatal

                if method in ('dbg', 'log', 'warn', 'err'):
                    PRINTVERBOSE('LOG')
                    assert code != TOKEN_COMMENT
                    puttingLog = len(tokens) + 2, sourceLine, ('dbg', 'log', 'warn', 'err').index(method)
                    puttingLogGotFMT = False
                    puttingLogError = None
                    tokens.extend((
                        (TOKEN_NAME, 'LOG_'),
                        (TOKEN_OPERATOR, '('),
                        None,
                        (TOKEN_OPERATOR, ','),
                        (TOKEN_NAME, obj),
                        ))
                    putCode = None

                # ERROR_ -> def ERROR_(): raise Err  MESMA COISA QU E O LOG MAS TERMINA EM ERROR
                # # # # # # # # # # # # # # # elif method == 'error':
                    # # # # # # # # # # # # # # # PRINTVERBOSE('ERROR !!!!!!!!!!!!')
                    # # # # # # # # # # # # # # # assert code != TOKEN_COMMENT
                    # # # # # # # # # # # # # # # puttingLog = len(tokens) + 2, sourceLine, 3
                    # # # # # # # # # # # # # # # puttingLogGotFMT = False
                    # # # # # # # # # # # # # # # puttingLogError = True
                    # # # # # # # # # # # # # # # # # errorStatus = 'AFTER_KEYWORD'
                    # # # # # # # # # # # # # # # errorBrackets = []
                    # # # # # # # # # # # # # # # tokens.extend((
                        # # # # # # # # # # # # # # # (TOKEN_NAME, 'LOG_'),
                        # # # # # # # # # # # # # # # (TOKEN_OPERATOR, 'PARENT'),
                        # # # # # # # # # # # # # # # None,
                        # # # # # # # # # # # # # # # (TOKEN_OPERATOR, ','),
                        # # # # # # # # # # # # # # # (TOKEN_NAME, obj),
                        # # # # # # # # # # # # # # # ))
                elif identifier == 'isclass': # TODO: FIXME: só se for call isclass()
                    PRINTVERBOSE('ISCLASS')
                    assert code != TOKEN_COMMENT
                    tokens.extend((
                        (TOKEN_OPERATOR, '('),
                        (TOKEN_OPERATOR, '('),
                        ))
                    MEXERICO=True
                else: # Insere ele antes deste
                    PRINTVERBOSE('NORMAL IDENTIFIER')
                    tokens.append((TOKEN_NAME, identifier))

                if code == TOKEN_NEWLINE or str_ in ('(', '%', '&', '*', '**', '+', ',', '-', '/', ':', ':=', ';', '<', '<=', '=', '==', '!=', '>', '>=', '[', '^', '{', '|', '~', '^=', '|=', '&=', '+=', '-=', '//', '//=', '/=', '*=', '%=', 'and', 'as', 'assert', 'await', 'class', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'if', 'import', 'in', 'is', 'not', 'or', 'raise', 'return', 'try', 'while', 'with', 'yield', '<<', '>>', 'lambda'):
                    identifierIsAllowed = True
                    identifierIsAllowedShortcut = True
                    identifier =  None
                elif str_ in ')]}':
                    identifierIsAllowed = False
                    identifierIsAllowedShortcut = True
                    identifier = str_
                    putCode = None
                else:
                    assert False

                # TODO: FIXME: tem que quebrar essas inserções em tokens
            elif code == TOKEN_NL:
                PRINTVERBOSE('CONTINUING IDENTIFIER - NEW LINE')
            elif str_ == '.':
                PRINTVERBOSE('CONTINUING IDENTIFIER - DOT')
                identifier += str_
                identifierIsAllowed = True
                identifierIsAllowedShortcut is True
                putCode = None
            elif str_ in '$@':
                PRINTVERBOSE('CONTINUING IDENTIFIER - SHORTCUT')
                assert (code, str_) in ((TOKEN_ERROR, '$'), (TOKEN_OPERATOR, '@'))
                identifier += str_
                identifierIsAllowed = True
                identifierIsAllowedShortcut is True
                putCode = None
            elif code == TOKEN_ERROR and str_ == ' ':
                pass
            else:
                PRINTVERBOSE('CONTINUING IDENTIFIER - NORMAL WORD')
                assert str_.strip()
                assert identifierIsAllowed is True
                assert code == TOKEN_NAME
                identifier += str_
                identifierIsAllowed = None
                identifierIsAllowedShortcut is True
                putCode = None # A palavra está nesse nosso buffer temporário; não escreve ela ainda
        elif code == TOKEN_NUMBER:
            PRINTVERBOSE('NUMBER')
            assert identifier is None
            identifierIsAllowed = None
            identifierIsAllowedShortcut = None
        elif code == TOKEN_NL:
            PRINTVERBOSE('IGNORING NEW LINE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!')
            assert identifier is None
            putCode = None
        elif code in (TOKEN_INDENT, TOKEN_DEDENT, TOKEN_ENCODING):
            PRINTVERBOSE('BLANK')
            assert identifier is None
        elif str_ in ('!=', '%', '%=', '&', '&=', '(', '*', '**', '*=', '+', '+=', ',', '-', '-=', '.', '/', '//', '//=', ':', ':=', ';', '<', '<<', '<<=', '<=', '=', '==', '>', '>=', '>>', '>>=', '[', '^', '^=', 'and', 'as', 'assert', 'await', 'class', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'if', 'import', 'in', 'is', 'lambda', 'not', 'or', 'raise', 'return', 'while', 'with', 'yield', '{', '|', '|=', '~'):
            PRINTVERBOSE('PRE IDENTIFIER')
            assert identifier is None
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
        elif str_ in ('async', 'await', 'break', 'class', 'continue', 'def', 'del', 'elif', 'else', 'except', 'finally', 'has', 'nonlocal', 'pass', 'return', 'try', 'while'):
            PRINTVERBOSE('NON IDENTIFIER')
            assert identifier is None
            identifierIsAllowed = None
            identifierIsAllowedShortcut = True
        elif str_ in ')]}':
            PRINTVERBOSE('STARTING IDENTIFIER BY BRACKETS')
            assert identifier is None
            identifier = str_
            identifierIsAllowed = None
            identifierIsAllowedShortcut = True
            putCode = None
        elif str_ == '$':
            PRINTVERBOSE('STARTING IDENTIFIER BY $')
            assert code == TOKEN_ERROR
            assert identifier is None
            assert identifierIsAllowedShortcut is True
            identifier = '$'
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
            putCode = None
        elif str_ == '@':
            PRINTVERBOSE('STARTING IDENTIFIER BY @')
            assert code == TOKEN_OPERATOR
            assert identifier is None
            assert identifierIsAllowedShortcut is True
            identifier = '@'
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
            putCode = None
        elif code == TOKEN_NAME:
            PRINTVERBOSE('STARTING IDENTIFIER BY NORMAL WORD')
            assert identifier is None
            assert identifierIsAllowed is True
            identifier = str_
            identifierIsAllowed = None
            identifierIsAllowedShortcut = True
            putCode = None
        elif code == TOKEN_STRING:
            PRINTVERBOSE('STRING')
            if str_.startswith('f'):
                PRINTVERBOSE('FSTRING')
                assert puttingLogGotFMT is None or puttingLogGotFMT is True
                putStr = fstringer(str_)
            elif puttingLogGotFMT is False:
                PRINTVERBOSE('IT IS THE LOG FMT')
                fmt = eval(str_)
                try:
                    fmt = LOG_FMT_MAP[fmt]
                except KeyError:
                    logFmts.append(fmt)
                    fmt = LOG_FMT_MAP[fmt] = len(LOG_FMT_MAP)
                logs.append((*puttingLog, fmt))
                puttingLogGotFMT = True
                putCode = None
            identifierIsAllowed = None
            identifierIsAllowedShortcut = False
        elif code == TOKEN_COMMENT:
            PRINTVERBOSE('COMMENT')
            assert identifier is None
            putCode = None
        elif code == TOKEN_NEWLINE: # TODO: FIXME: ele nao destá deixando ter comentários entre os parenteses de um log() por exemplo
            PRINTVERBOSE('LINE ENDED')
            assert identifier is None
            identifierIsAllowed = True
            identifierIsAllowedShortcut = True
            if puttingLog:
                PRINTVERBOSE('LOG CLOSED')
                assert puttingLogGotFMT is True
                puttingLog = None
                puttingLogGotFMT = None
                puttingLogError = None
            assert puttingLog is None
            assert puttingLogGotFMT is None
        else:
            assert code == TOKEN_ERROR
            assert str_ == ' ' # :/

        if MEXERICO is True and str_ == ')':
            MEXERICO = None
            tokens.extend((
                (TOKEN_OPERATOR, ')'),
                (TOKEN_OPERATOR, ')'),
                (TOKEN_OPERATOR, '.'),
                (TOKEN_NAME, '__class__'),
                (TOKEN_NAME, 'is'),
                (TOKEN_NAME, 'type'),
                ))

        # # # # if errorStatus is not None:
            # # # # if str_ in '([{':
                # # # # if errorStatus is 'AFTER_KEYWORD':
                    # # # # errorStatus = 'INSIDE_CALL'
                    # # # # assert str_ == '('
                # # # # errorBrackets.append(str_)
            # # # # elif str_ in ')]}':
                # # # # assert errorStatus is 'INSIDE_CALL'
                # # # # assert errorBrackets.pop() == '([{'[')]}'.index(str_)]
                # # # # assert identifier == ')'
                # # # # if not errorBrackets:
                    # # # # errorStatus = errorBrackets = putCode = tokens.extend((
                        # # # # (TOKEN_OPERATOR, ';'),
                        # # # # (TOKEN_NAME, 'raise'),
                        # # # # (TOKEN_NAME, 'Err'),
                        # # # # ))

        if putCode is not None:
            tokens.append((putCode, putStr))

        lastCode, lastStr, lastLine = code, str_, sourceLine

    cacheEntry[0] = sourceHash
    cacheEntry[1] = logs # [(offset,line,level,fmtID)]
    cacheEntry[2] = tokens

open('parser-cache.tmp', 'wb').write(cbor.dumps((hash_, logFmts, sources)))

os.rename('parser-cache.tmp', 'parser-cache')

logX, tokens = [], [
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    (TOKEN_NL, '\n'), (TOKEN_NEWLINE, '\n'),
    ]

for fileID, (_, logs, tokens_) in enumerate(sources):
    for offset, line, level, fmtID in logs:
        tokens_[offset] = (TOKEN_NUMBER, str(len(logX)))
        logX.append((fileID, line, level, fmtID))
    tokens.extend(tokens_)

log(f'TOKENS: {COLOR_CYAN}%d' % len(tokens))

# TODO: FIXME: ficará ainda menor se colocar uma lisa de inteiros direto :/ ao  invés d elist ad etuples d einteros

tokens[1] = (TOKEN_NAME, f'logFiles={tuple(sourcePaths)}')
tokens[3] = (TOKEN_NAME, f'logFmts={tuple(logFmts)}')
tokens[5] = (TOKEN_NAME, f'logX={tuple(logX)}')

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
