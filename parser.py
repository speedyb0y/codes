#!/usr/bin/python

import sys
import os
import tokenize
import io

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

definitions = {
    '$': 'self',
    '$$': 'self.parent',
    '@': 'self._',
    '@@': 'self._._',
    'COLOR_BOLD'        : COLOR_BOLD,
    'COLOR_BLUE'        : COLOR_BLUE,
    'COLOR_CYAN'        : COLOR_CYAN,
    'COLOR_CYAN_BOLD'   : COLOR_CYAN_BOLD,
    'COLOR_WHITE_BOLD'  : COLOR_WHITE_BOLD,
    'COLOR_YELLOW_BOLD' : COLOR_YELLOW_BOLD,
    'COLOR_GREEN_BOLD'  : COLOR_GREEN_BOLD,
    'COLOR_RED_BOLD'    : COLOR_RED_BOLD,
    'COLOR_PURPLE_BOLD' : COLOR_PURPLE_BOLD,
    'COLOR_RESET'       : COLOR_RESET,
    'DBG_COLOR'  : COLOR_GREEN_BOLD,
    'LOG_COLOR'  : COLOR_GREEN_BOLD,
    'WARN_COLOR' : COLOR_PURPLE_BOLD,
    'ERR_COLOR'  : COLOR_RED_BOLD,
    }

# SCRIPT OUTPUT SOURCE_0 SOURCE_1 ... SOURCE_N
assert len(sys.argv) >= 3

# Não pode ter duplicados
# Nenhum pode ser o script em si
# Output não pode ser um source
assert len(sys.argv) == len(set(sys.argv))

outputPath, sourcePaths = sys.argv[1], sys.argv[2:]

tokens = []

def PRINT(msg):
    ISALLOWED_  = f'{COLOR_PURPLE_BOLD}'  '%-4s' % ('YES' if isIdentifierAllowed else 'NO')
    IDENTIFIER  = f'{COLOR_WHITE_BOLD}'  '%-35s' % ('' if identifier is None else identifier)
    PARSERMSG_  = f'{COLOR_YELLOW_BOLD}'  '%-50s' % msg
    print(f'{SOURCEFILE_} {SOURCELINE_} {TOKENSTR_} {TOKENCODE_} {PARSERMSG_} {ISALLOWED_} {IDENTIFIER}')

def is_blank(_):
    return all((_ in ' \n\r\t\v') for _ in _)

