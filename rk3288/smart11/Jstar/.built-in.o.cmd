cmd_drivers/misc/Jstar/built-in.o :=  ../prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi-ld -EL    -r -o drivers/misc/Jstar/built-in.o drivers/misc/Jstar/ec25-e/built-in.o ; scripts/mod/modpost drivers/misc/Jstar/built-in.o