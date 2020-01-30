#!/usr/bin/python

# TODO: FIXME: substituir por ouitro nome assim forçar usa r$ e @, mas dar erro se tentar acessar paths inválidos :/

# $ é usado mesmo nas classes, significa SELF, SI MESMO, SHORTCUT, AQUILO QUE ESTÁ SENDO EXECUTADO
# se eu pego um objeto e digo "o seu $", como em objeto$, estou dizendo entre nele e assuma ele como si, então objeto$nome é ele dizendo "o meu nome"
print(f'olha {str$upper}')
# se substituir o nome por uma constante, então já estou fazendo o trabalho do {}, então tem que retirar eles!
# mas suportar coisas do tipo f'CONSTANTE[1:-1]'
# coisas mais complexas tem que usar DEFINITION ao invés d econstante

# DEFINITIONS NÃO SUBSTITUIRAO O VALOR, apenas serão incluidos o nome=valor no source
# solução: as constantes só podem ser {}
print(f'{COLOR_RED_BOLD}LOVE{COLOR_RESET}' )
print(ALWAYS_AND_FOREVER)

import sys
import time
import random
import signal
import asyncio

create_connection = None
signal_handler_quit = None

def _ASSERT(condition): assert condition

# id(logged) -> NAME
LOGNAMES = {}

#      0, id(logged), NAME      - é registro de novo logged
# LOG_ID, id(logged), values
logMsgs = []

def LOG_(fmt, loggedID, values):
    print(LOGNAMES[loggedID], fmt % values)

# USAR TAMBÉM uma macro, mas coisas externas não enxergarão :/
# a macro vai extrair o arquivo, linha, classe, fmt e level automaticamente
# task ID e valores são variáveis
def dbg   (logged, fmt, *values): LOG_ (fmt, id(logged), values)
def log   (logged, fmt, *values): LOG_ (fmt, id(logged), values)
def warn  (logged, fmt, *values): LOG_ (fmt, id(logged), values)
def err   (logged, fmt, *values): LOG_ (fmt, id(logged), values)

# se chamar log('mensagem')         -> LOG_(logID, id(TASK),   values)
# se chamar log(objeto, 'mensagem') -> LOG_(logID, id(objeto), values)

# Transforma qualquer objeto em algo logged
def LOGGED_NEW(logged, name):
    _ASSERT(isinstance(name, str))
    # Cadastra no nosso próprio map
    LOGNAMES[id(logged)] = name
    # Emite mensagem de crianção de um logged
    logMsgs.append((0, id(logged), name))
    return logged

TASK = None
TASKSINCE = None
TASKS = []

def TASK_NEW(task, name):
    LOGGED_NEW(task, name)
    task.runCount = 0
    task.runTime = 0
    TASKS.append(task)
    _ASSERT(len(TASKS) <= len(LOGNAMES))
    return task

def TASK_ENTER(task):
    global TASK
    global TASKSINCE
    _ASSERT(TASK is main)
    TASK = task
    TASK.runCount += 1
    TASKSINCE = LOOPTIME()

def TASK_EXIT(task):
    global TASK
    global TASKSINCE
    _ASSERT(TASK is task)
    TASK.runCount += 1
    TASK.runTime += LOOPTIME() - TASKSINCE
    TASK = main
    TASKSINCE = LOOPTIME()

class Core:

    def init_($, parent):
        log('INIT_')
        _ASSERT(issubclass($, Core))
        _ASSERT(issubclass(parent, Core) or parent is None)
        $$ = parent
        if $$ is not None:
            $$childs.append($)
        $childs = []
        $instances = []
        $init($)

    def init($):
        log('INIT')

    def init2_($):
        log('INIT2_')
        $init2($)

    def init2($):
        log('INIT2')

    def init3_($):
        log('INIT3_')
        $init3($)

    def init3($):
        log('INIT3')

    def init_instance_($, parent):
        log('INIT_INSTANCE_')
        _ASSERT(isinstance($, Core))
        $$ = parent
        if $$ is not None:
            $$childs.append($)
        @instances.append($)
        $childs = []
        $instances = []
        $init_instance()

    def init_instance($):
        log('INIT_INSTANCE')

    def init_end_($):
        log('INIT_END_')
        $init_end($)

    def init_end($):
        log('INIT_END')

