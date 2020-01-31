#!/usr/bin/python
#
# GOODIE: $.something won't be converted, and will result in parsing error =]
#
import sys
import os
import collections
import io
import re
import tokenize

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
NON_IDENTIFIERS = ('and', 'as', 'assert', 'async', 'await', 'break', 'class', 'continue', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'has', 'if', 'import', 'in', 'is', 'lambda', 'nonlocal', 'not', 'or', 'pass', 'raise', 'return', 'try', 'while', 'with', 'yield'
    '$', '@',
    '[', ']', '(', ')', '{', '}',
    '~', '+', '-', '/', '*', '%', '&', '|', '^', '~', '.', ':', ',', '==', '**',
    'as', 'is', 'from', 'in',
    'def', 'class', 'while',
    'except', 'raise',
    'return', 'yield',
    'assert',
    'del',
    )

KEYWORDS = 'and', 'as', 'assert', 'async', 'await', 'break', 'class', 'continue', 'def', 'del', 'elif', 'else', 'except', 'finally', 'for', 'from', 'global', 'has', 'if', 'import', 'in', 'is', 'lambda', 'nonlocal', 'not', 'or', 'pass', 'raise', 'return', 'try', 'while', 'with', 'yield'

OPERATORS = '[', ']', '(', ')', '{', '}', '+', '-', '/', '*', '%', '&', '|', '^', '~', '.', ':', ',', '==', '**'

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

assert all(((len(set(a) & set(b)) == 0) or print(set(a) & set(b)))
    for a in (KEYWORDS, OPERATORS, BUILTIN_FUNCTIONS, BUILTIN_TYPES, BUILTIN_CONSTANTS, MODULES)
    for b in (KEYWORDS, OPERATORS, BUILTIN_FUNCTIONS, BUILTIN_TYPES, BUILTIN_CONSTANTS, MODULES)
        if a is not b)

def is_keyword(s):
    return s in KEYWORDS

def is_operator(s):
    return s in OPERATORS

def is_constant(s):
    return s in BUILTIN_CONSTANTS

def is_function(s):
    return s in BUILTIN_FUNCTIONS

def is_type(s):
    return s in BUILTIN_TYPES

def is_module(s):
    return s in MODULES

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
REPLACE = {
    'COLOR_BOLD':         COLOR_BOLD,
    'COLOR_BLUE':         COLOR_BLUE,
    'COLOR_CYAN':         COLOR_CYAN,
    'COLOR_CYAN_BOLD':    COLOR_CYAN_BOLD,
    'COLOR_WHITE':        COLOR_WHITE,
    'COLOR_WHITE_BOLD':   COLOR_WHITE_BOLD,
    'COLOR_YELLOW':       COLOR_YELLOW,
    'COLOR_YELLOW_BOLD':  COLOR_YELLOW_BOLD,
    'COLOR_GREEN':        COLOR_GREEN,
    'COLOR_GREEN_BOLD':   COLOR_GREEN_BOLD,
    'COLOR_RED':          COLOR_RED,
    'COLOR_RED_BOLD':     COLOR_RED_BOLD,
    'COLOR_PURPLE':       COLOR_PURPLE,
    'COLOR_PURPLE_BOLD':  COLOR_PURPLE_BOLD,
    'COLOR_RESET':        COLOR_RESET,

    'DBG_COLOR':       COLOR_GREEN_BOLD,
    'LOG_COLOR':       COLOR_GREEN_BOLD,
    'WARN_COLOR':      COLOR_PURPLE_BOLD,
    'ERR_COLOR':       COLOR_RED_BOLD,

    'ALWAYS_AND_FOREVER': f"{COLOR_RED_BOLD}LOVE{COLOR_RESET}",
    'TRES': '(1 + 2)',
    }

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

