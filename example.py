# INVÁLIDOS
# $@@
# $@@something
# $@$
# @$@
# $.
# @._

# TODO: FIXME: substituir por ouitro nome assim forçar usa r$ e @, mas dar erro se tentar acessar paths inválidos :/

# $ é usado mesmo nas classes, significa SELF, SI MESMO, SHORTCUT, AQUILO QUE ESTÁ SENDO EXECUTADO
# se eu pego um objeto e digo "o seu $", como em objeto$, estou dizendo entre nele e assuma ele como si, então objeto$nome é ele dizendo "o meu nome"

import sys
import time
import asyncio

from time import time as LOOPTIME

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
def LOGGED(logged, name):
    assert isinstance(name, str)
    # Cadastra no nosso próprio map
    LOGNAMES[id(logged)] = name
    # Emite mensagem de crianção de um logged
    logMsgs.append((0, id(logged), name))
    return logged

def TASK(task, name):
    LOGGED(task, name)
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
        assert issubclass($, Core)
        assert issubclass(_, Core)
        @, $$, $childs, $instances = $, parent, [], []
        if @@ is not None:
            @@childs.append($)
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

    def init_instance_($, _, parent):
        log('INIT_INSTANCE_')
        assert isinstance($, _)
        @, $$, $childs, $instances = _, parent, [], []
        @instances.append($)
        if $$ is not None:
            $$childs.append($)
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

def main():

    # Temos os nomes das subclasses
    # Cria as classes
    for thingName in thingsNames:
        thing = eval(thingName)
        TASK_INIT(thing, thing, (eval(thingName.rsplit('.', 1)[0]) if '.' in thingName else None), thingName)
        TASK_ENTER(thing)
        thing.init_(thing)
        TASK_EXIT(thing)

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
            # TASK_ENTER(task)
            # log('CREATING MY %d INSTANCES', task.n)
            for instanceID in range(task.n):
                # print(task, task.n, instanceID)
                # log('WILL CREATE MY INSTANCE %d/%d ', instanceID, task.n)

                # pega a classe parente dessa classe, depois pega uma instância dessa classe parent
                if parent := task.parent:
                    parent = parent.instances[instanceID % len(parent.instances)]

                if parent is None:
                    name =                   f'{task.__name__} [{instanceID}]'
                else:
                    name = f'{parent.loggedName} {task.__name__} [{instanceID}]'

                # TASK_EXIT(task)
                instance = task(task, parent, name)

                TASK_ENTER(instance)
                instance.init_instance_()
                TASK_EXIT(instance)

                # TASK_ENTER(task)
            # TASK_EXIT(task)

    # Inicia tudo o que for sessão
    for task in TASKS:
        if isinstance(task, Core):
            TASK_ENTER          (task)
            task.init_instance_ ()
            TASK_EXIT           (task)

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

TASK = None
TASKSINCE = None
TASKS = []

TASK_ENTER(TASK_INIT(main, '-'))

log('Welcome')

thingsNames = (
    'TopSample', 'TopSample.SubSample',
    'A', 'A.A0', 'A.A1', 'A.A2',
    'B', 'B.B0', 'B.B1', 'B.B2',
    )

main()