class Top(Core):
    pass

class Sub(Core):
    pass

class TopSample(Top):

    def init($):
        log('INICIANDO O MEU LINDO SAMPLE 1')
        $n = 1
        $symbolsKnown = set()
        assert $ is TopSample

    def init_end($):
        log('TERMINANDO DE INICIAR O SAMPLE 1')

    class SubSample(Sub):

        def init($):
            log('INICIALIZANDO O SAMPLE 2')
            $n = 5
            $listA = 1, 2, 3
            $listB = $$symbolsKnown
            assert $ is TopSample.SubSample

        def init_instance($):
            log('SOU UMA INSTANCIA DO SAMPLE 2')
            assert @ is $_

        async def next($):
            pass

        async def do($):

            await $next()

            log('')

            @@symbolsKnown.add(symbol)

            instancia$

            # Atributo de uma instância
            instancia$attribute

            # Atributo da classe de uma instância
            instancia@attribute

            # Atributo da classe da classe de uma instancia
            instancia@@attribute

            # Minha própria classe
            @

class A(Top):
    def init($): $n = 2
    class A0(Sub):
        def init($): $n = 10
    class A1(Sub):
        def init($): $n = 10
    class A2(Sub):
        def init($): $n = 10

class B(Top):
    def init($): $n = 2
    class B0(Sub):
        def init($): $n = 3
    class B1(Sub):
        def init($): $n = 3
    class B2(Sub):
        def init($): $n = 3

async def main():

    log('Welcome')

    # Temos os nomes das subclasses
    # Cria as classes
    for thingName in thingsNames:
        task = TASK_NEW(eval(thingName), thingName)
        TASK_ENTER(task)
        task.init_(task, (eval(thingName.rsplit('.', 1)[0]) if '.' in thingName else None))
        TASK_EXIT(task)

    # Termina de iniciar as classes
    for task in TASKS:
        if task is not main:
            TASK_ENTER(task)
            task.init2_(task)
            TASK_EXIT(task)

    # Agora enforça
    for task in TASKS:
        if task is not main:
            TASK_ENTER(task)
            task.init3_(task)
            TASK_EXIT(task)

    # Cria as sessões de cada classe
    for task in tuple(TASKS):
        if task is not main:
            for instanceID in range(task.n):
                # pega a classe parente dessa classe, depois pega uma instância dessa classe parent
                if task$ is not None:
                    parent = task$instances[instanceID % len(task$instances)]
                    name = f'{parent.loggedName} {task.__name__} [{instanceID}]'
                else:
                    name =                     f'{task.__name__} [{instanceID}]'
                task2 = TASK_NEW(task(), name)
                TASK_ENTER(task2)
                task2.init_instance_(parent)
                TASK_EXIT(task2)

    # Finaliza as classes
    for task in TASKS:
        if not isinstance(task, Core) and task is not main:
            TASK_ENTER    (task)
            task.init_end_(task)
            TASK_EXIT     (task)

    log('Exiting')

    print(thingsNames)
    print(TASKS)
    print(logsNames)

    assert False

    log('ENTERING MAIN LOOP')

    while True:

        log('MAIN LOOP ITER HAUHAHAUAUHAUAHUAUHA')

    log('EXITING MAIN LOOP')

thingsNames = (
    'TopSample', 'TopSample.SubSample',
    'A', 'A.A0', 'A.A1', 'A.A2',
    'B', 'B.B0', 'B.B1', 'B.B2',
    )

loop_ = asyncio.get_event_loop()

# Hooks
create_connection_, loop_.create_connection = loop_.create_connection, create_connection

LOOPTIME = loop_.time
LOOPTIME0 = LOOPTIME()

TASK = TASK_NEW(main, '-')
TASK_ENTER(TASK)

loop_.add_signal_handler(signal.SIGTERM, signal_handler_quit)
loop_.add_signal_handler(signal.SIGUSR1, signal_handler_quit)
loop_.run_until_complete(main())