# TODO: FIXME: só exibir a mensagem e as colunas que mudarem
def PRINT(msg):

    if L0:
        SOURCEFILE_ = f'{COLOR_WHITE_BOLD}' f'%{MSPL}s' % sourcePath[-50:]
        SOURCELINE_ = f'{COLOR_BOLD}{COLOR_WHITE}' f'%{MSLL}d' % sourceLine
        TOKENCODE_  = f'{COLOR_GREEN_BOLD}'  '%-10s' % tokenThisName
        TOKENSTR_   = f'{COLOR_PURPLE_BOLD}' '%-20s' % str_.__repr__()[1:-1][:20]
        ISALLOWED_  = f'{COLOR_PURPLE_BOLD}'  '%-4s' % ('YES' if isIdentifierAllowed else 'NO')
    else:
        SOURCEFILE_ = f'%{MSPL}s' % ' '
        SOURCELINE_ = f'%{MSLL}s'  % ' '
        TOKENCODE_  = '%10s' % ' '
        TOKENSTR_   = '%20s' % ' '
        ISALLOWED_  = '%4s'  % ' '

    MSG_            = f'{COLOR_YELLOW_BOLD}' '%-50s' % msg
    IDENTIFIER      = f'{COLOR_WHITE_BOLD}'  '%-35s' % ('' if identifier is None else identifier)

    print(f'{SOURCEFILE_} {SOURCELINE_} {TOKENSTR_} {TOKENCODE_} {MSG_} {ISALLOWED_} {IDENTIFIER}')

def PRINTVERBOSE(msg):
    pass

PRINTVERBOSE = PRINT

LOGFMTS = []

# OBS.: NÃO PODE TERMINAR EM $, pois obj$ significaria obj em si
# TODO: FIXME: tem que substituir as constantes :O


def fstringer(fstring):
    last = None
    return ''.join((last := (f(x) if last == '{' else x)) for x in re.split(r'([{]|[}])', fstring.replace(r'\{', '\x00'))).replace('\x00', r'\{')

# tem que fazer melhor do qu eisso. após isolar o "{ F.efw@.x$[1] }", extrair só os identifiers contidos nele e convertê-los individualmente :S
# por enquant os uporta somente "{ MACRO_NAME }"
### SE FOR PARA SUBSTITUIR POR VALOR, tem qwue retirar o {}
# se for para substituir por nome, manter o {}?

#print(f('ALWAYS_AND_FOREVER'))

#print(         repr(r'f"{ALWAYS_AND_FOREVER}"')  )
#print(repr(fstringer('f"{ALWAYS_AND_FOREVER}"')))
#print(f('  $  '))
#print(f('  @  '))
#print(f('  @@@  '))
#input()

def ASSERT_EQ(a, b):
    assert a == b, (a, b)

#assert fstringer("f'{$}'") == "f'{self}'"
#assert fstringer("f'{@}'") == "f'{self.__class__}'"
#assert fstringer(r"f'{$}{$B}\{$}\{$X}{$$}\{@}some{$D$O}nice text{E}{F}here{G}'") == r"f'{self}{self.B}\{$}\{$X}{self.}\{@}some{self.D.O}nice text{E}{F}here{G}'"

# se começa com um . está errado

def faz(old):
    last = new = None
    for name in old:
        if new is not None:
            if name == '$':
                assert last != '@'
                if last == '$':
                    new += '.parent'
            elif name == '@':
                new += ('.parent' if last == '@' else '.__class__')
            else:
                new += '.' + name
        elif name == '$':
            new = 'self'
        elif name == '@':
            new = 'self.__class__'
        else:
            new = name
        last = name
    return new

ASSERT_EQ( faz(['$']) , 'self' )
ASSERT_EQ( faz(['$', '$']) , 'self.parent' )
ASSERT_EQ( faz(['$', '$', '$']) , 'self.parent.parent' )
ASSERT_EQ( faz(['@']) , 'self.__class__' )
ASSERT_EQ( faz(['@', '@']) , 'self.__class__.parent' )
ASSERT_EQ( faz(['@', '@', '@']) , 'self.__class__.parent.parent' )
ASSERT_EQ( faz(['A', '$', '$', '@', 'B', 'C', 'D', '@', 'E', '@']) , 'A.parent.__class__.B.C.D.__class__.E.__class__' )

sources, tokens = collections.deque(), []

for sourcePath in sourcePaths:

    log(f'LOADING SOURCE {COLOR_CYAN}%s' % sourcePath)

    try:
        source = open(sourcePath, 'r').read().encode('utf-8')
    except FileNotFoundError:
        err('FAILED TO LOAD FILE - FILE NOT FOUND')
        exit(1)

    sources.append(source)

