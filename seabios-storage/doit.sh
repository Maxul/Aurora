sudo \
    ../Aurora-qemu-sgx/x86_64-softmmu/qemu-system-x86_64 -enable-kvm \
    -cpu host,+sgx -smp 1,sockets=1,cores=1 -m 4G \
    -machine epc=64M -machine epc-below-4g=auto \
    -vga cirrus -nographic -vnc :0 \
    -hda ../images/ubuntu16.qcow2 \
    -bios ../seabios-storage/out/bios.bin \
    -chardev file,id=seabios,path=/tmp/bios.log \
    -device isa-debugcon,iobase=0x402,chardev=seabios \
    -drive if=none,id=stick,file=./usb.qcow2 \
    -device usb-ehci,id=ehci \
    -device usb-storage,bus=ehci.0,drive=stick

#    -drive file=./usb.qcow2,if=none,id=drv0 -device nvme,drive=drv0,serial=foo
