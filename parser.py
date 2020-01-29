import sys
import os
import tokenize
import io

TOKEN_ENCODING = tokenize.ENCODING
TOKEN_ERROR    = tokenize.ERRORTOKEN
TOKEN_NAME     = tokenize.NAME
TOKEN_NEWLINE  = tokenize.NEWLINE
TOKEN_NUMBER   = tokenize.NUMBER
TOKEN_OPERATOR = tokenize.OP
TOKEN_STRING   = tokenize.STRING

tokensNames = {
    TOKEN_ENCODING: 'ENCODING',
    TOKEN_ERROR   : 'ERRORTOKEN',
    TOKEN_NAME    : 'NAME',
    TOKEN_NEWLINE : 'NEWLINE',
    TOKEN_NUMBER  : 'NUMBER',
    TOKEN_OPERATOR: 'OP',
    TOKEN_STRING  : 'STRING',
    }

BUILTINS = (
    'and',
    'as',
    'assert',
    'async',
    'await',
    'break',
    'class',
    'def',
    'del',
    'dir',
    'elif',
    'else',
    'except',
    'for',
    'from',
    'global',
    'has',
    'hasattr',
    'if',
    'import',
    'in',
    'input',
    'is',
    'isclass',
    'isinstance',
    'issubclass',
    'locals',
    # 'next',
    'None',
    'not',
    'or',
    'pass',
    'print',
    'raise',
    'return',
    'True',
    'while',
    'yield',
    )


definitions = {
    '$': 'self',
    '$$': 'self.parent',
    '@': 'self._',
    '@@': 'self._._',
    'COLOR_BLUE'        : '\x1b[34m',
    'COLOR_CYAN'        : '\x1b[36m',
    'COLOR_CYAN_BOLD'   : '\x1b[36m\x1b[1m',
    'COLOR_WHITE_BOLD'  : '\x1b[37m\x1b[1m',
    'COLOR_YELLOW_BOLD' : '\x1b[33m\x1b[1m',
    'COLOR_GREEN_BOLD'  : '\x1b[32m\x1b[1m',
    'COLOR_RED_BOLD'    : '\x1b[31m\x1b[1m',
    'COLOR_PURPLE_BOLD' : '\x1b[35m\x1b[1m',
    'COLOR_RESET'       : '\x1b\x5b\x30\x6d',
    }

outPath = sys.argv[1]

tokens = []

for fpath in sys.argv[2:]:

    lastCode = None
    lastStr = None

    identifier = None
    MUSTBECLASS = None
    MUSTBEINSTANCE = None

    def PRINT(*msg):
        print('%5d %-90s %-15s %-100s %s' % (thisPos[0], msg[0], *P, ('' if identifier is None else identifier)))

    # SELFENIZE_ALLOWED = True
    for thisCode, thisStr, thisPos, *_ in tokenize.tokenize(io.BytesIO(open(fpath, 'r').read().encode('utf-8')).readline):

        try:
            tokenThisName = tokensNames[thisCode]
        except:
            tokenThisName = '?'

        P = tokenThisName, str([thisStr])[1:-1]

        #  O que acrescentar agora
        code, str_ = thisCode, thisStr

        if identifier is not None:
            assert identifier.endswith('.')
            if thisStr == '@':
                PRINT('ACRESCENTANDO @ COMO _.')
                assert MUSTBEINSTANCE is None
                identifier += '_.'
                code = None
            elif thisStr == '$':
                PRINT('ACRESCENTANDO $ COMO parent.')
                assert MUSTBECLASS is None
                identifier += 'parent.'
                code = None
            elif thisCode == TOKEN_NAME and thisStr not in BUILTINS:
                PRINT('ACRESCENTANDO PALAVRA')
                identifier += thisStr + '.'
                code = None
            elif thisStr == '.':
                PRINT('CONTINUANDO IGNORANDO .')
                assert lastStr != '.'
                assert lastCode == TOKEN_NAME
                code = None
            else: # Insere ele antes da próxima coisa
                PRINT('FLUSHING')
                tokens.append((TOKEN_NAME, identifier[:-1]))
                # finalizando o identifier =] - ver se é um objeto.dbg() :O
                identifier = None
                MUSTBECLASS = None
                MUSTBEINSTANCE = None
        elif thisStr == '$':
            PRINT('COMECANDO COM UM $')
            assert identifier is None
            assert MUSTBECLASS is None
            assert MUSTBEINSTANCE is None
            identifier = 'self.'
            MUSTBEINSTANCE = True
            MUSTBECLASS = None
            code = None
        elif thisStr == '@':
            PRINT('COMECANDO COM UM @')
            assert identifier is None
            assert MUSTBECLASS is None
            assert MUSTBEINSTANCE is None
            identifier = 'self._.'
            MUSTBECLASS = True
            MUSTBEINSTANCE = None
            code = None
        elif thisCode == TOKEN_NAME and thisStr not in BUILTINS:
            PRINT('COMECANDO COM CARACTERE NORMAL')
            assert identifier is None
            assert MUSTBECLASS is None
            assert MUSTBEINSTANCE is None
            identifier = thisStr + '.'
            code = None
        else:
            PRINT('NAO ESTOU MONTANDO IDENTIFIER, E NEM COMECOU UM NOVO')
            assert identifier is None
            assert MUSTBECLASS is None
            assert MUSTBEINSTANCE is None
            assert thisStr != '.'

        if code is not None:
            tokens.append((code, str_))

        lastCode, lastStr = thisCode, thisStr

# TODO: FIXME: autoexecuta o CPP
open(outPath, 'xb').write(tokenize.untokenize(tokens))

# TODO: FIXME: usa o grep para procurar possíveis erros tb
# TODO: FIXME: converteos replacements e macros
# TODO: FIXME: gera o info
os.system(f'python {outPath}')