for sourcePath in sourcePaths:

    log(f'PROCESSING SOURCE {COLOR_CYAN}%s' % sourcePath)

    SOURCEFILE_ = f'{COLOR_WHITE_BOLD}' '%50s' % sourcePath[-50:]

    lastCode = None
    lastStr = None

    identifier = None
    identifierPath = None

    # É sintaxamente possível haver um identifier?
    isIdentifierAllowed = True

    try:
        source = open(sourcePath, 'r').read().encode('utf-8')
    except FileNotFoundError:
        err('FAILED TO LOAD FILE - FILE NOT FOUND')
        exit(1)

    lastLine = None

    # SELFENIZE_ALLOWED = True
    for code, str_, (sourceLine, sourceCol), *_ in tokenize.tokenize(io.BytesIO(source).readline):

        try:
            tokenThisName = tokensNames[code]
        except KeyError:
            print((code, str_, sourceLine, sourceCol, _))
            raise

        if lastLine == sourceLine:
            SOURCELINE_ = '        '
        else:
            SOURCELINE_ = f'{COLOR_BOLD}{COLOR_WHITE}' '%5d   ' % sourceLine

        TOKENCODE_  = f'{COLOR_GREEN_BOLD}'  '%-10s' % tokenThisName
        TOKENSTR_   = f'{COLOR_PURPLE_BOLD}' '%-20s' % str_.__repr__()[1:-1][:20]

        #  O que acrescentar agora
        putCode, putStr = code, str_

        assert (lastStr, str_) not in (
            ('.', '.'),
            ('.', ','),
            ('.', '('),
            ('.', '{'),
            ('.', '['),
            ('.', '"'),
            (',', '.'),
            ('[', '}'), ('[', ')'),
            ('{', ']'), ('{', ')'),
            ('(', ']'), ('(', '}'),
            )

        # se o último for um ) então podemos ter um PONTo
        # mas não um identifier direto sem ser $@
        #assert not (lastCode != TOKEN_NAME and str_ == '.')

        # Para estar começando deveria ter tido algo antes
        assert not (lastStr == '.' and str_ in ('$', '@',
            'class', 'def', 'assert',
            'try', 'continue', 'except',
            ))

        if identifier is not None:
            assert identifier.endswith('.')
            if str_ == '@':
                PRINT('ACRESCENTANDO @ COMO _.')
                assert identifierPath in (None, '@')
                identifier += '_.'
                identifierPath = '@'
                isIdentifierAllowed = True
                putCode = None
            elif str_ == '$':
                PRINT('ACRESCENTANDO $ COMO parent.')
                assert identifierPath in (None, '$')
                identifier += 'parent.'
                identifierPath = '$'
                isIdentifierAllowed = True
                putCode = None
            elif code == TOKEN_NAME and str_ not in NON_IDENTIFIERS:
                PRINT('ACRESCENTANDO PALAVRA')
                assert isIdentifierAllowed is True
                isIdentifierAllowed = False # Não pode palavra depois de palavra
                identifier += str_ + '.'
                putCode = None # A palavra está nesse nosso buffer temporário; não escreve ela ainda
            elif str_ == '.':
                PRINT('CONTINUANDO IGNORANDO .')
                putCode = None
            else: # Terminoiu
                PRINT('FLUSHING')
                tokens.append((TOKEN_NAME, identifier[:-1])) # Insere ele antes da próxima coisa
                # finalizando o identifier =] - ver se é um objeto.dbg() :O
                identifier = None
                identifierPath = None
                isIdentifierAllowed = None
        elif str_ == '$': # and lastStr in (']', '@', '$')
            PRINT('STARTING IDENTIFIER FROM $')
            assert identifier is None
            assert identifierPath is None
            assert isIdentifierAllowed is True
            identifier = 'self.'
            identifierPath = '$'
            putCode = None
        elif str_ == '@':
            PRINT('STARTING IDENTIFIER FROM @')
            assert identifier is None
            assert identifierPath is None
            assert isIdentifierAllowed is True
            identifier = 'self._.'
            identifierPath = '@'
            putCode = None
        elif isIdentifierAllowed is True and code == TOKEN_NAME and str_ not in (*KEYWORDS, *OPERATORS, *BUILTIN_TYPES, *BUILTIN_FUNCTIONS, *BUILTIN_CONSTANTS, *MODULES):
            PRINT('STARTING IDENTIFIER')
            assert code == TOKEN_NAME
            assert not is_blank(str_)
            assert isIdentifierAllowed is True
            assert identifier is None
            assert identifierPath is None
            assert isIdentifierAllowed is True
            identifier = str_ + '.'
            putCode = None
        else:
            PRINT('NOT STARTING/CONTINUING IDENTIFIER')
            assert identifier is None
            assert identifierPath is None

        # quebra de linha, encoding/começo de arquivo
        isIdentifierAllowed = code in (TOKEN_NL, TOKEN_NEWLINE, TOKEN_INDENT, TOKEN_DEDENT, TOKEN_ERROR, TOKEN_ENDMARKER) or str_ in (
            '$', '@',
            '[', ']', '(', ')', '{', '}',
            '~', '+', '-', '/', '*', '%', '&', '|', '^', '~', '.', ':', ',', '==', '**',
            'as', 'is', 'from', 'in',
            'def', 'class', 'while',
            'except', 'raise', 'await', 'global',
            'return', 'yield',
            'assert', 'if', 'elif', 'else', 'while', 'for',
            'del', '#',
            )

        # s eé comment, identifier anula

        assert not (is_blank(str_) and code not in (
            TOKEN_NL,
            TOKEN_NEWLINE,
            TOKEN_INDENT,
            TOKEN_DEDENT,
            TOKEN_ENDMARKER,
            TOKEN_ERROR,
            ))

        # # and COMMENT

        if putCode is not None:
            tokens.append((putCode, putStr))

        lastCode, lastStr, lastLine = code, str_, sourceLine

    source = None

log(f'TOKENS: {COLOR_CYAN}%d' % len(tokens))

tokens = tokenize.untokenize(tokens)

log(f'SIZE: {COLOR_CYAN}%d' % len(tokens))

log(f'OUTPUTING TO FILE {COLOR_CYAN}%s' % outputPath)

# TODO: FIXME: autoexecuta o CPP
open(outputPath, 'wb').write(tokens)

# TODO: FIXME: usa o grep para procurar possíveis erros tb
# TODO: FIXME: converteos replacements e macros
# TODO: FIXME: gera o info
os.system(f'python {outputPath}')
