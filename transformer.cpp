/*

*/

// TODO: FIXME: um ASSERT() que funciona mesmo que inlined em alguma coisa :/

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifndef DEBUG
#   define DEBUG 0
#endif

#ifndef ASSERT_FATAL
#   define ASSERT_FATAL 0
#endif

#ifndef ASSERT_SIMPLE
#   define ASSERT_SIMPLE 0
#endif

#ifndef ASSERT_NORMAL
#   define ASSERT_NORMAL 0
#endif

#ifndef ASSERT_SUPPRESS
#   define ASSERT_SUPPRESS 0
#endif

// Se nenhum for especificado, então default
#if !(ASSERT_FATAL || ASSERT_SIMPLE || ASSERT_NORMAL || ASSERT_SUPPRESS)
#   if DEBUG
#       undef  ASSERT_SUPPRESS
#       define ASSERT_SUPPRESS 1
#   else
#       undef  ASSERT_SUPPRESS
#       define ASSERT_SUPPRESS 1
#   endif
#endif

#ifndef DEBUG_VALUE
#   if DEBUG
#       define DEBUG_VALUE 1
#else
#       define DEBUG_VALUE 0
#   endif
#endif

//
#if ASSERT_FATAL
#     define _ASSERT(condition) assert (condition), (sys.stdout.flush(), sys.stderr.flush(), os.write(1, (__FILE__ ":" + TOSTRING(__LINE__) + " ASSERT FAILED: " #condition "\n").encode()), os._exit(1))
#elif ASSERT_SIMPLE
#     define _ASSERT(condition) assert (condition), (sys.stdout.flush(), sys.stderr.flush(), os.write(1, (__FILE__ ":" + TOSTRING(__LINE__) + " ASSERT FAILED: " #condition "\n").encode()))
#elif ASSERT_NORMAL
#     define _ASSERT(condition) assert (condition)
#else // ASSERT_SUPPRESS
#     define _ASSERT(condition) pass
#endif

// Mostra o valor de uma expressão
// TODO: FIXME: remover _DEBUG
#if DEBUG_VALUE
#   define _VALUE(exp) (_ if not print(__FILE__ ":" TOSTRING(__LINE__) " " #exp " ===>", (_ if isinstance((_ := (exp)), (str, bool, int, float, bytes, bytearray)) else str(_))) else _)
#   define _DEBUG(exp) (_ if not print(__FILE__ ":" TOSTRING(__LINE__) " " #exp " ===>", (_ if isinstance((_ := (exp)), (str, bool, int, float, bytes, bytearray)) else str(_))) else _)
#else
#   define _VALUE(exp) (exp)
#   define _DEBUG(exp) (exp)
#endif

// Executa uma expressão quando debugando
#if DEBUG
#   define _DBG(exp) exp
#else
#   define _DBG(exp) pass
#endif

#if 1
#   define _FATAL (sys.stdout.flush(), sys.stderr.flush(), os.write(1, (__FILE__ ":" + TOSTRING(__LINE__) + " FATAL!!!\n").encode()), os._exit(1))
#else
#   define _FATAL pass
#endif

// mas não dá para ter o @ :/
#define $ self
#define $$ self.parent
#define $$$ self.parent.parent

//  Mas sem os testes
#if 0
def REACHED(when):
    _ASSERT(isinstance(when, (int, float)))
    return when <= LOOPTIME()
#else
#   define REACHED(when) ((when) <= LOOPTIME())
#endif

#if 0
def FROMNOW(_): # TODO: FIXME: allow something like LOOPTIME_FUTURE
    _ASSERT(isinstance(_, (int, float)))
    // _ASSERT(all((_ <= 0x4000000) for _ in _)) # nao dá para checar isso pq é clock monotonico
    // TODO: FIXME: se for _FUTURE/PAST mantem ele sem somar
    return LOOPTIME() + _
#else
#   define FROMNOW(_) (LOOPTIME() + (_))
#endif

#if 0
// _TRACE priunt(reached file, line)
#   define _TRACE pass
#else
#   define _TRACE pass
#endif

/*
def TIMEOUT(*_): # TODO: FIXME: allow something like LOOPTIME_PAST
    _ASSERT(_ := tuple(_))
    _ASSERT(all(isinstance(_, (int, float)) for _ in _))
    # _ASSERT(all((_ == 0 or _ >= 0x4000000) for _ in _)) # nao dá para checar isso pq é clock monotonico
    return min(_) - LOOPTIME()
*/

// para que objeto.dbg() vire objeto.__doc__ e dbg(mensagem) vire __doc__
// TODO: FIXME: implementar isso no tokenizer - qualquer objeto.objeto.objeto.dbg(str) virará pass
#define dbg(fmt, ...)  __doc__
