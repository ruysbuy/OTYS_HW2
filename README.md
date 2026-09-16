# OTYS_HW2
Домашнее задание 2 отус Системное программирование

Перехват SIGFPE и оптимизации компилятора

# Описание

Исследование поведения компиляторов GCC и Clang при различных уровнях оптимизации в задаче перехвата аппаратного исключения деления на ноль (SIGFPE).

# Компиляция

Для сборки использовались четыре варианта:

gcc -O0
gcc -O3
clang -O0
clang -O3

# Анализ ассемблерного кода

Для поиска инструкции деления использовался objdump -d:

objdump -d HW2

Необходимо найти строку с инструкцией div или idiv - это даёт байты инструкции и смещение для корректировки RIP.

Пример для clang -O3:

122a: f7 3c 24    idivl (%rsp)

Смещение - 3 байта, код инструкции - f7 3c 24. Для GCC и Clang коды отличаются.

Обработчик сигнала
```c
static void SigfpeHandler(int signo, siginfo_t *info, void *ucontext)
{
    (void)signo; (void)info;
    ucontext_t *uc = (ucontext_t *)ucontext;
    uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];

    char buf[64];
    int n = snprintf(buf, sizeof buf, "RIP = %p\n", (void *)rip);
    write(STDERR_FILENO, buf, n);

    if (rip[0] == 0xf7 && rip[1] == 0xf9) {
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 2;
    } else if (rip[0] == 0x41 && rip[1] == 0xf7 && rip[2] == 0xfc) {
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 3;
    } else if (rip[0] == 0xf7 && rip[1] == 0xf9) {
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 2;
    } else if (rip[0] == 0xf7 && rip[1] == 0x3c && rip[2] == 0x24) {
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 3;
    } else {
        _exit(EXIT_FAILURE);
    }
}
```
Функция определяет инструкцию деления по байтам, обнуляет RAX/RDX (результат деления) и сдвигает RIP за инструкцию, предотвращая падение программы.

Влияние оптимизаций

Проанализированы четыре сборки: gcc -O0, gcc -O3, clang -O0, clang -O3. В каждом случае исследовались функции ScenarioA и ScenarioB.

1. gcc -O0

Явные инструкции деления и последующая проверка делителя:

ScenarioA:
```
movl $0x64,-0xc(%rbp) — делимое 100
movl $0x0,-0x8(%rbp) — делитель 0 (volatile)
idiv %ecx — деление
cmpl $0x0,-0x4(%rbp) — проверка результата
test %eax,%eax — проверка делителя
ветвление je/jne для вывода "b is 0" или "b is NOT 0"
```
ScenarioB:
```
movl $0x1,-0x10(%rbp) — делимое 1
movl $0x0,-0xc(%rbp) — volatile-переменная
mov -0xc(%rbp),%eax — чтение в локальную
mov %eax,-0x4(%rbp) — сохранение локальной
idivl -0x4(%rbp) — деление
```
аналогичная проверка cmpl $0x0,-0x4(%rbp) и ветвление

SigfpeHandler при -O0 содержит полную логику: snprintf, write, проверку байтов инструкции и корректировку %rip/регистров. Ветвление if (b == 0) присутствует в явном виде.

2. gcc -O3

Функции ScenarioA и ScenarioB объединены в main:

ScenarioA:
```
movl $0x64,0x18(%rsp) — делимое 100
movl $0x0,0x1c(%rsp) — делитель 0
idiv %ecx — деление
test %r12d,%r12d — проверка результата
mov 0x1c(%rsp),%eax — повторное чтение делителя
test %eax,%eax — проверка делителя
ветвление je/jne сохранено
```
ScenarioB:
```
movl $0x1,0xc(%rsp) — делимое 1
movl $0x0,0x10(%rsp) — volatile-переменная
mov 0x10(%rsp),%r12d — чтение в регистр
idiv %r12d — деление
test %r12d,%r12d — проверка делителя
test %eax,%eax — проверка результата
```
ветвление je/jne сохранено

SigfpeHandler оптимизирован: вместо последовательных проверок байтов используется cmpb и je, обнуление регистров — через pxor %xmm0,%xmm0 и movups. Логика обработки SIGFPE сохранена.

3. clang -O0

Явные деления и проверки:

ScenarioA:
```
movl $0x64,-0x4(%rbp)
movl $0x0,-0x8(%rbp)
idiv %ecx
cmpl $0x0,-0xc(%rbp) — проверка результата
cmp $0x0,%eax — проверка делителя
ветвление je/jne
```
ScenarioB:
```
movl $0x1,-0x4(%rbp)
movl $0x0,-0x8(%rbp)
mov -0x8(%rbp),%eax
mov %eax,-0xc(%rbp)
idivl -0xc(%rbp)
```
проверки и ветвления аналогичны

SigfpeHandler содержит развёрнутые проверки байтов, вызовы snprintf, write, exit. Ветвление if (b == 0) присутствует.

4. clang -O3

Функции полностью встроены в main. Для ScenarioB применена оптимизация на основе Undefined Behavior:

ScenarioA:
```
movl $0x64,0x8(%rsp)
movl $0x0,(%rsp)
idivl (%rsp)
test %ebx,%ebx
cmpl $0x0,(%rsp)
```
ветвление je/jne сохранено

ScenarioB:
```
movl $0x1,0x8(%rsp)
movl $0x0,(%rsp)
mov (%rsp),%ecx
idiv %ecx
cmpl $0x0,0x4(%rsp)
```
Ветвление if (b == 0) полностью исчезло! Вместо проверки делителя компилятор знает: деление на ноль - это UB, и если программа дошла до этой точки, делитель не мог быть нулём.

Сводная таблица

| Комбинация | if (b == 0) в ScenarioA | if (b == 0) в ScenarioB | Примечание |
|------------|-------------------------|-------------------------|------------|
| gcc -O0    | Да                      | Да                      | Полный код, все проверки на месте |
| gcc -O3    | Да                      | Да                      | Код оптимизирован, но проверки сохранены |
| clang -O0  | Да                      | Да                      | Полный код, все проверки на месте |
| clang -O3  | Да                      | Нет                     | Ветвление исчезло из-за UB-оптимизации |

Вывод

При clang -O3 компилятор вывел из факта выполнения деления, потому что условие всегда выполняется.

Итог: перехват SIGFPE работает на уровне машинных инструкций и не влияет на статические предположения компилятора. Компилятор, обнаружив целочисленное деление, вправе считать делитель ненулевым и оптимизировать код исходя из этого: удалять проверки b == 0, переставлять их после деления, выбрасывать ветки. Обработчик сигнала не может восстановить удалённую логику и не отменяет UB.

