# Installing toolchain

Download and unpack MSP430-GCC, e.g.

```
mkdir -p ~/msp430-gcc
cd ~/msp430-gcc
wget http://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/MSPGCC/6_00_00_900/exports/msp430-gcc-7.3.0.9_linux64.tar.bz2
tar xvjf msp430-gcc-7.3.0.9_linux64.tar.bz2
```

Finally, add toolchain to the PATH:
```
export PATH=$PATH:~/msp430-gcc/msp430-gcc-7.3.0.9_linux64/bin
```
