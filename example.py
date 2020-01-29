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

def LOG_(fmt, taskID, values):
    print(tasksNames[taskID], fmt % values)

def dbg (*_): TASK.dbg (*_)
def log (*_): TASK.log (*_)
def warn(*_): TASK.warn(*_)
def err (*_): TASK.err (*_)

class Task:
    pass

# Possui log e um loop
class Main(Task):

    def dbg   (fmt, *values): LOG_ (fmt, 0, values)
    def log   (fmt, *values): LOG_ (fmt, 0, values)
    def warn  (fmt, *values): LOG_ (fmt, 0, values)
    def err   (fmt, *values): LOG_ (fmt, 0, values)

    taskID = 0
    taskName = '-'
    runCount = 0
    runTime = 0
    statsTransitions = 0

    def init($):
        pass

    def init_end($):
        pass

    async def loop($):
        pass

def TASK_ENTER(task):

    global TASK
    global TASKSINCE

    assert TASK is Main

    TASK = task
    TASK.statsTransitions += 1
    TASKSINCE = LOOPTIME()

def TASK_EXIT(task):

    global TASK
    global TASKSINCE

    assert TASK is task

    TASK.runTime += LOOPTIME() - TASKSINCE
    TASK.runCount += 1
    TASK = Main
    TASKSINCE = LOOPTIME()

class Core(Task):

    def __init__($, _, parent, name):

        # print($, '__init__', name)

        assert issubclass(_, Core)
        assert isinstance($, _) or issubclass($, _)

        # parecem estar apontando para a classe da instância :/
        #assert not hasattr($, 'taskID')
        #assert not hasattr($, 'name')
        #assert not hasattr($, 'dbg')
        #assert not hasattr($, 'log')
        #assert not hasattr($, 'warn')
        #assert not hasattr($, 'err')

        assert len(tasksObjs) == len(tasksNames)

        # task ID e valores são variáveis
        # a macro vai extrair o arquivo, linha, classe, fmt e level automaticamente
        def dbg   (fmt, *values): LOG_ (fmt, taskID, values)
        def log   (fmt, *values): LOG_ (fmt, taskID, values)
        def warn  (fmt, *values): LOG_ (fmt, taskID, values)
        def err   (fmt, *values): LOG_ (fmt, taskID, values)

        $taskID = taskID = len(tasksObjs)
        $taskName = name
        $parent = parent
        $childs = []
        $_ = (_ if isinstance($, _) else None)
        $instances = []
        $runCount = 0
        $runTime = 0
        $statsTransitions = 0
        $log = log
        $dbg = dbg
        $warn = warn
        $err = err

        if $parent is not None:
            $parent.childs.append($)

        if $_ is not None:
            $_.instances.append($)

        tasksObjs.append($)
        tasksNames.append($taskName)

    def init_($):
        log('INIT_')
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

    def init_instance_($):
        log('INIT_INSTANCE_')
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

TASK = Main
TASKSINCE = None
tasksObjs = [Main]
tasksNames = ['-'] # na verdade só precisa ser usado na hora d esalvar os logs :/

TASK_ENTER(Main)

log('Welcome')

thingsNames = (
    'TopSample', 'TopSample.SubSample',
    'A', 'A.A0', 'A.A1', 'A.A2',
    'B', 'B.B0', 'B.B1', 'B.B2',
    )

# Temos os nomes das subclasses

# Cria as classes
for thingName in thingsNames:
    thing = eval(thingName)
    Core.__init__(thing, thing, (eval(thingName.rsplit('.', 1)[0]) if '.' in thingName else None), thingName)
    TASK_ENTER(thing)
    thing.init_(thing)
    TASK_EXIT(thing)

# Termina de iniciar as classes
for task in tasksObjs:
    if task is not Main:
        TASK_ENTER(task)
        task.init2_(task)
        TASK_EXIT(task)

# Agora enforça
for task in tasksObjs:
    if task is not Main:
        TASK_ENTER(task)
        task.init3_(task)
        TASK_EXIT(task)

# Cria as sessões de cada classe
for task in tuple(tasksObjs):
    if task is not Main:
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
                name = f'{parent.taskName} {task.__name__} [{instanceID}]'

            # TASK_EXIT(task)
            instance = task(task, parent, name)

            TASK_ENTER(instance)
            instance.init_instance_()
            TASK_EXIT(instance)

            # TASK_ENTER(task)
        # TASK_EXIT(task)

# Inicia tudo o que for sessão
for task in tasksObjs:
    if isinstance(task, Core):
        TASK_ENTER(task)
        task.init_instance_()
        TASK_EXIT(task)

# Finaliza as classes
for task in tasksObjs:
    if not isinstance(task, Core) and task is not Main:
        TASK_ENTER(task)
        task.init_end_(task)
        TASK_EXIT(task)

log('Exiting')

print(thingsNames)
print(tasksObjs)