MSPL, MSLL = max(map(len, sourcePaths)), len(str(max((source.count(b'\n') for source in sources))))

for sourcePath in sourcePaths:

    source = sources.popleft()

    log(f'PROCESSING SOURCE {COLOR_CYAN}%s' % sourcePath)

    lastLine = lastCode = lastStr = identifier = None

    # É sintaxamente possível haver um identifier?
    isIdentifierAllowed = True

    NEXT_STR_IS_LOG_FMT = None

    # SELFENIZE_ALLOWED = True
    for code, str_, (sourceLine, sourceCol), *_ in tokenize.tokenize(io.BytesIO(source).readline):

        try:
            tokenThisName = tokensNames[code]
        except KeyError:
            err('UNKNOWN TOKEN CODE')
            print((code, str_, sourceLine, sourceCol, _))
            exit(1)

        L0 = True
        PRINT('TOKEN READ')
        L0 = False

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

        # TODO: FIXME: teria que ignorar espaços entre uma "coisa. e.outra" -> acumular todos os espaços até en contrar um (, etc??
        # TODO: FIXME: se está entre parenteses, entao newlines não quebram nomes :/
        #   vou deixar do jeito que está... e esperar que coisas como algo.seila\noutra_coisa deem erro aqui com os asserts

        # TODO: FIXME: vai ter que lembrar da quantidade de espaços quebrados entre os elementos do identifier para questões de número de linha? :/
        if identifier is not None:
            if str_ == '.':
                PRINTVERBOSE('CONTINUING IDENTIFIER - SKIPPING DOT')
                assert code == TOKEN_OPERATOR
                putCode = None
            elif str_ in ('$', '@'):
                PRINTVERBOSE('CONTINUING IDENTIFIER')
                assert (code, str_) in ((TOKEN_ERROR, '$'), (TOKEN_OPERATOR, '@'))
                identifier.append(str_)
                isIdentifierAllowed = True
                putCode = None
            elif code == TOKEN_NAME and str_ not in NON_IDENTIFIERS:
                PRINTVERBOSE('CONTINUING IDENTIFIER')
                assert isIdentifierAllowed is True
                identifier.append(str_)
                isIdentifierAllowed = False # Não pode palavra depois de palavra
                putCode = None # A palavra está nesse nosso buffer temporário; não escreve ela ainda
            else: # Terminoiu
                assert identifier
                PRINTVERBOSE('CONTINUING IDENTIFIER - NOT PART OF IT')
                identifier = faz(identifier)
                PRINTVERBOSE('JOINED')
                try: # Substitutions
                    identifier = repr(REPLACE[identifier])
                except KeyError:
                    pass
                else:
                    PRINT('REPLACED')

                *obj, method = identifier.rsplit('.', 1)
                # NOTE: quando implementar em C, pode passar None como objeto ao invés de task =]
                obj, = (obj if obj else ('TASK',))

                if method in ('dbg', 'log', 'warn', 'err'):
                    NEXT_STR_IS_LOG_FMT = (sourcePath, sourceLine, ('dbg', 'log', 'warn', 'err').index(method))
                    putCode, putStr = TOKEN_NAME, f'LOG_({len(LOGFMTS)},id({obj})'
                else: # Insere ele antes deste
                    tokens.append((TOKEN_NAME, identifier))
                # finalizando o identifier =] - ver se é um objeto.dbg() :O
                identifier =  None
                # TODO: FIXME: tem que quebrar essas inserções em tokens? nome dot, nome, etc? :/
        elif str_ in ('$', '@') or (isIdentifierAllowed is True and code == TOKEN_NAME and str_ not in (*KEYWORDS, *OPERATORS, *BUILTIN_TYPES, *BUILTIN_FUNCTIONS, *BUILTIN_CONSTANTS, *MODULES)):
            # Note que podemos ter um $ mesmo que não possamos ter um identifier - ex nome $ nome2
            PRINTVERBOSE('STARTING IDENTIFIER')
            assert str_ in ('$', '@') or (code == TOKEN_NAME and isIdentifierAllowed is True)
            identifier = [str_]
            putCode = None
        elif code == TOKEN_STRING:
            if str_.startswith('f'):
                #str_ = fstringer(str_) #repr()
                # coloca o que for constante dentro da própria literal
                # ('=P' '\x1b[31m"ewfwegwegew\'wefw!{1}' 's2' f'{x}' '\x1b[0m' 'xD')
                putStr = '(' + ' '.join([( repr(REPLACE[x2]) if (x2 := x[1:-1].strip()) in REPLACE else ('f' if '{' in x else '') + repr(''.join([faz([t for t in re.split(r'([$@.])', t) if t and t != '.']) for t in re.split(r'([\s\[\]\(\)"+%*/-])', x) if t]))) for x in re.split(r'([{][^}]*[}])',  str_[2:-1]) if x]) + ')'
                #'(' + ' '.join([( repr(REPLACE[x2]) if (x2 := x[1:-1].strip()) in REPLACE else ('f' if '{' in x else '') +repr(x)) for x in re.split(r'([{][^}]*[}])',  str_[2:-1]) if x]) + ')'

            if NEXT_STR_IS_LOG_FMT:
                LOGFMTS.append((*NEXT_STR_IS_LOG_FMT, eval(putStr)))
                NEXT_STR_IS_LOG_FMT = None
                putCode = None
        else:
            PRINTVERBOSE('NOT STARTING/CONTINUING IDENTIFIER / NOT STRING')
            assert NEXT_STR_IS_LOG_FMT is None

        if code == TOKEN_NEWLINE:
            PRINTVERBOSE('IDENTIFIER ALLOWED BY LINE')
            assert identifier is None
            isIdentifierAllowed = True
        elif str_ in ('$', '@',
            '[', ']', '(', ')', '{', '}', ':', ';', '<', '>', '<=', '>=', '=', '~', '+', '-', '/', '*', '%', '&', '|', '^', '~', '.', ':', ',', '==', '**', ':=',
            'as', 'is', 'from', 'in', 'not', 'and',
            'def', 'class', 'while',
            'except', 'raise', 'await', 'global',
            'return', 'yield',
            'assert', 'if', 'elif', 'else', 'while', 'for',
            'del', '#',
            ):
            PRINTVERBOSE('IDENTIFIER ALLOWED BY OPERATOR')
            isIdentifierAllowed = True
        elif code in (TOKEN_NL, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_COMMENT):
            if isIdentifierAllowed is True:
                PRINTVERBOSE('IDENTIFIER STILL ALLOWED')
            else:
                PRINTVERBOSE('IDENTIFIER STILL NOT ALLOWED')
                assert isIdentifierAllowed is False
        else:
            PRINTVERBOSE('IDENTIFIER NOT ALLOWED ANYMORE')
            isIdentifierAllowed = False

        # quebra de linha, encoding/começo de arquivo
        #isIdentifierAllowed = code in (TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_ENDMARKER) or

        assert not (is_blank(str_) and code not in (
            TOKEN_NL,
            TOKEN_NEWLINE,
            TOKEN_INDENT,
            TOKEN_DEDENT,
            TOKEN_ENDMARKER,
            TOKEN_ERROR,
            ))

        if putCode is not None:
            tokens.append((putCode, putStr))

        lastCode, lastStr, lastLine = code, str_, sourceLine

        PRINTVERBOSE('TOKEN DONE')

    source = None

log(f'TOKENS: {COLOR_CYAN}%d' % len(tokens))

tokens[1] = (TOKEN_NAME, ';'.join((f'LOGFMTS={LOGFMTS}', *(f'{k}={repr(v)}' for k,v in REPLACE.items()))) )

tokens = tokenize.untokenize(tokens)

log(f'SIZE: {COLOR_CYAN}%d' % len(tokens))

log(f'OUTPUTING TO FILE {COLOR_CYAN}%s' % outputPath)

# TODO: FIXME: autoexecuta o CPP
open(outputPath, 'wb').write(tokens)

# TODO: FIXME: usa o grep para procurar possíveis erros tb
# TODO: FIXME: converteos replacements e macros
# TODO: FIXME: gera o info
os.system(f'python {outputPath}')

# TODO: FIXME: teoricamente seria possível u sar uma linked list dos tokens e ir executando regras :/

# ou pelo menos carregar todos os arquivos em um único deque, inserindo  amarca fFILEEND  ou deixa ro filestart
# começar o deque com um deque((ENCODING,))
# e simplesmente retirar eles
