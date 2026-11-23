# Gamma OS

Навчальна 64-бітна операційна система для архітектури `x86_64` із завантаженням через UEFI (протокол Limine), написана мовою C++20 у режимі `freestanding`.

## Швидкий запуск (NixOS / Linux)

1. Клонувати репозиторій та завантажувач Limine (бінарна гілка v11.x):
```bash
git clone https://github.com/CherryX21/Gamma-OS.git
cd Gamma-OS
git clone https://github.com/limine-bootloader/limine.git --branch=v11.x-binary --depth=1
```
2. Увійти в середовище розробки (або встановити clang, lld, gnumake, qemu, OVMF):
```bash
nix develop
```
3. Запустити юніт-тести та автоматичний тест в QEMU:
```bash
cd kernel
make check
```
4. Запустити систему в інтерактивному режимі:
```
make run
```
